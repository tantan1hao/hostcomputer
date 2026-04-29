#!/usr/bin/env python3
import json
import os
import socket
import subprocess
import sys
import time
from typing import Any, Dict, List


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BRIDGE = os.path.join(ROOT, "ros1_bridge", "host_bridge_node.py")
CONFIG = os.path.join(ROOT, "ros1_bridge", "video_sources.example.yaml")
HOST = "127.0.0.1"
PORT = int(os.getenv("VIDEO_MANAGER_TEST_PORT", "19194"))

sys.path.insert(0, os.path.join(ROOT, "ros1_bridge"))
from video_manager import VideoManager, crop_filter_for_aspect, load_video_config  # noqa: E402


class JsonLineClient:
    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.create_connection((host, port), timeout=1.0)
        self.sock.settimeout(1.0)
        self.buffer = b""

    def close(self) -> None:
        self.sock.close()

    def send(self, payload: Dict[str, Any]) -> None:
        line = json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n"
        self.sock.sendall(line.encode("utf-8"))

    def recv(self, timeout: float = 1.0) -> Dict[str, Any]:
        deadline = time.time() + timeout
        while b"\n" not in self.buffer:
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for JSON line")
            self.sock.settimeout(remaining)
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("connection closed")
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line.decode("utf-8"))

    def recv_until(self, msg_type: str, timeout: float = 1.0) -> Dict[str, Any]:
        deadline = time.time() + timeout
        seen: List[str] = []
        while time.time() < deadline:
            msg = self.recv(max(0.01, deadline - time.time()))
            seen.append(str(msg.get("type")))
            if msg.get("type") == msg_type:
                return msg
        raise AssertionError(f"expected {msg_type}, seen {seen}")


def now_ms() -> int:
    return int(time.time() * 1000)


def wait_for_port(timeout: float = 3.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((HOST, PORT), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"bridge did not open {HOST}:{PORT}")


def stop_process(proc: subprocess.Popen) -> None:
    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def test_config_and_runner() -> None:
    cfg = load_video_config(CONFIG)
    assert cfg.rtsp.host == "192.168.1.50"
    assert cfg.rtsp.transport == "tcp"
    assert len(cfg.direct_sources) == 2
    assert cfg.direct_sources[0].crop_aspect == "4:3"
    assert crop_filter_for_aspect(1280, 720, "4:3") == "crop=960:720:160:0"

    manager = VideoManager(cfg, dry_run=True)
    started = manager.start(0)
    assert started["online"] is True
    assert started["camera_id"] == 0
    assert started["rtsp_url"] == "rtsp://192.168.1.50:8554/front_raw"
    assert started["rtsp_transport"] == "tcp"
    assert started["width"] == 960
    assert started["height"] == 720
    assert started["source_width"] == 1280
    assert started["source_height"] == 720
    assert started["crop_aspect"] == "4:3"
    assert "-vf crop=960:720:160:0" in started["command"]
    assert "-pix_fmt yuv420p" in started["command"]
    assert "-profile:v baseline" in started["command"]
    assert "-rtsp_transport tcp" in started["command"]
    assert "rtsp://127.0.0.1:8554/front_raw" in started["command"]

    stopped = manager.stop(0)
    assert stopped["online"] is False

    restarted = manager.restart(0)
    assert restarted["online"] is True
    assert restarted["restart_count"] == 1


def test_bridge_video_requests() -> None:
    proc = subprocess.Popen(
        [
            sys.executable,
            BRIDGE,
            "--dry-run",
            "--host",
            HOST,
            "--port",
            str(PORT),
            "--video-config",
            CONFIG,
            "--video-dry-run",
        ],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    wait_for_port()
    try:
        client = JsonLineClient(HOST, PORT)
        try:
            hello = client.recv_until("hello")
            assert hello["bridge_name"] == "host_bridge_node"
            capabilities = client.recv_until("capabilities")
            assert "camera_stream_request" in capabilities["supports"]
            assert len(capabilities["cameras"]) == 2
            assert capabilities["cameras"][0]["online"] is False

            client.send({
                "type": "camera_list_request",
                "protocol_version": 1,
                "seq": 1,
                "timestamp_ms": now_ms(),
            })
            camera_list = client.recv_until("camera_list_response")
            assert camera_list["seq"] == 1
            assert camera_list["cameras"][0]["source_id"] == "front_raw"

            client.send({
                "type": "camera_stream_request",
                "protocol_version": 1,
                "seq": 2,
                "timestamp_ms": now_ms(),
                "params": {"camera_id": 0, "action": "start"},
            })
            ack = client.recv_until("ack")
            assert ack["seq"] == 2
            assert ack["ack_type"] == "camera_stream_request"
            assert ack["ok"] is True
            camera_info = client.recv_until("camera_info")
            assert camera_info["camera_id"] == 0
            assert camera_info["online"] is True
            assert camera_info["rtsp_url"] == "rtsp://192.168.1.50:8554/front_raw"

            client.send({
                "type": "camera_stream_request",
                "protocol_version": 1,
                "seq": 3,
                "timestamp_ms": now_ms(),
                "params": {"camera_id": 0, "action": "stop"},
            })
            ack = client.recv_until("ack")
            assert ack["seq"] == 3
            assert ack["ok"] is True
            camera_info = client.recv_until("camera_info")
            assert camera_info["camera_id"] == 0
            assert camera_info["online"] is False
        finally:
            client.close()
    finally:
        stop_process(proc)


def main() -> None:
    test_config_and_runner()
    test_bridge_video_requests()
    print("video manager smoke: PASS")


if __name__ == "__main__":
    main()
