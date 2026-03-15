#include "ROS1TcpClient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>

static const QString MODULE = "ROS1TcpClient";

namespace Communication {

ROS1TcpClient::ROS1TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_port(9090)
    , m_isConnected(false)
    , m_autoReconnect(true)
    , m_reconnectAttempts(0)
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    // 初始化统计信息
    m_stats.messagesSent = 0;
    m_stats.messagesReceived = 0;
    m_stats.bytesSent = 0;
    m_stats.bytesReceived = 0;
    m_stats.connectionCount = 0;
    m_stats.reconnectCount = 0;

    setupConnection();

    LOG_INFO(MODULE, "TCP客户端已创建");
}

ROS1TcpClient::~ROS1TcpClient()
{
    disconnectFromROS();
    LOG_INFO(MODULE, "TCP客户端已销毁");
}

void ROS1TcpClient::setupConnection()
{
    // 连接socket信号
    connect(m_socket, &QTcpSocket::connected, this, &ROS1TcpClient::handleConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ROS1TcpClient::handleDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ROS1TcpClient::handleReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &ROS1TcpClient::handleError);

    // 设置心跳定时器
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ROS1TcpClient::checkConnection);

    // 设置重连定时器
    m_reconnectTimer->setInterval(RECONNECT_INTERVAL_MS);
    connect(m_reconnectTimer, &QTimer::timeout, [this]() {
        if (m_autoReconnect && !m_isConnected && m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            m_reconnectAttempts++;
            QString msg = QString("正在尝试重连... (%1/%2)").arg(m_reconnectAttempts).arg(MAX_RECONNECT_ATTEMPTS);
            LOG_WARNING(MODULE, msg);
            emit connectionError(msg);
            connectToROS(m_hostAddress, m_port);
        } else if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            m_reconnectTimer->stop();
            QString msg = "重连失败，已达到最大重试次数";
            LOG_ERROR(MODULE, msg);
            HANDLE_ERROR(Utils::ErrorCode::NetworkConnectionFailed, MODULE, msg);
            emit connectionError(msg);
        }
    });
}

bool ROS1TcpClient::connectToROS(const QString &hostAddress, quint16 port)
{
    if (m_isConnected) {
        LOG_DEBUG(MODULE, "已经连接到ROS，无需重复连接");
        return true;
    }

    m_hostAddress = hostAddress;
    m_port = port;
    m_reconnectAttempts = 0;

    LOG_INFO(MODULE, QString("正在连接到ROS节点: %1:%2").arg(hostAddress).arg(port));

    m_socket->connectToHost(hostAddress, port);

    // 等待连接完成（最多5秒）
    bool connected = m_socket->waitForConnected(5000);
    if (connected) {
        m_stats.connectionCount++;
        LOG_INFO(MODULE, QString("连接成功 %1:%2").arg(hostAddress).arg(port));
    } else {
        QString detail = QString("socketError=%1 | %2 | state=%3")
                             .arg(m_socket->error())
                             .arg(m_socket->errorString())
                             .arg(m_socket->state());
        LOG_ERROR(MODULE, QString("连接失败 %1:%2").arg(hostAddress).arg(port));
        HANDLE_ERROR_DETAIL(Utils::ErrorCode::NetworkConnectionFailed, MODULE,
                            QString("连接失败 %1:%2").arg(hostAddress).arg(port), detail);
    }
    return connected;
}

void ROS1TcpClient::disconnectFromROS()
{
    if (m_isConnected) {
        m_heartbeatTimer->stop();
        m_reconnectTimer->stop();
        m_socket->disconnectFromHost();

        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
        LOG_INFO(MODULE, "已主动断开连接");
    }
}

bool ROS1TcpClient::isConnected() const
{
    return m_isConnected && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool ROS1TcpClient::sendMotorCommand(const ESP32State &esp32State)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送电机命令");
        return false;
    }

    QJsonObject command;
    command["type"] = "motor_command";

    // 添加6关节数据
    QJsonArray jointsArray;
    for (int i = 0; i < 6; ++i) {
        QJsonObject joint;
        joint["position"] = esp32State.joints[i].position / 1000.0f;
        joint["current"] = esp32State.joints[i].current / 1000.0f;
        jointsArray.append(joint);
    }
    command["joints"] = jointsArray;

    // 添加执行器数据
    command["executor_position"] = esp32State.executor_position / 1000.0f;
    command["executor_torque"] = esp32State.executor_torque / 1000.0f;
    command["executor_flags"] = esp32State.executor_flags;
    command["reserved"] = esp32State.reserved;

    return sendMessage(command);
}

