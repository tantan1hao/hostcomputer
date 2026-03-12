#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "src/communication/SharedStructs.h"  // 先包含Communication命名空间
#include "src/communication/serialportmanager.h"
#include "src/communication/ROS1TcpClient.h"

#include "src/parser/ProtocolParser.h"
#include <QtGlobal>
#include <QSpinBox>

#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QDebug>
#include <QDateTime>
#include <QInputDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QApplication>
#include <QShortcut>
#include <QKeySequence>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serialManager(nullptr)
    , m_tcpClient(nullptr)
    , m_keyboardController(nullptr)
    , m_displayLayout(nullptr)
    , m_co2Widget(nullptr)
    , m_gamepadWidget(nullptr)
    , m_handleKey(nullptr)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("电机控制系统上位机 v1.0");
    resize(1920, 1080);
    setMinimumSize(1600, 900);

    // 设置布局到 CarWidget 上
    // 将 模型 类的 UI 添加到布局中

    // 初始化组件
    setupSerialPort();
    setupParser();
    setupTcpClient();
    setupDisplayLayout();

    // 初始化键盘控制器
    m_keyboardController = new KeyboardController(this);
    connect(m_keyboardController, &KeyboardController::velocityChanged,
            this, [this](float lx, float ly, float az) {
        // 只在方向变化时打一次日志
        static float lastLx = 0.0f, lastAz = 0.0f;
        bool changed = (lx != lastLx || az != lastAz);
        if (changed) {
            lastLx = lx;
            lastAz = az;
            bool isStopped = (lx == 0.0f && az == 0.0f);
            if (!isStopped) {
                QString dir;
                if (lx > 0) dir += "前进 ";
                if (lx < 0) dir += "后退 ";
                if (az > 0) dir += "左转 ";
                if (az < 0) dir += "右转 ";
                addCommand(QString("[键盘] %1 (lx=%.2f az=%.2f)").arg(dir.trimmed()).arg(lx).arg(az));
            } else {
                addCommand("[键盘] 停止");
            }
        }

        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendVelocityCommand(lx, ly, az);
        }
    });
    connect(m_keyboardController, &KeyboardController::emergencyStopRequested,
            this, [this]() {
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendEmergencyStop();
            addCommand("[键盘] 急停!");
        }
    });
    // 键盘机械臂模式信号
    connect(m_keyboardController, &KeyboardController::jointControlRequested,
            this, [this](int jointId, float position, float velocity) {
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendJointControl(jointId, position, velocity);
            addCommand(QString("[键盘-机械臂] 关节%1 位置:%2 速度:%3")
                       .arg(jointId).arg(position, 0, 'f', 3).arg(velocity, 0, 'f', 3));
        }
    });
    connect(m_keyboardController, &KeyboardController::executorControlRequested,
            this, [this](float value) {
        if (m_tcpClient && m_tcpClient->isConnected()) {
            QJsonObject params;
            params["value"] = value;
            m_tcpClient->sendSystemCommand("executor_control", params);
            addCommand(QString("[键盘-机械臂] 执行器: %1").arg(value > 0 ? "张开" : "闭合"));
        }
    });
    // 默认启用键盘控制
    m_keyboardController->setEnabled(true);

    // 初始化手柄输入驱动 (XInput本地轮询)
    m_handleKey = new HandleKey(this);
    connect(m_handleKey, &HandleKey::getHandleKey,
            this, &MainWindow::onGamepadStateReceived);

    // 初始化定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateSystemStatus);
    m_statusTimer->start(1000); // 每秒更新一次状态

    // 设置连接和状态栏
    setupConnections();
    setupStatusBar();

    
    // 初始化UI状态
    updateConnectionDisplay();
    updateHeartbeatDisplay();

    // 添加系统启动提示
    addCommand("[系统] 主窗口初始化完成");
    addCommand("[系统] 请连接串口设备开始接收数据");

    // USB-CDC握手提示
    addCommand("[USB-CDC] 提示：等待ESP32设备连接和握手...");
    addCommand("[USB-CDC] 如果ESP32发送握手信号，程序会自动响应");
}

MainWindow::~MainWindow()
{
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
    if (m_tcpClient) {
        m_tcpClient->disconnectFromROS();
    }
    delete ui;
}

void MainWindow::setupSerialPort()
{
    // 创建串口管理器
    m_serialManager = new Communication::SerialPortManager(this);

    // 连接串口数据接收信号（用于调试和日志）
    connect(m_serialManager, &Communication::SerialPortManager::dataReceived,
            this, &MainWindow::onSerialDataReceived);

    // 连接解析后的ESP32状态数据信号
    connect(m_serialManager, &Communication::SerialPortManager::esp32StateReceived,
            this, &MainWindow::onEsp32StateReceived);

    // 连接连接状态变化信号
    connect(m_serialManager, &Communication::SerialPortManager::connectionStatusChanged,
            this, &MainWindow::updateConnectionStatus);
}

