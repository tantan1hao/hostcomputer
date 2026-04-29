#include "ROS1TcpClient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>

static const QString MODULE = "ROS1TcpClient";

namespace Communication {

ROS1TcpClient::ROS1TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_port(9090)
    , m_isConnected(false)
    , m_autoReconnect(true)
    , m_manualDisconnectRequested(false)
    , m_heartbeatOnline(false)
    , m_reconnectAttempts(0)
    , m_lastMessageReceivedMs(0)
    , m_lastHeartbeatAckMs(0)
    , m_nextSequence(0)
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_ackTimer(new QTimer(this))
{
    // 初始化统计信息
    m_stats.messagesSent = 0;
    m_stats.messagesReceived = 0;
    m_stats.bytesSent = 0;
    m_stats.bytesReceived = 0;
    m_stats.connectionCount = 0;
    m_stats.reconnectCount = 0;
    m_stats.ackPendingCount = 0;
    m_stats.ackReceivedCount = 0;
    m_stats.ackTimeoutCount = 0;
    m_stats.protocolErrorCount = 0;
    m_stats.heartbeatTimeoutCount = 0;
    m_stats.lastHeartbeatRttMs = -1;
    m_stats.lastHeartbeatAckMs = 0;

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
        // 避免同一次连接尚未完成时重复计次，导致“提前耗尽重试次数”
        if (m_socket->state() == QAbstractSocket::ConnectingState) {
            return;
        }

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

    m_ackTimer->setInterval(ACK_CHECK_INTERVAL_MS);
    connect(m_ackTimer, &QTimer::timeout, this, &ROS1TcpClient::checkAckTimeouts);
}

bool ROS1TcpClient::connectToROS(const QString &hostAddress, quint16 port)
{
    if (m_isConnected) {
        LOG_DEBUG(MODULE, "已经连接到ROS，无需重复连接");
        return true;
    }

    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        LOG_DEBUG(MODULE, "正在连接ROS，忽略重复连接请求");
        return true;
    }

    m_hostAddress = hostAddress;
    m_port = port;
    m_reconnectAttempts = 0;
    m_manualDisconnectRequested = false;

    LOG_INFO(MODULE, QString("正在连接到ROS节点: %1:%2").arg(hostAddress).arg(port));

    m_socket->connectToHost(hostAddress, port);

    // 改为异步连接：通过 handleConnected/handleError 回调更新状态
    return true;
}

void ROS1TcpClient::disconnectFromROS()
{
    m_manualDisconnectRequested = true;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_heartbeatTimer->stop();
        m_reconnectTimer->stop();
        m_socket->disconnectFromHost();
        LOG_INFO(MODULE, "已主动断开连接");
    }
}

bool ROS1TcpClient::isConnected() const
{
    return m_isConnected && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool ROS1TcpClient::sendMotorCommand(const MotorState &motorState)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送电机命令");
        return false;
    }

    QJsonObject command;
    command["type"] = "motor_command";
    command["protocol_version"] = HostProtocol::ProtocolVersion;
    command["seq"] = static_cast<qint64>(nextSequence());
    command["timestamp_ms"] = HostProtocol::nowMs();

    // 添加6关节数据
    QJsonArray jointsArray;
    for (int i = 0; i < 6; ++i) {
        QJsonObject joint;
        joint["position"] = motorState.joints[i].position / 1000.0f;
        joint["current"] = motorState.joints[i].current / 1000.0f;
        jointsArray.append(joint);
    }
    command["joints"] = jointsArray;

    // 添加执行器数据
    command["executor_position"] = motorState.executor_position / 1000.0f;
    command["executor_torque"] = motorState.executor_torque / 1000.0f;
    command["executor_flags"] = motorState.executor_flags;
    command["reserved"] = motorState.reserved;

    return sendMessage(command);
}

bool ROS1TcpClient::sendOperatorInput(const OperatorInputState &inputState)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送操作者输入状态");
        return false;
    }

    return sendMessage(HostProtocol::makeOperatorInput(inputState, nextSequence()));
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

    QJsonObject command = HostProtocol::makeCommand(QStringLiteral("joint_control"), nextSequence());
    command["joint_id"] = jointId;
    command["position"] = position;
    command["velocity"] = velocity;

    return sendMessage(command);
}

