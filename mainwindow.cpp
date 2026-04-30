#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "src/communication/HostProtocol.h"
#include "src/communication/SharedStructs.h"
#include "src/controller/MotorRuntimeCarouselWidget.h"
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
#include <QTextEdit>
#include <QThread>
#include <QJsonArray>
#include <QGroupBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSizePolicy>
#include <QSplitter>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

double normalizeStickAxis(int16_t value)
{
    return qBound(-1.0, static_cast<double>(value) / 32767.0, 1.0);
}

double normalizeTriggerAxis(uint8_t value)
{
    return qBound(0.0, static_cast<double>(value) / 255.0, 1.0);
}

Communication::GamepadInputState makeGamepadInputState(const ControllerState &state, bool connected)
{
    Communication::GamepadInputState gamepad;
    gamepad.connected = connected;

    gamepad.buttons.a = state.buttonA;
    gamepad.buttons.b = state.buttonB;
    gamepad.buttons.x = state.buttonX;
    gamepad.buttons.y = state.buttonY;
    gamepad.buttons.start = state.buttonStart;
    gamepad.buttons.back = state.buttonBack;
    gamepad.buttons.lb = state.leftShoulder;
    gamepad.buttons.rb = state.rightShoulder;
    gamepad.buttons.l3 = state.leftThumb;
    gamepad.buttons.r3 = state.rightThumb;
    gamepad.buttons.dpadUp = state.dpadUp;
    gamepad.buttons.dpadDown = state.dpadDown;
    gamepad.buttons.dpadLeft = state.dpadLeft;
    gamepad.buttons.dpadRight = state.dpadRight;

    gamepad.axes.leftX = connected ? normalizeStickAxis(state.sThumbLX) : 0.0;
    gamepad.axes.leftY = connected ? normalizeStickAxis(state.sThumbLY) : 0.0;
    gamepad.axes.rightX = connected ? normalizeStickAxis(state.sThumbRX) : 0.0;
    gamepad.axes.rightY = connected ? normalizeStickAxis(state.sThumbRY) : 0.0;
    gamepad.axes.lt = connected ? normalizeTriggerAxis(state.bLeftTrigger) : 0.0;
    gamepad.axes.rt = connected ? normalizeTriggerAxis(state.bRightTrigger) : 0.0;

    return gamepad;
}

QString modeNameForProtocol(ControlMode mode)
{
    return mode == ControlMode::Arm ? QStringLiteral("arm") : QStringLiteral("vehicle");
}

QString displayNameForControlMode(ControlMode mode)
{
    return mode == ControlMode::Arm ? QStringLiteral("机械臂操控") : QStringLiteral("车体运动");
}

bool parseControlMode(const QString &modeText, ControlMode *mode)
{
    const QString normalized = modeText.trimmed().toLower();
    if (normalized == QStringLiteral("arm")) {
        *mode = ControlMode::Arm;
        return true;
    }
    if (normalized == QStringLiteral("vehicle") || normalized == QStringLiteral("base")) {
        *mode = ControlMode::Vehicle;
        return true;
    }
    return false;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_controller(nullptr)
    , m_keyboardController(nullptr)
    , m_cameraGridWidget(nullptr)
    , m_telemetryPanel(nullptr)
    , m_controlPanel(nullptr)
    , m_handleKey(nullptr)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("上位机v2");
    resize(1920, 1080);
    setMinimumSize(1600, 900);
    m_statusErrorLabel = ui->label_status_error_count;

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
        triggerEmergencyStop(QStringLiteral("keyboard_space"));
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
    connect(m_controller, &Controller::jointRuntimeStatesReceived, this, &MainWindow::onJointRuntimeStatesReceived);
    connect(m_controller, &Controller::co2DataReceived, this, &MainWindow::onCO2DataReceived);
    connect(m_controller, &Controller::imuDataReceived, this, &MainWindow::onIMUDataReceived);
    connect(m_controller, &Controller::cameraInfoReceived, this, &MainWindow::onCameraInfoReceived);
    connect(m_controller, &Controller::protocolMessageReceived, this, &MainWindow::onProtocolMessageReceived);

    // 系统错误
    connect(m_controller, &Controller::systemError, this, [this](const QString &error) {
        addError(error);
    });
}

