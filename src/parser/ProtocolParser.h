#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <QByteArray>
#include "../communication/SharedStructs.h"

namespace Parser {

/**
 * @brief 负责 52 字节 USB CDC 帧的编解码
 *
 * 帧结构：Header(0xAA) + Version + Length + Payload(48B) + Checksum
 * - parse(): 校验头部与校验和，解包到 ESP32State
 * - encode(): 将 Command 写入 payload，并生成完整帧
 *
 * 用法示例：
 * @code
 * ProtocolParser parser;
 * uint8_t frame[ProtocolParser::kFrameLength];
 * std::size_t len = parser.encode(cmd, frame, sizeof(frame));
 * if (parser.parse(frame, len, &state)) { ... }
 * @endcode
 */
class ProtocolParser {
public:
    static constexpr std::size_t kFrameLength = 52;
    static constexpr std::size_t kJointCount = 6;

    ProtocolParser() = default;

    /**
     * @brief 解析完整帧为 ESP32State (接收来自ESP32的状态数据)
     * @param frame 输入帧指针
     * @param length 必须等于 kFrameLength
     * @param state 输出结构体
     * @return 成功返回 true
     */
    bool parse(const uint8_t *frame, std::size_t length, Communication::ESP32State *state) const;

    /**
     * @brief 将 Command 编码为完整帧
     * @param command 输入指令
     * @param frame 输出缓冲区
     * @param length 缓冲区长度，需 >= kFrameLength
     * @return 写入的字节数，失败返回 0
     */
    std::size_t encode(const Communication::Command &command, uint8_t *frame, std::size_t length) const;

    /**
     * @brief 检查数据帧是否有效（包头+版本+长度）
     * @param frame 输入帧指针
     * @param length 帧长度
     * @return 有效返回true
     */
    bool isValidFrame(const uint8_t *frame, std::size_t length) const;

    /**
     * @brief 从QByteArray解析数据
     * @param data 输入数据
     * @param state 输出状态 (ESP32State)
     * @return 成功返回true
     */
    bool parseFromQByteArray(const QByteArray &data, Communication::ESP32State *state) const;

    /**
     * @brief 将Command编码为QByteArray
     * @param command 输入命令
     * @return 编码后的字节数组
     */
    QByteArray encodeToQByteArray(const Communication::Command &command) const;

private:
    static constexpr std::size_t kHeaderSize = 3; // header, version, length
    static constexpr std::size_t kPayloadLength = kFrameLength - kHeaderSize - 1; // checksum byte
    static constexpr uint8_t kHeaderByte = 0xAA;
    static constexpr uint8_t kVersion = 0x01;

    uint8_t computeChecksum(const uint8_t *data, std::size_t length) const;
    bool validateChecksum(const uint8_t *frame, std::size_t length) const;
};

} // namespace Parser

#endif // PROTOCOL_PARSER_H