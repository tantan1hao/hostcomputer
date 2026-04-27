from dataclasses import dataclass, field


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
