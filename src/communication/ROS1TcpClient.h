#ifndef ROS1_TCP_CLIENT_H
#define ROS1_TCP_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QHash>
#include "HostProtocol.h"
#include "SharedStructs.h"
#include "Logger.h"
#include "ErrorHandler.h"

namespace Communication {

/**
 * @brief 线程安全的ROS1 TCP客户端
 *
 * 运行在独立的Command线程中，与Intel NUC (ROS1)进行TCP+JSON通讯
 */
class ROS1TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit ROS1TcpClient(QObject *parent = nullptr);
    ~ROS1TcpClient();

    // 连接管理
    // 返回值表示“是否成功发起连接尝试”，不表示已经建立连接。
    // 连接成功/失败请以 connectedToROS / connectionError 信号为准。
    bool connectToROS(const QString &hostAddress, quint16 port = 9090);
    void disconnectFromROS();
    bool isConnected() const;

    // 数据发送（线程安全）
    bool sendMotorCommand(const MotorState &motorState);
    bool sendOperatorInput(const OperatorInputState &inputState);
    bool sendJointControl(int jointId, float position, float velocity = 0.0f);
    bool sendEmergencyStop(const QString &source);
    bool sendSystemCommand(const QString &command, const QJsonObject &params = QJsonObject());
    bool sendEndEffectorControl(float x, float y, float z, float roll, float pitch, float yaw);
    bool sendControlCommand(const Command &command);
    bool requestBridgeSync(const QString &reason);
    bool requestCameraList();

    // 状态查询
    QString getConnectionStatus() const;
    QString getROSHost() const;
    quint16 getROSPort() const;
    QTcpSocket* getSocket() const { return m_socket; }

    // 统计信息
    struct Stats {
        quint64 messagesSent;
        quint64 messagesReceived;
        quint64 bytesSent;
        quint64 bytesReceived;
        quint64 connectionCount;
        quint64 reconnectCount;
        quint64 ackPendingCount;
        quint64 ackReceivedCount;
        quint64 ackTimeoutCount;
        quint64 protocolErrorCount;
        quint64 heartbeatTimeoutCount;
        qint64 lastHeartbeatRttMs;
        qint64 lastHeartbeatAckMs;
    };
    Stats getStats() const;
    void resetStats();

signals:
    // 连接状态信号
    void connectedToROS();
    void disconnectedFromROS();
    void connectionError(const QString &error);

    // 数据接收信号
    void motorStateReceived(const MotorState &motorState);
    void jointRuntimeStatesReceived(const JointRuntimeStateList &states);
    void jointDataReceived(int jointId, float position, float current, float torque);
    void systemStatusReceived(const QJsonObject &status);
    void rawMessageReceived(const QByteArray &message);
    void co2DataReceived(float ppm);
    void imuDataReceived(float roll, float pitch, float yaw, float accelX, float accelY, float accelZ);
    void cameraInfoReceived(int cameraId, const QString &rtspUrl, bool online,
                            const QString &codec, int width, int height, int fps, int bitrateKbps);
    void heartbeatChanged(bool online);

    // 统计信息更新信号
    void statsUpdated(const Stats &stats);

public slots:
    // 供外部调用的槽（跨线程）
    void slotConnectToROS(const QString &hostAddress, quint16 port);
    void slotDisconnectFromROS();
    void slotSendMotorCommand(const MotorState &motorState);
    void slotSendJointControl(int jointId, float position, float velocity);
    void slotSendEmergencyStop();
    void slotSendSystemCommand(const QString &command, const QString &paramsJson);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleReadyRead();
    void handleError(QAbstractSocket::SocketError error);
    void checkConnection();
    void checkAckTimeouts();

private:
    struct PendingAck {
        quint64 seq = 0;
        QString ackType;
        qint64 sentAtMs = 0;
        qint64 deadlineMs = 0;
    };

    void setupConnection();
    bool sendMessage(const QJsonObject &message);
    bool sendTrackedMessage(const QJsonObject &message, const QString &ackType);
    quint64 nextSequence();
    void registerPendingAck(const QJsonObject &message, const QString &ackType);
    void handleAckMessage(const QJsonObject &message, qint64 receivedAtMs);
    void handleHeartbeatAck(const QJsonObject &message, qint64 receivedAtMs);
    void failPendingAck(const PendingAck &pending, const QString &eventType,
                        const QString &message, int code, qint64 nowMs);
    void failAllPendingAcks(const QString &eventType, const QString &message, int code);
    bool validateIncomingMessage(const QJsonObject &message, QString *errorMessage, int *errorCode) const;
    bool validateCameraObject(const QJsonObject &camera, QString *errorMessage, int *errorCode) const;
    void processCameraInfoMessage(const QJsonObject &message);
    void processCameraListResponse(const QJsonObject &message);
    void requestBridgeState(const QString &reason);
    void emitProtocolError(qint64 seq, int code, const QString &message);
    void closeForProtocolError(qint64 seq, int code, const QString &message);
    void trimExpiredHeartbeats(qint64 nowMs);
    void processReceivedData();
    MotorState parseMotorState(const QJsonObject &json);
    JointRuntimeStateList parseJointRuntimeStates(const QJsonObject &json);
    void setHeartbeatOnline(bool online);
    void emitStatsUpdate();

private:
    QTcpSocket *m_socket;
    QString m_hostAddress;
    quint16 m_port;
    QByteArray m_receivedData;
    QTimer *m_heartbeatTimer;
    QTimer *m_reconnectTimer;
    QTimer *m_ackTimer;

    bool m_isConnected;
    bool m_autoReconnect;
    bool m_manualDisconnectRequested;
    bool m_heartbeatOnline;
    int m_reconnectAttempts;
    qint64 m_lastMessageReceivedMs;
    qint64 m_lastHeartbeatAckMs;
    quint64 m_nextSequence;
    QHash<quint64, PendingAck> m_pendingAcks;
    QHash<quint64, qint64> m_pendingHeartbeats;

    // 统计信息
    Stats m_stats;

    // 静态常量
    static const int MAX_RECONNECT_ATTEMPTS = 5;
    static const int HEARTBEAT_INTERVAL_MS = 1000;
    static const int RECONNECT_INTERVAL_MS = 3000;
    static const int ACK_TIMEOUT_MS = 2000;
    static const int ACK_CHECK_INTERVAL_MS = 100;
    static const int MAX_FRAME_BYTES = 1024 * 1024;
    static const int MAX_CAMERA_COUNT = 5;
};

} // namespace Communication

#endif // ROS1_TCP_CLIENT_H
