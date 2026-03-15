#include "handlekey.h"
#include <QDebug>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <xinput.h>
#endif

HandleKey::HandleKey(QObject *parent)
    : QObject{parent}
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &HandleKey::readGamepad);
    timer->start(20); // 50Hz 轮询
}

void HandleKey::startPolling()
{
    if (!timer->isActive()) {
        timer->start(20);
    }
}

void HandleKey::stopPolling()
{
    timer->stop();
    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
    }
}

void HandleKey::readGamepad()
{
#ifdef Q_OS_WIN
    XINPUT_STATE xinputState;
    ZeroMemory(&xinputState, sizeof(XINPUT_STATE));

    DWORD result = XInputGetState(0, &xinputState);
    if (result != ERROR_SUCCESS) {
        if (m_connected) {
            m_connected = false;
            emit connectionChanged(false);
        }
        return; // 手柄未连接
    }

    if (!m_connected) {
        m_connected = true;
        emit connectionChanged(true);
    }

    const XINPUT_GAMEPAD &gp = xinputState.Gamepad;

    ControllerState newState = {};

    // 按钮
    newState.buttonA        = (gp.wButtons & XINPUT_GAMEPAD_A) != 0;
    newState.buttonB        = (gp.wButtons & XINPUT_GAMEPAD_B) != 0;
    newState.buttonX        = (gp.wButtons & XINPUT_GAMEPAD_X) != 0;
    newState.buttonY        = (gp.wButtons & XINPUT_GAMEPAD_Y) != 0;
    newState.buttonStart    = (gp.wButtons & XINPUT_GAMEPAD_START) != 0;
    newState.buttonBack     = (gp.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    newState.dpadUp         = (gp.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    newState.dpadDown       = (gp.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    newState.dpadLeft       = (gp.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    newState.dpadRight      = (gp.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    newState.leftThumb      = (gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
    newState.rightThumb     = (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
    newState.leftShoulder   = (gp.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    newState.rightShoulder  = (gp.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;

    // 摇杆
    newState.sThumbLX = gp.sThumbLX;
    newState.sThumbLY = gp.sThumbLY;
    newState.sThumbRX = gp.sThumbRX;
    newState.sThumbRY = gp.sThumbRY;

    // 扳机
    newState.bLeftTrigger  = gp.bLeftTrigger;
    newState.bRightTrigger = gp.bRightTrigger;

    // modeIndex: 用DPad方向映射模式 (上=0, 右=1, 下=2, 左=3)
    newState.modeIndex = state.modeIndex; // 保持上次模式
    if (newState.dpadUp)         newState.modeIndex = 0;
    else if (newState.dpadRight) newState.modeIndex = 1;
    else if (newState.dpadDown)  newState.modeIndex = 2;
    else if (newState.dpadLeft)  newState.modeIndex = 3;

    // 只在状态变化时才发送信号，避免刷爆UI
    if (std::memcmp(&newState, &state, sizeof(ControllerState)) != 0) {
        state = newState;
        emit getHandleKey(state);
    }
#endif
}
