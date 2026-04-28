# TCP JSON 协议文档

通信端口：**9090**（默认）
传输方式：TCP 长连接，每条 JSON 消息以 `\n` 结尾
数据编码：UTF-8

> 当前上位机发送协议以 `docs/通信与视频链路改造计划.md` 中的协议 v1 为准。旧版 `cmd_vel` JSON 帧已经废弃；上位机只发送 `operator_input` 输入快照，下位机 bridge 自行解析并发布 ROS `/cmd_vel` 或其他内部控制指令。

## 通用帧约束

- 每条 JSON 帧以 `\n` 结尾。
- 单帧最大长度：`1048576` 字节。
- v1 帧应包含 `protocol_version: 1`、`seq`、`timestamp_ms`。
- 上位机接收侧会对关键帧做基础 schema 校验；坏 JSON、超大帧、缺关键字段会生成 `protocol_error`。

---

## 一、上位机 → 下位机（发送）

### 1. motor_command — 电机控制命令

发送6关节+执行器的控制数据。

```json
{
  "type": "motor_command",
  "joints": [
    { "position": 1.0, "current": 0.5 },
    { "position": 2.0, "current": 0.6 },
    { "position": 1.5, "current": 0.4 },
    { "position": 0.8, "current": 0.3 },
    { "position": 1.2, "current": 0.7 },
    { "position": 0.9, "current": 0.5 }
  ],
  "executor_position": 0.5,
  "executor_torque": 1.0,
  "executor_flags": 1,
  "reserved": 0
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| joints | array[6] | 6个关节数据 |
| joints[].position | float | 关节位置（实际值，非×1000） |
| joints[].current | float | 关节电流（实际值） |
| executor_position | float | 执行器位置 |
| executor_torque | float | 执行器扭矩 |
| executor_flags | int | 执行器标志位 |
| reserved | int | 保留字段 |

---

### 2. joint_control — 单关节控制命令

控制单个关节的位置和速度。

```json
{
  "type": "joint_control",
  "joint_id": 0,
  "position": 1.5,
  "velocity": 0.8,
  "timestamp": 1709971200000
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| joint_id | int | 关节ID（0-5） |
| position | float | 目标位置 |
| velocity | float | 目标速度 |
| timestamp | int64 | 毫秒级时间戳 |

---

### 3. operator_input — 操作者输入快照

承载键盘与手柄当前输入状态。该帧不逐条 ACK，靠 `seq`、`ttl_ms` 和下位机 watchdog 保证高频输入安全。

```json
{
  "type": "operator_input",
  "protocol_version": 1,
  "seq": 12345,
  "timestamp_ms": 1709971200000,
  "ttl_ms": 300,
  "mode": "vehicle",
  "keyboard": {
    "pressed_keys": ["w", "d", "shift"]
  },
  "gamepad": {
    "connected": true,
    "buttons": {
      "a": false,
      "b": false,
      "x": false,
      "y": false,
      "start": false,
      "back": false,
      "lb": false,
      "rb": false,
      "l3": false,
      "r3": false,
      "dpad_up": false,
      "dpad_down": false,
      "dpad_left": false,
      "dpad_right": false
    },
    "axes": {
      "left_x": 0.0,
      "left_y": 0.82,
      "right_x": -0.31,
      "right_y": 0.0,
      "lt": 0.0,
      "rt": 0.5
    }
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| protocol_version | int | 协议版本，当前为 1 |
| seq | int64 | 上位机单调递增序号 |
| timestamp_ms | int64 | 毫秒级时间戳 |
| ttl_ms | int | 输入快照有效期 |
| mode | string | `vehicle` / `arm` |
| keyboard.pressed_keys | array[string] | 当前按下集合，未出现的键视为未按下 |
| gamepad.connected | bool | 手柄是否在线 |
| gamepad.buttons | object | 手柄按钮完整快照，摇杆按下使用 `l3` / `r3` |
| gamepad.axes | object | 摇杆范围 `[-1.0, 1.0]`，扳机范围 `[0.0, 1.0]` |

---

### 4. emergency_stop — 急停命令

紧急停止所有运动。

```json
{
  "type": "emergency_stop",
  "protocol_version": 1,
  "seq": 2001,
  "timestamp_ms": 1709971200000,
  "params": {
    "source": "button"
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| protocol_version | int | 协议版本，当前为 1 |
| seq | int64 | 上位机单调递增序号 |
| timestamp_ms | int64 | 毫秒级时间戳 |
| params.source | string | 触发来源，例如 `button`、`keyboard_space`、`gamepad_l3_r3` |

---

### 5. system_command — 系统命令

通用系统命令，可携带任意参数。

```json
{
  "type": "system_command",
  "command": "reset",
  "params": {
    "mode": "soft"
  },
  "timestamp": 1709971200000
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| command | string | 命令名称 |
| params | object | 命令参数（任意键值对） |
| timestamp | int64 | 毫秒级时间戳 |

---

### 6. heartbeat — 心跳包

每秒自动发送一次，用于保活检测。

```json
{
  "type": "heartbeat",
  "protocol_version": 1,
  "seq": 3000,
  "timestamp_ms": 1709971200000
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| protocol_version | int | 协议版本，当前为 1 |
| seq | int64 | 上位机单调递增序号 |
| timestamp_ms | int64 | 毫秒级时间戳 |

下位机应返回：

```json
{
  "type": "heartbeat_ack",
  "protocol_version": 1,
  "seq": 3000,
  "timestamp_ms": 1709971200100,
  "server_time_ms": 1709971200100
}
```

---

### 7. ack — 关键命令确认

关键离散命令必须返回 ACK。上位机会在发送后登记 pending，超时、断线、迟到 ACK 都会进入 UI 日志。

```json
{
  "type": "ack",
  "protocol_version": 1,
  "ack_type": "emergency_stop",
  "seq": 2001,
  "ok": true,
  "code": 0,
  "message": "emergency active",
  "timestamp_ms": 1709971200100
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| ack_type | string | 被确认的命令类型 |
| seq | int64 | 对应原命令的 `seq` |
| ok | bool | 是否成功 |
| code | int | 0 成功，非 0 为错误码 |
| message | string | 人类可读信息 |

mock server 可用环境变量模拟 ACK 行为：

- `MOCK_ACK_MODE=ok`：默认成功 ACK。
- `MOCK_ACK_MODE=fail`：返回失败 ACK。
- `MOCK_ACK_MODE=drop`：不返回 ACK，用于测试超时。
- `MOCK_ACK_DELAY_SEC=3`：延迟 ACK，用于测试迟到 ACK 或超时。

---

### 8. sync_request — 状态同步请求

上位机连接或重连后自动发送，请求下位机返回当前系统快照。

```json
{
  "type": "sync_request",
  "protocol_version": 1,
  "seq": 3010,
  "timestamp_ms": 1709971200000,
  "params": {
    "reason": "connected"
  }
}
```

### 9. camera_list_request — 摄像头列表请求

上位机连接或重连后自动发送，请求下位机返回全部摄像头状态。

```json
{
  "type": "camera_list_request",
  "protocol_version": 1,
  "seq": 3020,
  "timestamp_ms": 1709971200000
}
```

---

## 二、下位机 → 上位机（接收）

### 0. hello / capabilities — bridge 握手

下位机 bridge 建立连接后可以主动发送 `hello` 和 `capabilities`，用于说明 bridge 身份和支持的协议能力。

```json
{
  "type": "hello",
  "protocol_version": 1,
  "seq": 0,
  "timestamp_ms": 1709971200000,
  "bridge_name": "host_bridge_node",
  "bridge_version": "1.0.0",
  "robot_id": "robot_001"
}
```

```json
{
  "type": "capabilities",
  "protocol_version": 1,
  "seq": 0,
  "timestamp_ms": 1709971200000,
  "supports": [
    "operator_input",
    "heartbeat_ack",
    "critical_ack",
    "sync_request",
    "camera_list_request"
  ],
  "max_frame_bytes": 1048576
}
```

### 0.1 system_snapshot / camera_list_response — 同步响应

```json
{
  "type": "system_snapshot",
  "protocol_version": 1,
  "seq": 3010,
  "timestamp_ms": 1709971200100,
  "control_mode": "vehicle",
  "emergency": { "active": false, "source": "" },
  "motor": { "initialized": true, "enabled": true },
  "modules": {
    "base": "online",
    "arm": "online",
    "camera": "degraded"
  },
  "last_error": { "code": 0, "message": "" }
}
```

```json
{
  "type": "camera_list_response",
  "protocol_version": 1,
  "seq": 3020,
  "timestamp_ms": 1709971200100,
  "cameras": [
    {
      "camera_id": 0,
      "name": "front",
      "online": true,
      "rtsp_url": "rtsp://192.168.1.50:8554/front",
      "codec": "h264",
      "width": 1280,
      "height": 720,
      "fps": 25,
      "bitrate_kbps": 2500
    }
  ]
}
```

### 1. motor_state — 电机状态数据

下位机上报6关节+执行器的实时状态。

```json
{
  "type": "motor_state",
  "joints": [
    { "position": 1.0, "current": 0.5 },
    { "position": 2.0, "current": 0.6 },
    { "position": 1.5, "current": 0.4 },
    { "position": 0.8, "current": 0.3 },
    { "position": 1.2, "current": 0.7 },
    { "position": 0.9, "current": 0.5 }
  ],
  "executor_position": 0.5,
  "executor_torque": 1.0,
  "executor_flags": 1,
  "reserved": 0
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| joints | array[6] | 6个关节状态 |
| joints[].position | float | 关节位置（上位机内部×1000转int16存储） |
| joints[].current | float | 关节电流（上位机内部×1000转int16存储） |
| executor_position | float | 执行器位置 |
| executor_torque | float | 执行器扭矩 |
| executor_flags | int | 执行器标志位 |
| reserved | int | 保留字段 |

---

### 2. joint_data — 单关节数据

下位机上报单个关节的详细数据。

```json
{
  "type": "joint_data",
  "joint_id": 0,
  "position": 1.5,
  "current": 0.5,
  "torque": 0.8
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| joint_id | int | 关节ID（0-5） |
| position | float | 关节位置 |
| current | float | 关节电流 |
| torque | float | 关节扭矩 |

---

### 3. environment / co2 — 环境传感器数据

下位机上报CO2浓度数据。支持两种 `type` 值：`"environment"` 或 `"co2"`。

**格式一（嵌套 data）：**
```json
{
  "type": "environment",
  "data": {
    "co2_ppm": 420.5,
    "timestamp": 1709971200
  }
}
```

**格式二（扁平）：**
```json
{
  "type": "co2",
  "co2_ppm": 420.5,
  "timestamp": 1709971200
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| co2_ppm | float | CO2浓度（ppm） |
| timestamp | int | 秒级时间戳 |

**UI显示阈值：**
| 浓度范围 | 颜色 | 状态 |
|----------|------|------|
| < 800 ppm | 绿色 | 空气良好 |
| 800 - 1500 ppm | 橙色 | 注意通风 |
| > 1500 ppm | 红色 | 浓度过高 |

---

### 4. system_status — 系统状态

下位机上报系统运行状态，内容为任意键值对。

```json
{
  "type": "system_status",
  "cpu_usage": 45,
  "memory_usage": 60,
  "uptime": 3600,
  "ros_status": "running"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| (任意) | any | 由下位机自行定���的状态字段 |

---

### 5. camera_info — 摄像头信息推送

下位机上报摄像头的 RTSP 地址及参数信息，上位机收到后自动在对应格子启动 RTSP 播放。

```json
{
  "type": "camera_info",
  "camera_id": 0,
  "online": true,
  "codec": "h265",
  "width": 1280,
  "height": 720,
  "fps": 30,
  "bitrate_kbps": 2000,
  "rtsp_url": "rtsp://192.168.1.100:8554/cam0"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| camera_id | int | 摄像头ID（0-4），对应上位机 2x3 网格的前5个格子 |
| online | bool | true=上线自动播放，false=离线停止播放 |
| codec | string | 编码格式（如 "h264"、"h265"） |
| width | int | 视频宽度（像素） |
| height | int | 视频高度（像素） |
| fps | int | 帧率 |
| bitrate_kbps | int | 码率（kbps） |
| rtsp_url | string | RTSP 流地址 |

---

## 三、协议约定

1. **传输格式**：每条消息为一行紧凑 JSON，以 `\n` 分隔
2. **必要字段**：所有消息必须包含 `"type"` 字段
3. **时间戳**：发送方向的 `timestamp` 为毫秒级（`QDateTime::currentMSecsSinceEpoch()`），接收方向由下位机决定
4. **心跳机制**：上位机每 1 秒发送一次 heartbeat，若检测到连接断开则自动重连（最多 5 次，间隔 3 秒）
5. **数值精度**：motor_state 中的 position/current 为浮点值，上位机内部乘以 1000 转为 int16 存储
