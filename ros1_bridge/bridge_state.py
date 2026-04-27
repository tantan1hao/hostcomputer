from dataclasses import dataclass, field
from typing import Dict


@dataclass
class TwistCommand:
    linear_x: float = 0.0
    angular_z: float = 0.0
    source: str = "idle"

    def to_dict(self) -> Dict[str, float]:
        return {
            "linear_x": self.linear_x,
            "angular_z": self.angular_z,
            "source": self.source,
        }


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

    def to_dict(self) -> Dict[str, object]:
        return {
            "emergency_active": self.emergency_active,
            "emergency_source": self.emergency_source,
            "control_mode": self.control_mode,
            "motor_initialized": self.motor_initialized,
            "motor_enabled": self.motor_enabled,
            "last_error_code": self.last_error_code,
            "last_error_message": self.last_error_message,
            "last_operator_seq": self.last_operator_seq,
            "last_valid_input_ms": self.last_valid_input_ms,
            "watchdog_active": self.watchdog_active,
            "last_twist": self.last_twist.to_dict(),
        }
