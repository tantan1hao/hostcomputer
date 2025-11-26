#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include <QByteArray>
#include <QString>

// 单个关节的状态数据
struct JointState {
    float position;          // 关节位置
    float current;           // 关节电流
    float executor_position; // 执行器位置
    float executor_torque;   // 执行器扭矩
    int executor_flags;      // 执行器标志
    int reserved;            // 保留字段

    JointState() :
        position(0.0f),
        current(0.0f),
        executor_position(0.0f),
        executor_torque(0.0f),
        executor_flags(0),
        reserved(0)
    {}
};

struct MotorState {
    // 系统状态
    bool isOnline;           // 在线状态
    bool isRunning;          // 运行状态
    bool hasError;           // 错误状态

    // 6个关节的数据
    JointState joints[6];    // 6个关节的状态数据

    // 系统参数
    float currentSpeed;      // 当前速度 (RPM)
    float targetSpeed;       // 目标速度 (RPM)
    float currentAngle;      // 当前角度 (度)
    float targetAngle;       // 目标角度 (度)

    // 电气参数
    float voltage;           // 电压 (V)
    float power;             // 功率 (W)
    float temperature;       // 温度 (度C)

    // 末端位置
    float positionX;         // X坐标
    float positionY;         // Y坐标
    float positionZ;         // Z坐标

    // 控制参数
    int controlMode;         // 控制模式 (0:位置, 1:速度, 2:力矩)
    int motorId;             // 电机ID

    // 错误信息
    uint32_t errorCode;      // ���误代码
    QString errorDescription; // 错误描述

    // 时间戳
    qint64 timestamp;        // 时间戳 (毫秒)

    // 原始数据
    QByteArray rawData;      // 原始字节数据

    // 构造函数
    MotorState() :
        isOnline(false),
        isRunning(false),
        hasError(false),
        currentSpeed(0.0f),
        targetSpeed(0.0f),
        currentAngle(0.0f),
        targetAngle(0.0f),
        voltage(0.0f),
        power(0.0f),
        temperature(0.0f),
        positionX(0.0f),
        positionY(0.0f),
        positionZ(0.0f),
        controlMode(0),
        motorId(0),
        errorCode(0),
        timestamp(0)
    {}

    // 重置
    void reset() {
        *this = MotorState();
    }

    // 状态描述
    QString getStatusDescription() const {
        if (hasError) return "错误";
        if (!isOnline) return "离线";
        if (isRunning) return "运行中";
        return "停止";
    }

    // 获取关节数据的格式化字符串
    QString getJointsDataString() const {
        QString result;
        result += QString("=== 6关节数据 ===\n");
        for (int i = 0; i < 6; ++i) {
            result += QString("关节 %1:\n").arg(i + 1);
            result += QString("  位置: %1\n").arg(joints[i].position, 0, 'f', 3);
            result += QString("  电流: %1 A\n").arg(joints[i].current, 0, 'f', 3);
            result += QString("  执行器位置: %1\n").arg(joints[i].executor_position, 0, 'f', 3);
            result += QString("  执行器扭矩: %1\n").arg(joints[i].executor_torque, 0, 'f', 3);
            result += QString("  执行器标志: %1\n").arg(joints[i].executor_flags);
            result += QString("  保留字段: %1\n").arg(joints[i].reserved);
            if (i < 5) result += "\n";
        }
        return result;
    }
};

#endif // MOTOR_STATE_H