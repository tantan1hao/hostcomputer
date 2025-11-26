#include "parser.h"
#include <QDebug>
#include <QDateTime>
#include <cstring>

// 协议常量定义
const uint8_t Parser::PACKET_HEADER[] = {0xAA, 0x55};     // 包头
const uint8_t Parser::PACKET_FOOTER[] = {0x0D, 0x0A};     // 包尾
const int Parser::MIN_PACKET_SIZE = 10;                    // 最小包大小
const int Parser::MAX_PACKET_SIZE = 1024;                  // 最大包大小

Parser::Parser(QObject *parent)
    : QObject(parent)
    , m_protocolVersion(1)
    , m_expectedMotorCount(1)
    , m_parsedPacketCount(0)
    , m_errorCount(0)
{
    // 初始化电机状态数组
    m_motorStates.resize(m_expectedMotorCount);
}

Parser::~Parser()
{
}

bool Parser::parseByteStream(const QByteArray &byteStream)
{
    if (byteStream.isEmpty()) {
        return false;
    }

    qDebug() << "Parser: 接收数据长度:" << byteStream.length()
             << "数据:" << byteStream.toHex();

    // 将新数据添加到缓冲区
    m_remainingBuffer.append(byteStream);

    // 从缓冲区中提取完整的数据包
    QByteArray packets = extractPacketsFromStream(m_remainingBuffer);

    if (packets.isEmpty()) {
        return false;
    }

    // 解析每个数据包
    bool success = true;
    int packetCount = 0;

    for (int i = 0; i < packets.length(); ) {
        int packetStart = findPacketStart(packets, i);
        if (packetStart == -1) break;

        int packetEnd = findPacketEnd(packets, packetStart);
        if (packetEnd == -1) break;

        // 提取单个数据包
        QByteArray packet = packets.mid(packetStart, packetEnd - packetStart + 1);

        if (parsePacket(packet)) {
            packetCount++;
            m_parsedPacketCount++;
        } else {
            success = false;
            m_errorCount++;
        }

        i = packetEnd + 1;
    }

    qDebug() << "Parser: 解析完成，包数量:" << packetCount << "成功:" << success;

    // 如果有有效的电机状态数据，发送更新信号
    if (!m_motorStates.isEmpty()) {
        emit motorStateUpdated(m_motorStates.last());
        emit allMotorStatesUpdated(m_motorStates);
    }

    return success;
}

QVector<MotorState> Parser::getMotorStates() const
{
    return m_motorStates;
}

MotorState Parser::getLastMotorState() const
{
    if (m_motorStates.isEmpty()) {
        return MotorState();
    }
    return m_motorStates.last();
}

void Parser::clearStates()
{
    m_motorStates.clear();
    m_remainingBuffer.clear();
    m_parsedPacketCount = 0;
    m_errorCount = 0;
}

void Parser::setProtocolVersion(int version)
{
    m_protocolVersion = version;
}

void Parser::setExpectedMotorCount(int count)
{
    m_expectedMotorCount = count;
    m_motorStates.resize(count);
}

int Parser::getParsedPacketCount() const
{
    return m_parsedPacketCount;
}

int Parser::getErrorCount() const
{
    return m_errorCount;
}

bool Parser::parsePacket(const QByteArray &packet)
{
    try {
        // 检查包格式
        // [0xAA][0x55][长度][电机ID][数据...][校验和][0x0D][0x0A]

        if (packet.length() < 8) {
            emit parseError("包长度太短");
            return false;
        }

        // 检查包头
        if (packet[0] != PACKET_HEADER[0] || packet[1] != PACKET_HEADER[1]) {
            emit parseError("包头错误");
            return false;
        }

        // 检查包尾
        int lastPos = packet.length() - 1;
        int secondLastPos = packet.length() - 2;
        if (packet[lastPos] != PACKET_FOOTER[1] ||
            packet[secondLastPos] != PACKET_FOOTER[0]) {
            emit parseError("包尾错误");
            return false;
        }

        // 提取电机ID (假设在位置2)
        int motorId = static_cast<uint8_t>(packet[2]);

        // 提取数据部分
        QByteArray data = packet.mid(3, packet.length() - 6); // 减去包头、包尾、校验和

        // 解析电机状态
        MotorState state = parseMotorState(data, motorId);

        // 更新电机状态数组
        if (motorId >= 0 && motorId < m_motorStates.size()) {
            m_motorStates[motorId] = state;
        }

        return true;

    } catch (const std::exception &e) {
        emit parseError(QString("解析异常: %1").arg(e.what()));
        return false;
    }
}

