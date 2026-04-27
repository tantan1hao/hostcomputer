# Host Computer 项目开发文档

## 项目概述

这是一个基于Qt6的C++桌面应用程序，用于电机控制和通信。项目采用严格的四层分层架构设计，使用CMake构建系统，目标平台为Windows。

## 构建说明

### 基础构建命令
```bash
# 创建构建目录
mkdir build
cd build

# 配置CMake项目
cmake .. -G "Ninja"

# 编译项目
ninja

# 运行程序
./hostcomputer.exe
```

### 开发调试
```bash
# 增量编译（build目录下）
ninja

# 清理构建产物
ninja clean

# 重新配置CMake
cmake .. -G "Ninja"
```

## 四层分层架构

项目严格遵循四层架构设计，每层职责明确，相互隔离：

```
┌─────────────────┐
│   Controller    │ ← 业务逻辑层，UI交互
├─────────────────┤
│ Communication   │ ← 通信层，设备连接
├─────────────────┤
│     Parser      │ ← 解析层，协议处理
├─────────────────┤
│      Model      │ ← 模型层，数据绑定
└─────────────────┘
```

### 各层详细职责

#### 1. Model 层 (src/model/)
**核心职责**: UI数据模型，使用QObject+Q_PROPERTY实现数据绑定

**关键特性**:
- 继承自QObject，支持Qt的元对象系统
- 使用Q_PROPERTY宏定义属性，支持QML绑定
- 发出信号通知UI数据变化
- 不包含任何业务逻辑

**核心组件**:
```cpp
class MotorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    // ... 其他属性
public:
    double speed() const;
    void setSpeed(double speed);

signals:
    void speedChanged();
    void positionChanged();
    void temperatureChanged();

private:
    double m_speed;
    double m_position;
    double m_temperature;
};
```

**依赖**: Qt6::Core

**规则**:
- ✅ 只能用于UI数据绑定
- ❌ 不包含业务逻辑
- ❌ 不包含通信逻辑
- ❌ 不直接访问设备

---

#### 2. Parser 层 (src/parser/)
**核心职责**: 将原始数据(QByteArray)解析为结构化数据(MotorState)

**关键特性**:
- 处理通信协议解析
- 数据校验和完整性检查
- 将二进制数据转换为C++结构体
- 支持多种数据格式

**核心组件**:
```cpp
struct MotorState {
    double speed;
    double position;
    double temperature;
    bool isOnline;
    QDateTime timestamp;
};

class Parser : public QObject {
    Q_OBJECT
public:
    explicit Parser(QObject* parent = nullptr);

    MotorState parseFrame(const QByteArray& data);
    bool validateFrame(const QByteArray& data);

signals:
    void stateUpdated(const MotorState& state);
    void parseError(const QString& error);

private:
    MotorState parseMotorData(const QByteArray& data);
    bool checkCRC(const QByteArray& data);
};
```

**依赖**: Qt6::Core

**规则**:
- ✅ 处理协议解析和数据转换
- ✅ 数据校验和完整性检查
- ❌ 不发送命令
- ❌ 不访问UI
- ❌ 不直接访问设备

---

#### 3. Communication 层 (src/communication/)
**核心职责**: CAN总线、串口、TCP通信管理

**关键特性**:
- 管理物理通信连接
- 发送和接收原始数据帧
- 连接状态监控
- 错误处理和重连机制

**核心组件**:
```cpp
class CANManager : public QObject {
    Q_OBJECT
public:
    explicit CANManager(QObject* parent = nullptr);

    bool connectToDevice(const QString& device);
    void disconnect();
    bool isConnected() const;

    void sendFrame(const QByteArray& frame);

signals:
    void frameReceived(const QByteArray& frame);
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString& error);

private slots:
    void onFrameReceived();

private:
    QSerialPort* m_serialPort;
    bool m_isConnected;
};
```

**依赖**: Qt6::Core, Qt6::Network, Qt6::SerialPort

**规则**:
- ✅ 管理通信连接和数据传输
- ✅ 发送和接收原始数据帧
- ❌ 不包含业务逻辑
- ❌ 不直接修改UI
- ✅ 只发出raw frame（原始数据帧）

---

#### 4. Controller 层 (src/controller/)
**核心职责**: 业务逻辑处理，协调其他各层

**关键特性**:
- 协调各层之间的数据流
- 处理用户交互逻辑
- 状态管理和决策
- 错误处理和异常恢复