void MainWindow::setupDisplayLayout()
{
    m_cameraGridWidget = ui->cameraGridWidget;
    connect(m_cameraGridWidget, &CameraGridWidget::cameraListRefreshRequested,
            this, [this]() {
        if (m_controller && m_controller->requestCameraList()) {
            addCommand("[视频] 已请求刷新视频源列表");
        }
    });

    m_motorRuntimeWidget = ui->motorRuntimeWidget;
    m_controlPanel = ui->controlPanelWidget;
    m_robotAttitudeWidget = ui->robotAttitudeWidget;
    m_logTabs = ui->logTabs;
    setupBottomActionPanel();
    ui->verticalLayout_left_workspace->setStretch(0, 1);
    ui->verticalLayout_left_workspace->setStretch(1, 0);
    ui->verticalLayout_right->setStretch(0, 1);
    ui->verticalLayout_right->setStretch(1, 1);
    ui->verticalLayout_right->setStretch(2, 2);
    ui->verticalLayout_right->setStretch(3, 0);

    auto connectLifecycleButton = [this](QPushButton *button, const QString &command) {
        connect(button, &QPushButton::clicked, this, [this, command]() {
            addCommand(QString("[生命周期] 请求 %1").arg(command));
            if (m_controller && m_controller->isTcpConnected()) {
                m_controller->sendSystemCommand(command);
            } else {
                addError(QString("[生命周期] TCP未连接，未发送 %1").arg(command));
            }
        });
    };

    connectLifecycleButton(ui->btn_lifecycle_init, QStringLiteral("init"));
    connectLifecycleButton(ui->btn_lifecycle_enable, QStringLiteral("enable"));
    connectLifecycleButton(ui->btn_lifecycle_disable, QStringLiteral("disable"));
    connectLifecycleButton(ui->btn_lifecycle_halt, QStringLiteral("halt"));
    connectLifecycleButton(ui->btn_lifecycle_resume, QStringLiteral("resume"));
    connectLifecycleButton(ui->btn_lifecycle_recover, QStringLiteral("recover"));
    connectLifecycleButton(ui->btn_lifecycle_shutdown, QStringLiteral("shutdown"));

    auto connectControlModeButton = [this](QPushButton *button, ControlMode controlMode,
                                           const QString &protocolMode,
                                           const QString &label) {
        connect(button, &QPushButton::clicked, this, [this, controlMode, protocolMode, label]() {
            addCommand(QString("[控制域] 请求切换到 %1").arg(label));
            if (m_controller && m_controller->isTcpConnected()) {
                QJsonObject params;
                params["mode"] = protocolMode;
                if (m_controller->sendSystemCommand(QStringLiteral("set_control_mode"), params)) {
                    applyControlMode(controlMode);
                    sendOperatorInputSnapshot();
                }
            } else {
                addError(QString("[控制域] TCP未连接，未发送 %1").arg(label));
            }
        });
    };

    connectControlModeButton(ui->btn_mode_vehicle, ControlMode::Vehicle,
                             QStringLiteral("vehicle"), QStringLiteral("底盘"));
    connectControlModeButton(ui->btn_mode_arm, ControlMode::Arm,
                             QStringLiteral("arm"), QStringLiteral("机械臂"));

    connect(m_controlPanel, &ControlPanelWidget::gamepadConnectRequested,
            this, &MainWindow::on_btn_gamepad_connect_clicked);
    connect(m_controlPanel, &ControlPanelWidget::emergencyStopRequested,
            this, [this]() {
        triggerEmergencyStop(QStringLiteral("control_panel"));
    });
}

