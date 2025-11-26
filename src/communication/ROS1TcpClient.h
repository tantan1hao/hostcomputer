#ifndef ROS1_TCP_CLIENT_H
#define ROS1_TCP_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include "src/parser/motor_state.h"

namespace Communication {

/**
 * @brief ROS1 TCP客户端 - 用于与Ubuntu ROS1节点通信
 *
 * 功能：
 * - 建立与ROS1节点的TCP连接
 * - 发送电机控制命令
 * - 接收6关节数据
 * - 自动重连机制
 * - JSON格式数据交换
 */
class ROS1TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit ROS1TcpClient(QObject *parent = nullptr);
    ~ROS1TcpClient();

    // 连接管理
    bool connectToROS(const QString &hostAddress, quint16 port = 9090);
    void disconnectFromROS();
    bool isConnected() const;

    // 数据发送
    bool sendMotorCommand(const Parser::MotorState &motorState);
    bool sendJointControl(int jointId, float position, float velocity = 0.0f);
    bool sendEmergencyStop();
    bool sendSystemCommand(const QString &command, const QJsonObject &params = QJsonObject());

    // 状态查询
    QString getConnectionStatus() const;
    QString getROSHost() const;
    quint16 getROSPort() const;

signals:
    // 连接状态信号
    void connectedToROS();
    void disconnectedFromROS();
    void connectionError(const QString &error);

    // 数据接收信号
    void motorStateReceived(const Parser::MotorState &motorState);
    void jointDataReceived(int jointId, float position, float current, float torque);
    void systemStatusReceived(const QJsonObject &status);
    void rawMessageReceived(const QByteArray &message);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleReadyRead();
    void handleError(QAbstractSocket::SocketError error);
    void checkConnection();

private:
    void setupConnection();
    bool sendMessage(const QJsonObject &message);
    void processReceivedData();
    Parser::MotorState parseMotorState(const QJsonObject &json);

private:
    QTcpSocket *m_socket;
    QString m_hostAddress;
    quint16 m_port;
    QByteArray m_receivedData;
    QTimer *m_heartbeatTimer;
    QTimer *m_reconnectTimer;

    bool m_isConnected;
    bool m_autoReconnect;
    int m_reconnectAttempts;
    static const int MAX_RECONNECT_ATTEMPTS = 5;
    static const int HEARTBEAT_INTERVAL_MS = 1000;
    static const int RECONNECT_INTERVAL_MS = 3000;
};

} // namespace Communication

#endif // ROS1_TCP_CLIENT_H