void MainWindow::setupDisplayLayout()
{
    // 创建布局管理器（2行3列）
    m_displayLayout = new DisplayLayoutManager(2, 3, this);

    // 创建CO2显示控件并放到布局格子中（索引5 = 第2行第3列）
    m_co2Widget = new CO2DisplayWidget();
    m_displayLayout->setWidget(5, m_co2Widget);

    // 创建手柄显示控件，放到右侧面板（小车姿态模型下方）
    m_gamepadWidget = new GamepadDisplayWidget();
    m_gamepadWidget->setMaximumHeight(180);
    ui->verticalLayout_right->insertWidget(1, m_gamepadWidget);

    // 把 DisplayLayoutManager 塞进 group_cameras 的布局中
    ui->group_cameras->layout()->addWidget(m_displayLayout);
}

void MainWindow::setupParser()
{
    // 协议解析已经移到SerialPortManager中处理
}

void MainWindow::setupTcpClient()
{
    m_tcpClient = new Communication::ROS1TcpClient(this);

    // 连接TCP状态信号
    connect(m_tcpClient, &Communication::ROS1TcpClient::connectedToROS, this, [this]() {
        addCommand("[TCP] 已连接到ROS节点");
        statusBar()->showMessage(QString("TCP已连接: %1:%2")
            .arg(m_tcpClient->getROSHost()).arg(m_tcpClient->getROSPort()));
    });

    connect(m_tcpClient, &Communication::ROS1TcpClient::disconnectedFromROS, this, [this]() {
        addCommand("[TCP] 与ROS节点断开连接");
        statusBar()->showMessage("TCP连接断开");
    });

    connect(m_tcpClient, &Communication::ROS1TcpClient::connectionError, this, [this](const QString &error) {
        addError(QString("[TCP] %1").arg(error));
    });

    // 连接数据接收信号
    connect(m_tcpClient, &Communication::ROS1TcpClient::motorStateReceived,
            this, &MainWindow::onEsp32StateReceived);

    connect(m_tcpClient, &Communication::ROS1TcpClient::systemStatusReceived, this, [this](const QJsonObject &status) {
        addCommand(QString("[TCP] 收到系统状态: %1").arg(
            QString(QJsonDocument(status).toJson(QJsonDocument::Compact))));
    });

    // CO2数据接收
    connect(m_tcpClient, &Communication::ROS1TcpClient::co2DataReceived,
            this, &MainWindow::onCO2DataReceived);

    // IMU数据接收
    connect(m_tcpClient, &Communication::ROS1TcpClient::imuDataReceived,
            this, &MainWindow::onIMUDataReceived);
}

void MainWindow::setupConnections()
{
    // 按钮连接已在UI文件中自动处理，这里可以添加额外的连接
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("系统就绪");
}

void MainWindow::updateConnectionStatus(bool connected)
{
    m_isConnected = connected;
    updateConnectionDisplay();

    if (connected) {
        statusBar()->showMessage("设备已连接");
        addCommand("[系统] 设备连接成功");
    } else {
        statusBar()->showMessage("设备已断开");
        addCommand("[系统] 设备连接断开");
    }
}

void MainWindow::updateHeartbeatStatus(bool online)
{
    m_heartbeatOnline = online;
    updateHeartbeatDisplay();

    if (!online && m_isConnected) {
        addError("[警告] 下位机心跳丢失");
    }
}

void MainWindow::updateFPS(int fps)
{
    m_currentFPS = fps;
    ui->label_fps_value->setText(QString("%1 FPS").arg(fps));
}

void MainWindow::updateBandwidthAndPacketLoss()
{
    if (!m_tcpClient) {
        ui->label_cpu->setText("带宽压力:");
        ui->label_cpu_value->setText("N/A");
        return;
    }

    auto stats = m_tcpClient->getStats();

    // 计算带宽压力（本周期收发字节数，单位KB/s，定时器间隔1秒）
    quint64 deltaBytes = (stats.bytesSent - m_lastBytesSent) + (stats.bytesReceived - m_lastBytesReceived);
    double bandwidthKBs = deltaBytes / 1024.0;
    m_lastBytesSent = stats.bytesSent;
    m_lastBytesReceived = stats.bytesReceived;

    // 带宽压力等级
    QString pressure;
    if (bandwidthKBs < 10.0)
        pressure = QString("%1 KB/s (低)").arg(bandwidthKBs, 0, 'f', 1);
    else if (bandwidthKBs < 100.0)
        pressure = QString("%1 KB/s (中)").arg(bandwidthKBs, 0, 'f', 1);
    else
        pressure = QString("%1 KB/s (高)").arg(bandwidthKBs, 0, 'f', 1);

    ui->label_cpu->setText("带宽压力:");
    ui->label_cpu_value->setText(pressure);

    // 计算丢包概率：发送数 - 接收数的��值比例
    quint64 deltaSent = stats.messagesSent - m_lastMessagesSent;
    quint64 deltaReceived = stats.messagesReceived - m_lastMessagesReceived;
    m_lastMessagesSent = stats.messagesSent;
    m_lastMessagesReceived = stats.messagesReceived;

    double lossRate = 0.0;
    if (deltaSent > 0 && deltaSent > deltaReceived) {
        lossRate = (double)(deltaSent - deltaReceived) / deltaSent * 100.0;
    }
    ui->label_cpu_value->setText(QString("%1 | 丢包: %2%").arg(pressure).arg(lossRate, 0, 'f', 1));
}