void MainWindow::setupBottomActionPanel()
{
    if (!m_logTabs) {
        return;
    }

    auto *bottomSplit = new QSplitter(Qt::Horizontal, ui->widget_left_workspace);
    bottomSplit->setObjectName(QStringLiteral("splitter_bottom_logs_actions"));
    bottomSplit->setChildrenCollapsible(false);
    bottomSplit->setHandleWidth(6);
    bottomSplit->setMinimumHeight(145);
    bottomSplit->setMaximumHeight(185);

    ui->verticalLayout_left_workspace->removeWidget(m_logTabs);
    m_logTabs->setMinimumHeight(0);
    m_logTabs->setMaximumHeight(16777215);
    bottomSplit->addWidget(m_logTabs);

    auto *actionPanel = new QWidget(bottomSplit);
    actionPanel->setObjectName(QStringLiteral("widget_bottom_action_panel"));
    actionPanel->setMinimumWidth(300);
    actionPanel->setStyleSheet(
        "QGroupBox { font-weight: 700; border: 1px solid #dbe3ef; border-radius: 6px; margin-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #334155; }"
        "QPushButton { min-height: 26px; padding: 2px 10px; }");

    auto *actionLayout = new QHBoxLayout(actionPanel);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);

    auto *armGroup = new QGroupBox(QStringLiteral("机械臂 Action"), actionPanel);
    auto *armLayout = new QVBoxLayout(armGroup);
    armLayout->setContentsMargins(8, 8, 8, 8);
    armLayout->setSpacing(6);

    m_armActionList = new QListWidget(armGroup);
    m_armActionList->setObjectName(QStringLiteral("list_arm_named_targets"));
    m_armActionList->setMinimumHeight(76);
    m_armActionList->addItem(QStringLiteral("等待下位机下发 MoveIt 固定位姿..."));
    m_armActionList->item(0)->setFlags(Qt::NoItemFlags);

    auto *armButtonLayout = new QHBoxLayout();
    m_refreshArmActionButton = new QPushButton(QStringLiteral("刷新位姿"), armGroup);
    m_executeArmActionButton = new QPushButton(QStringLiteral("运动到该位置"), armGroup);
    m_executeArmActionButton->setEnabled(false);
    armButtonLayout->addWidget(m_refreshArmActionButton);
    armButtonLayout->addWidget(m_executeArmActionButton);

    armLayout->addWidget(m_armActionList, 1);
    armLayout->addLayout(armButtonLayout);

    auto *monitorGroup = new QGroupBox(QStringLiteral("任务标志"), actionPanel);
    monitorGroup->setMaximumWidth(118);
    auto *monitorLayout = new QVBoxLayout(monitorGroup);
    monitorLayout->setContentsMargins(8, 8, 8, 8);
    monitorLayout->setSpacing(8);

    auto *thermalButton = new QPushButton(QStringLiteral("热源监测"), monitorGroup);
    auto *dynamicButton = new QPushButton(QStringLiteral("动态监测"), monitorGroup);
    auto *dangerButton = new QPushButton(QStringLiteral("危险标志"), monitorGroup);
    monitorLayout->addWidget(thermalButton);
    monitorLayout->addWidget(dynamicButton);
    monitorLayout->addWidget(dangerButton);
    monitorLayout->addStretch();

    actionLayout->addWidget(armGroup, 1);
    actionLayout->addWidget(monitorGroup, 0);

    bottomSplit->addWidget(actionPanel);
    bottomSplit->setStretchFactor(0, 3);
    bottomSplit->setStretchFactor(1, 1);
    bottomSplit->setSizes({900, 300});
    ui->verticalLayout_left_workspace->addWidget(bottomSplit);

    connect(m_refreshArmActionButton, &QPushButton::clicked,
            this, &MainWindow::refreshArmNamedTargets);
    connect(m_executeArmActionButton, &QPushButton::clicked,
            this, &MainWindow::executeSelectedArmNamedTarget);
    connect(m_armActionList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current) {
        m_executeArmActionButton->setEnabled(current && current->flags().testFlag(Qt::ItemIsEnabled)
                                             && !current->data(Qt::UserRole).toString().isEmpty());
    });

    connect(thermalButton, &QPushButton::clicked, this, [this]() {
        handleMonitorCommand(QStringLiteral("热源监测"), QStringLiteral("thermal_monitor"));
    });
    connect(dynamicButton, &QPushButton::clicked, this, [this]() {
        handleMonitorCommand(QStringLiteral("动态监测"), QStringLiteral("dynamic_monitor"));
    });
    connect(dangerButton, &QPushButton::clicked, this, [this]() {
        handleMonitorCommand(QStringLiteral("危险标志"), QStringLiteral("danger_sign"));
    });
}

void MainWindow::handleMonitorCommand(const QString &label, const QString &command)
{
    addCommand(QString("[监测] 请求 %1").arg(label));
    if (m_controller && m_controller->isTcpConnected()) {
        m_controller->sendSystemCommand(command);
    } else {
        addError(QString("[监测] TCP未连接，未发送 %1").arg(label));
    }
}

void MainWindow::refreshArmNamedTargets()
{
    addCommand("[机械臂] 请求刷新 MoveIt 固定位姿");
    if (m_controller && m_controller->isTcpConnected()) {
        m_controller->sendSystemCommand(QStringLiteral("request_arm_named_targets"));
    } else {
        addError("[机械臂] TCP未连接，未请求固定位姿");
    }
}

