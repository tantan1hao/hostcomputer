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
HOST = "127.0.0.1"
PORT = int(os.getenv("BRIDGE_LOOPBACK_PORT", "19091"))


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
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout as exc:
                raise TimeoutError("timed out waiting for JSON line") from exc
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

    def recv_status(self, status: str, timeout: float = 1.0) -> Dict[str, Any]:
        deadline = time.time() + timeout
        seen: List[str] = []
        while time.time() < deadline:
            msg = self.recv(max(0.01, deadline - time.time()))
            seen.append(f"{msg.get('type')}:{msg.get('operator_input_status')}")
            if msg.get("type") == "system_status" and msg.get("operator_input_status") == status:
                return msg
        raise AssertionError(f"expected system_status {status}, seen {seen}")


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


def start_bridge() -> subprocess.Popen:
    proc = subprocess.Popen(
        [
            sys.executable,
            BRIDGE,
            "--dry-run",
            "--host",
            HOST,
            "--port",
            str(PORT),
            "--watchdog-ms",
            "150",
        ],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    wait_for_port()
    return proc


def stop_bridge(proc: subprocess.Popen) -> None:
    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def main() -> None:
    proc = start_bridge()
    try:
        client = JsonLineClient(HOST, PORT)
        try:
            hello = client.recv_until("hello")
            assert hello["protocol_version"] == 1
            assert hello["bridge_name"] == "host_bridge_node"

            capabilities = client.recv_until("capabilities")
            assert "operator_input" in capabilities["supports"]
            assert "watchdog" in capabilities["supports"]

            client.send({
                "type": "heartbeat",
                "protocol_version": 1,
                "seq": 1,
                "timestamp_ms": now_ms(),
            })
            heartbeat_ack = client.recv_until("heartbeat_ack")
            assert heartbeat_ack["seq"] == 1

            client.send({
                "type": "sync_request",
                "protocol_version": 1,
                "seq": 2,
                "timestamp_ms": now_ms(),
                "params": {"reason": "bridge_loopback"},
            })
            snapshot = client.recv_until("system_snapshot")
            assert snapshot["seq"] == 2
            assert snapshot["emergency"]["active"] is False

            client.send({
                "type": "camera_list_request",
                "protocol_version": 1,
                "seq": 3,
                "timestamp_ms": now_ms(),
            })
            cameras = client.recv_until("camera_list_response")
            assert cameras["seq"] == 3
            assert len(cameras["cameras"]) >= 1

            client.send({
                "type": "operator_input",
                "protocol_version": 1,
                "seq": 4,
                "timestamp_ms": now_ms(),
                "ttl_ms": 500,
                "mode": "vehicle",
                "keyboard": {"pressed_keys": ["w", "d"]},
                "gamepad": {"connected": False, "buttons": {}, "axes": {}},
            })
            accepted = client.recv_status("accepted")
            assert accepted["last_operator_input_seq"] == 4
            assert accepted["cmd_vel"]["linear_x"] > 0
            assert accepted["cmd_vel"]["angular_z"] < 0

            watchdog = client.recv_status("watchdog_timeout", timeout=1.0)
            assert watchdog["watchdog_active"] is True
            assert watchdog["cmd_vel"]["linear_x"] == 0.0
            assert watchdog["cmd_vel"]["angular_z"] == 0.0

            client.send({
                "type": "emergency_stop",
                "protocol_version": 1,
                "seq": 5,
                "timestamp_ms": now_ms(),
                "params": {"source": "bridge_loopback"},
            })
            ack = client.recv_until("ack")
            assert ack["seq"] == 5
            assert ack["ack_type"] == "emergency_stop"
            assert ack["ok"] is True
            emergency = client.recv_until("emergency_state")
            assert emergency["active"] is True

            client.send({
                "type": "operator_input",
                "protocol_version": 1,
                "seq": 6,
                "timestamp_ms": now_ms(),
                "ttl_ms": 500,
                "mode": "vehicle",
                "keyboard": {"pressed_keys": ["w"]},
                "gamepad": {"connected": False, "buttons": {}, "axes": {}},
            })
            ignored = client.recv_status("ignored_emergency")
            assert ignored["cmd_vel"]["linear_x"] == 0.0
        finally:
            client.close()
    finally:
        stop_bridge(proc)

    print("bridge loopback: PASS")


if __name__ == "__main__":
    main()