void MainWindow::updateMotorMode(const QString& mode)
{
    m_motorMode = mode;
    ui->label_mode_value->setText(mode);
    addCommand(QString("[系统] 电机模式切换: %1").arg(mode));
}

void MainWindow::addCommand(const QString& command)
{
    formatAndAddCommand(command);
}

void MainWindow::addError(const QString& error)
{
    m_errorCount++;
    formatAndAddError(error);
    ui->label_error_count->setText(QString("错误: %1").arg(m_errorCount));
}

void MainWindow::updateCarAttitude(double roll, double pitch, double yaw)
{
    m_roll = roll;
    m_pitch = pitch;
    m_yaw = yaw;


}

void MainWindow::updateJointsData(const Communication::ESP32State& esp32State)
{
    // 清空text_errors控件
    ui->text_errors->clear();

    // 添加时间戳
    QString timestamp = getCurrentTimestamp();
    QString header = QString("[%1] 6关节数据接收\n").arg(timestamp);

    // 构建关节数据字符串
    QString jointsData = "=== 6关节数据 ===\n";
    for (int i = 0; i < 6; ++i) {
        jointsData += QString("关节 %1:\n").arg(i + 1);
        jointsData += QString("  位置: %1\n").arg(esp32State.joints[i].position / 1000.0f, 0, 'f', 3);
        jointsData += QString("  电流: %1 A\n").arg(esp32State.joints[i].current / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器位置: %1\n").arg(esp32State.executor_position / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器扭矩: %1\n").arg(esp32State.executor_torque / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器标志: %1\n").arg(esp32State.executor_flags);
        jointsData += QString("  保留字段: %1\n").arg(esp32State.reserved);
        if (i < 5) jointsData += "\n";
    }

    // 添加执行器数据摘要
    jointsData += "\n=== 执行器数据摘要 ===\n";
    jointsData += QString("执行器位置: %1\n").arg(esp32State.executor_position / 1000.0f, 0, 'f', 3);
    jointsData += QString("执行器扭矩: %1\n").arg(esp32State.executor_torque / 1000.0f, 0, 'f', 3);
    jointsData += QString("执行器标志: 0x%1\n").arg(esp32State.executor_flags, 2, 16, QChar('0'));
    jointsData += QString("保留字段: %1\n").arg(esp32State.reserved);

    // 设置文本内容
    ui->text_errors->setText(header + jointsData);

    // 在命令区域添加格式化的关节数据
    addCommand("[关节数据] 数据已更新到显示区域");

    // 格式化显示6个关节的位置数据（用于命令区域）
    QString positionData = QString("[关节数据] 位置: ");
    for (int i = 0; i < 6; ++i) {
        positionData += QString("J%1:%2 ").arg(i+1).arg(esp32State.joints[i].position / 1000.0f, 0, 'f', 3);
    }
    addCommand(positionData);

    // 格式化显示6个关节的电流数据（用于命令区域）
    QString currentData = QString("[电流数据] 电流: ");
    for (int i = 0; i < 6; ++i) {
        currentData += QString("J%1:%2A ").arg(i+1).arg(esp32State.joints[i].current / 1000.0f, 0, 'f', 3);
    }
    addCommand(currentData);

    // 可选：滚动到底部
    QTextCursor cursor = ui->text_errors->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->text_errors->setTextCursor(cursor);
    ui->text_errors->ensureCursorVisible();

    // 同时在命令区域显示接收消息
    addCommand("[数据] 已接收并显示6关节数据");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isConnected) {
        updateConnectionStatus(false);
    }
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_keyboardController && m_keyboardController->isEnabled()) {
        m_keyboardController->handleKeyPress(event);
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_keyboardController && m_keyboardController->isEnabled()) {
        m_keyboardController->handleKeyRelease(event);
    }
    QMainWindow::keyReleaseEvent(event);
}

// 菜单槽函数
void MainWindow::on_action_connect_triggered()
{
    // 显示串口选择对话框
    showSerialPortSelection();
}

void MainWindow::on_action_tcp_connect_triggered()
{
    showTcpConnectionDialog();
}

void MainWindow::on_action_disconnect_triggered()
{
    if (m_serialManager && m_serialManager->isOpen()) {
        m_serialManager->closePort();
        updateConnectionStatus(false);
        addCommand("[操作] 串口连接已断开");
    }

    if (m_tcpClient && m_tcpClient->isConnected()) {
        m_tcpClient->disconnectFromROS();
        addCommand("[操作] TCP连接已断开");
    }
}