bool ROS1TcpClient::sendJointControl(int jointId, float position, float velocity)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送关节控制命令");
        return false;
    }

    if (jointId < 0 || jointId >= 6) {
        HANDLE_ERROR_DETAIL(Utils::ErrorCode::InvalidParameter, MODULE,
                            "关节ID无效", QString("jointId=%1，必须在0-5范围内").arg(jointId));
        return false;
    }

    QJsonObject command;
    command["type"] = "joint_control";
    command["joint_id"] = jointId;
    command["position"] = position;
    command["velocity"] = velocity;
    command["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    return sendMessage(command);
}

bool ROS1TcpClient::sendVelocityCommand(float linearX, float linearY, float angularZ)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送速度命令");
        return false;
    }

    QJsonObject command;
    command["type"] = "cmd_vel";
    command["linear_x"] = linearX;
    command["linear_y"] = linearY;
    command["angular_z"] = angularZ;
    command["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    return sendMessage(command);
}

bool ROS1TcpClient::sendEmergencyStop()
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送急停命令");
        return false;
    }

    LOG_WARNING(MODULE, "发送急停命令");

    QJsonObject command;
    command["type"] = "emergency_stop";
    command["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    return sendMessage(command);
}

bool ROS1TcpClient::sendSystemCommand(const QString &command, const QJsonObject &params)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送系统命令");
        return false;
    }

    QJsonObject msg;
    msg["type"] = "system_command";
    msg["command"] = command;
    msg["params"] = params;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    LOG_INFO(MODULE, QString("发送系统命令: %1").arg(command));

    return sendMessage(msg);
}

bool ROS1TcpClient::sendEndEffectorControl(float x, float y, float z, float roll, float pitch, float yaw)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送末端控制命令");
        return false;
    }

    QJsonObject command;
    command["type"] = "cartesian_control";
    command["x"] = x;
    command["y"] = y;
    command["z"] = z;
    command["roll"] = roll;
    command["pitch"] = pitch;
    command["yaw"] = yaw;
    command["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    return sendMessage(command);
}

bool ROS1TcpClient::sendControlCommand(const Command &command)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送控制命令");
        return false;
    }

    QJsonObject msg;
    msg["type"] = "control_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    // IMU数据
    QJsonObject imuData;
    imuData["yaw"] = command.imu_yaw;
    imuData["roll"] = command.imu_roll;
    imuData["pitch"] = command.imu_pitch;
    msg["imu"] = imuData;

    // 4个摆臂电流
    QJsonArray swingArmCurrents;
    for (int i = 0; i < 4; ++i) {
        swingArmCurrents.append(command.swing_arm_current[i]);
    }
    msg["swing_arm_current"] = swingArmCurrents;

    // 机械臂末端位置
    QJsonObject armEndPos;
    armEndPos["x"] = command.arm_end_x;
    armEndPos["y"] = command.arm_end_y;
    armEndPos["z"] = command.arm_end_z;
    armEndPos["roll"] = command.arm_end_roll;
    armEndPos["pitch"] = command.arm_end_pitch;
    armEndPos["yaw"] = command.arm_end_yaw;
    msg["arm_end_position"] = armEndPos;

    // 标志位
    msg["command_flags"] = command.command_flags;

    return sendMessage(msg);
}

QString ROS1TcpClient::getConnectionStatus() const
{
    if (isConnected()) {
        return QString("已连接到 %1:%2").arg(m_hostAddress).arg(m_port);
    } else if (m_socket->state() == QAbstractSocket::ConnectingState) {
        return QString("正在连接到 %1:%2...").arg(m_hostAddress).arg(m_port);
    } else {
        return QString("未连接 (上次连接: %1:%2)").arg(m_hostAddress).arg(m_port);
    }
}

QString ROS1TcpClient::getROSHost() const
{
    return m_hostAddress;
}

quint16 ROS1TcpClient::getROSPort() const
{
    return m_port;
}

