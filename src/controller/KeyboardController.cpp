#include "KeyboardController.h"
#include <QDebug>

KeyboardController::KeyboardController(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_linearSpeed(0.5f)
    , m_angularSpeed(1.0f)
    , m_linearX(0.0f)
    , m_angularZ(0.0f)
    , m_controlMode(0)
    , m_selectedJoint(0)
    , m_jointSpeed(0.1f)
{
    m_sendTimer = new QTimer(this);
    m_sendTimer->setInterval(SEND_INTERVAL_MS);
    connect(m_sendTimer, &QTimer::timeout, this, &KeyboardController::updateVelocity);
}

KeyboardController::~KeyboardController()
{
    m_sendTimer->stop();
}

void KeyboardController::handleKeyPress(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;

    int key = event->key();

    // 急停（无论是否启用都响应）
    if (key == Qt::Key_Space) {
        m_pressedKeys.clear();
        m_linearX = 0.0f;
        m_angularZ = 0.0f;
        emit velocityChanged(0.0f, 0.0f, 0.0f);
        emit emergencyStopRequested();
        return;
    }

    if (!m_enabled) return;

    if (m_controlMode == 2) {
        // === 机械臂模式 ===
        // 数字键1-6选择关节
        if (key >= Qt::Key_1 && key <= Qt::Key_6) {
            m_selectedJoint = key - Qt::Key_1;
            qDebug() << "[键盘-机械臂] 选中关节:" << m_selectedJoint;
            return;
        }
        // W/↑ 当前关节位置+
        if (key == Qt::Key_W || key == Qt::Key_Up) {
            emit jointControlRequested(m_selectedJoint, m_jointSpeed, m_jointSpeed);
            return;
        }
        // S/↓ 当前关节位置-
        if (key == Qt::Key_S || key == Qt::Key_Down) {
            emit jointControlRequested(m_selectedJoint, -m_jointSpeed, m_jointSpeed);
            return;
        }
        // A/← 执行器闭合
        if (key == Qt::Key_A || key == Qt::Key_Left) {
            emit executorControlRequested(-1.0f);
            return;
        }
        // D/→ 执行器张开
        if (key == Qt::Key_D || key == Qt::Key_Right) {
            emit executorControlRequested(1.0f);
            return;
        }
    } else {
        // === 车体模式 ===
        m_pressedKeys.insert(key);
        computeVelocity();
    }
}

void KeyboardController::handleKeyRelease(QKeyEvent *event)
{
    if (!m_enabled || event->isAutoRepeat()) return;

    // 机械臂模式下不处理释放事件
    if (m_controlMode == 2) return;

    m_pressedKeys.remove(event->key());
    computeVelocity();
}

void KeyboardController::computeVelocity()
{
    float lx = 0.0f;
    float az = 0.0f;

    // 前进 W / ↑
    if (m_pressedKeys.contains(Qt::Key_W) || m_pressedKeys.contains(Qt::Key_Up))
        lx += m_linearSpeed;

    // 后退 S / ↓
    if (m_pressedKeys.contains(Qt::Key_S) || m_pressedKeys.contains(Qt::Key_Down))
        lx -= m_linearSpeed;

    // 左转 A / ←
    if (m_pressedKeys.contains(Qt::Key_A) || m_pressedKeys.contains(Qt::Key_Left))
        az += m_angularSpeed;

    // 右转 D / →
    if (m_pressedKeys.contains(Qt::Key_D) || m_pressedKeys.contains(Qt::Key_Right))
        az -= m_angularSpeed;

    m_linearX = lx;
    m_angularZ = az;
}

void KeyboardController::updateVelocity()
{
    // 仅车体模式下周期发送速度
    if (m_controlMode == 0) {
        emit velocityChanged(m_linearX, 0.0f, m_angularZ);
    }
}

void KeyboardController::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;

    if (enabled) {
        m_sendTimer->start();
    } else {
        m_sendTimer->stop();
        m_pressedKeys.clear();
        m_linearX = 0.0f;
        m_angularZ = 0.0f;
        // 禁用时发送一次零速度确保停车
        emit velocityChanged(0.0f, 0.0f, 0.0f);
    }

    emit enabledChanged(enabled);
}

bool KeyboardController::isEnabled() const { return m_enabled; }

void KeyboardController::setLinearSpeed(float speed) { m_linearSpeed = speed; }
float KeyboardController::linearSpeed() const { return m_linearSpeed; }

void KeyboardController::setAngularSpeed(float speed) { m_angularSpeed = speed; }
float KeyboardController::angularSpeed() const { return m_angularSpeed; }

void KeyboardController::setControlMode(int mode)
{
    if (m_controlMode == mode) return;
    m_controlMode = mode;
    // 切换模式时清空按键状态，重置关节选择
    m_pressedKeys.clear();
    m_linearX = 0.0f;
    m_angularZ = 0.0f;
    m_selectedJoint = 0;
    qDebug() << "[键盘] 控制模式切换:" << (mode == 0 ? "车体" : "机械臂");
}

int KeyboardController::controlMode() const { return m_controlMode; }
