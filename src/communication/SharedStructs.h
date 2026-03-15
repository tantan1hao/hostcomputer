#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#include <cstdint>
#include <array>

namespace Communication {

/**
 * @brief MJPEG帧类型
 */
enum class MjpegFrameType : uint8_t {
    Invalid = 0x00,
    Header = 0x01,
    Data = 0x02,
    End = 0x03,
    Heartbeat = 0x04
};

/**
 * @brief MJPEG视频帧头结构
 */
#pragma pack(push, 1)
struct MjpegFrameHeader {
    uint8_t  magic;           // 魔数标识 0xAA
    uint8_t  frameType;       // 帧类型 MjpegFrameType
    uint16_t frameId;         // 帧序号
    uint16_t packetIndex;     // 当前分片序号 (从0开始)
    uint16_t totalPackets;    // 总分片数
    uint32_t payloadSize;     // 完整JPEG数据大小
    uint32_t timestamp;       // 时间戳(毫秒)
    uint8_t  cameraId;        // 摄像头ID (0-5)
    uint8_t  reserved[3];     // 保��字段
};
#pragma pack(pop)

/**
 * @brief UDP视频流数据包信息
 */
struct UdpVideoPacket {
    int sequence;         // 序列号
    int totalPackets;     // 总包数（用于分片传输）
    int packetIndex;      // 当前包索引
    int payloadSize;      // 载荷大小
    bool isComplete;      // 是否为完整帧
};

/**
 * @brief 单个关节状态信息 (与ESP32端JointTelemetry对应)
 */
struct JointState {
    int16_t position;      // 关节位置
    int16_t current;       // 关节电流
};

/**
 * @brief ESP32发送给PC的系统状态数据 (与ESP32端SystemState对应)
 *
 * 注意：ESP32端定义此结构体为 PC -> ESP32，但实际使用中ESP32发送此状态给PC
 * PC端使用此结构体接收和解析来自ESP32的状态数据
 */
struct ESP32State {
    static constexpr size_t kJointCount = 6;      // 6个关节 (对应ESP32端JOINT_NUM)

    std::array<JointState, kJointCount> joints;   // 关节状态数组
    int16_t executor_position;                    // 执行器位置
    int16_t executor_torque;                      // 执行器扭矩
    uint8_t executor_flags;                       // 执行器标志位
    uint8_t reserved;                            // 保留字节
};

/**
 * @brief PC发送给下位机的控制命令 (通过TCP JSON发送)
 *
 * PC端使用此结构体创建控制命令并发送给Intel NUC (ROS1)
 */
struct Command {
    static constexpr size_t kSwingArmCount = 4;   // 4个摆臂

    // IMU数据
    float imu_yaw;                                 // IMU偏航角 (度)
    float imu_roll;                                // IMU横滚角 (度)
    float imu_pitch;                               // IMU俯仰角 (度)

    // 4个摆臂电流
    std::array<float, kSwingArmCount> swing_arm_current;  // 摆臂电流 (A)

    // 机械臂末端位置 (笛卡尔空间)
    float arm_end_x;                               // 末端X位置 (m)
    float arm_end_y;                               // 末端Y位置 (m)
    float arm_end_z;                               // 末端Z位置 (m)
    float arm_end_roll;                            // 末端横滚角 (度)
    float arm_end_pitch;                           // 末端俯仰角 (度)
    float arm_end_yaw;                             // 末端偏航角 (度)

    uint8_t command_flags;                         // 命令标志位
    uint8_t reserved;                              // 保留字节

    Command() {
        // 初始化为默认值
        imu_yaw = 0.0f;
        imu_roll = 0.0f;
        imu_pitch = 0.0f;
        swing_arm_current.fill(0.0f);
        arm_end_x = 0.0f;
        arm_end_y = 0.0f;
        arm_end_z = 0.0f;
        arm_end_roll = 0.0f;
        arm_end_pitch = 0.0f;
        arm_end_yaw = 0.0f;
        command_flags = 0;
        reserved = 0;
    }
};

} // namespace Communication

#endif // SHARED_STRUCTS_H
