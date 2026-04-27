#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "src/communication/SharedStructs.h"
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
#include <QSettings>
#include <QNetworkInterface>
#include <QFrame>
#include <QThread>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_controller(nullptr)
    , m_keyboardController(nullptr)
    , m_displayLayout(nullptr)
    , m_co2Widget(nullptr)
    , m_gamepadWidget(nullptr)
    , m_handleKey(nullptr)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("上位机v2");
    resize(1920, 1080);
    setMinimumSize(1600, 900);

    // 初始化组件
    setupController();
    setupDisplayLayout();
    setupKeyboardController();
    setupHandleKey();
    setupTimers();

    // 全局Space急停快捷键（不受焦点影响，只需创建一次）
    QShortcut *spaceStop = new QShortcut(QKeySequence(Qt::Key_Space), this);
    spaceStop->setContext(Qt::ApplicationShortcut);
    connect(spaceStop, &QShortcut::activated, this, [this]() {
        triggerEmergencyStop();
    });

    // 设置连接和状态栏
    setupConnections();
    setupStatusBar();

    
    // 初始化UI状态
    updateConnectionDisplay();
    updateHeartbeatDisplay();
    updateGamepadDisplay();

    // 添加系统启动提示
    addCommand("[系统] 主窗口初始化完成");
    addCommand("[系统] 请连接TCP设备开始接收数据");
}

MainWindow::~MainWindow()
{
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
    if (m_controller) {
        m_controller->stop();
    }
    delete ui;
}

void MainWindow::setupController()
{
    m_controller = new Controller(this);
    m_controller->initialize();
    m_controller->start();

    // 连接Controller信号到UI更新

    // TCP状态
    connect(m_controller, &Controller::tcpConnected, this, &MainWindow::onTcpConnected);
    connect(m_controller, &Controller::tcpDisconnected, this, &MainWindow::onTcpDisconnected);
    connect(m_controller, &Controller::tcpError, this, &MainWindow::onTcpError);
    connect(m_controller, &Controller::tcpHeartbeatChanged, this, &MainWindow::onTcpHeartbeatChanged);

    // 数据接收
    connect(m_controller, &Controller::motorStateReceived, this, &MainWindow::onMotorStateReceived);
    connect(m_controller, &Controller::co2DataReceived, this, &MainWindow::onCO2DataReceived);
    connect(m_controller, &Controller::imuDataReceived, this, &MainWindow::onIMUDataReceived);
    connect(m_controller, &Controller::cameraInfoReceived, this, &MainWindow::onCameraInfoReceived);

    // 系统错误
    connect(m_controller, &Controller::systemError, this, [this](const QString &error) {
        addError(error);
    });
}

void MainWindow::setupDisplayLayout()
{
    // 创建布局管理器（2行3列）
    m_displayLayout = new DisplayLayoutManager(2, 3, this);

    // 创建5个RTSP视频播放控件，放入索引0-4
    for (int i = 0; i < 5; ++i) {
        m_rtspWidgets[i] = new RtspPlayerWidget(i);
        m_displayLayout->setWidget(i, m_rtspWidgets[i]);
    }

    // 索引5：垂直堆叠容器，CO2置顶，剩余空间留给后续控件
    QWidget *statusPanel = new QWidget();
    QVBoxLayout *statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(3);

    m_co2Widget = new CO2DisplayWidget();
    m_co2Widget->setMaximumHeight(120);
    statusLayout->addWidget(m_co2Widget);

    statusLayout->addStretch();

    m_displayLayout->setWidget(5, statusPanel);

    // 创建手柄显示控件，放到姿态模型右侧
    m_gamepadWidget = new GamepadDisplayWidget();
    m_gamepadWidget->setMaximumWidth(200);

    // 手柄 + 下方预留空容器，垂直堆叠
    QWidget *gamepadColumn = new QWidget();
    QVBoxLayout *gamepadColumnLayout = new QVBoxLayout(gamepadColumn);
    gamepadColumnLayout->setContentsMargins(0, 0, 0, 0);
    gamepadColumnLayout->setSpacing(3);
    gamepadColumnLayout->addWidget(m_gamepadWidget);

    QWidget *reservedPanel = new QWidget();
    gamepadColumnLayout->addWidget(reservedPanel);

    gamepadColumn->setMaximumWidth(200);
    ui->horizontalLayout_model_gamepad->addWidget(gamepadColumn);

    // 把 DisplayLayoutManager 塞进 group_cameras 的布局中
    ui->group_cameras->layout()->addWidget(m_displayLayout);

    QVBoxLayout *carLayout = new QVBoxLayout(ui->CarWidget);
    carLayout->setContentsMargins(0, 0, 0, 0);
    m_robotAttitudeWidget = new RobotAttitudeWidget(ui->CarWidget);
    carLayout->addWidget(m_robotAttitudeWidget);
    ui->CarWidget->setLayout(carLayout);
}