void MainWindow::executeSelectedArmNamedTarget()
{
    if (!m_armActionList) {
        return;
    }

    QListWidgetItem *item = m_armActionList->currentItem();
    const QString targetName = item ? item->data(Qt::UserRole).toString() : QString();
    if (targetName.isEmpty()) {
        addError("[机械臂] 未选择有效固定位姿");
        return;
    }

    addCommand(QString("[机械臂] 请求运动到固定位姿: %1").arg(targetName));
    if (m_controller && m_controller->isTcpConnected()) {
        QJsonObject params;
        params["target"] = targetName;
        m_controller->sendSystemCommand(QStringLiteral("move_arm_named_target"), params);
    } else {
        addError(QString("[机械臂] TCP未连接，未发送固定位姿: %1").arg(targetName));
    }
}

void MainWindow::updateArmNamedTargets(const QJsonObject &message)
{
    if (!m_armActionList) {
        return;
    }

    const QStringList targetFields = {
        QStringLiteral("targets"),
        QStringLiteral("named_targets"),
        QStringLiteral("poses"),
        QStringLiteral("actions"),
        QStringLiteral("positions"),
        QStringLiteral("arm_named_targets"),
        QStringLiteral("moveit_named_targets"),
    };

    QJsonArray targets;
    for (const QString &field : targetFields) {
        const QJsonValue value = message.value(field);
        if (value.isArray()) {
            targets = value.toArray();
            if (!targets.isEmpty()) {
                break;
            }
        }
    }

    m_armActionList->clear();

    for (const QJsonValue &value : targets) {
        QString name;
        QString description;
        if (value.isString()) {
            name = value.toString();
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            name = object.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                name = object.value(QStringLiteral("target")).toString();
            }
            if (name.isEmpty()) {
                name = object.value(QStringLiteral("id")).toString();
            }
            if (name.isEmpty()) {
                name = object.value(QStringLiteral("label")).toString();
            }
            description = object.value(QStringLiteral("description")).toString();
            if (description.isEmpty()) {
                description = object.value(QStringLiteral("display_name")).toString();
            }
        }

        if (name.isEmpty()) {
            continue;
        }

        auto *item = new QListWidgetItem(description.isEmpty()
                                             ? name
                                             : QString("%1  -  %2").arg(name, description));
        item->setData(Qt::UserRole, name);
        m_armActionList->addItem(item);
    }

    if (m_armActionList->count() == 0) {
        const QString messageText = message.value(QStringLiteral("message")).toString();
        auto *emptyItem = new QListWidgetItem(messageText.isEmpty()
                                                 ? QStringLiteral("下位机未返回可用固定位姿")
                                                 : messageText);
        emptyItem->setFlags(Qt::NoItemFlags);
        m_armActionList->addItem(emptyItem);
        m_executeArmActionButton->setEnabled(false);
        addError(messageText.isEmpty()
                     ? QStringLiteral("[机械臂] 固定位姿列表为空")
                     : QString("[机械臂] %1").arg(messageText));
        return;
    }

    m_armActionList->setCurrentRow(0);
    addCommand(QString("[机械臂] 已更新固定位姿列表: %1 个").arg(m_armActionList->count()));
}

void MainWindow::setupKeyboardController()
{
    m_keyboardController = new KeyboardController(this);
    connect(m_keyboardController, &KeyboardController::operatorInputChanged,
            this, [this](const QStringList &pressedKeys) {
        static qint64 lastLogTime = 0;
        const bool changed = (pressedKeys != m_keyboardPressedKeys);
        m_keyboardPressedKeys = pressedKeys;

        if (changed) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - lastLogTime >= 500) {
                lastLogTime = now;
                if (!pressedKeys.isEmpty()) {
                    addCommand(QString("[键盘] 按下: %1").arg(pressedKeys.join(",")));
                } else {
                    addCommand("[键盘] 停止");
                }
            }
        }

        sendOperatorInputSnapshot();
    });
    connect(m_keyboardController, &KeyboardController::emergencyStopRequested,
            this, [this]() {
        triggerEmergencyStop(QStringLiteral("keyboard_space"));
    });

    // 默认启用键盘控制
    m_keyboardController->setEnabled(true);

}