void MainWindow::on_action_exit_triggered()
{
    close();
}

void MainWindow::on_action_fullscreen_triggered()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::on_action_reset_layout_triggered()
{
    // 确认重置
    auto reply = QMessageBox::question(this, "确认重置",
        "确定要重置界面吗？\n\n这将清除所有视频显示并重新初始化组件，可能解决卡顿问题。",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        reinitialize();
    }
}

void MainWindow::cleanupResources()
{

    // 1. 停止并断开串口
    if (m_serialManager) {
        if (m_serialManager->isOpen()) {
            m_serialManager->closePort();
        }
        m_serialManager->disconnect();
    }

    // 1.5 断开TCP
    if (m_tcpClient) {
        m_tcpClient->disconnectFromROS();
        m_tcpClient->disconnect();
    }

    // 2. 清空命令和错误显示
    if (ui->text_commands) {
        ui->text_commands->clear();
    }
    if (ui->text_errors) {
        ui->text_errors->clear();
    }

    // 3. 处理待处理的事件，确保UI刷新
    QApplication::processEvents();

}

void MainWindow::reinitialize()
{

    // 1. 清理现有资源
    cleanupResources();

    // 2. 删除旧组件
    if (m_serialManager) {
        delete m_serialManager;
        m_serialManager = nullptr;
    }

    if (m_tcpClient) {
        delete m_tcpClient;
        m_tcpClient = nullptr;
    }

    // 3. 删除旧的布局管理器
    if (m_displayLayout) {
        delete m_displayLayout;
        m_displayLayout = nullptr;
        m_co2Widget = nullptr;  // CO2控件随布局一起销毁
    }

    if (m_gamepadWidget) {
        delete m_gamepadWidget;
        m_gamepadWidget = nullptr;
    }

    // 4. 重置状态变量
    m_isConnected = false;
    m_heartbeatOnline = false;
    m_currentFPS = 0;
    m_lastBytesSent = 0;
    m_lastBytesReceived = 0;
    m_lastMessagesSent = 0;
    m_lastMessagesReceived = 0;
    m_motorMode = "待机";
    m_errorCount = 0;

    // 5. 处理待处理的事件
    QApplication::processEvents();

    // 6. 重新初始化组件
    setupSerialPort();
    setupParser();
    setupTcpClient();
    setupDisplayLayout();

    // 7. 重新更新UI状态
    updateConnectionDisplay();
    updateHeartbeatDisplay();

    // 8. 添加提示信息
    addCommand("[系统] 界面重置完成");
    addCommand("[系统] 所有组件已重新初始化");
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, "关于",
        "电机控制系统上位机 v1.0\n\n"
        "基于Qt6开发的电机控制和监控系统\n"
        "支持多相机显示、姿态监控、指令管理等功能\n\n"
        "开发团队: AI Assistant\n"
        "技术支持: Qt6.8 + CMake");
}

void MainWindow::on_action_keyboard_control_toggled(bool checked)
{
    m_keyboardController->setEnabled(checked);

    if (checked) {
        addCommand("[键盘] 键盘控制已启用 (W前进 S后退 A左转 D右转 Space急停)");
        statusBar()->showMessage("键盘控制: 开启");
    } else {
        addCommand("[键盘] 键盘控制已禁用");
        statusBar()->showMessage("键盘控制: 关闭");
    }
}

void MainWindow::on_btn_clear_commands_clicked()
{
    ui->text_commands->clear();
    addCommand("[系统] 指令记录已清空");
}

void MainWindow::on_btn_clear_errors_clicked()
{
    ui->text_errors->clear();
    m_errorCount = 0;
    ui->label_error_count->setText("错误: 0");
    addCommand("[系统] 错误记录已清空");
}

void MainWindow::on_btn_emergency_stop_clicked()
{
    // === 最高优先级急停处理 ===
    addCommand("[急停] ⚠️ 用户触发急停按钮!");

    // 1. 立即停止所有运动（TCP通道）
    if (m_tcpClient && m_tcpClient->isConnected()) {
        m_tcpClient->sendEmergencyStop();
        m_tcpClient->sendVelocityCommand(0.0f, 0.0f, 0.0f);
        addCommand("[急停] TCP急停指令已发送");
    }

    // 2. 清空键盘控制器状态
    if (m_keyboardController) {
        m_keyboardController->setEnabled(false);
        m_keyboardController->setEnabled(true);  // 重置状态
        addCommand("[急停] 键盘控制器已重置");
    }

    // 3. 切换回车体模式（安全模式）
    if (m_controlMode != ControlMode::Vehicle) {
        switchControlMode(ControlMode::Vehicle);
        addCommand("[急停] 已切换回车体模式");
    }

    // 4. 状态栏提示
    statusBar()->showMessage("⚠️ 急停已触发！所有运动已停止", 5000);

    // 5. 视觉反馈：按钮闪烁效果
    QPushButton* btn = ui->btn_emergency_stop;
    QString originalStyle = btn->styleSheet();
    btn->setStyleSheet("QPushButton { background-color: #FFFF00; color: black; border: 3px solid #FF0000; }");
    QTimer::singleShot(500, this, [btn, originalStyle]() {
        btn->setStyleSheet(originalStyle);
    });
}