**核心组件**:
```cpp
class MotorController : public QObject {
    Q_OBJECT
public:
    explicit MotorController(QObject* parent = nullptr);

    void connectToMotor(const QString& device);
    void disconnectFromMotor();
    void startMotor();
    void stopMotor();
    void setSpeed(double speed);

    MotorModel* motorModel() const;

signals:
    void motorConnected();
    void motorDisconnected();
    void errorOccurred(const QString& error);

private slots:
    void onFrameReceived(const QByteArray& frame);
    void onStateChanged(const MotorState& state);

private:
    CANManager* m_communication;
    Parser* m_parser;
    MotorModel* m_model;
};
```

**依赖**: 其他所有层级

**规则**:
- ✅ 负责业务逻辑处理
- ✅ 协调各层之间的交互
- ❌ 不处理底层通信细节（交给Communication层）
- ❌ 不直接操作QByteArray（交给Parser层）

## 代码生成规则

### 通用规则
1. **严格遵循分层架构**: 每层只能调用下层，不能跨层调用
2. **使用Qt信号槽机制**: 实现异步通信和组件解耦
3. **模块化设计**: 每层作为独立的静态库
4. **实时性考虑**: 作为上位机应用，重点关注通信实时性和数据处理的稳定性

### 各层特定规则

#### Controller 层代码生成规则
- ✅ 处理业务逻辑和用户交互
- ✅ 协调Model、Parser、Communication三层
- ❌ 不处理底层通信细节
- ❌ 不直接操作QByteArray（交给Parser）
- ✅ 可以访问UI组件
- ✅ 可以发出高级命令

#### Communication 层代码生成规则
- ✅ 管理设备连接和断开
- ✅ 发送和接收原始数据帧
- ❌ 不包含业务逻辑
- ❌ 不直接修改UI
- ✅ 只发出raw frame（原始数据帧）
- ❌ 不解析数据内容

#### Parser 层代码生成规则
- ✅ 解析原始数据为结构化数据
- ✅ 数据校验和完整性检查
- ❌ 不发送命令
- ❌ 不访问UI
- ❌ 不访问设备
- ✅ 纯数据处理逻辑

#### Model 层代码生成规则
- ✅ 定义数据属性和访问方法
- ✅ 发出数据变化信号
- ❌ 不包含业务逻辑
- ❌ 不包含通信逻辑
- ✅ 支持QML数据绑定
- ✅ 继承QObject并使用Q_PROPERTY

## 技术栈

- **框架**: Qt6.8.3
- **C++标准**: C++20
- **构建系统**: CMake 3.16+ + Ninja
- **UI技术**: Qt Widgets + Qt Quick (QML)
- **通信模块**: CAN、串口、TCP
- **图形渲染**: OpenGL
- **数据可视化**: Qt Charts

## Qt模块依赖

项目配置了以下Qt6模块：
- Qt6::Core (核心功能)
- Qt6::Widgets (传统Widget UI)
- Qt6::Quick + Qt6::QuickWidgets (QML支持)
- Qt6::Network (网络通信)
- Qt6::SerialPort (串口通信)
- Qt6::OpenGL + Qt6::OpenGLWidgets (3D渲染)
- Qt6::Charts (图表显示)

## 开发优先级

建议按以下顺序实现各层：

1. **Model层**: 首先实现数据结构和MotorModel
2. **Parser层**: 基础数据解析功能
3. **Communication层**: CAN通信管理
4. **Controller层**: 业务逻辑和UI界面

## 项目结构

```
hostcomputer/
├── src/
│   ├── communication/   # TCP/JSON 通信层
│   │   ├── ROS1TcpClient.h/.cpp
│   │   └── SharedStructs.h
│   ├── controller/      # 业务逻辑和可复用 UI 组件
│   │   ├── controller.h/.cpp
│   │   ├── CameraGridWidget.h/.cpp
│   │   ├── ControlPanelWidget.h/.cpp
│   │   ├── TelemetryPanelWidget.h/.cpp
│   │   ├── RobotAttitudeWidget.h/.cpp
│   │   ├── RtspPlayerWidget.h/.cpp
│   │   ├── KeyboardController.h/.cpp
│   │   └── handlekey.h/.cpp
│   └── utils/           # 日志和错误处理
│       ├── Logger.h/.cpp
│       └── ErrorHandler.h/.cpp
├── resources/           # 图标、QRC、QML资源
├── scripts/             # 构建冒烟和联调辅助脚本
├── CMakeLists.txt      # 主CMake配置
├── main.cpp           # 程序入口
├── mainwindow.h/.cpp  # 主窗口
└── mainwindow.ui      # UI设计文件
```

