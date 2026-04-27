#!/usr/bin/env python3
import argparse
import json
import math
import socket
import threading
import time
from dataclasses import dataclass, field
from typing import Any, Dict, Iterable, List, Optional, Tuple


PROTOCOL_VERSION = 1
MAX_FRAME_BYTES = 1024 * 1024
DEFAULT_WATCHDOG_MS = 500


def now_ms() -> int:
    return int(time.time() * 1000)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def json_line(payload: Dict[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")


@dataclass
class TwistCommand:
    linear_x: float = 0.0
    angular_z: float = 0.0
    source: str = "idle"


@dataclass
class BridgeState:
    emergency_active: bool = False
    emergency_source: str = ""
    control_mode: str = "vehicle"
    motor_initialized: bool = True
    motor_enabled: bool = True
    last_error_code: int = 0
    last_error_message: str = ""
    last_operator_seq: int = 0
    last_valid_input_ms: int = 0
    watchdog_active: bool = False
    last_twist: TwistCommand = field(default_factory=TwistCommand)


class OutputAdapter:
    def publish_twist(self, twist: TwistCommand) -> None:
        raise NotImplementedError


class DryRunOutput(OutputAdapter):
    def publish_twist(self, twist: TwistCommand) -> None:
        print(
            f"[bridge] cmd_vel source={twist.source} "
            f"linear_x={twist.linear_x:+.3f} angular_z={twist.angular_z:+.3f}",
            flush=True,
        )


class RosOutput(OutputAdapter):
    def __init__(self, node_name: str, topic: str) -> None:
        import rospy
        from geometry_msgs.msg import Twist

        self._rospy = rospy
        self._twist_type = Twist
        rospy.init_node(node_name, anonymous=False)
        self._publisher = rospy.Publisher(topic, Twist, queue_size=1)
        rospy.loginfo("host_bridge_node publishing Twist to %s", topic)

    def publish_twist(self, twist: TwistCommand) -> None:
        msg = self._twist_type()
        msg.linear.x = twist.linear_x
        msg.angular.z = twist.angular_z
        self._publisher.publish(msg)


class HostBridgeServer:
    def __init__(
        self,
        host: str,
        port: int,
        output: OutputAdapter,
        watchdog_ms: int = DEFAULT_WATCHDOG_MS,
        linear_speed: float = 0.6,
        angular_speed: float = 1.0,
        cameras: Optional[List[Dict[str, Any]]] = None,
    ) -> None:
        self.host = host
        self.port = port
        self.output = output
        self.watchdog_ms = watchdog_ms
        self.linear_speed = linear_speed
        self.angular_speed = angular_speed
        self.cameras = cameras if cameras is not None else self.default_cameras()

        self.state = BridgeState()
        self.state_lock = threading.Lock()
        self.stop_event = threading.Event()
        self.client_lock = threading.Lock()
        self.active_clients: List["BridgeClient"] = []

    @staticmethod
    def default_cameras() -> List[Dict[str, Any]]:
        return [
            {
                "camera_id": 0,
                "name": "front",
                "online": False,
                "rtsp_url": "",
                "codec": "h264",
                "width": 1280,
                "height": 720,
                "fps": 25,
                "bitrate_kbps": 0,
            }
        ]

    def serve_forever(self) -> None:
        threading.Thread(target=self._watchdog_loop, daemon=True).start()
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((self.host, self.port))
            server.listen(5)
            print(f"[bridge] listening on {self.host}:{self.port}", flush=True)
            while not self.stop_event.is_set():
                try:
                    conn, addr = server.accept()
                except OSError:
                    break
                client = BridgeClient(self, conn, addr)
                with self.client_lock:
                    self.active_clients.append(client)
                threading.Thread(target=client.run, daemon=True).start()

    def remove_client(self, client: "BridgeClient") -> None:
        with self.client_lock:
            if client in self.active_clients:
                self.active_clients.remove(client)
        self.publish_zero("client_disconnected")

    def broadcast(self, payload: Dict[str, Any]) -> None:
        with self.client_lock:
            clients = list(self.active_clients)
        for client in clients:
            client.send_json(payload)

    def make_hello(self) -> Dict[str, Any]:
        return {
            "type": "hello",
            "protocol_version": PROTOCOL_VERSION,
            "seq": 0,
            "timestamp_ms": now_ms(),
            "bridge_name": "host_bridge_node",
            "bridge_version": "loopback-mvp",
            "robot_id": "loopback_robot",
        }

    def make_capabilities(self) -> Dict[str, Any]:
        return {
            "type": "capabilities",
            "protocol_version": PROTOCOL_VERSION,
            "seq": 0,
            "timestamp_ms": now_ms(),
            "supports": [
                "operator_input",
                "heartbeat_ack",
                "critical_ack",
                "sync_request",
                "system_snapshot",
                "camera_list_request",
                "camera_list_response",
                "emergency_state",
                "watchdog",
            ],
            "max_frame_bytes": MAX_FRAME_BYTES,
            "watchdog_ms": self.watchdog_ms,
            "cameras": self.cameras,
        }

    def make_system_snapshot(self, seq: int) -> Dict[str, Any]:
        with self.state_lock:
            return {
                "type": "system_snapshot",
                "protocol_version": PROTOCOL_VERSION,
                "seq": seq,
                "timestamp_ms": now_ms(),
                "control_mode": self.state.control_mode,
                "emergency": {
                    "active": self.state.emergency_active,
                    "source": self.state.emergency_source,
                },
                "motor": {
                    "initialized": self.state.motor_initialized,
                    "enabled": self.state.motor_enabled,
                },
                "modules": {
                    "base": "online",
                    "arm": "online",
                    "camera": "online" if any(cam.get("online", False) for cam in self.cameras) else "offline",
                },
                "last_error": {
                    "code": self.state.last_error_code,
                    "message": self.state.last_error_message,
                },
                "last_operator_input_seq": self.state.last_operator_seq,
                "watchdog_active": self.state.watchdog_active,
            }

    def make_camera_list_response(self, seq: int) -> Dict[str, Any]:
        return {
            "type": "camera_list_response",
            "protocol_version": PROTOCOL_VERSION,
            "seq": seq,
            "timestamp_ms": now_ms(),
            "cameras": self.cameras,
        }

    def make_ack(self, msg: Dict[str, Any], ok: bool = True, code: int = 0, message: str = "ok") -> Dict[str, Any]:
        return {
            "type": "ack",
            "protocol_version": PROTOCOL_VERSION,
            "ack_type": str(msg.get("type", "unknown")),
            "seq": int(msg.get("seq", 0) or 0),
            "ok": ok,
            "code": code,
            "message": message,
            "timestamp_ms": now_ms(),
        }

    def make_protocol_error(self, msg: Dict[str, Any], code: int, message: str) -> Dict[str, Any]:
        with self.state_lock:
            self.state.last_error_code = code
            self.state.last_error_message = message
        return {
            "type": "protocol_error",
            "protocol_version": PROTOCOL_VERSION,
            "seq": int(msg.get("seq", 0) or 0),
            "code": code,
            "message": message,
            "timestamp_ms": now_ms(),
        }

    def make_emergency_state(self, message: str) -> Dict[str, Any]:
        with self.state_lock:
            active = self.state.emergency_active
            source = self.state.emergency_source
        return {
            "type": "emergency_state",
            "protocol_version": PROTOCOL_VERSION,
            "seq": 0,
            "timestamp_ms": now_ms(),
            "active": active,
            "source": source,
            "message": message,
        }

    def publish_zero(self, source: str) -> None:
        self.publish_twist(TwistCommand(0.0, 0.0, source))

    def publish_twist(self, twist: TwistCommand) -> None:
        with self.state_lock:
            self.state.last_twist = twist
        self.output.publish_twist(twist)

    def handle_message(self, msg: Dict[str, Any]) -> Iterable[Dict[str, Any]]:
        msg_type = str(msg.get("type", ""))
        seq = int(msg.get("seq", 0) or 0)

        if not msg_type:
            yield self.make_protocol_error(msg, 2101, "missing type")
            return

        if msg.get("protocol_version", PROTOCOL_VERSION) != PROTOCOL_VERSION:
            yield self.make_protocol_error(msg, 2103, "unsupported protocol_version")
            return

        if msg_type == "heartbeat":
            yield {
                "type": "heartbeat_ack",
                "protocol_version": PROTOCOL_VERSION,
                "seq": seq,
                "timestamp_ms": now_ms(),
                "server_time_ms": now_ms(),
            }
            return

        if msg_type in ("sync_request", "system_snapshot_request"):
            yield self.make_system_snapshot(seq)
            return

        if msg_type == "camera_list_request":
            yield self.make_camera_list_response(seq)
            return

        if msg_type == "operator_input":
            status = self.handle_operator_input(msg)
            if status is not None:
                yield status
            return

        if msg_type == "emergency_stop":
            source = str(msg.get("params", {}).get("source", "unknown"))
            with self.state_lock:
                self.state.emergency_active = True
                self.state.emergency_source = source
                self.state.watchdog_active = False
            self.publish_zero("emergency_stop")
            yield self.make_ack(msg, True, 0, "emergency active")
            yield self.make_emergency_state("emergency active")
            return

        if msg_type == "clear_emergency":
            with self.state_lock:
                self.state.emergency_active = False
                self.state.emergency_source = ""
            yield self.make_ack(msg, True, 0, "emergency cleared")
            yield self.make_emergency_state("emergency cleared")
            return

        if msg_type == "system_command":
            command = str(msg.get("command", ""))
            if command == "clear_emergency":
                with self.state_lock:
                    self.state.emergency_active = False
                    self.state.emergency_source = ""
                yield self.make_ack(msg, True, 0, "emergency cleared")
                yield self.make_emergency_state("emergency cleared")
            else:
                yield self.make_ack(msg, True, 0, f"system command {command} accepted")
            return

        yield self.make_protocol_error(msg, 1001, f"unsupported message type: {msg_type}")

    def handle_operator_input(self, msg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        seq = int(msg.get("seq", 0) or 0)
        timestamp = int(msg.get("timestamp_ms", 0) or 0)
        ttl_ms = int(msg.get("ttl_ms", 0) or 0)
        current_ms = now_ms()

        with self.state_lock:
            if seq <= self.state.last_operator_seq:
                return self.input_status(seq, "dropped_old_seq")
            if ttl_ms > 0 and timestamp > 0 and current_ms > timestamp + ttl_ms:
                self.state.last_operator_seq = seq
                return self.input_status(seq, "dropped_expired")
            if self.state.emergency_active:
                self.state.last_operator_seq = seq
                self.publish_zero("emergency_active")
                return self.input_status(seq, "ignored_emergency")

        twist = self.operator_input_to_twist(msg)
        with self.state_lock:
            self.state.last_operator_seq = seq
            self.state.last_valid_input_ms = current_ms
            self.state.watchdog_active = False
            self.state.control_mode = str(msg.get("mode", self.state.control_mode))
        self.publish_twist(twist)
        return self.input_status(seq, "accepted")

    def input_status(self, seq: int, status: str) -> Dict[str, Any]:
        with self.state_lock:
            twist = self.state.last_twist
            watchdog_active = self.state.watchdog_active
            emergency_active = self.state.emergency_active
        return {
            "type": "system_status",
            "protocol_version": PROTOCOL_VERSION,
            "seq": 0,
            "timestamp_ms": now_ms(),
            "last_operator_input_seq": seq,
            "operator_input_status": status,
            "watchdog_active": watchdog_active,
            "emergency_active": emergency_active,
            "cmd_vel": {
                "linear_x": twist.linear_x,
                "angular_z": twist.angular_z,
                "source": twist.source,
            },
        }

    def operator_input_to_twist(self, msg: Dict[str, Any]) -> TwistCommand:
        keyboard = msg.get("keyboard", {})
        pressed = set(keyboard.get("pressed_keys", []) or [])
        gamepad = msg.get("gamepad", {})
        axes = gamepad.get("axes", {}) or {}

        linear = 0.0
        angular = 0.0

        if "w" in pressed:
            linear += 1.0
        if "s" in pressed:
            linear -= 1.0
        if "a" in pressed:
            angular += 1.0
        if "d" in pressed:
            angular -= 1.0

        if gamepad.get("connected", False):
            linear += float(axes.get("left_y", 0.0) or 0.0)
            angular += float(axes.get("right_x", 0.0) or 0.0)

        speed_scale = 1.6 if "shift" in pressed else 1.0
        linear = clamp(linear, -1.0, 1.0) * self.linear_speed * speed_scale
        angular = clamp(angular, -1.0, 1.0) * self.angular_speed * speed_scale

        return TwistCommand(
            linear_x=linear,
            angular_z=angular,
            source="operator_input",
        )

    def _watchdog_loop(self) -> None:
        while not self.stop_event.wait(0.05):
            emit_status = False
            with self.state_lock:
                if (
                    self.state.last_valid_input_ms > 0
                    and not self.state.emergency_active
                    and not self.state.watchdog_active
                    and now_ms() - self.state.last_valid_input_ms > self.watchdog_ms
                ):
                    self.state.watchdog_active = True
                    emit_status = True
            if emit_status:
                self.publish_zero("watchdog_timeout")
                self.broadcast(self.input_status(self.state.last_operator_seq, "watchdog_timeout"))


class BridgeClient:
    def __init__(self, server: HostBridgeServer, conn: socket.socket, addr: Tuple[str, int]) -> None:
        self.server = server
        self.conn = conn
        self.addr = addr
        self.send_lock = threading.Lock()
        self.closed = False

    def run(self) -> None:
        print(f"[bridge] client connected: {self.addr}", flush=True)
        self.conn.settimeout(1.0)
        try:
            self.send_json(self.server.make_hello())
            self.send_json(self.server.make_capabilities())
            self._recv_loop()
        finally:
            self.closed = True
            try:
                self.conn.close()
            except OSError:
                pass
            self.server.remove_client(self)
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
                self.send_json(self.server.make_protocol_error({}, 2001, "TCP frame exceeds max length"))
                break

            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                if not line.strip():
                    continue
                if len(line) > MAX_FRAME_BYTES:
                    self.send_json(self.server.make_protocol_error({}, 2001, "TCP frame exceeds max length"))
                    return
                try:
                    msg = json.loads(line.decode("utf-8"))
                    if not isinstance(msg, dict):
                        raise ValueError("JSON frame is not an object")
                except Exception as exc:
                    self.send_json(self.server.make_protocol_error({}, 2100, f"invalid JSON: {exc}"))
                    continue

                for response in self.server.handle_message(msg):
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
    mode.add_argument("--dry-run", action="store_true", help="run without ROS and print cmd_vel")
    mode.add_argument("--ros", action="store_true", help="publish geometry_msgs/Twist with rospy")
    parser.add_argument("--cmd-vel-topic", default="/cmd_vel")
    parser.add_argument("--watchdog-ms", type=int, default=DEFAULT_WATCHDOG_MS)
    parser.add_argument("--linear-speed", type=float, default=0.6)
    parser.add_argument("--angular-speed", type=float, default=1.0)
    parser.add_argument(
        "--camera",
        action="append",
        type=parse_camera,
        help="camera entry as id,name,rtsp_url; may be repeated",
    )
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()
    if args.ros:
        output: OutputAdapter = RosOutput("host_bridge_node", args.cmd_vel_topic)
    else:
        output = DryRunOutput()

    server = HostBridgeServer(
        host=args.host,
        port=args.port,
        output=output,
        watchdog_ms=args.watchdog_ms,
        linear_speed=args.linear_speed,
        angular_speed=args.angular_speed,
        cameras=args.camera,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("[bridge] stopping", flush=True)
        server.stop_event.set()


if __name__ == "__main__":
    main()
