#include "controller.h"
#include "ROS1TcpClient.h"
#include <QDebug>

Controller::Controller(QObject *parent)
    : QObject(parent)
    , m_tcpClient(nullptr)
    , m_initialized(false)
    , m_running(false)
{
}

Controller::~Controller()
{
    stop();

    delete m_tcpClient;
}

bool Controller::initialize()
{
    if (m_initialized) {
        return true;
    }

    // 初始化TCP客户端
    m_tcpClient = new Communication::ROS1TcpClient(this);

    // 设置信号连接
    setupConnections();

    m_initialized = true;
    qDebug() << "Controller initialized successfully";
    return true;
}

void Controller::start()
{
    if (!m_initialized) {
        qWarning() << "Controller not initialized";
        return;
    }

    m_running = true;
    qDebug() << "Controller started";
}

void Controller::stop()
{
    m_running = false;

    disconnectTcp();

    qDebug() << "Controller stopped";
}

// ==================== TCP通信接口 ====================

bool Controller::connectTcp(const QString &host, quint16 port)
{
    if (!m_tcpClient) {
        qCritical() << "ROS1TcpClient not initialized";
        return false;
    }

    m_tcpClient->connectToROS(host, port);
    return true;
}

void Controller::disconnectTcp()
{
    if (m_tcpClient && m_tcpClient->isConnected()) {
        m_tcpClient->disconnectFromROS();
    }
}

bool Controller::isTcpConnected() const
{
    return m_tcpClient && m_tcpClient->isConnected();
}

// ==================== 命令发送接口 ====================

bool Controller::sendMotorCommand(const Communication::MotorState &state)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send motor command";
        return false;
    }

    return m_tcpClient->sendMotorCommand(state);
}

bool Controller::sendOperatorInput(const Communication::OperatorInputState &inputState)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send operator input";
        return false;
    }

    return m_tcpClient->sendOperatorInput(inputState);
}

bool Controller::sendControlCommand(const Communication::Command &command)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send control command";
        return false;
    }

    return m_tcpClient->sendControlCommand(command);
}

bool Controller::sendJointControl(int jointId, float position, float velocity)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send joint control";
        return false;
    }

    return m_tcpClient->sendJointControl(jointId, position, velocity);
}

bool Controller::sendEmergencyStop(const QString &source)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send emergency stop";
        return false;
    }

    return m_tcpClient->sendEmergencyStop(source);
}

bool Controller::sendSystemCommand(const QString &command, const QJsonObject &params)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send system command";
        return false;
    }

    return m_tcpClient->sendSystemCommand(command, params);
}

bool Controller::sendEndEffectorControl(float x, float y, float z, float roll, float pitch, float yaw)
{
    if (!m_tcpClient || !m_tcpClient->isConnected()) {
        qWarning() << "TCP not connected, cannot send end effector control";
        return false;
    }

    return m_tcpClient->sendEndEffectorControl(x, y, z, roll, pitch, yaw);
}

// ==================== TCP连接管理（供UI层对话框使用） ====================

bool Controller::connectToROS(const QString &host, quint16 port)
{
    if (!m_tcpClient) {
        qCritical() << "ROS1TcpClient not initialized";
        return false;
    }

    return m_tcpClient->connectToROS(host, port);
}

void Controller::disconnectFromROS()
{
    if (m_tcpClient) {
        m_tcpClient->disconnectFromROS();
    }
}

QString Controller::getROSHost() const
{
    if (m_tcpClient) {
        return m_tcpClient->getROSHost();
    }
    return QString();
}

quint16 Controller::getROSPort() const
{
    if (m_tcpClient) {
        return m_tcpClient->getROSPort();
    }
    return 0;
}

// ==================== 统计信息 ====================