void MainWindow::updateSystemStatus()
{
    // 模拟系统状态更新
    // TODO: 实现真实的CPU/GPU监控
    static int counter = 0;
    counter++;

    // 模拟FPS变化
    if (counter % 3 == 0 && m_isConnected) {
        int simulatedFPS = 25 + (counter % 10);
        updateFPS(simulatedFPS);
    }

    // 更新带宽压力与丢包概率
    if (counter % 5 == 0) {
        updateBandwidthAndPacketLoss();
    }
}

void MainWindow::updateConnectionDisplay()
{
    QString statusText = m_isConnected ? "已连接" : "断开";
    QString styleSheet = m_isConnected ?
        "color: green; font-weight: bold;" :
        "color: red; font-weight: bold;";

    ui->label_connection_value->setText(statusText);
    ui->label_connection_value->setStyleSheet(styleSheet);
}

void MainWindow::updateHeartbeatDisplay()
{
    QString statusText = m_heartbeatOnline ? "在线" : "丢失";
    QString styleSheet = m_heartbeatOnline ?
        "color: green; font-weight: bold;" :
        "color: red; font-weight: bold;";

    ui->label_heartbeat_value->setText(statusText);
    ui->label_heartbeat_value->setStyleSheet(styleSheet);
}

void MainWindow::formatAndAddCommand(const QString& command)
{
    QString timestamp = getCurrentTimestamp();
    QString formattedCommand = QString("[%1] %2").arg(timestamp, command);

    QTextCursor cursor = ui->text_commands->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(formattedCommand + "\n");

    // 自动滚动到底部
    if (ui->checkbox_auto_scroll->isChecked()) {
        ui->text_commands->setTextCursor(cursor);
        ui->text_commands->ensureCursorVisible();
    }

    // 限制显示行数，避免内存占用过多
    QStringList lines = ui->text_commands->toPlainText().split('\n');
    if (lines.size() > 1000) {
        lines = lines.mid(-500); // 保留最近500行
        ui->text_commands->setText(lines.join('\n'));
    }
}

void MainWindow::formatAndAddError(const QString& error)
{
    QString timestamp = getCurrentTimestamp();
    QString formattedError = QString("[%1] %2").arg(timestamp, error);

    QTextCursor cursor = ui->text_errors->textCursor();
    cursor.movePosition(QTextCursor::End);

    // 设置错误文本颜色为红色
    QTextCharFormat format;
    format.setForeground(QColor(200, 0, 0));
    cursor.setCharFormat(format);
    cursor.insertText(formattedError + "\n");

    // 自动滚动到底部
    ui->text_errors->setTextCursor(cursor);
    ui->text_errors->ensureCursorVisible();

    // 限制显示行数
    QStringList lines = ui->text_errors->toPlainText().split('\n');
    if (lines.size() > 500) {
        lines = lines.mid(-200); // 保留最近200行错误
        ui->text_errors->setText(lines.join('\n'));
    }
}

QString MainWindow::getCurrentTimestamp() const
{
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

QStringList MainWindow::getAvailableSerialPorts()
{
    if (!m_serialManager) {
        return QStringList();
    }
    return m_serialManager->getAvailablePorts();
}

bool MainWindow::connectToSerialPort(const QString& portName, int baudRate)
{
    if (!m_serialManager) {
        addError("[错误] 串口管理器未初始化");
        return false;
    }

    bool success = m_serialManager->openPort(portName, baudRate);

    if (success) {
        updateConnectionStatus(true);
        addCommand(QString("[串口] 成功连接 %1, 波特率: %2").arg(portName).arg(baudRate));
    } else {
        addError(QString("[错误] 串口连接失败 %1, 波特率: %2").arg(portName).arg(baudRate));
    }

    return success;
}

void MainWindow::showSerialPortSelection()
{
    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("选择串口设备");
    dialog.resize(400, 200);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 添加说明标签
    QLabel* infoLabel = new QLabel("请选择串口设备和波特率进行连接:");
    layout->addWidget(infoLabel);

    // 串口选择
    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("串口:"));
    QComboBox* portCombo = new QComboBox();
    portCombo->setMinimumWidth(200);
    portLayout->addWidget(portCombo);
    layout->addLayout(portLayout);

    // 波特率选择
    QHBoxLayout* baudLayout = new QHBoxLayout();
    baudLayout->addWidget(new QLabel("波特率:"));
    QComboBox* baudCombo = new QComboBox();
    baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    baudCombo->setCurrentText("115200");
    baudCombo->setMinimumWidth(200);
    baudLayout->addWidget(baudCombo);
    layout->addLayout(baudLayout);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* refreshButton = new QPushButton("刷新串口列表");
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonLayout->addWidget(buttonBox);
    layout->addLayout(buttonLayout);

    // 刷新串口列表的函数
    auto refreshPorts = [&]() {
        QStringList availablePorts = getAvailableSerialPorts();
        portCombo->clear();
        portCombo->addItems(availablePorts);

        if (availablePorts.isEmpty()) {
            addCommand("[系统] 串口刷新完成 - 未发现可用串口");
        } else {
            addCommand(QString("[系统] 串口刷新完成 - 发现 %1 个可用串口").arg(availablePorts.size()));
        }
    };

    // 连接刷新按钮
    connect(refreshButton, &QPushButton::clicked, refreshPorts);

    // 连接按钮
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 初始化串口列表
    refreshPorts();

    // 如果没有可用串口，显示提示
    if (portCombo->count() == 0) {
        QMessageBox::warning(this, "Serial Port Error",
            "No serial port devices detected.\n"
            "Please check device connection and click 'Refresh Serial Port List' button to retry.\n\n"
            "Common causes:\n"
            "- USB to serial driver not installed\n"
            "- Device not connected\n"
            "- Device occupied by other programs");
    }

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedPort = portCombo->currentText();
        int selectedBaudRate = baudCombo->currentText().toInt();

        if (!selectedPort.isEmpty()) {
            // 连接选中的串口
            connectToSerialPort(selectedPort, selectedBaudRate);
        }
    }
}

