#include "ProtocolParser.h"
#include <QByteArray>
#include <QDebug>

namespace Parser {

uint8_t ProtocolParser::computeChecksum(const uint8_t *data, std::size_t length) const
{
    uint32_t sum = 0;
    for (std::size_t i = 0; i < length; ++i) {
        sum += data[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

bool ProtocolParser::validateChecksum(const uint8_t *frame, std::size_t length) const
{
    if (length != kFrameLength) {
        return false;
    }

    const std::size_t checksumOffset = kFrameLength - 1;
    const uint8_t checksum = frame[checksumOffset];
    const uint8_t computed = computeChecksum(frame, checksumOffset);

    return checksum == computed;
}

bool ProtocolParser::isValidFrame(const uint8_t *frame, std::size_t length) const
{
    if (frame == nullptr || length != kFrameLength) {
        return false;
    }

    // 检查包头、版本、长度
    return (frame[0] == kHeaderByte &&
            frame[1] == kVersion &&
            frame[2] == static_cast<uint8_t>(kFrameLength));
}

bool ProtocolParser::parse(const uint8_t *frame, std::size_t length, Communication::ESP32State *state) const
{
    if (frame == nullptr || state == nullptr || length != kFrameLength) {
        qDebug() << "ProtocolParser: Invalid parameters for parse";
        return false;
    }

    // Header/Version/Length 检查，避免误解析随机数据
    if (!isValidFrame(frame, length)) {
        qDebug() << "ProtocolParser: Invalid frame header or length";
        return false;
    }

    // 校验和验证
    if (!validateChecksum(frame, length)) {
        qDebug() << "ProtocolParser: Checksum validation failed";
        return false;
    }

    // 指向 payload 的起始位置，后续依次读取整数字段
    const uint8_t *payload = frame + kHeaderSize;
    std::size_t offset = 0;

    auto readInt16 = [&](int16_t &dest) {
        if (offset + sizeof(int16_t) <= kPayloadLength) {
            std::memcpy(&dest, payload + offset, sizeof(int16_t));
            offset += sizeof(int16_t);
            return true;
        }
        return false;
    };

    // 读取关节数据
    for (auto &joint : state->joints) {
        if (!readInt16(joint.position)) {
            qDebug() << "ProtocolParser: Failed to read joint position";
            return false;
        }
        if (!readInt16(joint.current)) {
            qDebug() << "ProtocolParser: Failed to read joint current";
            return false;
        }
    }

    // 读取执行器数据
    if (!readInt16(state->executor_position)) {
        qDebug() << "ProtocolParser: Failed to read executor position";
        return false;
    }
    if (!readInt16(state->executor_torque)) {
        qDebug() << "ProtocolParser: Failed to read executor torque";
        return false;
    }

    // 读取标志位
    if (offset < kPayloadLength) {
        state->executor_flags = payload[offset++];
    } else {
        state->executor_flags = 0;
    }

    // 读取保留字节
    if (offset < kPayloadLength) {
        state->reserved = payload[offset++];
    } else {
        state->reserved = 0;
    }

    qDebug() << "ProtocolParser: Successfully parsed frame";
    return true;
}

std::size_t ProtocolParser::encode(const Communication::Command &command, uint8_t *frame, std::size_t length) const
{
    if (frame == nullptr || length < kFrameLength) {
        qDebug() << "ProtocolParser: Invalid buffer for encoding";
        return 0;
    }

    // 填充帧头
    frame[0] = kHeaderByte;
    frame[1] = kVersion;
    frame[2] = static_cast<uint8_t>(kFrameLength);

    uint8_t *payload = frame + kHeaderSize;
    std::size_t offset = 0;

    auto writeFloat = [&](float value) {
        if (offset + sizeof(float) <= kPayloadLength) {
            std::memcpy(payload + offset, &value, sizeof(float));
            offset += sizeof(float);
            return true;
        }
        return false;
    };

    // 写入IMU数据
    if (!writeFloat(command.imu_yaw) ||
        !writeFloat(command.imu_roll) ||
        !writeFloat(command.imu_pitch)) {
        qDebug() << "ProtocolParser: Failed to write IMU data";
        return 0;
    }

    // 写入4个摆臂电流
    for (int i = 0; i < 4; ++i) {
        if (!writeFloat(command.swing_arm_current[i])) {
            qDebug() << "ProtocolParser: Failed to write swing arm current";
            return 0;
        }
    }

    // 写入标志位
    if (offset < kPayloadLength) {
        payload[offset++] = command.command_flags;
    }
    if (offset < kPayloadLength) {
        payload[offset++] = command.reserved;
    }

    // 填充剩余空���为0
    if (offset < kPayloadLength) {
        std::memset(payload + offset, 0, kPayloadLength - offset);
    }

    // 计算并添加校验和
    const std::size_t checksumOffset = kFrameLength - 1;
    frame[checksumOffset] = computeChecksum(frame, checksumOffset);

    qDebug() << "ProtocolParser: Successfully encoded command frame";
    return kFrameLength;
}

bool ProtocolParser::parseFromQByteArray(const QByteArray &data, Communication::ESP32State *state) const
{
    if (data.size() != static_cast<int>(kFrameLength)) {
        qDebug() << "ProtocolParser: QByteArray size mismatch, expected:"
                 << kFrameLength << "got:" << data.size();
        return false;
    }

    return parse(reinterpret_cast<const uint8_t*>(data.constData()),
                 data.size(),
                 state);
}

QByteArray ProtocolParser::encodeToQByteArray(const Communication::Command &command) const
{
    QByteArray frame(kFrameLength, 0);

    std::size_t result = encode(command,
                              reinterpret_cast<uint8_t*>(frame.data()),
                              frame.size());

    if (result == kFrameLength) {
        return frame;
    } else {
        qDebug() << "ProtocolParser: Failed to encode command to QByteArray";
        return QByteArray();
    }
}

} // namespace Parser