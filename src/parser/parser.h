#ifndef PARSER_H
#define PARSER_H

#include <QObject>
#include <QByteArray>
#include <QVector>
#include "motor_state.h"

class Parser : public QObject
{
    Q_OBJECT

public:
    explicit Parser(QObject *parent = nullptr);
    ~Parser();

    // 数据解析
    bool parseByteStream(const QByteArray &byteStream);

    // 状态获取
    QVector<MotorState> getMotorStates() const;

    // 获取最后一个状态
    MotorState getLastMotorState() const;

    // 清理数据
    void clearStates();

    // 配置参数
    void setProtocolVersion(int version);
    void setExpectedMotorCount(int count);

    // 统计信息
    int getParsedPacketCount() const;
    int getErrorCount() const;

signals:
    // 状态更新
    void motorStateUpdated(const MotorState &state);
    void allMotorStatesUpdated(const QVector<MotorState> &states);

    // 错误信号
    void parseError(const QString &error);
    void protocolError(const QString &error);

private:
    // 解析功能
    bool parsePacket(const QByteArray &packet);
    MotorState parseMotorState(const QByteArray &data, int motorId);
    bool validatePacket(const QByteArray &packet);

    // 数据提取
    uint16_t extractUInt16(const QByteArray &data, int offset);
    uint32_t extractUInt32(const QByteArray &data, int offset);
    float extractFloat32(const QByteArray &data, int offset);
    int8_t extractInt8(const QByteArray &data, int offset);
    int16_t extractInt16(const QByteArray &data, int offset);
    int32_t extractInt32(const QByteArray &data, int offset);

    // 数据包处理
    QByteArray extractPacketsFromStream(const QByteArray &stream);
    int findPacketStart(const QByteArray &data, int startIndex);
    int findPacketEnd(const QByteArray &data, int startIndex);

private:
    QVector<MotorState> m_motorStates;
    QByteArray m_remainingBuffer;    // 剩余数据缓冲区

    // 配置参数
    int m_protocolVersion;
    int m_expectedMotorCount;

    // 统计信息
    int m_parsedPacketCount;
    int m_errorCount;

    // 协议常量
    static const uint8_t PACKET_HEADER[];
    static const uint8_t PACKET_FOOTER[];
    static const int MIN_PACKET_SIZE;
    static const int MAX_PACKET_SIZE;
};

#endif // PARSER_H