void MainWindow::refreshSerialPorts()
{
    QStringList availablePorts = getAvailableSerialPorts();

    if (availablePorts.isEmpty()) {
        addCommand("[快捷键F5] 串口刷新完成 - 未发现可用串口");
        statusBar()->showMessage("未发现可用串口设备", 3000);
    } else {
        addCommand(QString("[快捷键F5] 串口刷新完成 - 发现 %1 个可用串口: %2")
                   .arg(availablePorts.size())
                   .arg(availablePorts.join(", ")));
        statusBar()->showMessage(QString("发现 %1 个可用串口").arg(availablePorts.size()), 3000);
    }
}

void MainWindow::showTcpConnectionDialog()
{
    // 如果已连接，先确认是否断开
    if (m_tcpClient && m_tcpClient->isConnected()) {
        auto reply = QMessageBox::question(this, "TCP连接",
            QString("当前已连接到 %1:%2\n是否断开并重新连接？")
                .arg(m_tcpClient->getROSHost()).arg(m_tcpClient->getROSPort()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        m_tcpClient->disconnectFromROS();
    }

    QDialog dialog(this);
    dialog.setWindowTitle("TCP连接到ROS节点");
    dialog.resize(400, 180);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* infoLabel = new QLabel("请输入ROS节点(Intel NUC)的IP地址和端口:");
    layout->addWidget(infoLabel);

    // IP地址输入
    QHBoxLayout* ipLayout = new QHBoxLayout();
    ipLayout->addWidget(new QLabel("IP地址:"));
    QLineEdit* ipEdit = new QLineEdit("192.168.1.100");
    ipEdit->setMinimumWidth(200);
    ipEdit->setPlaceholderText("例如: 192.168.1.100");
    ipLayout->addWidget(ipEdit);
    layout->addLayout(ipLayout);

    // 端口输入
    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("端口:"));
    QSpinBox* portSpin = new QSpinBox();
    portSpin->setRange(1, 65535);
    portSpin->setValue(9090);
    portSpin->setMinimumWidth(200);
    portLayout->addWidget(portSpin);
    layout->addLayout(portLayout);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString host = ipEdit->text().trimmed();
        quint16 port = static_cast<quint16>(portSpin->value());

        if (host.isEmpty()) {
            addError("[TCP] IP地址不能为空");
            return;
        }

        addCommand(QString("[TCP] 正在连接到 %1:%2 ...").arg(host).arg(port));
        bool success = m_tcpClient->connectToROS(host, port);
        if (!success) {
            addError(QString("[TCP] 连接失败: %1:%2").arg(host).arg(port));
        }
    }
}

void MainWindow::onSerialDataReceived(const QByteArray &data)
{
    // 显示原始USB-CDC数据（用于调试帧头问题）
    qDebug() << "[USB-CDC] 收到数据，长度:" << data.size()
             << "内容:" << data.toHex(' ');

    // 在命令窗口显示接收到的数据
    addCommand(QString("[串口] 收到���据 长度:%1 内容:%2")
               .arg(data.size())
               .arg(data.toHex(' ')));

    // 检查是否包含帧头 0xAA
    if (data.contains(0xAA)) {
        addCommand("[串口] ✓ 检测到帧头 0xAA");
    } else {
        addCommand("[串口] ✗ 未检测到帧头 0xAA");

        // 显示每个字节的十进制值
        QString byteStr = "[串口] 字节值: ";
        for (int i = 0; i < data.size(); ++i) {
            byteStr += QString("%1 ").arg(static_cast<uint8_t>(data[i]));
        }
        addCommand(byteStr);
    }
}