void MainWindow::setupKeyboardController()
{
    m_keyboardController = new KeyboardController(this);
    connect(m_keyboardController, &KeyboardController::velocityChanged,
            this, [this](float lx, float ly, float az) {
        // 时间节流：500ms内最多打一次日志
        static float lastLx = 0.0f, lastAz = 0.0f;
        static qint64 lastLogTime = 0;
        bool changed = (lx != lastLx || az != lastAz);
        if (changed) {
            lastLx = lx;
            lastAz = az;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - lastLogTime >= 500) {
                lastLogTime = now;
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
        }

        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendVelocityCommand(lx, ly, az);
        }
    });
    connect(m_keyboardController, &KeyboardController::emergencyStopRequested,
            this, &MainWindow::on_btn_emergency_stop_clicked);

    // 键盘机械臂模式信号
    connect(m_keyboardController, &KeyboardController::jointControlRequested,
            this, [this](int jointId, float position, float velocity) {
        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendJointControl(jointId, position, velocity);
            addCommand(QString("[键盘-机械臂] 关节%1 位置:%2 速度:%3")
                       .arg(jointId).arg(position, 0, 'f', 3).arg(velocity, 0, 'f', 3));
        }
    });
    connect(m_keyboardController, &KeyboardController::executorControlRequested,
            this, [this](float value) {
        if (m_controller && m_controller->isTcpConnected()) {
            QJsonObject params;
            params["value"] = value;
            m_controller->sendSystemCommand("executor_control", params);
            addCommand(QString("[键盘-机械臂] 执行器: %1").arg(value > 0 ? "张开" : "闭合"));
        }
    });

    // 默认启用键盘控制
    m_keyboardController->setEnabled(true);

    // 同步当前控制模式
    m_keyboardController->setControlMode(static_cast<int>(m_controlMode));
}

void MainWindow::setupHandleKey()
{
    m_handleKey = new HandleKey(this);
    connect(m_handleKey, &HandleKey::getHandleKey,
            this, &MainWindow::onGamepadStateReceived);
    connect(m_handleKey, &HandleKey::connectionChanged,
            this, [this](bool connected) {
        updateGamepadDisplay();
        if (connected) {
            addCommand("[手柄] 手柄已连接");
            // 手柄连接后禁用键盘控制（急停除外）
            if (m_keyboardController) {
                m_keyboardController->setEnabled(false);
                addCommand("[键盘] 键盘控制已禁用（急停仍可用）");
            }
        } else {
            addCommand("[手柄] 手柄已断开");
            // 手柄断开后恢复键盘控制
            if (m_keyboardController) {
                m_keyboardController->setEnabled(true);
                addCommand("[键盘] 键盘控制已恢复");
            }
        }
    });
}

