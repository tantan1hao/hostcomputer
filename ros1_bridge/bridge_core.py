import threading
from typing import Any, Callable, Dict, Iterable, List, Optional

from debug_events import EventSink, NullEventSink
from bridge_protocol import MAX_FRAME_BYTES, PROTOCOL_VERSION, clamp, now_ms
from bridge_state import BridgeState, TwistCommand
from output_adapters import OutputAdapter


class BridgeCore:
    def __init__(
        self,
        output: OutputAdapter,
        watchdog_ms: int,
        linear_speed: float,
        angular_speed: float,
        cameras: Optional[List[Dict[str, Any]]] = None,
        events: Optional[EventSink] = None,
    ) -> None:
        self.output = output
        self.events = events or NullEventSink()
        self.watchdog_ms = watchdog_ms
        self.linear_speed = linear_speed
        self.angular_speed = angular_speed
        self.cameras = cameras if cameras is not None else self.default_cameras()
        self.state = BridgeState()
        self.state_lock = threading.Lock()

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

    def state_snapshot(self) -> Dict[str, Any]:
        with self.state_lock:
            state = self.state.to_dict()
        state["cameras"] = self.cameras
        state["watchdog_ms"] = self.watchdog_ms
        state["max_frame_bytes"] = MAX_FRAME_BYTES
        return state

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
        self.events.emit("protocol", "message received", data={"type": msg_type, "seq": seq})

        if not msg_type:
            self.events.emit("protocol", "missing message type", level="error", data={"seq": seq})
            yield self.make_protocol_error(msg, 2101, "missing type")
            return

        if msg.get("protocol_version", PROTOCOL_VERSION) != PROTOCOL_VERSION:
            self.events.emit("protocol", "unsupported protocol version", level="error", data={"seq": seq})
            yield self.make_protocol_error(msg, 2103, "unsupported protocol_version")
            return

        if msg_type == "heartbeat":
            self.events.emit("protocol", "heartbeat ack", data={"seq": seq})
            yield {
                "type": "heartbeat_ack",
                "protocol_version": PROTOCOL_VERSION,
                "seq": seq,
                "timestamp_ms": now_ms(),
                "server_time_ms": now_ms(),
            }
            return

        if msg_type in ("sync_request", "system_snapshot_request"):
            self.events.emit("sync", "system snapshot requested", data={"seq": seq})
            yield self.make_system_snapshot(seq)
            return

        if msg_type == "camera_list_request":
            self.events.emit("camera", "camera list requested", data={"seq": seq, "count": len(self.cameras)})
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
            self.events.emit("emergency", "emergency stop accepted", level="warning", data={
                "seq": seq,
                "source": source,
            })
            self.publish_zero("emergency_stop")
            yield self.make_ack(msg, True, 0, "emergency active")
            yield self.make_emergency_state("emergency active")
            return

        if msg_type == "clear_emergency":
            with self.state_lock:
                self.state.emergency_active = False
                self.state.emergency_source = ""
            self.events.emit("emergency", "emergency cleared", data={"seq": seq})
            yield self.make_ack(msg, True, 0, "emergency cleared")
            yield self.make_emergency_state("emergency cleared")
            return

        if msg_type == "system_command":
            command = str(msg.get("command", ""))
            if command == "clear_emergency":
                with self.state_lock:
                    self.state.emergency_active = False
                    self.state.emergency_source = ""
                self.events.emit("emergency", "emergency cleared", data={"seq": seq, "via": "system_command"})
                yield self.make_ack(msg, True, 0, "emergency cleared")
                yield self.make_emergency_state("emergency cleared")
            else:
                yield self.make_ack(msg, True, 0, f"system command {command} accepted")
            return

        self.events.emit("protocol", "unsupported message type", level="warning", data={
            "seq": seq,
            "type": msg_type,
        })
        yield self.make_protocol_error(msg, 1001, f"unsupported message type: {msg_type}")

    def handle_operator_input(self, msg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        seq = int(msg.get("seq", 0) or 0)
        timestamp = int(msg.get("timestamp_ms", 0) or 0)
        ttl_ms = int(msg.get("ttl_ms", 0) or 0)
        current_ms = now_ms()

        with self.state_lock:
            if seq <= self.state.last_operator_seq:
                self.events.emit("operator_input", "operator input dropped old seq", level="warning", data={"seq": seq})
                return self.input_status(seq, "dropped_old_seq")
            if ttl_ms > 0 and timestamp > 0 and current_ms > timestamp + ttl_ms:
                self.state.last_operator_seq = seq
                self.events.emit("operator_input", "operator input dropped expired", level="warning", data={"seq": seq})
                return self.input_status(seq, "dropped_expired")
            if self.state.emergency_active:
                self.state.last_operator_seq = seq
                should_publish_zero = True
            else:
                should_publish_zero = False

        if should_publish_zero:
            self.publish_zero("emergency_active")
            self.events.emit("operator_input", "operator input ignored emergency", level="warning", data={"seq": seq})
            return self.input_status(seq, "ignored_emergency")

        twist = self.operator_input_to_twist(msg)
        with self.state_lock:
            self.state.last_operator_seq = seq
            self.state.last_valid_input_ms = current_ms
            self.state.watchdog_active = False
            self.state.control_mode = str(msg.get("mode", self.state.control_mode))
        self.publish_twist(twist)
        self.events.emit("operator_input", "operator input accepted", data={
            "seq": seq,
            "mode": msg.get("mode", "unknown"),
            "cmd_vel": {
                "linear_x": twist.linear_x,
                "angular_z": twist.angular_z,
                "source": twist.source,
            },
            "pressed_keys": msg.get("keyboard", {}).get("pressed_keys", []),
        })
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

    def check_watchdog(self) -> List[Dict[str, Any]]:
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
                seq = self.state.last_operator_seq
            else:
                seq = self.state.last_operator_seq
        if not emit_status:
            return []
        self.publish_zero("watchdog_timeout")
        self.events.emit("watchdog", "operator input watchdog timeout", level="warning", data={"seq": seq})
        return [self.input_status(seq, "watchdog_timeout")]


class BridgeRuntime:
    def __init__(self, core: BridgeCore, broadcast: Callable[[Dict[str, Any]], None]) -> None:
        self.core = core
        self.broadcast = broadcast
        self.stop_event = threading.Event()

    def start_watchdog(self) -> None:
        threading.Thread(target=self._watchdog_loop, daemon=True).start()

    def _watchdog_loop(self) -> None:
        while not self.stop_event.wait(0.05):
            for event in self.core.check_watchdog():
                self.broadcast(event)
