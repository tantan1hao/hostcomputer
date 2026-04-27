#ifndef KEYBOARDCONTROLLER_H
#define KEYBOARDCONTROLLER_H

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QKeyEvent>
#include <QStringList>

/**
 * @brief 键盘输入驱动 - 采集协议层稳定按键名
 *
 * 车体模式键位映射:
 *   W/↑  - 前进
 *   S/↓  - 后退
 *   A/←  - 左转
 *   D/→  - 右转
 *   Space - 急停
 *
 * 机械臂模式键位映射:
 *   1-6  - 选择关节0-5
 *   W/↑  - 当前关节位置+
 *   S/↓  - 当前关节位置-
 *   A/←  - 执行器闭合
 *   D/→  - 执行器张开
 *   Space - 急停
 *
 * 支持多键同时按下(如 W+A = 前进+左转)
 * 松开所有键后发送空按下集合，由下位机 watchdog 和输入解析负责停车。
 */
class KeyboardController : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardController(QObject *parent = nullptr);
    ~KeyboardController();

    void handleKeyPress(QKeyEvent *event);
    void handleKeyRelease(QKeyEvent *event);

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setLinearSpeed(float speed);
    float linearSpeed() const;

    void setAngularSpeed(float speed);
    float angularSpeed() const;

    void setControlMode(int mode);
    int controlMode() const;

signals:
    void operatorInputChanged(const QStringList &pressedKeys);

    /// 速度命令信号: linearX=前后, angularZ=左右转
    void velocityChanged(float linearX, float linearY, float angularZ);
    void emergencyStopRequested();
    void enabledChanged(bool enabled);

    /// 机械臂模式信号
    void jointControlRequested(int jointId, float position, float velocity);
    void executorControlRequested(float value);

private slots:
    void updateVelocity();

private:
    void computeVelocity();
    QStringList pressedKeyNames() const;
    QString protocolKeyName(int key) const;

    QSet<int> m_pressedKeys;
    QTimer *m_sendTimer;

    bool m_enabled;
    float m_linearSpeed;   // 基础线速度 m/s
    float m_angularSpeed;  // 基础角速度 rad/s

    float m_linearX;       // 前进/后退
    float m_angularZ;      // 左转/右转

    int m_controlMode;     // 0=车体, 2=机械臂
    int m_selectedJoint;   // 当前选中的关节 (0-5)
    float m_jointSpeed;    // 关节控制步进速度

    static const int SEND_INTERVAL_MS = 100;  // 10Hz
};

#endif // KEYBOARDCONTROLLER_H