void MainWindow::setupTimers()
{
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateSystemStatus);
    m_statusTimer->start(1000); // 每秒更新一次状态
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
    if (!m_controller || !m_controller->isTcpConnected()) {
        ui->label_cpu->setText("带宽压力:");
        ui->label_cpu_value->setText("N/A");
        return;
    }

    auto stats = m_controller->getTcpStatistics();

    // 计算带宽压力（本周期收发字节数，定时器间隔见调用频率）
    quint64 deltaBytes = (stats.bytesSent - m_lastBytesSent) + (stats.bytesReceived - m_lastBytesReceived);
    m_lastBytesSent = stats.bytesSent;
    m_lastBytesReceived = stats.bytesReceived;

    // 自动选择单位 KB/s 或 MB/s
    QString pressure;
    double bandwidthKBs = deltaBytes / 1024.0;
    if (bandwidthKBs < 1024.0) {
        if (bandwidthKBs < 10.0)
            pressure = QString("%1 KB/s (低)").arg(bandwidthKBs, 0, 'f', 1);
        else if (bandwidthKBs < 100.0)
            pressure = QString("%1 KB/s (中)").arg(bandwidthKBs, 0, 'f', 1);
        else
            pressure = QString("%1 KB/s (高)").arg(bandwidthKBs, 0, 'f', 1);
    } else {
        double bandwidthMBs = bandwidthKBs / 1024.0;
        pressure = QString("%1 MB/s (高)").arg(bandwidthMBs, 0, 'f', 2);
    }

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

    if (m_robotAttitudeWidget) {
        m_robotAttitudeWidget->updateAttitude(roll, pitch, yaw);
    }
}

void MainWindow::updateJointsData(const Communication::MotorState& motorState)
{
    // 更新3D视图腿部角度（关节0-3对应4条腿，位置值直接作为角度度数）
    if (m_robotAttitudeWidget) {
        m_robotAttitudeWidget->updateLegs(
            motorState.joints[0].position / 10.0,
            motorState.joints[1].position / 10.0,
            motorState.joints[2].position / 10.0,
            motorState.joints[3].position / 10.0
        );
    }

    // 清空text_errors控件
    ui->text_errors->clear();

    // 添加时间戳
    QString timestamp = getCurrentTimestamp();
    QString header = QString("[%1] 6关节数据接收\n").arg(timestamp);

    // 构建关节数据字符串
    QString jointsData = "=== 6关节数据 ===\n";
    for (int i = 0; i < 6; ++i) {
        jointsData += QString("关节 %1:\n").arg(i + 1);
        jointsData += QString("  位置: %1\n").arg(motorState.joints[i].position / 1000.0f, 0, 'f', 3);
        jointsData += QString("  电流: %1 A\n").arg(motorState.joints[i].current / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器位置: %1\n").arg(motorState.executor_position / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器扭矩: %1\n").arg(motorState.executor_torque / 1000.0f, 0, 'f', 3);
        jointsData += QString("  执行器标志: %1\n").arg(motorState.executor_flags);
        jointsData += QString("  保留字段: %1\n").arg(motorState.reserved);
        if (i < 5) jointsData += "\n";
    }

    // 添加执行器数据摘要
    jointsData += "\n=== 执行器数据摘要 ===\n";
    jointsData += QString("执行器位置: %1\n").arg(motorState.executor_position / 1000.0f, 0, 'f', 3);
    jointsData += QString("执行器扭矩: %1\n").arg(motorState.executor_torque / 1000.0f, 0, 'f', 3);
    jointsData += QString("执行器标志: 0x%1\n").arg(motorState.executor_flags, 2, 16, QChar('0'));
    jointsData += QString("保留字段: %1\n").arg(motorState.reserved);

    // 设置文本内容
    ui->text_errors->setText(header + jointsData);

    // 在命令区域添加格式化的关节数据
    addCommand("[关节数据] 数据已更新到显示区域");

    // 格式化显示6个关节的位置数据（用于命令区域）
    QString positionData = QString("[关节数据] 位置: ");
    for (int i = 0; i < 6; ++i) {
        positionData += QString("J%1:%2 ").arg(i+1).arg(motorState.joints[i].position / 1000.0f, 0, 'f', 3);
    }
    addCommand(positionData);

    // 格式化显示6个关节的电流数据（用于命令区域）
    QString currentData = QString("[电流数据] 电流: ");
    for (int i = 0; i < 6; ++i) {
        currentData += QString("J%1:%2A ").arg(i+1).arg(motorState.joints[i].current / 1000.0f, 0, 'f', 3);
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
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        triggerEmergencyStop();
        event->accept();
        return;
    }

    if (m_keyboardController && m_keyboardController->isEnabled()) {
        m_keyboardController->handleKeyPress(event);
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        event->accept();
        return;
    }

    if (m_keyboardController && m_keyboardController->isEnabled()) {
        m_keyboardController->handleKeyRelease(event);
    }
    QMainWindow::keyReleaseEvent(event);
}

