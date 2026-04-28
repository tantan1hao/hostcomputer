#!/usr/bin/env python3
import argparse
import json
import socket
import threading
from typing import Any, Dict, List, Optional, Tuple

from bridge_core import BridgeCore, BridgeRuntime
from bridge_protocol import DEFAULT_WATCHDOG_MS, MAX_FRAME_BYTES, PROTOCOL_VERSION
from bridge_protocol import json_line
from debug_ui import DebugHttpServer
from debug_events import EventSink, RingBufferEventSink
from output_adapters import DryRunOutput, OutputAdapter, RosOutput
from video_manager import VideoConfigError, VideoManager


class HostBridgeServer:
    def __init__(
        self,
        host: str,
        port: int,
        output: OutputAdapter,
        watchdog_ms: int = DEFAULT_WATCHDOG_MS,
        linear_speed: float = 0.8,
        angular_speed: float = 1.5,
        servo_frame: str = "catch_camera",
        gripper_min_position: float = 0.0,
        gripper_max_position: float = 0.044,
        gripper_initial_position: float = 0.022,
        cameras: Optional[List[Dict[str, Any]]] = None,
        video_manager: Optional[VideoManager] = None,
        video_autostart: bool = False,
        video_poll_sec: float = 1.0,
        events: Optional[EventSink] = None,
    ) -> None:
        self.host = host
        self.port = port
        self.stop_event = threading.Event()
        self.client_lock = threading.Lock()
        self.active_clients: List["BridgeClient"] = []
        self.events = events or RingBufferEventSink()
        self.video_manager = video_manager
        self.video_autostart = video_autostart
        self.video_poll_sec = max(video_poll_sec, 0.1)
        self.core = BridgeCore(
            output,
            watchdog_ms,
            linear_speed,
            angular_speed,
            servo_frame,
            gripper_min_position,
            gripper_max_position,
            gripper_initial_position,
            cameras,
            self.video_manager.camera_infos if self.video_manager else None,
            self.handle_camera_stream_request if self.video_manager else None,
            self.events,
        )
        self.runtime = BridgeRuntime(self.core, self.broadcast)

    def serve_forever(self) -> None:
        self.start_video_manager()
        self.runtime.start_watchdog()
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind((self.host, self.port))
                server.listen(5)
                print(f"[bridge] listening on {self.host}:{self.port}", flush=True)
                self.events.emit("tcp", "bridge listening", data={"host": self.host, "port": self.port})
                while not self.stop_event.is_set():
                    try:
                        conn, addr = server.accept()
                    except OSError:
                        break
                    client = BridgeClient(self, conn, addr)
                    with self.client_lock:
                        self.active_clients.append(client)
                    threading.Thread(target=client.run, daemon=True).start()
        finally:
            self.stop_video_manager()

    def start_video_manager(self) -> None:
        if not self.video_manager:
            return
        if self.video_autostart:
            started = self.video_manager.start_enabled()
            self.events.emit("video", "video autostart completed", data={"count": len(started)})
        threading.Thread(target=self._video_monitor_loop, daemon=True).start()

    def stop_video_manager(self) -> None:
        if not self.video_manager:
            return
        stopped = self.video_manager.stop_all()
        self.events.emit("video", "video manager stopped", data={"count": len(stopped)})

    def _video_monitor_loop(self) -> None:
        if not self.video_manager:
            return
        while not self.stop_event.wait(self.video_poll_sec):
            for camera in self.video_manager.refresh():
                self.broadcast(self.core.make_camera_info(camera))

    def handle_camera_stream_request(self, params: Dict[str, Any]) -> Tuple[bool, int, str, Optional[Dict[str, Any]]]:
        if not self.video_manager:
            return False, 2400, "video manager unavailable", None

        try:
            camera_id = int(params.get("camera_id"))
        except (TypeError, ValueError):
            return False, 2401, "camera_id must be an integer", None

        action = str(params.get("action", "")).strip().lower()
        try:
            if action == "start":
                camera = self.video_manager.start(camera_id)
            elif action == "stop":
                camera = self.video_manager.stop(camera_id)
            elif action == "restart":
                camera = self.video_manager.restart(camera_id)
            else:
                return False, 2402, f"unsupported camera stream action: {action}", None
        except KeyError:
            return False, 2403, f"unknown camera_id: {camera_id}", None

        self.events.emit("video", "camera stream request handled", data={
            "camera_id": camera_id,
            "action": action,
            "online": camera.get("online", False),
        })
        return True, 0, f"camera stream {action} accepted", camera

    def remove_client(self, client: "BridgeClient") -> None:
        with self.client_lock:
            if client in self.active_clients:
                self.active_clients.remove(client)
        self.core.publish_zero("client_disconnected")

    def broadcast(self, payload: Dict[str, Any]) -> None:
        with self.client_lock:
            clients = list(self.active_clients)
        for client in clients:
            client.send_json(payload)


