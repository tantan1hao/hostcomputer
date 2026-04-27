#include "KeyboardController.h"
#include <QDebug>
#include <algorithm>

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
        emit operatorInputChanged(pressedKeyNames());
        emit emergencyStopRequested();
        return;
    }

    if (!m_enabled) return;

    m_pressedKeys.insert(key);
    computeVelocity();
    emit operatorInputChanged(pressedKeyNames());
}

void KeyboardController::handleKeyRelease(QKeyEvent *event)
{
    if (!m_enabled || event->isAutoRepeat()) return;

    m_pressedKeys.remove(event->key());
    computeVelocity();
    emit operatorInputChanged(pressedKeyNames());
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
    emit operatorInputChanged(pressedKeyNames());

    // 保留旧信号兼容内部旧调用；主链路不再使用它下发cmd_vel。
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
        emit operatorInputChanged(pressedKeyNames());
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
    emit operatorInputChanged(pressedKeyNames());
    qDebug() << "[键盘] 控制模式切换:" << (mode == 0 ? "车体" : "机械臂");
}

int KeyboardController::controlMode() const { return m_controlMode; }

QStringList KeyboardController::pressedKeyNames() const
{
    QStringList keys;
    keys.reserve(m_pressedKeys.size());

    for (int key : m_pressedKeys) {
        const QString name = protocolKeyName(key);
        if (!name.isEmpty()) {
            keys.append(name);
        }
    }

    keys.removeDuplicates();
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

QString KeyboardController::protocolKeyName(int key) const
{
    switch (key) {
    case Qt::Key_W: return QStringLiteral("w");
    case Qt::Key_A: return QStringLiteral("a");
    case Qt::Key_S: return QStringLiteral("s");
    case Qt::Key_D: return QStringLiteral("d");
    case Qt::Key_Up: return QStringLiteral("up");
    case Qt::Key_Down: return QStringLiteral("down");
    case Qt::Key_Left: return QStringLiteral("left");
    case Qt::Key_Right: return QStringLiteral("right");
    case Qt::Key_Space: return QStringLiteral("space");
    case Qt::Key_Shift: return QStringLiteral("shift");
    case Qt::Key_Control: return QStringLiteral("ctrl");
    case Qt::Key_Alt: return QStringLiteral("alt");
    case Qt::Key_Tab: return QStringLiteral("tab");
    case Qt::Key_Return:
    case Qt::Key_Enter: return QStringLiteral("enter");
    case Qt::Key_Escape: return QStringLiteral("escape");
    default:
        break;
    }

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QStringLiteral("num_%1").arg(key - Qt::Key_0);
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QString(QChar(static_cast<char>('a' + key - Qt::Key_A)));
    }

    return QString();
}