// 菜单槽函数
void MainWindow::on_action_connect_triggered()
{
    // 显示TCP连接对话框
    showTcpConnectionDialog();
}

void MainWindow::on_action_tcp_connect_triggered()
{
    showTcpConnectionDialog();
}

void MainWindow::on_action_disconnect_triggered()
{
    if (m_controller && m_controller->isTcpConnected()) {
        m_controller->disconnectFromROS();
        updateConnectionStatus(false);
        addCommand("[操作] TCP连接已断开");
    }
}

void MainWindow::on_action_exit_triggered()
{
    close();
}

void MainWindow::on_action_reset_layout_triggered()
{
    // 姿态归零
    m_roll = 0.0;
    m_pitch = 0.0;
    m_yaw = 0.0;
    if (m_robotAttitudeWidget) {
        m_robotAttitudeWidget->resetView();
    }

    // 刷新所有UI显示（基于当前实际状态）
    updateConnectionDisplay();
    updateHeartbeatDisplay();
    updateGamepadDisplay();
    updateBandwidthAndPacketLoss();

    ui->label_fps_value->setText(QString::number(m_currentFPS) + " FPS");
    ui->label_mode_value->setText(m_controlMode == ControlMode::Vehicle ? "车体运动" : "机械臂操控");
    ui->label_error_count->setText(QString("错误: %1").arg(m_errorCount));

    update();

    addCommand("[系统] 视图已刷新，姿态已归零");
}

void MainWindow::cleanupResources()
{
    // 1. 停止定时器
    if (m_statusTimer) {
        m_statusTimer->stop();
        m_statusTimer->disconnect();
    }

    // 2. 停止手柄轮询
    if (m_handleKey) {
        m_handleKey->stopPolling();
        m_handleKey->disconnect();
    }

    // 3. 禁用并断开键盘控制器
    if (m_keyboardController) {
        m_keyboardController->setEnabled(false);
        m_keyboardController->disconnect();
    }

    // 4. 停止Controller（会自动断开TCP）
    if (m_controller) {
        m_controller->stop();
        m_controller->disconnect();
    }

    // 5. 清空命令和错误显示
    if (ui->text_commands) {
        ui->text_commands->clear();
    }
    if (ui->text_errors) {
        ui->text_errors->clear();
    }

    // 6. 处理待处理的事件，确保UI刷新
    QApplication::processEvents();
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, "关于",
        "上位机 v2\n\n"
        );
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
    const bool restoreKeyboardControl = m_keyboardController && m_keyboardController->isEnabled();
    // === 最高优先级急停处理 ===
    addCommand("[急停] ⚠️ 用户触发急停按钮!");

    // 1. 立即停止所有运动（TCP通道）
    if (m_controller && m_controller->isTcpConnected()) {
        m_controller->sendEmergencyStop();
        m_controller->sendVelocityCommand(0.0f, 0.0f, 0.0f);
        addCommand("[急停] TCP急停指令已发送");
    }

    // 2. 清空键盘控制器状态
    if (m_keyboardController) {
        m_keyboardController->setEnabled(false);
        m_keyboardController->setEnabled(true);  // 重置状态
        addCommand("[急停] 键盘控制器已重置");
    }

    // 3. 切换回车体模式（安全模式）
    if (m_keyboardController && !restoreKeyboardControl) {
        m_keyboardController->setEnabled(false);
        addCommand("[Emergency Stop] Keyboard control kept disabled");
    }

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

