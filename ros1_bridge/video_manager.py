import os
import shlex
import subprocess
import threading
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from bridge_protocol import now_ms
from debug_events import EventSink, NullEventSink

try:
    import yaml
except ImportError:  # pragma: no cover - exercised only on minimal systems
    yaml = None


class VideoConfigError(ValueError):
    pass


@dataclass
class RtspConfig:
    host: str = "127.0.0.1"
    publish_host: str = "127.0.0.1"
    port: int = 8554
    server: str = "mediamtx"
    transport: str = "tcp"

    def public_url(self, path: str) -> str:
        return f"rtsp://{self.host}:{self.port}/{path}"

    def publish_url(self, path: str) -> str:
        return f"rtsp://{self.publish_host}:{self.port}/{path}"


@dataclass
class DirectVideoSource:
    camera_id: int
    name: str
    device: str
    rtsp_path: str
    source_id: str = ""
    runner: str = "ffmpeg"
    input_format: str = "mjpeg"
    width: int = 1280
    height: int = 720
    fps: int = 30
    codec: str = "h264"
    bitrate_kbps: int = 2500
    enabled: bool = True
    slot_hint: Optional[int] = None
    ffmpeg_path: str = "ffmpeg"
    extra_input_args: List[str] = field(default_factory=list)
    extra_output_args: List[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        if not self.source_id:
            self.source_id = f"camera_{self.camera_id}"
        if self.slot_hint is None:
            self.slot_hint = self.camera_id

    def camera_info(
        self,
        rtsp: RtspConfig,
        online: bool = False,
        last_error: str = "",
    ) -> Dict[str, Any]:
        return {
            "camera_id": self.camera_id,
            "source_id": self.source_id,
            "slot_hint": self.slot_hint,
            "name": self.name,
            "kind": "direct",
            "online": online,
            "rtsp_url": rtsp.public_url(self.rtsp_path) if online else "",
            "rtsp_transport": rtsp.transport,
            "codec": self.codec,
            "width": self.width,
            "height": self.height,
            "fps": self.fps,
            "bitrate_kbps": self.bitrate_kbps if online else 0,
            "profile": "low_latency",
            "last_error": last_error,
        }


@dataclass
class VideoConfig:
    rtsp: RtspConfig
    direct_sources: List[DirectVideoSource]

    def camera_infos(self) -> List[Dict[str, Any]]:
        return [source.camera_info(self.rtsp, online=False) for source in self.direct_sources]


@dataclass
class DirectSourceRuntime:
    source: DirectVideoSource
    online: bool = False
    last_error: str = ""
    process: Optional[subprocess.Popen] = None
    started_at_ms: int = 0
    stopped_at_ms: int = 0
    restart_count: int = 0
    last_exit_code: Optional[int] = None
    command: List[str] = field(default_factory=list)

    def camera_info(self, rtsp: RtspConfig) -> Dict[str, Any]:
        info = self.source.camera_info(rtsp, online=self.online, last_error=self.last_error)
        info["pid"] = self.process.pid if self.process and self.process.poll() is None else 0
        info["started_at_ms"] = self.started_at_ms
        info["stopped_at_ms"] = self.stopped_at_ms
        info["restart_count"] = self.restart_count
        info["last_exit_code"] = self.last_exit_code
        info["command"] = " ".join(shlex.quote(part) for part in self.command)
        return info


class VideoManager:
    def __init__(
        self,
        config: VideoConfig,
        dry_run: bool = False,
        events: Optional[EventSink] = None,
    ) -> None:
        self.config = config
        self.dry_run = dry_run
        self.events = events or NullEventSink()
        self._lock = threading.RLock()
        self._runtime: Dict[int, DirectSourceRuntime] = {
            source.camera_id: DirectSourceRuntime(source=source)
            for source in config.direct_sources
        }

    @classmethod
    def from_config_path(
        cls,
        path: str,
        dry_run: bool = False,
        events: Optional[EventSink] = None,
    ) -> "VideoManager":
        return cls(load_video_config(path), dry_run=dry_run, events=events)

    def start_enabled(self) -> List[Dict[str, Any]]:
        changed: List[Dict[str, Any]] = []
        for source in self.config.direct_sources:
            if source.enabled:
                changed.append(self.start(source.camera_id))
        return changed

    def stop_all(self) -> List[Dict[str, Any]]:
        changed: List[Dict[str, Any]] = []
        for camera_id in list(self._runtime.keys()):
            changed.append(self.stop(camera_id))
        return changed

    def start(self, camera_id: int) -> Dict[str, Any]:
        with self._lock:
            runtime = self._get_runtime(camera_id)
            self._refresh_runtime_locked(runtime)
            if runtime.online:
                return runtime.camera_info(self.config.rtsp)

            command = self.build_ffmpeg_command(runtime.source)
            runtime.command = command
            runtime.last_error = ""
            runtime.last_exit_code = None
            runtime.started_at_ms = now_ms()
            runtime.stopped_at_ms = 0

            if self.dry_run:
                runtime.online = True
                runtime.process = None
                self.events.emit("video", "dry-run stream started", data={
                    "camera_id": camera_id,
                    "command": runtime.camera_info(self.config.rtsp)["command"],
                })
                return runtime.camera_info(self.config.rtsp)

            try:
                runtime.process = subprocess.Popen(
                    command,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                runtime.online = True
                self.events.emit("video", "stream process started", data={
                    "camera_id": camera_id,
                    "pid": runtime.process.pid,
                })
            except OSError as exc:
                runtime.online = False
                runtime.process = None
                runtime.last_error = str(exc)
                runtime.stopped_at_ms = now_ms()
                self.events.emit("video", "stream process failed to start", level="error", data={
                    "camera_id": camera_id,
                    "error": str(exc),
                })

            return runtime.camera_info(self.config.rtsp)

    def stop(self, camera_id: int) -> Dict[str, Any]:
        with self._lock:
            runtime = self._get_runtime(camera_id)
            process = runtime.process
            if process and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2.0)
            runtime.process = None
            runtime.online = False
            runtime.stopped_at_ms = now_ms()
            runtime.last_exit_code = process.returncode if process else runtime.last_exit_code
            self.events.emit("video", "stream stopped", data={"camera_id": camera_id})
            return runtime.camera_info(self.config.rtsp)

    def restart(self, camera_id: int) -> Dict[str, Any]:
        with self._lock:
            runtime = self._get_runtime(camera_id)
            runtime.restart_count += 1
        self.stop(camera_id)
        return self.start(camera_id)

    def camera_infos(self) -> List[Dict[str, Any]]:
        with self._lock:
            return [
                self._runtime[source.camera_id].camera_info(self.config.rtsp)
                for source in self.config.direct_sources
            ]

    def refresh(self) -> List[Dict[str, Any]]:
        changed: List[Dict[str, Any]] = []
        with self._lock:
            for runtime in self._runtime.values():
                before = runtime.online
                self._refresh_runtime_locked(runtime)
                if runtime.online != before:
                    changed.append(runtime.camera_info(self.config.rtsp))
        return changed

    def build_ffmpeg_command(self, source: DirectVideoSource) -> List[str]:
        output_url = self.config.rtsp.publish_url(source.rtsp_path)
        command = [
            source.ffmpeg_path,
            "-hide_banner",
            "-loglevel",
            "warning",
            "-f",
            "v4l2",
        ]
        if source.input_format:
            command += ["-input_format", source.input_format]
        command += [
            "-video_size",
            f"{source.width}x{source.height}",
            "-framerate",
            str(source.fps),
        ]
        command += list(source.extra_input_args)
        command += ["-i", source.device, "-an"]
        command += self._ffmpeg_codec_args(source)
        if self.config.rtsp.transport:
            command += ["-rtsp_transport", self.config.rtsp.transport]
        command += list(source.extra_output_args)
        command += ["-f", "rtsp", output_url]
        return command

    def _ffmpeg_codec_args(self, source: DirectVideoSource) -> List[str]:
        codec = source.codec.lower()
        if codec in ("copy", "passthrough"):
            return ["-c:v", "copy"]
        if codec in ("h264", "libx264"):
            return [
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-tune",
                "zerolatency",
                "-g",
                str(max(source.fps, 1)),
                "-b:v",
                f"{source.bitrate_kbps}k",
            ]
        if codec in ("mjpeg", "mjpg"):
            return ["-c:v", "mjpeg", "-q:v", "5"]
        return ["-c:v", source.codec, "-b:v", f"{source.bitrate_kbps}k"]

    def _refresh_runtime_locked(self, runtime: DirectSourceRuntime) -> None:
        process = runtime.process
        if self.dry_run or not process:
            return
        exit_code = process.poll()
        if exit_code is None:
            runtime.online = True
            return
        runtime.online = False
        runtime.last_exit_code = exit_code
        runtime.process = None
        runtime.stopped_at_ms = now_ms()
        runtime.last_error = f"ffmpeg exited with code {exit_code}"
        self.events.emit("video", "stream process exited", level="warning", data={
            "camera_id": runtime.source.camera_id,
            "exit_code": exit_code,
        })

    def _get_runtime(self, camera_id: int) -> DirectSourceRuntime:
        if camera_id not in self._runtime:
            raise KeyError(f"unknown camera_id: {camera_id}")
        return self._runtime[camera_id]


def load_video_config(path: str) -> VideoConfig:
    if yaml is None:
        raise VideoConfigError("PyYAML is required to load video source configuration")
    if not path:
        raise VideoConfigError("video config path is empty")
    if not os.path.exists(path):
        raise VideoConfigError(f"video config not found: {path}")

    with open(path, "r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle) or {}
    if not isinstance(data, dict):
        raise VideoConfigError("video config root must be a mapping")
    return parse_video_config(data)


def parse_video_config(data: Dict[str, Any]) -> VideoConfig:
    rtsp_data = data.get("rtsp", {}) or {}
    if not isinstance(rtsp_data, dict):
        raise VideoConfigError("rtsp must be a mapping")

    host = _required_str(rtsp_data, "host", default="127.0.0.1")
    publish_host = _required_str(rtsp_data, "publish_host", default="127.0.0.1")
    rtsp = RtspConfig(
        host=host,
        publish_host=publish_host,
        port=_positive_int(rtsp_data, "port", default=8554),
        server=_required_str(rtsp_data, "server", default="mediamtx"),
        transport=_rtsp_transport(rtsp_data),
    )

    source_items = data.get("direct_sources", []) or []
    if not isinstance(source_items, list):
        raise VideoConfigError("direct_sources must be a list")

    sources = [parse_direct_source(item, index) for index, item in enumerate(source_items)]
    _validate_unique_camera_ids(sources)
    _validate_unique_rtsp_paths(sources)
    return VideoConfig(rtsp=rtsp, direct_sources=sources)


def parse_direct_source(item: Any, index: int) -> DirectVideoSource:
    if not isinstance(item, dict):
        raise VideoConfigError(f"direct_sources[{index}] must be a mapping")

    camera_id = _positive_int(item, "camera_id")
    if camera_id > 4:
        raise VideoConfigError(f"direct_sources[{index}].camera_id must be between 0 and 4")

    source = DirectVideoSource(
        camera_id=camera_id,
        source_id=_required_str(item, "source_id", default=f"camera_{camera_id}"),
        slot_hint=_optional_int(item, "slot_hint"),
        name=_required_str(item, "name", default=f"Camera {camera_id}"),
        runner=_required_str(item, "runner", default="ffmpeg"),
        device=_required_str(item, "device"),
        input_format=_required_str(item, "input_format", default="mjpeg"),
        width=_positive_int(item, "width", default=1280),
        height=_positive_int(item, "height", default=720),
        fps=_positive_int(item, "fps", default=30),
        codec=_required_str(item, "codec", default="h264"),
        bitrate_kbps=_positive_int(item, "bitrate_kbps", default=2500),
        rtsp_path=_required_str(item, "rtsp_path"),
        enabled=_bool_value(item, "enabled", default=True),
        ffmpeg_path=_required_str(item, "ffmpeg_path", default="ffmpeg"),
        extra_input_args=_string_list(item, "extra_input_args"),
        extra_output_args=_string_list(item, "extra_output_args"),
    )

    if source.runner != "ffmpeg":
        raise VideoConfigError(
            f"direct_sources[{index}].runner={source.runner!r} is not supported yet"
        )
    if "/" in source.rtsp_path.strip("/"):
        raise VideoConfigError(f"direct_sources[{index}].rtsp_path must be a single path segment")
    return source


def _validate_unique_camera_ids(sources: List[DirectVideoSource]) -> None:
    seen: Dict[int, str] = {}
    for source in sources:
        if source.camera_id in seen:
            raise VideoConfigError(
                f"duplicate camera_id {source.camera_id}: {seen[source.camera_id]} and {source.name}"
            )
        seen[source.camera_id] = source.name


def _validate_unique_rtsp_paths(sources: List[DirectVideoSource]) -> None:
    seen: Dict[str, str] = {}
    for source in sources:
        if source.rtsp_path in seen:
            raise VideoConfigError(
                f"duplicate rtsp_path {source.rtsp_path}: {seen[source.rtsp_path]} and {source.name}"
            )
        seen[source.rtsp_path] = source.name


def _required_str(data: Dict[str, Any], key: str, default: Optional[str] = None) -> str:
    value = data.get(key, default)
    if value is None:
        raise VideoConfigError(f"{key} is required")
    if not isinstance(value, str) or not value.strip():
        raise VideoConfigError(f"{key} must be a non-empty string")
    return value.strip()


def _rtsp_transport(data: Dict[str, Any]) -> str:
    transport = _required_str(data, "transport", default="tcp").lower()
    if transport not in ("tcp", "udp"):
        raise VideoConfigError("rtsp.transport must be tcp or udp")
    return transport


def _positive_int(data: Dict[str, Any], key: str, default: Optional[int] = None) -> int:
    value = data.get(key, default)
    if value is None:
        raise VideoConfigError(f"{key} is required")
    if isinstance(value, bool):
        raise VideoConfigError(f"{key} must be an integer")
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise VideoConfigError(f"{key} must be an integer") from exc
    if parsed < 0:
        raise VideoConfigError(f"{key} must be >= 0")
    return parsed


def _optional_int(data: Dict[str, Any], key: str) -> Optional[int]:
    if key not in data or data[key] is None:
        return None
    return _positive_int(data, key)


def _bool_value(data: Dict[str, Any], key: str, default: bool) -> bool:
    value = data.get(key, default)
    if not isinstance(value, bool):
        raise VideoConfigError(f"{key} must be true or false")
    return value


def _string_list(data: Dict[str, Any], key: str) -> List[str]:
    value = data.get(key, []) or []
    if not isinstance(value, list):
        raise VideoConfigError(f"{key} must be a list of strings")
    result: List[str] = []
    for item in value:
        if not isinstance(item, str) or not item:
            raise VideoConfigError(f"{key} must contain only non-empty strings")
        result.append(item)
    return result