void MainWindow::onCompleteFrameReceived(const QByteArray &frame)
{
    // 注意：实际的协议解析已经移到 SerialPortManager 中
    // 解析后的数据会通过 onEsp32StateReceived 槽函数接收
    Q_UNUSED(frame)
}

void MainWindow::onEsp32StateReceived(const Communication::ESP32State &state)
{
    // 直接使用ESP32State更新UI显示
    updateJointsData(state);
}

void MainWindow::onCO2DataReceived(float ppm)
{
    if (m_co2Widget) {
        m_co2Widget->setCO2Value(ppm);
    }
}

void MainWindow::onIMUDataReceived(float roll, float pitch, float yaw,
                                    float accelX, float accelY, float accelZ)
{
    Q_UNUSED(accelX)
    Q_UNUSED(accelY)
    Q_UNUSED(accelZ)
    updateCarAttitude(roll, pitch, yaw);
}

void MainWindow::onGamepadStateReceived(const ControllerState &state)
{
    // === 1. 更新 GamepadDisplayWidget 显示 ===
    if (m_gamepadWidget) {
        // 摇杆归一化到 -1.0 ~ 1.0
        float lx = state.sThumbLX / 32767.0f;
        float ly = state.sThumbLY / 32767.0f;
        float rx = state.sThumbRX / 32767.0f;
        float ry = state.sThumbRY / 32767.0f;
        float lt = state.bLeftTrigger / 255.0f;
        float rt = state.bRightTrigger / 255.0f;

        m_gamepadWidget->updateAll(lx, ly, rx, ry, lt, rt);

        // 按钮状态显示
        if (state.buttonA) m_gamepadWidget->updateButton("A", true);
        else if (state.buttonB) m_gamepadWidget->updateButton("B", true);
        else if (state.buttonX) m_gamepadWidget->updateButton("X", true);
        else if (state.buttonY) m_gamepadWidget->updateButton("Y", true);
        else if (state.leftShoulder) m_gamepadWidget->updateButton("LB", true);
        else if (state.rightShoulder) m_gamepadWidget->updateButton("RB", true);
        else if (state.buttonBack) m_gamepadWidget->updateButton("Back", true);
        else if (state.buttonStart) m_gamepadWidget->updateButton("Start", true);
        else if (state.dpadUp) m_gamepadWidget->updateButton("DPad↑", true);
        else if (state.dpadDown) m_gamepadWidget->updateButton("DPad↓", true);
        else if (state.dpadLeft) m_gamepadWidget->updateButton("DPad←", true);
        else if (state.dpadRight) m_gamepadWidget->updateButton("DPad→", true);
        else m_gamepadWidget->updateButton("--", false);
    }

    // === 2. D-Pad检测模式切换 ===
    if (state.dpadUp) {
        switchControlMode(ControlMode::Vehicle);
        return;
    }
    if (state.dpadDown) {
        switchControlMode(ControlMode::Arm);
        return;
    }

    // === 3. 根据当前模式分发控制逻辑 ===
    if (m_controlMode == ControlMode::Arm) {
        handleGamepadArmMode(state);
    } else {
        handleGamepadVehicleMode(state);
    }
}

void MainWindow::switchControlMode(ControlMode mode)
{
    if (m_controlMode == mode) return;
    m_controlMode = mode;

    QString modeName = (mode == ControlMode::Vehicle) ? "车体运动" : "机械臂操控";
    ui->label_mode_value->setText(modeName);
    addCommand(QString("[模式切换] %1").arg(modeName));

    // 同步通知键盘控制器
    if (m_keyboardController) {
        m_keyboardController->setControlMode(static_cast<int>(mode));
    }

    // 切换到机械臂模式时，发送零速度确保车体停止
    if (mode == ControlMode::Arm) {
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendVelocityCommand(0.0f, 0.0f, 0.0f);
        }
    }
}

void MainWindow::handleGamepadVehicleMode(const ControllerState &state)
{
    // 左摇杆Y轴 → 前后线速度 (linearX)
    // 右摇杆X轴 → 左右角速度 (angularZ)
    const int16_t DEADZONE = 3000;
    const float MAX_LINEAR_SPEED = 0.5f;
    const float MAX_ANGULAR_SPEED = 1.0f;

    float linearX = 0.0f;
    float angularZ = 0.0f;

    if (qAbs(state.sThumbLY) > DEADZONE) {
        linearX = (state.sThumbLY / 32767.0f) * MAX_LINEAR_SPEED;
    }
    if (qAbs(state.sThumbRX) > DEADZONE) {
        angularZ = -(state.sThumbRX / 32767.0f) * MAX_ANGULAR_SPEED;
    }

    // A按钮 → 急停
    if (state.buttonA) {
        linearX = 0.0f;
        angularZ = 0.0f;
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendEmergencyStop();
        }
        addCommand("[手柄] 急停!");
        return;
    }

    // 发送速度命令
    if (m_tcpClient && m_tcpClient->isConnected()) {
        m_tcpClient->sendVelocityCommand(linearX, 0.0f, angularZ);
    }

    // 方向变化日志（防止刷屏）
    static float lastLx = 0.0f, lastAz = 0.0f;
    bool changed = (qAbs(linearX - lastLx) > 0.01f || qAbs(angularZ - lastAz) > 0.01f);
    if (changed) {
        lastLx = linearX;
        lastAz = angularZ;
        bool isStopped = (linearX == 0.0f && angularZ == 0.0f);
        if (!isStopped) {
            QString dir;
            if (linearX > 0) dir += "前进 ";
            if (linearX < 0) dir += "后退 ";
            if (angularZ > 0) dir += "左转 ";
            if (angularZ < 0) dir += "右转 ";
            addCommand(QString("[手柄-车体] %1 (lx=%.2f az=%.2f)")
                       .arg(dir.trimmed()).arg(linearX).arg(angularZ));
        }
    }
}