void MainWindow::on_btn_gamepad_connect_clicked()
{
    if (m_handleKey && m_handleKey->isConnected()) {
        // 已连接 → 停止轮询（断开）
        m_handleKey->stopPolling();
        addCommand("[手柄] 手动断开手柄轮询");
        updateGamepadDisplay();
    } else {
        // 未连接 → 启动轮询尝试连接
        if (m_handleKey) {
            m_handleKey->startPolling();
            addCommand("[手柄] 正在尝试连接手柄...");
        }
    }
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

    // 每秒更新带宽压力与丢包概率
    updateBandwidthAndPacketLoss();
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

void MainWindow::updateGamepadDisplay()
{
    bool connected = m_handleKey && m_handleKey->isConnected();
    QString statusText = connected ? "已连接" : "未连接";
    QString styleSheet = connected ?
        "color: green; font-weight: bold;" :
        "color: red; font-weight: bold;";

    ui->label_gamepad_value->setText(statusText);
    ui->label_gamepad_value->setStyleSheet(styleSheet);
    ui->btn_gamepad_connect->setText(connected ? "断开手柄" : "连接手柄");
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

void MainWindow::triggerEmergencyStop()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastEmergencyStopMs < 150) {
        return;
    }

    m_lastEmergencyStopMs = now;
    on_btn_emergency_stop_clicked();
}