## 当前 UI 组件架构

主窗口仍由 `mainwindow.ui` 提供基础容器、菜单和日志区域，但主要功能区已经拆成代码化 Widget：

- `CameraGridWidget`：管理 2x3 摄像头网格，其中 5 格为 RTSP 视频，1 格承载辅助状态面板。
- `TelemetryPanelWidget`：显示连接、心跳、FPS、带宽、模式、手柄、错误数和 CO2。
- `ControlPanelWidget`：显示手柄状态/映射，并发出手柄连接和急停请求。
- `RobotAttitudeWidget`：封装 `QQuickWidget + RobotViewModel`，负责 QML 3D 姿态视图。
- `MainWindow`：负责组件组装、信号转发、菜单动作、TCP 连接对话框和高层控制决策。

大改 UI 时优先改这些独立 Widget。不要把新面板继续堆回 `mainwindow.cpp`，除非它只是组件装配或信号连接。

## 开发示例

### 示例1: 电机速度控制流程

```cpp
// Controller层处理用户输入
void MotorController::setSpeed(double targetSpeed) {
    if (!m_communication->isConnected()) {
        emit errorOccurred("电机未连接");
        return;
    }

    // 构造速度控制命令帧
    QByteArray speedFrame = buildSpeedCommand(targetSpeed);

    // 通过Communication层发送
    m_communication->sendFrame(speedFrame);
}

// Communication层发送原始数据
void CANManager::sendFrame(const QByteArray& frame) {
    if (m_serialPort->isOpen()) {
        m_serialPort->write(frame);
    }
}

// 电机响应数据流程
// Communication -> Parser -> Model -> UI
```

### 示例2: 数据接收处理流程

```cpp
// Communication层接收原始数据
void CANManager::onFrameReceived() {
    QByteArray data = m_serialPort->readAll();
    emit frameReceived(data);  // 发出原始数据帧
}

// Parser层解析数据
MotorState Parser::parseFrame(const QByteArray& data) {
    MotorState state;
    if (validateFrame(data)) {
        state = parseMotorData(data);
    }
    return state;
}

// Controller层协调处理
void MotorController::onFrameReceived(const QByteArray& frame) {
    MotorState state = m_parser->parseFrame(frame);
    m_model->updateState(state);  // 更新Model层数据
}

// Model层通知UI更新
void MotorModel::updateState(const MotorState& state) {
    setSpeed(state.speed);
    setPosition(state.position);
    setTemperature(state.temperature);
}
```

## 最佳实践

### 1. 错误处理
```cpp
// 每层都应该有适当的错误处理
void MotorController::handleError(const QString& error) {
    qWarning() << "Controller错误:" << error;
    emit errorOccurred(error);
}

void CANManager::handleCommunicationError(const QString& error) {
    qCritical() << "通信错误:" << error;
    disconnect();
    emit errorOccurred(error);
}
```

### 2. 信号槽连接
```cpp
// 在Controller层建立各层连接
void MotorController::initializeConnections() {
    connect(m_communication, &CANManager::frameReceived,
            this, &MotorController::onFrameReceived);
    connect(m_communication, &CANManager::connectionStateChanged,
            this, &MotorController::onConnectionStateChanged);
}
```

### 3. 状态管理
```cpp
// 使用枚举定义清晰的状态
enum class MotorStatus {
    Disconnected,
    Connected,
    Running,
    Error
};
```

## 注意事项

- 开发时需要遵循Qt6的API规范，注意与Qt5的差异
- 作为工业级应用，需重点关注错误处理和异常安全性
- 严格遵守分层架构，避免跨层调用
- 使用Qt的信号槽机制实现组件间的松耦合
- 优先考虑实时性和性能要求
- 构建产物、Qt Creator 用户文件、日志和 `build-*` 目录不应提交到版本库

## 常见问题

### Q: 能否在Model层直接调用Communication层？
A: **不能**。Model层只能用于数据绑定，不能包含通信逻辑。应该通过Controller层协调。

### Q: Parser层能否发送数据到设备？
A: **不能**。Parser层只负责数据解析，不能发送命令。发送命令应该通过Controller层调用Communication层。

### Q: 如何处理跨层的数据共享？
A: 使用Qt的信号槽机制。下层通过信号通知上层，上层通过槽函数接收数据。

### Q: 可以在Controller层直接解析QByteArray吗？
A: **不建议**。应该将解析逻辑放在Parser层，Controller层只负责业务逻辑协调。