void MainWindow::setupHandleKey()
{
    m_handleKey = new HandleKey(this);
    connect(m_handleKey, &HandleKey::getHandleKey,
            this, &MainWindow::onGamepadStateReceived);
    connect(m_handleKey, &HandleKey::connectionChanged,
            this, [this](bool connected) {
        m_gamepadConnected = connected;
        if (!connected) {
            m_latestGamepadState = {};
            m_gamepadStickEmergencyHeld = false;
        }
        updateGamepadDisplay();
        if (connected) {
            addCommand("[手柄] 手柄已连接");
        } else {
            addCommand("[手柄] 手柄已断开");
        }
        sendOperatorInputSnapshot();
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
    // 按钮连接已在UI文件和组件初始化中处理，这里保留给跨区域连接扩展。
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
    const QString fpsText = QString("%1 FPS").arg(fps);
    ui->label_fps_value->setText(fpsText);
    if (m_telemetryPanel) {
        m_telemetryPanel->setFps(fps);
    }
}

void MainWindow::updateBandwidthAndPacketLoss()
{
    if (!m_controller || !m_controller->isTcpConnected()) {
        ui->label_cpu->setText("带宽压力:");
        ui->label_cpu_value->setText("N/A");
        if (m_telemetryPanel) {
            m_telemetryPanel->setBandwidthText("N/A");
        }
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
    const QString heartbeatRtt = stats.lastHeartbeatRttMs >= 0
                                     ? QString("%1ms").arg(stats.lastHeartbeatRttMs)
                                     : QStringLiteral("N/A");
    const QString bandwidthText = QString("%1 | 丢包: %2% | ACK: %3/%4 | RTT: %5 | 协议错: %6")
                                      .arg(pressure)
                                      .arg(lossRate, 0, 'f', 1)
                                      .arg(stats.ackPendingCount)
                                      .arg(stats.ackTimeoutCount)
                                      .arg(heartbeatRtt)
                                      .arg(stats.protocolErrorCount);
    ui->label_cpu_value->setText(bandwidthText);
    if (m_telemetryPanel) {
        m_telemetryPanel->setBandwidthText(bandwidthText);
    }
}

void MainWindow::updateMotorMode(const QString& mode)
{
    m_motorMode = mode;
    ui->label_mode_value->setText(mode);
    if (m_controlPanel) {
        m_controlPanel->setModeText(mode);
    }
    if (m_telemetryPanel) {
        m_telemetryPanel->setModeText(mode);
    }
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
    if (m_statusErrorLabel) {
        m_statusErrorLabel->setText(QString("错误: %1").arg(m_errorCount));
    }
    if (m_telemetryPanel) {
        m_telemetryPanel->setErrorCount(m_errorCount);
    }
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

    if (m_textData) {
        m_textData->setText(header + jointsData);
        QTextCursor cursor = m_textData->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_textData->setTextCursor(cursor);
        m_textData->ensureCursorVisible();
    }
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
        triggerEmergencyStop(QStringLiteral("keyboard_space"));
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
    ui->label_mode_value->setText(displayNameForControlMode(m_controlMode));
    ui->label_error_count->setText(QString("错误: %1").arg(m_errorCount));
    if (m_telemetryPanel) {
        m_telemetryPanel->setFps(m_currentFPS);
        m_telemetryPanel->setModeText(displayNameForControlMode(m_controlMode));
        m_telemetryPanel->setErrorCount(m_errorCount);
    }
    if (m_controlPanel) {
        m_controlPanel->setModeText(displayNameForControlMode(m_controlMode));
    }

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
    if (m_statusErrorLabel) {
        m_statusErrorLabel->setText("错误: 0");
    }
    if (m_telemetryPanel) {
        m_telemetryPanel->setErrorCount(0);
    }
    addCommand("[系统] 错误记录已清空");
}

void MainWindow::on_btn_emergency_stop_clicked()
{
    triggerEmergencyStop(QStringLiteral("button"));
}

void MainWindow::on_btn_clear_emergency_clicked()
{
    addCommand("[急停] 请求解除急停");

    clearOperatorInputSnapshot();
    if (m_keyboardController) {
        m_keyboardController->clearPressedKeys();
    }

    if (m_controller && m_controller->isTcpConnected()) {
        if (m_controller->sendSystemCommand(QStringLiteral("clear_emergency"))) {
            addCommand("[急停] 解除急停指令已发送，等待ACK");
        } else {
            addError("[急停] 解除急停指令发送失败");
        }
    } else {
        addError("[急停] TCP未连接，无法解除下位机急停状态");
    }

    statusBar()->showMessage("已请求解除急停", 3000);
}

void MainWindow::clearOperatorInputSnapshot()
{
    m_keyboardPressedKeys.clear();
    m_latestGamepadState = {};
}

void MainWindow::triggerEmergencyStop(const QString &source)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastEmergencyStopMs < 150) {
        return;
    }

    m_lastEmergencyStopMs = now;

    // === 最高优先级急停处理 ===
    addCommand(QString("[急停] 用户触发急停 source=%1").arg(source));

    // 1. 立即停止所有运动（TCP通道）
    if (m_controller && m_controller->isTcpConnected()) {
        if (m_controller->sendEmergencyStop(source)) {
            addCommand("[急停] TCP急停指令已发送，等待ACK");
        } else {
            addError("[急停] TCP急停指令发送失败");
        }
    } else {
        addError("[急停] TCP未连接，急停未下发到下位机");
    }

    // 2. 清空输入快照，确保急停后不会继续发送旧输入意图
    clearOperatorInputSnapshot();
    if (m_keyboardController) {
        m_keyboardController->clearPressedKeys();
        addCommand("[急停] 键盘控制器已重置");
    }

    sendOperatorInputSnapshot();

    // 3. 状态栏提示
    statusBar()->showMessage("⚠️ 急停已触发！所有运动已停止", 5000);

    // 4. 视觉反馈：按钮闪烁效果
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
    if (m_telemetryPanel) {
        m_telemetryPanel->setConnectionStatus(m_isConnected);
    }
}

void MainWindow::updateHeartbeatDisplay()
{
    QString statusText = m_heartbeatOnline ? "在线" : "丢失";
    QString styleSheet = m_heartbeatOnline ?
        "color: green; font-weight: bold;" :
        "color: red; font-weight: bold;";

    ui->label_heartbeat_value->setText(statusText);
    ui->label_heartbeat_value->setStyleSheet(styleSheet);
    if (m_telemetryPanel) {
        m_telemetryPanel->setHeartbeatStatus(m_heartbeatOnline);
    }
}

void MainWindow::updateGamepadDisplay()
{
    bool connected = m_handleKey && m_handleKey->isConnected();
    if (m_controlPanel) {
        m_controlPanel->setGamepadConnected(connected);
    }
    if (m_telemetryPanel) {
        m_telemetryPanel->setGamepadConnected(connected);
    }
}

void MainWindow::sendOperatorInputSnapshot()
{
    if (!m_controller || !m_controller->isTcpConnected()) {
        return;
    }

    Communication::OperatorInputState inputState;
    inputState.mode = modeNameForProtocol(m_controlMode);
    inputState.ttlMs = 300;
    inputState.keyboard.pressedKeys = m_keyboardPressedKeys;
    inputState.gamepad = makeGamepadInputState(m_latestGamepadState, m_gamepadConnected);

    m_controller->sendOperatorInput(inputState);
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

void MainWindow::applyControlMode(ControlMode mode)
{
    m_controlMode = mode;
    const QString modeText = displayNameForControlMode(mode);
    ui->label_mode_value->setText(modeText);
    if (m_controlPanel) {
        m_controlPanel->setModeText(modeText);
    }
    if (m_telemetryPanel) {
        m_telemetryPanel->setModeText(modeText);
    }
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
    sendOperatorInputSnapshot();
    refreshArmNamedTargets();
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

void MainWindow::onJointRuntimeStatesReceived(const Communication::JointRuntimeStateList &states)
{
    if (m_motorRuntimeWidget) {
        m_motorRuntimeWidget->setRuntimeStates(states);
    }
}

void MainWindow::onCO2DataReceived(float ppm)
{
    if (m_telemetryPanel) {
        m_telemetryPanel->setCO2Value(ppm);
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
    if (m_cameraGridWidget) {
        m_cameraGridWidget->setCameraInfo(cameraId, rtspUrl, online, codec, width, height, fps, bitrate);
        addCommand(QString("[TCP] 摄像头%1 %2 %3")
                   .arg(cameraId).arg(online ? "上线" : "离线").arg(rtspUrl));
    }
}

void MainWindow::onProtocolMessageReceived(const QJsonObject &message)
{
    const QString type = message["type"].toString();

    if (type == "ack_pending") {
        addCommand(QString("[ACK] waiting %1 seq=%2 timeout=%3ms")
                   .arg(message["ack_type"].toString("unknown"))
                   .arg(message["seq"].toVariant().toLongLong())
                   .arg(message["timeout_ms"].toInt()));
        return;
    }

    if (type == "ack") {
        const QString ackType = message["ack_type"].toString("unknown");
        const qint64 seq = message["seq"].toVariant().toLongLong();
        const bool ok = message["ok"].toBool(false);
        const int code = message["code"].toInt(-1);
        const QString text = message["message"].toString();
        const QString line = QString("[ACK] %1 seq=%2 %3 code=%4 elapsed=%5ms %6")
                                 .arg(ackType)
                                 .arg(seq)
                                 .arg(ok ? "OK" : "FAIL")
                                 .arg(code)
                                 .arg(message["elapsed_ms"].toVariant().toLongLong())
                                 .arg(text);
        if (ok) {
            addCommand(line);
        } else {
            addError(line);
        }
        return;
    }

    if (type == "ack_timeout" || type == "ack_disconnected") {
        addError(QString("[ACK] %1 %2 seq=%3 elapsed=%4ms")
                 .arg(type == "ack_timeout" ? "TIMEOUT" : "DISCONNECTED")
                 .arg(message["ack_type"].toString("unknown"))
                 .arg(message["seq"].toVariant().toLongLong())
                 .arg(message["elapsed_ms"].toVariant().toLongLong()));
        return;
    }

    if (type == "ack_unmatched") {
        addError(QString("[ACK] UNMATCHED %1 seq=%2 code=%3 %4")
                 .arg(message["ack_type"].toString("unknown"))
                 .arg(message["seq"].toVariant().toLongLong())
                 .arg(message["code"].toInt(-1))
                 .arg(message["message"].toString()));
        return;
    }

    if (type == "ack_mismatch") {
        addError(QString("[ACK] MISMATCH actual=%1 expected=%2 seq=%3 elapsed=%4ms code=%5 %6")
                 .arg(message["ack_type"].toString("unknown"))
                 .arg(message["expected_ack_type"].toString("unknown"))
                 .arg(message["seq"].toVariant().toLongLong())
                 .arg(message["elapsed_ms"].toVariant().toLongLong())
                 .arg(message["code"].toInt(-1))
                 .arg(message["message"].toString()));
        return;
    }

    if (type == "service_call_result") {
        const bool ok = message["ok"].toBool(false);
        const QString line = QString("[ROS service] %1 %2 seq=%3 code=%4 %5ms %6")
                                 .arg(message["command"].toString("unknown"))
                                 .arg(ok ? "OK" : "FAIL")
                                 .arg(message["seq"].toVariant().toLongLong())
                                 .arg(message["code"].toInt(-1))
                                 .arg(message["duration_ms"].toVariant().toLongLong())
                                 .arg(message["message"].toString());
        const QString detail = QString("%1 service=%2 request=%3")
                                   .arg(line)
                                   .arg(message["service"].toString("unknown"))
                                   .arg(message["request_type"].toString("unknown"));
        if (ok) {
            addCommand(detail);
        } else {
            addError(detail);
        }
        return;
    }

    if (type == "emergency_state") {
        const bool active = message["active"].toBool(false);
        const QString source = message["source"].toString();
        const QString text = message["message"].toString();
        const QString line = QString("[急停状态] %1 source=%2 %3")
                                 .arg(active ? "ACTIVE" : "CLEAR")
                                 .arg(source)
                                 .arg(text);
        addCommand(line);
        return;
    }

    if (type == "hello") {
        addCommand(QString("[Bridge] hello name=%1 version=%2 robot=%3")
                   .arg(message["bridge_name"].toString("unknown"))
                   .arg(message["bridge_version"].toString("unknown"))
                   .arg(message["robot_id"].toString("unknown")));
        return;
    }

    if (type == "capabilities") {
        addCommand(QString("[Bridge] capabilities supports=%1 max_frame=%2")
                   .arg(message["supports"].toArray().size())
                   .arg(message["max_frame_bytes"].toVariant().toLongLong()));
        return;
    }

    if (type == "arm_named_targets"
        || type == "moveit_named_targets"
        || type == "moveit_named_poses"
        || type == "arm_action_list") {
        updateArmNamedTargets(message);
        return;
    }

    if (type == "sync_request_sent") {
        addCommand(QString("[同步] request reason=%1 sync=%2 cameras=%3")
                   .arg(message["reason"].toString("unknown"))
                   .arg(message["sync_sent"].toBool(false) ? "sent" : "failed")
                   .arg(message["camera_list_sent"].toBool(false) ? "sent" : "failed"));
        return;
    }

    if (type == "system_snapshot") {
        const QJsonObject emergency = message["emergency"].toObject();
        const QJsonObject motor = message["motor"].toObject();
        const QJsonObject lastError = message["last_error"].toObject();
        ControlMode snapshotMode = m_controlMode;
        if (parseControlMode(message["control_mode"].toString(), &snapshotMode)) {
            applyControlMode(snapshotMode);
        }
        addCommand(QString("[同步] system_snapshot seq=%1 mode=%2 emergency=%3 motor=%4/%5 last_error=%6:%7")
                   .arg(message["seq"].toVariant().toLongLong())
                   .arg(message["control_mode"].toString("unknown"))
                   .arg(emergency["active"].toBool(false) ? "active" : "clear")
                   .arg(motor["initialized"].toBool(false) ? "init" : "not_init")
                   .arg(motor["enabled"].toBool(false) ? "enabled" : "disabled")
                   .arg(lastError["code"].toInt(0))
                   .arg(lastError["message"].toString()));
        return;
    }

    if (type == "camera_list_response") {
        addCommand(QString("[同步] camera_list_response seq=%1 cameras=%2")
                   .arg(message["seq"].toVariant().toLongLong())
                   .arg(message["cameras"].toArray().size()));
        return;
    }

    if (type == "param_response") {
        addCommand(QString("[参数] %1=%2")
                   .arg(message["name"].toString("unknown"))
                   .arg(message["value"].toVariant().toString()));
        return;
    }

    if (type == "protocol_error") {
        addError(QString("[协议错误] seq=%1 code=%2 %3")
                 .arg(message["seq"].toVariant().toLongLong())
                 .arg(message["code"].toInt(-1))
                 .arg(message["message"].toString()));
        return;
    }

    if (type == "system_status") {
        ControlMode statusMode = m_controlMode;
        if (parseControlMode(message["control_mode"].toString(), &statusMode)) {
            applyControlMode(statusMode);
        }
        if (message.contains("targets")
            || message.contains("named_targets")
            || message.contains("poses")
            || message.contains("actions")
            || message.contains("positions")
            || message.contains("arm_named_targets")
            || message.contains("moveit_named_targets")) {
            updateArmNamedTargets(message);
        }
        if (message.contains("last_operator_input_seq")) {
            return;
        }
        addCommand(QString("[状态] system_status seq=%1")
                   .arg(message["seq"].toVariant().toLongLong()));
    }
}

void MainWindow::onGamepadStateReceived(const ControllerState &state)
{
    m_latestGamepadState = state;
    m_gamepadConnected = true;

    // === 1. 更新 GamepadDisplayWidget 显示 ===
    if (m_controlPanel) {
        // 摇杆归一化到 -1.0 ~ 1.0
        float lx = state.sThumbLX / 32767.0f;
        float ly = state.sThumbLY / 32767.0f;
        float rx = state.sThumbRX / 32767.0f;
        float ry = state.sThumbRY / 32767.0f;
        float lt = state.bLeftTrigger / 255.0f;
        float rt = state.bRightTrigger / 255.0f;

        m_controlPanel->updateGamepadAxes(lx, ly, rx, ry, lt, rt);

        // 按钮状态显示
        if (state.buttonA) m_controlPanel->updateGamepadButton("A", true);
        else if (state.buttonB) m_controlPanel->updateGamepadButton("B", true);
        else if (state.buttonX) m_controlPanel->updateGamepadButton("X", true);
        else if (state.buttonY) m_controlPanel->updateGamepadButton("Y", true);
        else if (state.leftShoulder) m_controlPanel->updateGamepadButton("LB", true);
        else if (state.rightShoulder) m_controlPanel->updateGamepadButton("RB", true);
        else if (state.buttonBack) m_controlPanel->updateGamepadButton("Back", true);
        else if (state.buttonStart) m_controlPanel->updateGamepadButton("Start", true);
        else if (state.dpadUp) m_controlPanel->updateGamepadButton("DPad↑", true);
        else if (state.dpadDown) m_controlPanel->updateGamepadButton("DPad↓", true);
        else if (state.dpadLeft) m_controlPanel->updateGamepadButton("DPad←", true);
        else if (state.dpadRight) m_controlPanel->updateGamepadButton("DPad→", true);
        else m_controlPanel->updateGamepadButton("--", false);
    }

    const bool stickEmergencyPressed = state.leftThumb && state.rightThumb;
    if (stickEmergencyPressed && !m_gamepadStickEmergencyHeld) {
        m_gamepadStickEmergencyHeld = true;
        addCommand("[手柄] L3+R3 急停!");
        triggerEmergencyStop(QStringLiteral("gamepad_l3_r3"));
        return;
    }
    if (!stickEmergencyPressed) {
        m_gamepadStickEmergencyHeld = false;
    }

    sendOperatorInputSnapshot();
}