void MainWindow::showTcpConnectionDialog()
{
    // 如果已连接，先确认是否断开
    if (m_controller && m_controller->isTcpConnected()) {
        auto reply = QMessageBox::question(this, "TCP连接",
            QString("当前已连接到 %1:%2\n是否断开并重新连接？")
                .arg(m_controller->getROSHost()).arg(m_controller->getROSPort()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        m_controller->disconnectFromROS();
    }

    QDialog dialog(this);
    dialog.setWindowTitle("TCP连接到ROS节点");
    dialog.resize(450, 300);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* infoLabel = new QLabel("请输入ROS节点(Intel NUC)的IP地址和端口:");
    layout->addWidget(infoLabel);

    // 从QSettings读取上次使用的IP和端口
    QSettings settings("HostComputer", "TCPConnection");
    QString lastHost = settings.value("tcp/host", "192.168.1.123").toString();
    int lastPort = settings.value("tcp/port", 9090).toInt();

    // IP地址输入
    QHBoxLayout* ipLayout = new QHBoxLayout();
    ipLayout->addWidget(new QLabel("IP地址:"));
    QLineEdit* ipEdit = new QLineEdit(lastHost);
    ipEdit->setMinimumWidth(200);
    ipEdit->setPlaceholderText("下位机IP地址");
    ipLayout->addWidget(ipEdit);
    layout->addLayout(ipLayout);

    // 端口输入
    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("端口:"));
    QSpinBox* portSpin = new QSpinBox();
    portSpin->setRange(1, 65535);
    portSpin->setValue(lastPort);
    portSpin->setMinimumWidth(200);
    portLayout->addWidget(portSpin);
    layout->addLayout(portLayout);

    // ====== 网络配置区域 ======
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    QLabel* netConfigTitle = new QLabel("网络配置");
    netConfigTitle->setStyleSheet("font-weight: bold; color: #555;");
    layout->addWidget(netConfigTitle);

    QLabel* networkStatusLabel = new QLabel("正在检测网卡...");
    networkStatusLabel->setWordWrap(true);
    layout->addWidget(networkStatusLabel);

    QPushButton* autoConfigBtn = new QPushButton("自动配置网络");
    autoConfigBtn->setToolTip("根据目标NUC的IP，自动为本机网卡分配同网段的不冲突IP");
    autoConfigBtn->setEnabled(false);
    layout->addWidget(autoConfigBtn);

    // --- 检测有线网卡的 lambda ---
    struct EthernetInfo {
        QString adapterName;  // Windows网卡名称（用于netsh）
        QString currentIp;
        bool found = false;
    };

    auto detectEthernetAdapter = [](EthernetInfo &info) {
        info.found = false;
        info.adapterName.clear();
        info.currentIp.clear();

        // 用于排除虚拟网卡的关键词
        const QStringList excludeKeywords = {
            "vmware", "virtual", "bluetooth", "loopback",
            "vethernet", "hyper-v", "vpn", "tunnel",
            "wsl", "docker", "vbox"
        };

        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : interfaces) {
            // 必须是以太网类型、已启用且正在运行
            if (iface.type() != QNetworkInterface::Ethernet)
                continue;
            if (!(iface.flags() & QNetworkInterface::IsUp))
                continue;
            if (iface.flags() & QNetworkInterface::IsLoopBack)
                continue;

            // 排除虚拟网卡
            QString nameLower = iface.humanReadableName().toLower();
            bool isVirtual = false;
            for (const QString &kw : excludeKeywords) {
                if (nameLower.contains(kw)) {
                    isVirtual = true;
                    break;
                }
            }
            if (isVirtual) continue;

            // 找到有线网卡
            info.found = true;
            info.adapterName = iface.humanReadableName();

            // 获取IPv4地址
            const auto entries = iface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol
                    && !entry.ip().isLoopback()) {
                    info.currentIp = entry.ip().toString();
                    break;
                }
            }
            break;  // 取第一个匹配的有线网卡
        }
    };

    // --- 根据目标NUC IP计算本机同网段不冲突IP ---
    auto calcLocalIp = [](const QString &nucIp) -> QString {
        QStringList parts = nucIp.split('.');
        if (parts.size() != 4) return QString();
        int lastOctet = parts[3].toInt();
        // 从200开始找一个不冲突的末位
        for (int candidate : {200, 201, 202, 210, 220, 250}) {
            if (candidate != lastOctet) {
                return QString("%1.%2.%3.%4").arg(parts[0], parts[1], parts[2]).arg(candidate);
            }
        }
        return QString();
    };

    // --- 根据目标IP提取网段前缀（如 "192.168.1."）---
    auto getSubnetPrefix = [](const QString &ip) -> QString {
        QStringList parts = ip.split('.');
        if (parts.size() != 4) return QString();
        return QString("%1.%2.%3.").arg(parts[0], parts[1], parts[2]);
    };

    // --- 刷新网卡状态的 lambda ---
    auto refreshNetworkStatus = [&](const EthernetInfo &info) {
        if (!info.found) {
            networkStatusLabel->setText("未检测到有线网卡");
            networkStatusLabel->setStyleSheet("color: red;");
            autoConfigBtn->setEnabled(false);
            return;
        }

        QString nucIp = ipEdit->text().trimmed();
        QString subnetPrefix = getSubnetPrefix(nucIp);

        // 判断本机IP是否已在目标NUC同网段，且不与NUC冲突
        bool isReady = !subnetPrefix.isEmpty()
                       && info.currentIp.startsWith(subnetPrefix)
                       && info.currentIp != nucIp;

        QString statusText = QString("有线网卡: %1").arg(info.adapterName);
        if (info.currentIp.isEmpty()) {
            statusText += " (未分配IP)";
        } else {
            statusText += QString(" (%1)").arg(info.currentIp);
        }

        if (isReady) {
            statusText += "  ✓ 已就绪";
            networkStatusLabel->setStyleSheet("color: green;");
            autoConfigBtn->setEnabled(false);
            autoConfigBtn->setText("网络已就绪");
        } else {
            statusText += "  ✗ 需要配置";
            networkStatusLabel->setStyleSheet("color: #CC6600;");
            autoConfigBtn->setEnabled(true);
            autoConfigBtn->setText("自动配置网络");
        }
        networkStatusLabel->setText(statusText);
    };

    // 初始检测
    EthernetInfo ethInfo;
    detectEthernetAdapter(ethInfo);
    refreshNetworkStatus(ethInfo);

    // IP输入框变化时重新刷新网卡就绪状态
    connect(ipEdit, &QLineEdit::textChanged, &dialog, [&]() {
        refreshNetworkStatus(ethInfo);
    });

    // --- 自动配置按钮点击逻辑 ---
    connect(autoConfigBtn, &QPushButton::clicked, &dialog, [&]() {
        QString nucIp = ipEdit->text().trimmed();
        QString localIp = calcLocalIp(nucIp);

        if (localIp.isEmpty()) {
            QMessageBox::warning(&dialog, "配置失败",
                "请先在上方填入有效的NUC IP地址（如 192.168.1.50）");
            return;
        }

        QString subnetMask = "255.255.255.0";

        auto confirmReply = QMessageBox::question(&dialog, "确认网络配置",
            QString("将对网卡 \"%1\" 执行以下配置:\n\n"
                    "  本机IP: %2\n"
                    "  子网掩码: %3\n"
                    "  目标NUC: %4\n\n"
                    "此操作需要管理员权限（UAC弹窗），是否继续？")
                .arg(ethInfo.adapterName, localIp, subnetMask, nucIp),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (confirmReply != QMessageBox::Yes) {
            return;
        }

#ifdef Q_OS_WIN
        // 构建 netsh 命令参数
        QString netshArgs = QString("interface ip set address name=\"%1\" static %2 %3")
                                .arg(ethInfo.adapterName, localIp, subnetMask);

        // 使用 ShellExecuteExW 提权执行
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = reinterpret_cast<HWND>(dialog.winId());
        sei.lpVerb = L"runas";
        sei.lpFile = L"netsh";
        sei.lpParameters = reinterpret_cast<LPCWSTR>(netshArgs.utf16());
        sei.nShow = SW_HIDE;

        autoConfigBtn->setEnabled(false);
        autoConfigBtn->setText("正在配置...");
        networkStatusLabel->setText("等待UAC授权...");
        networkStatusLabel->setStyleSheet("color: #0066CC;");
        QApplication::processEvents();

        if (ShellExecuteExW(&sei)) {
            // 等待 netsh 进程执行完毕（最多10秒）
            if (sei.hProcess) {
                WaitForSingleObject(sei.hProcess, 10000);
                CloseHandle(sei.hProcess);
            }

            // 等一下让系统应用网络设置
            QThread::msleep(1500);
            QApplication::processEvents();

            // 重新检测网卡状态
            detectEthernetAdapter(ethInfo);
            refreshNetworkStatus(ethInfo);

            if (ethInfo.currentIp == localIp) {
                QMessageBox::information(&dialog, "配置成功",
                    QString("网卡 \"%1\" 已成功配置为:\n"
                            "IP: %2\n"
                            "子网掩码: %3")
                        .arg(ethInfo.adapterName, localIp, subnetMask));
                addCommand(QString("[网络] 有线网卡已配置为 %1").arg(localIp));
            } else {
                // 可能IP变了但不是精确匹配，再刷新看看
                QMessageBox::warning(&dialog, "配置结果",
                    QString("netsh命令已执行，当前检测到IP: %1\n"
                            "如果IP未生效，请稍等几秒后重新打开此对话框。")
                        .arg(ethInfo.currentIp.isEmpty() ? "未分配" : ethInfo.currentIp));
            }
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                networkStatusLabel->setText(QString("有线网卡: %1 (%2)  ✗ UAC已取消")
                    .arg(ethInfo.adapterName, ethInfo.currentIp.isEmpty() ? "未分配IP" : ethInfo.currentIp));
                networkStatusLabel->setStyleSheet("color: #CC6600;");
            } else {
                networkStatusLabel->setText(QString("配置失败 (错误码: %1)").arg(err));
                networkStatusLabel->setStyleSheet("color: red;");
            }
            autoConfigBtn->setEnabled(true);
            autoConfigBtn->setText("自动配置网络");
        }
#else
        QMessageBox::information(&dialog, "提示", "此功能仅支持Windows系统");
#endif
    });

    // ====== 按钮 ======
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("连接");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
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
        bool started = m_controller ? m_controller->connectToROS(host, port) : false;
        if (started) {
            // 已成功发起连接尝试；最终连接结果由信号回调
            settings.setValue("tcp/host", host);
            settings.setValue("tcp/port", port);
        } else {
            addError(QString("[TCP] 发起连接失败: %1:%2").arg(host).arg(port));
        }
    }
}


