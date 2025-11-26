# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于Qt6的C++桌面应用程序，用于电机控制、实时监控和多摄像头显示。项目采用分层架构设计，使用CMake构建系统，目标平台为Windows。主要功能包括：

- **串口通信**: 支持设备选择、实时刷新和自动重连
- **多摄像头管理**: 支持最多6路USB摄像头的并发显示
- **数据解析**: 52字节协议帧解析，支持6关节数据显示
- **实时监控**: 电机状态、系统性能和设备连接状态监控

## 构建命令

### 基础构建
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

## 项目架构

项目采用四层分层架构，按职责分离��

### 1. Model层 (src/model/)
- **职责**: UI数据模型，使用QObject+Q_PROPERTY实现数据绑定
- **核心组件**: `MotorModel`类
- **依赖**: Qt6::Core

### 2. Parser层 (src/parser/)
- **职责**: 将原始数据(QByteArray)解析为结构化数据(MotorState)
- **核心组件**: `ProtocolParser`类、`MotorState`结构体、`JointState`结构体
- **功能**: 52字节协议帧解析、6关节数据结构化
- **依赖**: Qt6::Core

#### 数据结构
```cpp
struct JointState {
    float position;           // 关节位置
    float current;            // 电流值
    float executor_position;  // 执行器位置
    float executor_torque;    // 执行器扭矩
    int executor_flags;       // 执行器标志位
    int reserved;             // 保留字段
};

struct MotorState {
    JointState joints[6];     // 6个关节数据
    bool isOnline;           // 在线状态
    bool isRunning;          // 运行状态
    bool hasError;           // 错误状态
    qint64 timestamp;        // 时间戳
};
```

### 3. Communication层 (src/communication/)
- **职责**: CAN总线、串口、TCP通信管理
- **核心组件**: `SerialPortManager`类、`CANManager`类
- **功能**: 串口设备检测、自动重连、数据收发、错误处理
- **依赖**: Qt6::Core, Qt6::Network, Qt6::SerialPort

#### 串口管理特性
- **自动设备检测**: 实时扫描可用串口设备
- **动态连接管理**: 支持运行时连接/断开
- **错误恢复**: 自动重连机制和异常处理
- **多波特率支持**: 9600-921600bps常用波特率

### 4. Controller层 (src/controller/)
- **职责**: 业务逻辑处理，协调其他各层
- **核心组件**: `CameraManager`类、`CameraWorker`类
- **功能**: 多线程摄像头管理、非阻塞视频流处理、设备热插拔
- **依赖**: 其他所有层级、Qt6::Multimedia、OpenCV

#### 摄像头管理系统
```cpp
class CameraManager : public QObject {
    // 支持最多6路摄像头并发显示
    bool startCamera(int cameraId, QLabel* displayWidget);
    void stopCamera(int cameraId);
    void startAllCameras();
    QStringList getAvailableCameras() const;

signals:
    void frameReady(int cameraId, const QPixmap& frame);
    void cameraListChanged(const QStringList& cameraNames);
};
```

#### 技术特点
- **多线程处理**: 每个摄像头在独立线程中运行
- **Qt/OpenCV双引擎**: 优先使用Qt Camera，OpenCV作为备用
- **自动设备检测**: 支持USB摄像头热插拔
- **非阻塞UI**: 信号槽机制确保主界面流畅
- **自适应分辨率**: 根据显示控件自动调整视频尺寸

## 技术栈

- **框架**: Qt6.8.3 (兼容Qt5)
- **C++标准**: C++20
- **构建系统**: CMake 3.16+ + Ninja
- **UI技术**: Qt Widgets
- **通信模块**: CAN、串口、TCP
- **多媒体处理**: Qt6::Multimedia + Qt6::MultimediaWidgets
- **计算机视觉**: OpenCV 4.x (摄像头备用方案)
- **图形渲染**: OpenGL
- **数据可视化**: Qt Charts

## Qt模块依赖

项目配置了以下Qt模块：
### 必需模块
- Qt6::Core (核心功能)
- Qt6::Widgets (传统Widget UI)
- Qt6::Network (网络通信)
- Qt6::SerialPort (串口通信)
- Qt6::Multimedia (摄像头支持)
- Qt6::MultimediaWidgets (视频显示控件)

### 可选模块
- Qt6::OpenGL + Qt6::OpenGLWidgets (3D渲染)
- Qt6::Charts (图表显示)
- Qt5兼容层 (Qt5.x版本支持)

### 第三方依赖
- **OpenCV 4.x**: 计算机视觉库，摄像头备用方案
  - `opencv_core`
  - `opencv_imgproc`
  - `opencv_imgcodecs`
  - `opencv_videoio`
  - `opencv_highgui`