bool ROS1TcpClient::sendEmergencyStop(const QString &source)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送急停命令");
        return false;
    }

    LOG_WARNING(MODULE, "发送急停命令");

    QJsonObject params;
    params["source"] = source.isEmpty() ? QStringLiteral("upper_computer") : source;
    return sendTrackedMessage(
        HostProtocol::makeCommand(QStringLiteral("emergency_stop"), nextSequence(), params),
        QStringLiteral("emergency_stop"));
}

bool ROS1TcpClient::sendSystemCommand(const QString &command, const QJsonObject &params)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送系统命令");
        return false;
    }

    QJsonObject msg = HostProtocol::makeCommand(QStringLiteral("system_command"), nextSequence());
    msg["command"] = command;
    msg["params"] = params;

    LOG_INFO(MODULE, QString("发送系统命令: %1").arg(command));

    return sendTrackedMessage(msg, QStringLiteral("system_command"));
}

bool ROS1TcpClient::sendEndEffectorControl(float x, float y, float z, float roll, float pitch, float yaw)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送末端控制命令");
        return false;
    }

    QJsonObject command = HostProtocol::makeCommand(QStringLiteral("cartesian_control"), nextSequence());
    command["x"] = x;
    command["y"] = y;
    command["z"] = z;
    command["roll"] = roll;
    command["pitch"] = pitch;
    command["yaw"] = yaw;

    return sendMessage(command);
}

bool ROS1TcpClient::sendControlCommand(const Command &command)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法发送控制命令");
        return false;
    }

    QJsonObject msg = HostProtocol::makeCommand(QStringLiteral("control_command"), nextSequence());

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

bool ROS1TcpClient::requestBridgeSync(const QString &reason)
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法请求状态同步");
        return false;
    }

    return sendMessage(HostProtocol::makeSyncRequest(nextSequence(), reason));
}

bool ROS1TcpClient::requestCameraList()
{
    if (!isConnected()) {
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "未连接到ROS，无法请求摄像头列表");
        return false;
    }

    return sendMessage(HostProtocol::makeCameraListRequest(nextSequence()));
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
    Stats stats = m_stats;
    stats.ackPendingCount = static_cast<quint64>(m_pendingAcks.size());
    return stats;
}