// ==================== Controller信号处理 ====================

void MainWindow::onTcpConnected()
{
    updateConnectionStatus(true);
    addCommand("[TCP] 已连接到ROS节点");
    if (m_controller) {
        statusBar()->showMessage(QString("TCP已连接: %1:%2")
            .arg(m_controller->getROSHost()).arg(m_controller->getROSPort()));
    }
}

void MainWindow::onTcpDisconnected()
{
    updateConnectionStatus(false);
    addCommand("[TCP] 与ROS节点断开连接");
    statusBar()->showMessage("TCP连接断开");
    updateCarAttitude(0.0, 0.0, 0.0);
}

void MainWindow::onTcpError(const QString &error)
{
    addError(QString("[TCP] %1").arg(error));
}

void MainWindow::onTcpHeartbeatChanged(bool online)
{
    updateHeartbeatStatus(online);
}

void MainWindow::onMotorStateReceived(const Communication::MotorState &state)
{
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

void MainWindow::onCameraInfoReceived(int cameraId, bool online, const QString &codec,
                                      int width, int height, int fps, int bitrate,
                                      const QString &rtspUrl)
{
    if (cameraId >= 0 && cameraId < 5 && m_rtspWidgets[cameraId]) {
        m_rtspWidgets[cameraId]->setCameraInfo(rtspUrl, online, codec, width, height, fps, bitrate);
        addCommand(QString("[TCP] 摄像头%1 %2 %3")
                   .arg(cameraId).arg(online ? "上线" : "离线").arg(rtspUrl));
    }
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
        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendVelocityCommand(0.0f, 0.0f, 0.0f);
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
        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendEmergencyStop();
        }
        addCommand("[手柄] 急停!");
        return;
    }

    // 发送速度命令
    if (m_controller && m_controller->isTcpConnected()) {
        m_controller->sendVelocityCommand(linearX, 0.0f, angularZ);
    }

    // 方向变化日志（500ms节流防止刷屏）
    static float lastLx = 0.0f, lastAz = 0.0f;
    static qint64 lastLogTime = 0;
    bool changed = (qAbs(linearX - lastLx) > 0.01f || qAbs(angularZ - lastAz) > 0.01f);
    if (changed) {
        lastLx = linearX;
        lastAz = angularZ;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastLogTime >= 500) {
            lastLogTime = now;
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
}