void MainWindow::handleGamepadArmMode(const ControllerState &state)
{
    const int16_t DEADZONE = 3000;
    const float POSITION_SPEED = 0.01f;  // 位置增量 m
    const float ROTATION_SPEED = 0.05f;  // 姿态增量 rad

    // A按钮 → 急停
    if (state.buttonA) {
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendEmergencyStop();
        }
        addCommand("[手柄-机械臂] 急停!");
        return;
    }

    if (!m_tcpClient || !m_tcpClient->isConnected()) return;

    // 末端笛卡尔空间控制
    float deltaX = 0.0f;  // 前后
    float deltaY = 0.0f;  // 左右
    float deltaZ = 0.0f;  // 上下
    float deltaRoll = 0.0f;
    float deltaPitch = 0.0f;
    float deltaYaw = 0.0f;

    // 左摇杆X → 末端左右移动 (Y轴)
    if (qAbs(state.sThumbLX) > DEADZONE) {
        deltaY = (state.sThumbLX / 32767.0f) * POSITION_SPEED;
    }

    // 左摇杆Y → 末端前后移动 (X轴)
    if (qAbs(state.sThumbLY) > DEADZONE) {
        deltaX = (state.sThumbLY / 32767.0f) * POSITION_SPEED;
    }

    // 右摇杆Y → 末端上下移动 (Z轴)
    if (qAbs(state.sThumbRY) > DEADZONE) {
        deltaZ = (state.sThumbRY / 32767.0f) * POSITION_SPEED;
    }

    // 右摇杆X → 末端偏航 (Yaw)
    if (qAbs(state.sThumbRX) > DEADZONE) {
        deltaYaw = (state.sThumbRX / 32767.0f) * ROTATION_SPEED;
    }

    // LT → 末端俯仰- (Pitch-)
    if (state.bLeftTrigger > 30) {
        deltaPitch = -(state.bLeftTrigger / 255.0f) * ROTATION_SPEED;
    }

    // RT → 末端俯仰+ (Pitch+)
    if (state.bRightTrigger > 30) {
        deltaPitch = (state.bRightTrigger / 255.0f) * ROTATION_SPEED;
    }

    // LB → 末端滚转- (Roll-)
    if (state.leftShoulder) {
        deltaRoll = -ROTATION_SPEED;
    }

    // RB → 末端滚转+ (Roll+)
    if (state.rightShoulder) {
        deltaRoll = ROTATION_SPEED;
    }

    // 发送末端控制命令
    bool hasMovement = (qAbs(deltaX) > 0.001f || qAbs(deltaY) > 0.001f || qAbs(deltaZ) > 0.001f ||
                        qAbs(deltaRoll) > 0.001f || qAbs(deltaPitch) > 0.001f || qAbs(deltaYaw) > 0.001f);

    if (hasMovement) {
        m_tcpClient->sendEndEffectorControl(deltaX, deltaY, deltaZ, deltaRoll, deltaPitch, deltaYaw);

        // 日志（防止刷屏）
        static float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
        static float lastRoll = 0.0f, lastPitch = 0.0f, lastYaw = 0.0f;
        bool changed = (qAbs(deltaX - lastX) > 0.001f || qAbs(deltaY - lastY) > 0.001f ||
                        qAbs(deltaZ - lastZ) > 0.001f || qAbs(deltaRoll - lastRoll) > 0.001f ||
                        qAbs(deltaPitch - lastPitch) > 0.001f || qAbs(deltaYaw - lastYaw) > 0.001f);
        if (changed) {
            lastX = deltaX; lastY = deltaY; lastZ = deltaZ;
            lastRoll = deltaRoll; lastPitch = deltaPitch; lastYaw = deltaYaw;
            addCommand(QString("[手柄-末端] XYZ:(%.3f,%.3f,%.3f) RPY:(%.3f,%.3f,%.3f)")
                       .arg(deltaX).arg(deltaY).arg(deltaZ)
                       .arg(deltaRoll).arg(deltaPitch).arg(deltaYaw));
        }
    }
}