class BridgeClient:
    def __init__(self, server: HostBridgeServer, conn: socket.socket, addr: Tuple[str, int]) -> None:
        self.server = server
        self.conn = conn
        self.addr = addr
        self.send_lock = threading.Lock()
        self.closed = False

    def run(self) -> None:
        print(f"[bridge] client connected: {self.addr}", flush=True)
        self.server.events.emit("tcp", "client connected", data={"addr": f"{self.addr[0]}:{self.addr[1]}"})
        self.conn.settimeout(1.0)
        try:
            self.send_json(self.server.core.make_hello())
            self.send_json(self.server.core.make_capabilities())
            self._recv_loop()
        finally:
            self.closed = True
            try:
                self.conn.close()
            except OSError:
                pass
            self.server.remove_client(self)
            self.server.events.emit("tcp", "client disconnected", data={"addr": f"{self.addr[0]}:{self.addr[1]}"})
            print(f"[bridge] client disconnected: {self.addr}", flush=True)

    def send_json(self, payload: Dict[str, Any]) -> None:
        if self.closed:
            return
        try:
            with self.send_lock:
                self.conn.sendall(json_line(payload))
        except OSError:
            self.closed = True

    def _recv_loop(self) -> None:
        buffer = b""
        while not self.server.stop_event.is_set() and not self.closed:
            try:
                chunk = self.conn.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break

            if not chunk:
                break

            buffer += chunk
            if len(buffer) > MAX_FRAME_BYTES and b"\n" not in buffer:
                self.server.events.emit("protocol", "frame exceeds max length", level="error")
                self.send_json(self.server.core.make_protocol_error({}, 2001, "TCP frame exceeds max length"))
                break

            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                if not line.strip():
                    continue
                if len(line) > MAX_FRAME_BYTES:
                    self.server.events.emit("protocol", "line exceeds max length", level="error")
                    self.send_json(self.server.core.make_protocol_error({}, 2001, "TCP frame exceeds max length"))
                    return
                try:
                    msg = json.loads(line.decode("utf-8"))
                    if not isinstance(msg, dict):
                        raise ValueError("JSON frame is not an object")
                except Exception as exc:
                    self.server.events.emit("protocol", "invalid JSON", level="error", data={"error": str(exc)})
                    self.send_json(self.server.core.make_protocol_error({}, 2100, f"invalid JSON: {exc}"))
                    continue

                for response in self.server.core.handle_message(msg):
                    self.send_json(response)


def parse_camera(value: str) -> Dict[str, Any]:
    parts = value.split(",", 2)
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("camera format: id,name,rtsp_url")
    camera_id = int(parts[0])
    name = parts[1]
    rtsp_url = parts[2]
    return {
        "camera_id": camera_id,
        "name": name,
        "online": bool(rtsp_url),
        "rtsp_url": rtsp_url,
        "codec": "h264",
        "width": 1280,
        "height": 720,
        "fps": 25,
        "bitrate_kbps": 2500 if rtsp_url else 0,
    }


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ROS1 host bridge TCP/JSON node")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9090)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="run without ROS and print bridge outputs")
    mode.add_argument("--ros", action="store_true", help="publish ROS control topics with rospy")
    parser.add_argument("--cmd-vel-topic", default="/cmd_vel")
    parser.add_argument("--servo-topic", default="/servo_server/delta_twist_cmds")
    parser.add_argument("--servo-frame", default="catch_camera")
    parser.add_argument("--gripper-position-topic", default="/arm_control/gripper_position")
    parser.add_argument("--gripper-min-position", type=float, default=0.0)
    parser.add_argument("--gripper-max-position", type=float, default=0.044)
    parser.add_argument("--gripper-initial-position", type=float, default=0.022)
    parser.add_argument("--watchdog-ms", type=int, default=DEFAULT_WATCHDOG_MS)
    parser.add_argument("--linear-speed", type=float, default=0.8, help="vehicle level-5 linear speed")
    parser.add_argument("--angular-speed", type=float, default=1.5, help="vehicle level-5 angular speed")
    parser.add_argument("--debug-ui", action="store_true", help="start read-only debug HTTP UI")
    parser.add_argument("--debug-host", default="127.0.0.1")
    parser.add_argument("--debug-port", type=int, default=18080)
    parser.add_argument(
        "--camera",
        action="append",
        type=parse_camera,
        help="camera entry as id,name,rtsp_url; may be repeated",
    )
    parser.add_argument("--video-config", default="", help="YAML config for direct RTSP video sources")
    parser.add_argument(
        "--video-dry-run",
        action="store_true",
        help="build video commands and mark streams online without starting ffmpeg",
    )
    parser.add_argument(
        "--video-autostart",
        action="store_true",
        help="start enabled direct video sources when the bridge starts",
    )
    parser.add_argument("--video-poll-sec", type=float, default=1.0)
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()
    events = RingBufferEventSink()
    if args.ros:
        output: OutputAdapter = RosOutput(
            "host_bridge_node",
            args.cmd_vel_topic,
            args.servo_topic,
            args.gripper_position_topic,
            events,
        )
    else:
        output = DryRunOutput(events)

    video_manager: Optional[VideoManager] = None
    if args.video_config:
        try:
            video_manager = VideoManager.from_config_path(
                args.video_config,
                dry_run=args.video_dry_run,
                events=events,
            )
            events.emit("video", "video manager configured", data={
                "config": args.video_config,
                "dry_run": args.video_dry_run,
                "sources": len(video_manager.config.direct_sources),
            })
        except VideoConfigError as exc:
            raise SystemExit(f"failed to load video config: {exc}") from exc

    server = HostBridgeServer(
        host=args.host,
        port=args.port,
        output=output,
        watchdog_ms=args.watchdog_ms,
        linear_speed=args.linear_speed,
        angular_speed=args.angular_speed,
        servo_frame=args.servo_frame,
        gripper_min_position=args.gripper_min_position,
        gripper_max_position=args.gripper_max_position,
        gripper_initial_position=args.gripper_initial_position,
        cameras=args.camera,
        video_manager=video_manager,
        video_autostart=args.video_autostart,
        video_poll_sec=args.video_poll_sec,
        events=events,
    )
    if args.debug_ui:
        DebugHttpServer(args.debug_host, args.debug_port, server.core, events).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("[bridge] stopping", flush=True)
        server.stop_event.set()
        server.runtime.stop_event.set()


if __name__ == "__main__":
    main()