Controller::Statistics Controller::getTcpStatistics() const
{
    Statistics stats = {};
    stats.lastHeartbeatRttMs = -1;

    if (m_tcpClient) {
        auto tcpStats = m_tcpClient->getStats();
        stats.messagesSent = tcpStats.messagesSent;
        stats.messagesReceived = tcpStats.messagesReceived;
        stats.bytesSent = tcpStats.bytesSent;
        stats.bytesReceived = tcpStats.bytesReceived;
        stats.connectionCount = tcpStats.connectionCount;
        stats.reconnectCount = tcpStats.reconnectCount;
        stats.ackPendingCount = tcpStats.ackPendingCount;
        stats.ackReceivedCount = tcpStats.ackReceivedCount;
        stats.ackTimeoutCount = tcpStats.ackTimeoutCount;
        stats.protocolErrorCount = tcpStats.protocolErrorCount;
        stats.heartbeatTimeoutCount = tcpStats.heartbeatTimeoutCount;
        stats.lastHeartbeatRttMs = tcpStats.lastHeartbeatRttMs;
        stats.lastHeartbeatAckMs = tcpStats.lastHeartbeatAckMs;
    }

    return stats;
}

// ==================== 内部方法 ====================

void Controller::setupConnections()
{
    // TCP信号连接
    if (m_tcpClient) {
        connect(m_tcpClient, &Communication::ROS1TcpClient::connectedToROS,
                this, &Controller::onTcpConnected);
        connect(m_tcpClient, &Communication::ROS1TcpClient::disconnectedFromROS,
                this, &Controller::onTcpDisconnected);
        connect(m_tcpClient, &Communication::ROS1TcpClient::connectionError,
                this, &Controller::onTcpError);
        connect(m_tcpClient, &Communication::ROS1TcpClient::heartbeatChanged,
                this, &Controller::tcpHeartbeatChanged);
        connect(m_tcpClient, &Communication::ROS1TcpClient::motorStateReceived,
                this, &Controller::onTcpMotorStateReceived);
        connect(m_tcpClient, &Communication::ROS1TcpClient::co2DataReceived,
                this, &Controller::onTcpCO2DataReceived);
        connect(m_tcpClient, &Communication::ROS1TcpClient::imuDataReceived,
                this, &Controller::onTcpIMUDataReceived);
        connect(m_tcpClient, &Communication::ROS1TcpClient::cameraInfoReceived,
                this, &Controller::onTcpCameraInfoReceived);
        connect(m_tcpClient, &Communication::ROS1TcpClient::systemStatusReceived,
                this, &Controller::onTcpSystemStatusReceived);
    }

    qDebug() << "Signal connections established";
}

void Controller::onTcpConnected()
{
    qDebug() << "TCP connected";
    emit tcpConnected();
}

void Controller::onTcpDisconnected()
{
    qDebug() << "TCP disconnected";
    emit tcpDisconnected();
}

void Controller::onTcpError(const QString &error)
{
    qCritical() << QString("TCP error: %1").arg(error);
    emit tcpError(error);
}

void Controller::onTcpMotorStateReceived(const Communication::MotorState &state)
{
    emit motorStateReceived(state);
}

void Controller::onTcpCO2DataReceived(float ppm)
{
    emit co2DataReceived(ppm);
}

void Controller::onTcpIMUDataReceived(float roll, float pitch, float yaw,
                                      float accelX, float accelY, float accelZ)
{
    emit imuDataReceived(roll, pitch, yaw, accelX, accelY, accelZ);
}

void Controller::onTcpCameraInfoReceived(int cameraId, const QString &rtspUrl, bool online,
                                         const QString &codec, int width, int height, int fps, int bitrate)
{
    emit cameraInfoReceived(cameraId, online, codec, width, height, fps, bitrate, rtspUrl);
}

void Controller::onTcpSystemStatusReceived(const QJsonObject &status)
{
    emit protocolMessageReceived(status);
}

void Controller::handleError(const QString &error)
{
    qCritical() << error;
    emit systemError(error);
}
