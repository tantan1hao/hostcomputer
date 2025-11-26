#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "src/communication/serialportmanager.h"
#include "src/parser/ProtocolParser.h"
#include "src/communication/SharedStructs.h"

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
#include <QDialogButtonBox>
#include <QShortcut>
#include <QKeySequence>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serialManager(nullptr)
    , m_protocolParser(nullptr)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("电机控制系统上位机 v1.0");
    resize(1920, 1080);
    setMinimumSize(1600, 900);

    // 初始化组件
    setupSerialPort();
    setupParser();

    // 初始化定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateSystemStatus);
    m_statusTimer->start(1000); // 每秒更新一次状态

    // 设置连接和状态栏
    setupConnections();
    setupStatusBar();

    // 设置F5快捷键刷新串口列表
    QShortcut* refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(refreshShortcut, &QShortcut::activated, this, &MainWindow::refreshSerialPorts);

    // 初始化UI状态
    updateConnectionDisplay();
    updateHeartbeatDisplay();

    qDebug() << "主窗口初始化完成";
}

MainWindow::~MainWindow()
{
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
    delete ui;
}

void MainWindow::setupSerialPort()
{
    // 创建串口管理器
    m_serialManager = new Communication::SerialPortManager(this);

    // 连接串口数据接收信号
    connect(m_serialManager, &Communication::SerialPortManager::dataReceived,
            this, &MainWindow::onSerialDataReceived);

    // 连接连接状态变化信号
    connect(m_serialManager, &Communication::SerialPortManager::connectionStatusChanged,
            this, &MainWindow::updateConnectionStatus);

    qDebug() << "串口管理器初���化完成";
}

void MainWindow::setupParser()
{
    // 创建协议解析器
    m_protocolParser = new Parser::ProtocolParser();

    qDebug() << "协议解析器初始化完成";
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

void MainWindow::updateCPUUsage(int cpu, int gpu)
{
    m_cpuUsage = cpu;
    m_gpuUsage = gpu;
    ui->label_cpu_value->setText(QString("%1% / %2%").arg(cpu).arg(gpu));
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

    ui->label_roll->setText(QString("Roll: %1°").arg(roll, 0, 'f', 1));
    ui->label_pitch->setText(QString("Pitch: %1°").arg(pitch, 0, 'f', 1));
    ui->label_yaw->setText(QString("Yaw: %1°").arg(yaw, 0, 'f', 1));

    // TODO: 这里可以更新3D姿态模型显示
}

void MainWindow::updateJointsData(const MotorState& motorState)
{
    // 清空text_errors控件
    ui->text_errors->clear();

    // 获取关节数据的格式化字符串并显示
    QString jointsData = motorState.getJointsDataString();

    // 添加时间戳
    QString timestamp = getCurrentTimestamp();
    QString header = QString("[%1] 6关节数据接收\n").arg(timestamp);

    // 设置文本内容
    ui->text_errors->setText(header + jointsData);

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
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认退出",
        "确定要退出电机控制系统吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (m_isConnected) {
            // 发送断开连接命令
            updateConnectionStatus(false);
        }
        event->accept();
    } else {
        event->ignore();
    }
}

// 菜单槽函数
void MainWindow::on_action_connect_triggered()
{
    // 显示串口选择对话框
    showSerialPortSelection();
}

void MainWindow::on_action_disconnect_triggered()
{
    if (!m_serialManager) {
        addError("[错误] 串口管理器未初始化");
        return;
    }

    m_serialManager->closePort();
    updateConnectionStatus(false);
    addCommand("[操作] 用户点击断开连接");
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
    // TODO: 重置窗口布局
    QMessageBox::information(this, "提示", "布局重置功能开发中...");
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

    // 模拟CPU/GPU使用率
    if (counter % 5 == 0) {
        int simulatedCPU = 20 + (counter % 30);
        int simulatedGPU = 15 + (counter % 25);
        updateCPUUsage(simulatedCPU, simulatedGPU);
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

void MainWindow::onSerialDataReceived(const QByteArray &data)
{
    qDebug() << "��收到串口数据，长度:" << data.size();

    if (!m_protocolParser) {
        qDebug() << "协议解析器未初始化";
        return;
    }

    // 尝试解析协议帧
    Communication::ESP32State esp32State;
    if (m_protocolParser->parseFromQByteArray(data, &esp32State)) {
        qDebug() << "协议解析成功";

        // 转换为MotorState用于显示
        MotorState displayState;
        displayState.isOnline = true;
        displayState.isRunning = true;
        displayState.hasError = false;
        displayState.timestamp = QDateTime::currentMSecsSinceEpoch();

        // 转换6个关节数据
        for (int i = 0; i < 6; ++i) {
            displayState.joints[i].position = esp32State.joints[i].position / 1000.0f;  // 缩小1000倍
            displayState.joints[i].current = esp32State.joints[i].current / 1000.0f;    // 缩小1000倍
            displayState.joints[i].executor_position = esp32State.executor_position / 1000.0f;
            displayState.joints[i].executor_torque = esp32State.executor_torque / 1000.0f;
            displayState.joints[i].executor_flags = esp32State.executor_flags;
            displayState.joints[i].reserved = esp32State.reserved;
        }

        // 更新UI显示
        updateJointsData(displayState);

        // 在命令区域添加接收消息
        addCommand("[串口] 接收并解析关节数据成功");

    } else {
        qDebug() << "协议解析失败";
        addError("[错误] 协议帧解析失败");
    }
}