MotorState Parser::parseMotorState(const QByteArray &data, int motorId)
{
    MotorState state;
    state.motorId = motorId;
    state.timestamp = QDateTime::currentMSecsSinceEpoch();
    state.rawData = data;

    // 根据数据长度解析不同类型的状态数据
    if (data.length() >= 4) {
        // 假设数据格式: [速度低字节][速度高字节][角度低字节][角度高字节]
        state.currentSpeed = extractInt16(data, 0);
        state.currentAngle = extractInt16(data, 2);
        state.isOnline = true;
        state.isRunning = (state.currentSpeed != 0);
    }

    if (data.length() >= 8) {
        // 扩展数据: 总电流、电压
        float totalCurrent = extractInt16(data, 4) / 100.0f;  // 假设放大100倍
        state.voltage = extractInt16(data, 6) / 100.0f;

        // 将总电流分配给所有关节（平均分配）
        for (int i = 0; i < 6; ++i) {
            state.joints[i].current = totalCurrent / 6.0f;
        }
    }

    if (data.length() >= 12) {
        // 更多数据: 温度、功率
        state.temperature = extractInt16(data, 8) / 10.0f;   // 假设放大10倍

        // 计算功率
        float totalCurrent = 0.0f;
        for (int i = 0; i < 6; ++i) {
            totalCurrent += state.joints[i].current;
        }
        state.power = totalCurrent * state.voltage;
    }

    return state;
}

bool Parser::validatePacket(const QByteArray &packet)
{
    if (packet.length() < MIN_PACKET_SIZE || packet.length() > MAX_PACKET_SIZE) {
        return false;
    }

    // 检查包头
    if (packet[0] != PACKET_HEADER[0] || packet[1] != PACKET_HEADER[1]) {
        return false;
    }

    // 检查包尾
    int lastPos = packet.length() - 1;
    int secondLastPos = packet.length() - 2;
    if (packet[lastPos] != PACKET_FOOTER[1] ||
        packet[secondLastPos] != PACKET_FOOTER[0]) {
        return false;
    }

    // 可以添加校验和验证
    return true;
}

uint16_t Parser::extractUInt16(const QByteArray &data, int offset)
{
    if (offset + 1 >= data.length()) return 0;

    return (static_cast<uint8_t>(data[offset]) |
            (static_cast<uint8_t>(data[offset + 1]) << 8));
}

uint32_t Parser::extractUInt32(const QByteArray &data, int offset)
{
    if (offset + 3 >= data.length()) return 0;

    return (static_cast<uint8_t>(data[offset]) |
            (static_cast<uint8_t>(data[offset + 1]) << 8) |
            (static_cast<uint8_t>(data[offset + 2]) << 16) |
            (static_cast<uint8_t>(data[offset + 3]) << 24));
}

float Parser::extractFloat32(const QByteArray &data, int offset)
{
    if (offset + 3 >= data.length()) return 0.0f;

    uint32_t intValue = extractUInt32(data, offset);
    float floatValue;
    std::memcpy(&floatValue, &intValue, sizeof(floatValue));
    return floatValue;
}

int8_t Parser::extractInt8(const QByteArray &data, int offset)
{
    if (offset >= data.length()) return 0;
    return static_cast<int8_t>(data[offset]);
}

int16_t Parser::extractInt16(const QByteArray &data, int offset)
{
    return static_cast<int16_t>(extractUInt16(data, offset));
}

int32_t Parser::extractInt32(const QByteArray &data, int offset)
{
    return static_cast<int32_t>(extractUInt32(data, offset));
}

QByteArray Parser::extractPacketsFromStream(const QByteArray &stream)
{
    QByteArray packets;
    int i = 0;

    while (i < stream.length()) {
        int packetStart = findPacketStart(stream, i);
        if (packetStart == -1) {
            break;
        }

        int packetEnd = findPacketEnd(stream, packetStart);
        if (packetEnd == -1) {
            // 包不完整，保留在缓冲区
            m_remainingBuffer = stream.mid(packetStart);
            break;
        }

        // 添加完整的数据包
        packets.append(stream.mid(packetStart, packetEnd - packetStart + 1));
        i = packetEnd + 1;
    }

    // 清理已处理的数据
    if (i > 0) {
        m_remainingBuffer = stream.mid(i);
    }

    return packets;
}

int Parser::findPacketStart(const QByteArray &data, int startIndex)
{
    for (int i = startIndex; i < data.length() - 1; i++) {
        if (data[i] == PACKET_HEADER[0] &&
            (i + 1 < data.length() && data[i + 1] == PACKET_HEADER[1])) {
            return i;
        }
    }
    return -1;
}

int Parser::findPacketEnd(const QByteArray &data, int startIndex)
{
    for (int i = startIndex; i < data.length() - 1; i++) {
        if (data[i] == PACKET_FOOTER[0] &&
            (i + 1 < data.length() && data[i + 1] == PACKET_FOOTER[1])) {
            return i + 1;  // 返回包尾的最后一个字节位置
        }
    }
    return -1;
}