## 开发优先级

建议按以下顺序实现各层：

1. **Model层**: 首先实现数据结构和MotorModel ✅
2. **Parser层**: 基础数据解析功能 ✅
3. **Communication层**: 串口通信管理 ✅
4. **Controller层**: 业务逻辑和UI界面，包含摄像头管理 ✅

## 已实现功能

### 1. 串口通信系统
- **设备选择对话框**: 支持实时刷新和波特��选择
- **自动设备检测**: F5快捷键刷新串口列表
- **连接管理**: 连接/断开、状态监控、错误处理
- **数据收发**: QByteArray格式数据传输

### 2. 协议解析系统
- **52字节帧解析**: ESP32State结构体解析
- **6关节数据**: position、current、executor_position、executor_torque、executor_flags、reserved
- **错误处理**: 解析失败检测和报告
- **数据转换**: 原始数据到MotorState结构体转换

### 3. 多摄像头管理系统
- **多线程支持**: 每个摄像头独立线程处理
- **Qt/OpenCV双引擎**: 优先Qt Camera，OpenCV备用
- **设备管理**: 自动检测、热插拔支持
- **非阻塞UI**: 信号槽机制保证界面流畅

### 4. 主界面集成
- **实时数据显示**: text_errors控件显示6关节数据
- **系统状态**: 连接状态、心跳、FPS、CPU/GPU监控
- **快捷操作**: F5刷新串口、菜单操作
- **日志管理**: 命令和错误信息显示

## 关键设计原则

1. **严格遵循分层架构**: 每层只能调用下层，不能跨层调用
2. **使用Qt信号槽机制**: 实现异步通信和组件解耦
3. **模块化设计**: 每层作为独立的静态库
4. **实时性考虑**: 作为上位机应用，重点关注通信实时性和数据处理的稳定性

## 项目结构

```
hostcomputer/
├── src/
│   ├── controller/      # 业务逻辑层
│   ├── communication/   # 通信层
│   ├── parser/          # 协议解析层
│   └── model/           # 数据模型层
├── resources/           # 资源文件
├── build/              # 构建输出
├── CMakeLists.txt      # 主CMake配置
├── main.cpp           # 程序入口
├── mainwindow.h/.cpp  # 主窗口
└── mainwindow.ui      # UI设计文件
```

## 数据流架构

```
硬件设备 → 串口通信 → 协议解析 → UI显示
   ↓           ↓         ↓         ↓
USB摄像头  CameraManager  CameraWorker  QLabel控件
ESP32设备 SerialPortManager ProtocolParser text_errors
```

### 串口数据流
1. **设备层**: ESP32通过串口发送52字节协议帧
2. **通信层**: SerialPortManager接收原始字节流
3. **解析层**: ProtocolParser解析为结构化数据
4. **显示层**: MainWindow更新UI控件显示关节数据

### 摄像头数据流
1. **硬件层**: USB摄像头设备
2. **管理层**: CameraManager多线程管理
3. **处理层**: CameraWorker帧捕获和处理
4. **显示层**: QLabel控件实时显示视频流

## 使用指南

### 1. 串口连接
- 点击菜单"连接" → 选择串口设备和波特率
- 或使用快捷键F5刷新串口列表
- 连接成功后状态栏显示"设备已连接"

### 2. 数据查看
- 6关节数据实时显示在text_errors控件
- 命令记录显示在text_commands控件
- 系统状态显示在对应标签控件

### 3. 摄像头使用
- 创建CameraManager实例
- 调用startAllCameras()自动启动所有可用摄像头
- 视频流显示在对应的QLabel控件中

## 构建说明

### Windows平台
```bash
# 确保已安装Qt6.8.3和OpenCV 4.x
# Qt安装路径示例: C:/Qt/6.8.3/mingw_64
# OpenCV安装路径示例: C:/opencv/build

mkdir build && cd build
cmake .. -G "Ninja" -DQt6_DIR=C:/Qt/6.8.3/mingw_64/lib/cmake/Qt6
ninja
```

### 依赖检查
- Qt6::Core, Qt6::Widgets, Qt6::Network
- Qt6::SerialPort, Qt6::Multimedia, Qt6::MultimediaWidgets
- OpenCV核心模块（可选，用于摄像头备用方案）

## 注意事项

- 项目核心功能已实现：串口通信、协议解析、摄像头管理
- 支持Qt5/Qt6双版本兼容
- 作为工业级应用，重点关注错误处理和异常安全性
- 摄像头USB设备不使用串口通信，直接通过USB接口
- 6关节数据每秒更新多次，注意UI刷新频率优化