ROS1TcpClient::Stats ROS1TcpClient::getStats() const
{
    return m_stats;
}

void ROS1TcpClient::resetStats()
{
    m_stats.messagesSent = 0;
    m_stats.messagesReceived = 0;
    m_stats.bytesSent = 0;
    m_stats.bytesReceived = 0;
    m_stats.connectionCount = 0;
    m_stats.reconnectCount = 0;
    emitStatsUpdate();
    LOG_INFO(MODULE, "统计信息已重置");
}

// ==================== 跨线程槽函数 ====================

void ROS1TcpClient::slotConnectToROS(const QString &hostAddress, quint16 port)
{
    connectToROS(hostAddress, port);
}

void ROS1TcpClient::slotDisconnectFromROS()
{
    disconnectFromROS();
}

void ROS1TcpClient::slotSendMotorCommand(const ESP32State &esp32State)
{
    sendMotorCommand(esp32State);
}

void ROS1TcpClient::slotSendJointControl(int jointId, float position, float velocity)
{
    sendJointControl(jointId, position, velocity);
}

void ROS1TcpClient::slotSendEmergencyStop()
{
    sendEmergencyStop();
}

void ROS1TcpClient::slotSendSystemCommand(const QString &command, const QString &paramsJson)
{
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8());
    QJsonObject params = doc.object();
    sendSystemCommand(command, params);
}

// ==================== 事件处理 ====================

void ROS1TcpClient::handleConnected()
{
    m_isConnected = true;
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
    m_heartbeatTimer->start();

    if (m_stats.connectionCount > 1) {
        m_stats.reconnectCount++;
    }

    LOG_INFO(MODULE, QString("成功连接到ROS节点: %1").arg(getConnectionStatus()));
    emit connectedToROS();
    emitStatsUpdate();
}

void ROS1TcpClient::handleDisconnected()
{
    bool wasConnected = m_isConnected;
    m_isConnected = false;
    m_heartbeatTimer->stop();

    if (wasConnected) {
        LOG_WARNING(MODULE, "与ROS节点断开连接");
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "与ROS节点断开连接");
        emit disconnectedFromROS();

        // 如果启用自动重连，启动重连定时器
        if (m_autoReconnect) {
            LOG_INFO(MODULE, "启动自动重连...");
            m_reconnectTimer->start();
        }
    }
}

void ROS1TcpClient::handleReadyRead()
{
    QByteArray chunk = m_socket->readAll();
    m_stats.bytesReceived += chunk.size();
    m_receivedData.append(chunk);
    processReceivedData();
}

void ROS1TcpClient::handleError(QAbstractSocket::SocketError error)
{
    Utils::ErrorCode errorCode = Utils::ErrorCode::NetworkConnectionFailed;
    QString errorMsg;

    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "连接被拒绝";
        errorCode = Utils::ErrorCode::NetworkConnectionFailed;
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMsg = "远程主机关闭连接";
        errorCode = Utils::ErrorCode::NetworkDisconnected;
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "找不到主机";
        errorCode = Utils::ErrorCode::NetworkConnectionFailed;
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMsg = "连接超时";
        errorCode = Utils::ErrorCode::NetworkTimeout;
        break;
    case QAbstractSocket::NetworkError:
        errorMsg = "网络错误";
        errorCode = Utils::ErrorCode::NetworkConnectionFailed;
        break;
    default:
        errorMsg = QString("网络错误: %1").arg(m_socket->errorString());
    }

    HANDLE_ERROR_DETAIL(errorCode, MODULE, errorMsg,
                        QString("host=%1:%2, socketError=%3").arg(m_hostAddress).arg(m_port).arg(error));
    emit connectionError(errorMsg);
}

