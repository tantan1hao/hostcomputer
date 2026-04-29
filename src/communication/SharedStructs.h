#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#include <cstdint>
#include <array>
#include <QString>
#include <QVector>

namespace Communication {

/**
 * @brief 单个关节状态信息
 */
struct JointState {
    int16_t position = 0;  // 关节位置
    int16_t current = 0;   // 关节电流
};

/**
 * @brief 下位机发送给PC的电机状态数据 (通过TCP JSON接收)
 */
struct MotorState {
    static constexpr size_t kJointCount = 6;      // 6个关节

    std::array<JointState, kJointCount> joints{}; // 关节状态数组
    int16_t executor_position = 0;                // 执行器位置
    int16_t executor_torque = 0;                  // 执行器扭矩
    uint8_t executor_flags = 0;                   // 执行器标志位
    uint8_t reserved = 0;                         // 保留字节
};

struct JointRuntimeState {
    QString jointName;
    QString backend;
    QString lifecycleState;
    bool online = false;
    bool enabled = false;
    bool fault = false;
};

using JointRuntimeStateList = QVector<JointRuntimeState>;

/**
 * @brief PC发送给下位机的控制命令 (通过TCP JSON发送)
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