void MainWindow::handleGamepadArmMode(const ControllerState &state)
{
    const int16_t DEADZONE = 3000;
    const float POSITION_SPEED = 0.01f;  // 位置增量 m
    const float ROTATION_SPEED = 0.05f;  // 姿态增量 rad

    // A按钮 → 急停
    if (state.buttonA) {
        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendEmergencyStop();
        }
        addCommand("[手柄-机械臂] 急停!");
        return;
    }

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
        if (m_controller && m_controller->isTcpConnected()) {
            m_controller->sendEndEffectorControl(deltaX, deltaY, deltaZ, deltaRoll, deltaPitch, deltaYaw);
        }

        // 日志（500ms节流防止刷屏）
        static float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
        static float lastRoll = 0.0f, lastPitch = 0.0f, lastYaw = 0.0f;
        static qint64 lastLogTime = 0;
        bool changed = (qAbs(deltaX - lastX) > 0.001f || qAbs(deltaY - lastY) > 0.001f ||
                        qAbs(deltaZ - lastZ) > 0.001f || qAbs(deltaRoll - lastRoll) > 0.001f ||
                        qAbs(deltaPitch - lastPitch) > 0.001f || qAbs(deltaYaw - lastYaw) > 0.001f);
        if (changed) {
            lastX = deltaX; lastY = deltaY; lastZ = deltaZ;
            lastRoll = deltaRoll; lastPitch = deltaPitch; lastYaw = deltaYaw;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - lastLogTime >= 500) {
                lastLogTime = now;
                addCommand(QString("[手柄-末端] XYZ:(%.3f,%.3f,%.3f) RPY:(%.3f,%.3f,%.3f)")
                           .arg(deltaX).arg(deltaY).arg(deltaZ)
                           .arg(deltaRoll).arg(deltaPitch).arg(deltaYaw));
            }
        }
    }
}