void ROS1TcpClient::checkConnection()
{
    if (!isConnected()) {
        LOG_DEBUG(MODULE, "心跳检测发现连接断开");
        handleDisconnected();
    } else {
        // 发送心跳包
        QJsonObject heartbeat;
        heartbeat["type"] = "heartbeat";
        heartbeat["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        sendMessage(heartbeat);
    }
}

bool ROS1TcpClient::sendMessage(const QJsonObject &message)
{
    if (!isConnected()) {
        return false;
    }

    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    qint64 bytesWritten = m_socket->write(data);
    if (bytesWritten == -1) {
        HANDLE_ERROR_DETAIL(Utils::ErrorCode::NetworkSendFailed, MODULE,
                            "发送消息失败", m_socket->errorString());
        return false;
    }

    m_stats.messagesSent++;
    m_stats.bytesSent += bytesWritten;

    bool flushed = m_socket->flush();
    emitStatsUpdate();

    return flushed;
}

void ROS1TcpClient::processReceivedData()
{
    while (m_receivedData.contains('\n')) {
        int index = m_receivedData.indexOf('\n');
        QByteArray line = m_receivedData.left(index);
        m_receivedData = m_receivedData.mid(index + 1);

        if (line.isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);

        if (error.error != QJsonParseError::NoError) {
            HANDLE_ERROR_DETAIL(Utils::ErrorCode::ProtocolParseError, MODULE,
                                "JSON解析错误", QString("%1 | 原始数据: %2")
                                    .arg(error.errorString())
                                    .arg(QString::fromUtf8(line.left(200))));
            continue;
        }

        QJsonObject msg = doc.object();
        emit rawMessageReceived(line);

        m_stats.messagesReceived++;

        // 根据消息类型处理
        QString msgType = msg["type"].toString();
        if (msgType.isEmpty()) {
            LOG_WARNING(MODULE, "收到无类型的JSON消息");
            continue;
        }

        if (msgType == "motor_state") {
            ESP32State state = parseMotorState(msg);
            emit motorStateReceived(state);
        } else if (msgType == "joint_data") {
            int jointId = msg["joint_id"].toInt();
            float position = msg["position"].toDouble();
            float current = msg["current"].toDouble();
            float torque = msg["torque"].toDouble();
            emit jointDataReceived(jointId, position, current, torque);
        } else if (msgType == "system_status") {
            emit systemStatusReceived(msg);
        } else if (msgType == "co2_data") {
            float ppm = msg["ppm"].toDouble();
            emit co2DataReceived(ppm);
        } else if (msgType == "imu_data") {
            float roll = msg["roll"].toDouble();
            float pitch = msg["pitch"].toDouble();
            float yaw = msg["yaw"].toDouble();
            float accelX = msg["accel_x"].toDouble();
            float accelY = msg["accel_y"].toDouble();
            float accelZ = msg["accel_z"].toDouble();
            emit imuDataReceived(roll, pitch, yaw, accelX, accelY, accelZ);
        } else if (msgType == "camera_info") {
            int cameraId = msg["camera_id"].toInt();
            QString rtspUrl = msg["rtsp_url"].toString();
            bool online = msg["online"].toBool();
            QString codec = msg["codec"].toString();
            int width = msg["width"].toInt();
            int height = msg["height"].toInt();
            int fps = msg["fps"].toInt();
            int bitrateKbps = msg["bitrate_kbps"].toInt();
            emit cameraInfoReceived(cameraId, rtspUrl, online, codec, width, height, fps, bitrateKbps);
        } else if (msgType != "heartbeat") {
            LOG_DEBUG(MODULE, QString("收到未处理的消息类型: %1").arg(msgType));
        }
    }

    emitStatsUpdate();
}

ESP32State ROS1TcpClient::parseMotorState(const QJsonObject &json)
{
    ESP32State state;

    QJsonArray jointsArray = json["joints"].toArray();
    for (int i = 0; i < 6 && i < jointsArray.size(); ++i) {
        QJsonObject joint = jointsArray[i].toObject();
        state.joints[i].position = static_cast<int16_t>(joint["position"].toDouble() * 1000.0f);
        state.joints[i].current = static_cast<int16_t>(joint["current"].toDouble() * 1000.0f);
    }

    state.executor_position = static_cast<int16_t>(json["executor_position"].toDouble() * 1000.0f);
    state.executor_torque = static_cast<int16_t>(json["executor_torque"].toDouble() * 1000.0f);
    state.executor_flags = static_cast<uint8_t>(json["executor_flags"].toInt(0));
    state.reserved = static_cast<uint8_t>(json["reserved"].toInt(0));

    return state;
}

void ROS1TcpClient::emitStatsUpdate()
{
    emit statsUpdated(m_stats);
}

} // namespace Communication