void ROS1TcpClient::resetStats()
{
    m_stats.messagesSent = 0;
    m_stats.messagesReceived = 0;
    m_stats.bytesSent = 0;
    m_stats.bytesReceived = 0;
    m_stats.connectionCount = 0;
    m_stats.reconnectCount = 0;
    m_stats.ackReceivedCount = 0;
    m_stats.ackTimeoutCount = 0;
    m_stats.protocolErrorCount = 0;
    m_stats.heartbeatTimeoutCount = 0;
    m_stats.lastHeartbeatRttMs = -1;
    m_stats.lastHeartbeatAckMs = m_lastHeartbeatAckMs;
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

void ROS1TcpClient::slotSendMotorCommand(const MotorState &motorState)
{
    sendMotorCommand(motorState);
}

void ROS1TcpClient::slotSendJointControl(int jointId, float position, float velocity)
{
    sendJointControl(jointId, position, velocity);
}

void ROS1TcpClient::slotSendEmergencyStop()
{
    sendEmergencyStop(QStringLiteral("slot"));
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
    m_lastMessageReceivedMs = QDateTime::currentMSecsSinceEpoch();
    m_lastHeartbeatAckMs = 0;
    setHeartbeatOnline(false);
    m_pendingHeartbeats.clear();
    m_manualDisconnectRequested = false;
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
    m_heartbeatTimer->start();

    m_stats.connectionCount++;

    if (m_stats.connectionCount > 1) {
        m_stats.reconnectCount++;
    }

    LOG_INFO(MODULE, QString("成功连接到ROS节点: %1").arg(getConnectionStatus()));
    emit connectedToROS();
    requestBridgeState(m_stats.connectionCount > 1 ? QStringLiteral("reconnected")
                                                   : QStringLiteral("connected"));
    emitStatsUpdate();
}

void ROS1TcpClient::handleDisconnected()
{
    bool wasConnected = m_isConnected;
    m_isConnected = false;
    setHeartbeatOnline(false);
    m_heartbeatTimer->stop();
    failAllPendingAcks(QStringLiteral("ack_disconnected"),
                       QStringLiteral("connection lost before ACK"), -2);

    if (wasConnected) {
        LOG_WARNING(MODULE, "与ROS节点断开连接");
        HANDLE_ERROR(Utils::ErrorCode::NetworkDisconnected, MODULE, "与ROS节点断开连接");
        emit disconnectedFromROS();

        if (m_manualDisconnectRequested) {
            LOG_INFO(MODULE, "主动断开连接，不启动自动重连");
        } else if (m_autoReconnect) {
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

    if (m_receivedData.size() > MAX_FRAME_BYTES && !m_receivedData.contains('\n')) {
        closeForProtocolError(0, 2001, QStringLiteral("TCP frame exceeds max length before newline"));
        return;
    }

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
        const qint64 now = HostProtocol::nowMs();
        trimExpiredHeartbeats(now);

        if (m_lastHeartbeatAckMs > 0 && now - m_lastHeartbeatAckMs > HEARTBEAT_INTERVAL_MS * 3) {
            setHeartbeatOnline(false);
        }

        const quint64 seq = nextSequence();
        if (sendMessage(HostProtocol::makeHeartbeat(seq, now))) {
            m_pendingHeartbeats.insert(seq, now);
        }
    }
}

void ROS1TcpClient::checkAckTimeouts()
{
    if (m_pendingAcks.isEmpty()) {
        m_ackTimer->stop();
        return;
    }

    const qint64 now = HostProtocol::nowMs();
    QList<PendingAck> expired;
    for (auto it = m_pendingAcks.cbegin(); it != m_pendingAcks.cend(); ++it) {
        if (now >= it.value().deadlineMs) {
            expired.append(it.value());
        }
    }

    for (const PendingAck &pending : expired) {
        if (!m_pendingAcks.remove(pending.seq)) {
            continue;
        }
        m_stats.ackTimeoutCount++;
        failPendingAck(pending, QStringLiteral("ack_timeout"),
                       QStringLiteral("ACK timeout"), -1, now);
    }

    if (m_pendingAcks.isEmpty()) {
        m_ackTimer->stop();
    }

    emitStatsUpdate();
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

    m_socket->flush();
    emitStatsUpdate();

    return bytesWritten == data.size();
}

bool ROS1TcpClient::sendTrackedMessage(const QJsonObject &message, const QString &ackType)
{
    if (!sendMessage(message)) {
        return false;
    }

    registerPendingAck(message, ackType);
    return true;
}

quint64 ROS1TcpClient::nextSequence()
{
    ++m_nextSequence;
    if (m_nextSequence == 0) {
        m_nextSequence = 1;
    }
    return m_nextSequence;
}

void ROS1TcpClient::registerPendingAck(const QJsonObject &message, const QString &ackType)
{
    const qint64 seqValue = message["seq"].toVariant().toLongLong();
    if (seqValue <= 0) {
        LOG_WARNING(MODULE, QString("无法跟踪ACK，缺少有效seq: %1").arg(ackType));
        return;
    }

    const qint64 now = HostProtocol::nowMs();
    PendingAck pending;
    pending.seq = static_cast<quint64>(seqValue);
    pending.ackType = ackType;
    pending.sentAtMs = now;
    pending.deadlineMs = now + ACK_TIMEOUT_MS;
    m_pendingAcks.insert(pending.seq, pending);

    QJsonObject event;
    event["type"] = "ack_pending";
    event["protocol_version"] = HostProtocol::ProtocolVersion;
    event["ack_type"] = ackType;
    event["seq"] = static_cast<qint64>(pending.seq);
    event["timestamp_ms"] = now;
    event["deadline_ms"] = pending.deadlineMs;
    event["timeout_ms"] = ACK_TIMEOUT_MS;
    emit systemStatusReceived(event);

    if (!m_ackTimer->isActive()) {
        m_ackTimer->start();
    }

    emitStatsUpdate();
}

void ROS1TcpClient::handleAckMessage(const QJsonObject &message, qint64 receivedAtMs)
{
    const qint64 seqValue = message["seq"].toVariant().toLongLong();
    const QString ackType = message["ack_type"].toString("unknown");

    if (seqValue <= 0) {
        QJsonObject event;
        event["type"] = "ack_unmatched";
        event["protocol_version"] = HostProtocol::ProtocolVersion;
        event["ack_type"] = ackType;
        event["seq"] = seqValue;
        event["code"] = -3;
        event["message"] = QStringLiteral("ACK missing valid seq");
        event["timestamp_ms"] = receivedAtMs;
        event["ack"] = message;
        emit systemStatusReceived(event);
        return;
    }

    const quint64 seq = static_cast<quint64>(seqValue);
    auto it = m_pendingAcks.find(seq);
    if (it == m_pendingAcks.end()) {
        QJsonObject event;
        event["type"] = "ack_unmatched";
        event["protocol_version"] = HostProtocol::ProtocolVersion;
        event["ack_type"] = ackType;
        event["seq"] = static_cast<qint64>(seq);
        event["code"] = -4;
        event["message"] = QStringLiteral("ACK has no pending command");
        event["timestamp_ms"] = receivedAtMs;
        event["ack"] = message;
        emit systemStatusReceived(event);
        return;
    }

    const PendingAck pending = it.value();
    m_pendingAcks.erase(it);
    m_stats.ackReceivedCount++;

    if (ackType != pending.ackType) {
        QJsonObject mismatch;
        mismatch["type"] = "ack_mismatch";
        mismatch["protocol_version"] = HostProtocol::ProtocolVersion;
        mismatch["ack_type"] = ackType;
        mismatch["expected_ack_type"] = pending.ackType;
        mismatch["seq"] = static_cast<qint64>(pending.seq);
        mismatch["code"] = -5;
        mismatch["message"] = QStringLiteral("ACK type does not match pending command");
        mismatch["timestamp_ms"] = receivedAtMs;
        mismatch["elapsed_ms"] = receivedAtMs - pending.sentAtMs;
        mismatch["ack"] = message;
        emit systemStatusReceived(mismatch);

        if (m_pendingAcks.isEmpty()) {
            m_ackTimer->stop();
        }

        emitStatsUpdate();
        return;
    }

    QJsonObject resolved = message;
    resolved["elapsed_ms"] = receivedAtMs - pending.sentAtMs;
    emit systemStatusReceived(resolved);

    if (m_pendingAcks.isEmpty()) {
        m_ackTimer->stop();
    }

    emitStatsUpdate();
}

void ROS1TcpClient::handleHeartbeatAck(const QJsonObject &message, qint64 receivedAtMs)
{
    const qint64 seqValue = message["seq"].toVariant().toLongLong();
    if (seqValue > 0) {
        const quint64 seq = static_cast<quint64>(seqValue);
        auto it = m_pendingHeartbeats.find(seq);
        if (it != m_pendingHeartbeats.end()) {
            m_stats.lastHeartbeatRttMs = receivedAtMs - it.value();
            m_pendingHeartbeats.erase(it);
        }
    }

    m_lastHeartbeatAckMs = receivedAtMs;
    m_stats.lastHeartbeatAckMs = receivedAtMs;
    setHeartbeatOnline(true);
}

void ROS1TcpClient::failPendingAck(const PendingAck &pending, const QString &eventType,
                                   const QString &message, int code, qint64 nowMs)
{
    QJsonObject event;
    event["type"] = eventType;
    event["protocol_version"] = HostProtocol::ProtocolVersion;
    event["ack_type"] = pending.ackType;
    event["seq"] = static_cast<qint64>(pending.seq);
    event["code"] = code;
    event["message"] = message;
    event["timestamp_ms"] = nowMs;
    event["elapsed_ms"] = nowMs - pending.sentAtMs;
    event["deadline_ms"] = pending.deadlineMs;
    emit systemStatusReceived(event);
}

void ROS1TcpClient::failAllPendingAcks(const QString &eventType, const QString &message, int code)
{
    if (m_pendingAcks.isEmpty()) {
        m_ackTimer->stop();
        return;
    }

    const qint64 now = HostProtocol::nowMs();
    const QList<PendingAck> pendingAcks = m_pendingAcks.values();
    m_pendingAcks.clear();
    m_ackTimer->stop();

    for (const PendingAck &pending : pendingAcks) {
        failPendingAck(pending, eventType, message, code, now);
    }

    emitStatsUpdate();
}

static bool isIntegerJsonValue(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

static qint64 jsonInteger(const QJsonValue &value)
{
    return value.toVariant().toLongLong();
}

bool ROS1TcpClient::validateIncomingMessage(const QJsonObject &message,
                                            QString *errorMessage,
                                            int *errorCode) const
{
    const QJsonValue typeValue = message.value(QStringLiteral("type"));
    if (!typeValue.isString() || typeValue.toString().isEmpty()) {
        *errorCode = 2101;
        *errorMessage = QStringLiteral("missing or invalid type");
        return false;
    }

    if (message.contains(QStringLiteral("protocol_version"))) {
        const QJsonValue versionValue = message.value(QStringLiteral("protocol_version"));
        if (!isIntegerJsonValue(versionValue)) {
            *errorCode = 2102;
            *errorMessage = QStringLiteral("invalid protocol_version");
            return false;
        }

        const qint64 version = jsonInteger(versionValue);
        if (version != HostProtocol::ProtocolVersion) {
            *errorCode = 2103;
            *errorMessage = QStringLiteral("unsupported protocol_version");
            return false;
        }
    }

    if (message.contains(QStringLiteral("seq")) && !isIntegerJsonValue(message.value(QStringLiteral("seq")))) {
        *errorCode = 2104;
        *errorMessage = QStringLiteral("invalid seq");
        return false;
    }

    const QString type = typeValue.toString();
    if (type == QStringLiteral("heartbeat_ack")) {
        if (!isIntegerJsonValue(message.value(QStringLiteral("seq"))) || jsonInteger(message.value(QStringLiteral("seq"))) <= 0) {
            *errorCode = 2110;
            *errorMessage = QStringLiteral("heartbeat_ack missing valid seq");
            return false;
        }
    } else if (type == QStringLiteral("ack")) {
        if (!message.value(QStringLiteral("ack_type")).isString()) {
            *errorCode = 2120;
            *errorMessage = QStringLiteral("ack missing ack_type");
            return false;
        }
        if (!isIntegerJsonValue(message.value(QStringLiteral("seq"))) || jsonInteger(message.value(QStringLiteral("seq"))) <= 0) {
            *errorCode = 2121;
            *errorMessage = QStringLiteral("ack missing valid seq");
            return false;
        }
        if (!message.value(QStringLiteral("ok")).isBool()) {
            *errorCode = 2122;
            *errorMessage = QStringLiteral("ack missing ok");
            return false;
        }
        if (!isIntegerJsonValue(message.value(QStringLiteral("code")))) {
            *errorCode = 2123;
            *errorMessage = QStringLiteral("ack missing code");
            return false;
        }
    } else if (type == QStringLiteral("camera_info")) {
        if (!validateCameraObject(message, errorMessage, errorCode)) {
            return false;
        }
    } else if (type == QStringLiteral("camera_list_response")) {
        const QJsonValue camerasValue = message.value(QStringLiteral("cameras"));
        if (!camerasValue.isArray()) {
            *errorCode = 2140;
            *errorMessage = QStringLiteral("camera_list_response missing cameras array");
            return false;
        }
        const QJsonArray cameras = camerasValue.toArray();
        for (const QJsonValue &cameraValue : cameras) {
            if (!cameraValue.isObject()) {
                *errorCode = 2141;
                *errorMessage = QStringLiteral("camera_list_response camera item is not object");
                return false;
            }
            if (!validateCameraObject(cameraValue.toObject(), errorMessage, errorCode)) {
                return false;
            }
        }
    } else if (type == QStringLiteral("system_snapshot")) {
        if (message.contains(QStringLiteral("control_mode"))
            && !message.value(QStringLiteral("control_mode")).isString()) {
            *errorCode = 2150;
            *errorMessage = QStringLiteral("system_snapshot invalid control_mode");
            return false;
        }
        if (message.contains(QStringLiteral("emergency"))
            && !message.value(QStringLiteral("emergency")).isObject()) {
            *errorCode = 2151;
            *errorMessage = QStringLiteral("system_snapshot invalid emergency object");
            return false;
        }
        if (message.contains(QStringLiteral("motor"))
            && !message.value(QStringLiteral("motor")).isObject()) {
            *errorCode = 2152;
            *errorMessage = QStringLiteral("system_snapshot invalid motor object");
            return false;
        }
    } else if (type == QStringLiteral("protocol_error")) {
        if (!isIntegerJsonValue(message.value(QStringLiteral("code")))
            || !message.value(QStringLiteral("message")).isString()) {
            *errorCode = 2160;
            *errorMessage = QStringLiteral("protocol_error missing code or message");
            return false;
        }
    } else if (type == QStringLiteral("service_call_result")) {
        if (!message.value(QStringLiteral("request_type")).isString()
            || !message.value(QStringLiteral("command")).isString()
            || !message.value(QStringLiteral("service")).isString()
            || !message.value(QStringLiteral("ok")).isBool()
            || !isIntegerJsonValue(message.value(QStringLiteral("code")))
            || !message.value(QStringLiteral("message")).isString()) {
            *errorCode = 2165;
            *errorMessage = QStringLiteral("service_call_result missing required fields");
            return false;
        }
        if (message.contains(QStringLiteral("duration_ms"))
            && !isIntegerJsonValue(message.value(QStringLiteral("duration_ms")))) {
            *errorCode = 2166;
            *errorMessage = QStringLiteral("service_call_result invalid duration_ms");
            return false;
        }
    } else if (type == QStringLiteral("motor_state")) {
        if (!message.value(QStringLiteral("joints")).isArray()) {
            *errorCode = 2170;
            *errorMessage = QStringLiteral("motor_state missing joints array");
            return false;
        }
    } else if (type == QStringLiteral("joint_runtime_states")) {
        if (!message.value(QStringLiteral("states")).isArray()) {
            *errorCode = 2175;
            *errorMessage = QStringLiteral("joint_runtime_states missing states array");
            return false;
        }
    } else if (type == QStringLiteral("co2_data")) {
        if (!message.value(QStringLiteral("ppm")).isDouble()) {
            *errorCode = 2180;
            *errorMessage = QStringLiteral("co2_data missing ppm");
            return false;
        }
    } else if (type == QStringLiteral("imu_data")) {
        if (!message.value(QStringLiteral("roll")).isDouble()
            || !message.value(QStringLiteral("pitch")).isDouble()
            || !message.value(QStringLiteral("yaw")).isDouble()) {
            *errorCode = 2190;
            *errorMessage = QStringLiteral("imu_data missing attitude fields");
            return false;
        }
    }

    return true;
}

bool ROS1TcpClient::validateCameraObject(const QJsonObject &camera,
                                         QString *errorMessage,
                                         int *errorCode) const
{
    const QJsonValue idValue = camera.value(QStringLiteral("camera_id"));
    if (!isIntegerJsonValue(idValue)) {
        *errorCode = 2130;
        *errorMessage = QStringLiteral("camera missing camera_id");
        return false;
    }

    const qint64 cameraId = jsonInteger(idValue);
    if (cameraId < 0 || cameraId >= MAX_CAMERA_COUNT) {
        *errorCode = 2131;
        *errorMessage = QStringLiteral("camera_id out of range");
        return false;
    }

    if (!camera.value(QStringLiteral("online")).isBool()) {
        *errorCode = 2132;
        *errorMessage = QStringLiteral("camera missing online");
        return false;
    }

    const bool online = camera.value(QStringLiteral("online")).toBool();
    const QJsonValue urlValue = camera.value(QStringLiteral("rtsp_url"));
    if (!urlValue.isString()) {
        *errorCode = 2133;
        *errorMessage = QStringLiteral("camera missing rtsp_url");
        return false;
    }
    if (online && urlValue.toString().isEmpty()) {
        *errorCode = 2134;
        *errorMessage = QStringLiteral("online camera has empty rtsp_url");
        return false;
    }

    const QStringList optionalIntFields = {
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("fps"),
        QStringLiteral("bitrate_kbps"),
    };
    for (const QString &field : optionalIntFields) {
        if (camera.contains(field) && !isIntegerJsonValue(camera.value(field))) {
            *errorCode = 2135;
            *errorMessage = QStringLiteral("camera field is not integer: %1").arg(field);
            return false;
        }
    }

    if (camera.contains(QStringLiteral("codec")) && !camera.value(QStringLiteral("codec")).isString()) {
        *errorCode = 2136;
        *errorMessage = QStringLiteral("camera codec is not string");
        return false;
    }

    return true;
}

void ROS1TcpClient::processCameraInfoMessage(const QJsonObject &message)
{
    const int cameraId = message["camera_id"].toInt();
    const QString rtspUrl = message["rtsp_url"].toString();
    const bool online = message["online"].toBool();
    const QString codec = message["codec"].toString(QStringLiteral("unknown"));
    const int width = message["width"].toInt();
    const int height = message["height"].toInt();
    const int fps = message["fps"].toInt();
    const int bitrateKbps = message["bitrate_kbps"].toInt();
    emit cameraInfoReceived(cameraId, rtspUrl, online, codec, width, height, fps, bitrateKbps);
}

void ROS1TcpClient::processCameraListResponse(const QJsonObject &message)
{
    emit systemStatusReceived(message);

    const QJsonArray cameras = message["cameras"].toArray();
    for (const QJsonValue &cameraValue : cameras) {
        processCameraInfoMessage(cameraValue.toObject());
    }
}

void ROS1TcpClient::requestBridgeState(const QString &reason)
{
    const bool syncSent = requestBridgeSync(reason);
    const bool cameraListSent = requestCameraList();

    QJsonObject event;
    event["type"] = "sync_request_sent";
    event["protocol_version"] = HostProtocol::ProtocolVersion;
    event["reason"] = reason;
    event["sync_sent"] = syncSent;
    event["camera_list_sent"] = cameraListSent;
    event["timestamp_ms"] = HostProtocol::nowMs();
    emit systemStatusReceived(event);
}

void ROS1TcpClient::emitProtocolError(qint64 seq, int code, const QString &message)
{
    m_stats.protocolErrorCount++;

    QJsonObject event;
    event["type"] = "protocol_error";
    event["protocol_version"] = HostProtocol::ProtocolVersion;
    event["seq"] = seq;
    event["code"] = code;
    event["message"] = message;
    event["source"] = QStringLiteral("upper_computer");
    event["timestamp_ms"] = HostProtocol::nowMs();
    emit systemStatusReceived(event);
    emitStatsUpdate();
}

void ROS1TcpClient::closeForProtocolError(qint64 seq, int code, const QString &message)
{
    emitProtocolError(seq, code, message);
    m_receivedData.clear();
    m_socket->abort();
}

void ROS1TcpClient::trimExpiredHeartbeats(qint64 nowMs)
{
    QList<quint64> expired;
    for (auto it = m_pendingHeartbeats.cbegin(); it != m_pendingHeartbeats.cend(); ++it) {
        if (nowMs - it.value() > HEARTBEAT_INTERVAL_MS * 3) {
            expired.append(it.key());
        }
    }

    for (quint64 seq : expired) {
        if (m_pendingHeartbeats.remove(seq)) {
            m_stats.heartbeatTimeoutCount++;
        }
    }
}

void ROS1TcpClient::processReceivedData()
{
    while (true) {
        int index = m_receivedData.indexOf('\n');
        if (index < 0) {
            if (m_receivedData.size() > MAX_FRAME_BYTES) {
                closeForProtocolError(0, 2001, QStringLiteral("TCP frame exceeds max length"));
            }
            break;
        }

        if (index > MAX_FRAME_BYTES) {
            closeForProtocolError(0, 2001, QStringLiteral("TCP frame exceeds max length"));
            break;
        }

        QByteArray line = m_receivedData.left(index);
        m_receivedData = m_receivedData.mid(index + 1);

        if (line.isEmpty()) continue;

        if (line.size() > MAX_FRAME_BYTES) {
            closeForProtocolError(0, 2001, QStringLiteral("TCP frame exceeds max length"));
            break;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);

        if (error.error != QJsonParseError::NoError) {
            HANDLE_ERROR_DETAIL(Utils::ErrorCode::ProtocolParseError, MODULE,
                                "JSON解析错误", QString("%1 | 原始数据: %2")
                                    .arg(error.errorString())
                                    .arg(QString::fromUtf8(line.left(200))));
            continue;
        }

        if (!doc.isObject()) {
            emitProtocolError(0, 2100, QStringLiteral("JSON frame is not an object"));
            continue;
        }

        QJsonObject msg = doc.object();
        emit rawMessageReceived(line);

        m_stats.messagesReceived++;
        const qint64 receivedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_lastMessageReceivedMs = receivedAtMs;

        QString schemaError;
        int schemaCode = 0;
        if (!validateIncomingMessage(msg, &schemaError, &schemaCode)) {
            emitProtocolError(msg["seq"].toVariant().toLongLong(), schemaCode, schemaError);
            continue;
        }

        QString msgType = msg["type"].toString();
        if (msgType == "heartbeat_ack") {
            handleHeartbeatAck(msg, receivedAtMs);
        } else if (msgType == "heartbeat") {
            m_lastHeartbeatAckMs = receivedAtMs;
            m_stats.lastHeartbeatAckMs = receivedAtMs;
            setHeartbeatOnline(true);
        } else if (msgType == "ack") {
            handleAckMessage(msg, receivedAtMs);
        } else if (msgType == "camera_list_response") {
            processCameraListResponse(msg);
        } else if (msgType == "system_snapshot" || msgType == "param_response"
                   || msgType == "emergency_state" || msgType == "protocol_error"
                   || msgType == "service_call_result"
                   || msgType == "hello" || msgType == "capabilities") {
            emit systemStatusReceived(msg);
        } else if (msgType == "motor_state") {
            MotorState state = parseMotorState(msg);
            emit motorStateReceived(state);
        } else if (msgType == "joint_runtime_states") {
            emit jointRuntimeStatesReceived(parseJointRuntimeStates(msg));
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
            processCameraInfoMessage(msg);
        } else {
            LOG_DEBUG(MODULE, QString("收到未处理的消息类型: %1").arg(msgType));
        }
    }

    emitStatsUpdate();
}

MotorState ROS1TcpClient::parseMotorState(const QJsonObject &json)
{
    MotorState state;

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

JointRuntimeStateList ROS1TcpClient::parseJointRuntimeStates(const QJsonObject &json)
{
    JointRuntimeStateList states;
    const QJsonArray statesArray = json.value(QStringLiteral("states")).toArray();
    states.reserve(statesArray.size());

    for (const QJsonValue &value : statesArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        JointRuntimeState state;
        state.jointName = object.value(QStringLiteral("joint_name")).toString();
        state.backend = object.value(QStringLiteral("backend")).toString();
        state.lifecycleState = object.value(QStringLiteral("lifecycle_state")).toString();
        state.online = object.value(QStringLiteral("online")).toBool(false);
        state.enabled = object.value(QStringLiteral("enabled")).toBool(false);
        state.fault = object.value(QStringLiteral("fault")).toBool(false);
        states.append(state);
    }

    return states;
}

void ROS1TcpClient::emitStatsUpdate()
{
    emit statsUpdated(getStats());
}

void ROS1TcpClient::setHeartbeatOnline(bool online)
{
    if (m_heartbeatOnline == online) {
        return;
    }

    m_heartbeatOnline = online;
    emit heartbeatChanged(online);
}

} // namespace Communication
