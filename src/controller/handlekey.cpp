#include "handlekey.h"
#include <QDebug>
#include <QtGlobal>
#include <cstring>

#ifdef USE_SDL3_GAMEPAD
#include <SDL3/SDL.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <xinput.h>
#endif

namespace {

#ifdef USE_SDL3_GAMEPAD
uint8_t sdlTriggerToByte(Sint16 value)
{
    const int clamped = qBound(0, static_cast<int>(value), 32767);
    return static_cast<uint8_t>(clamped * 255 / 32767);
}

int16_t invertSdlStickAxis(Sint16 value)
{
    if (value == -32768) {
        return 32767;
    }
    return static_cast<int16_t>(-value);
}
#endif

} // namespace

HandleKey::HandleKey(QObject *parent)
    : QObject{parent}
{
#ifdef USE_SDL3_GAMEPAD
    initSdl();
    openFirstSdlGamepad();
#endif

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &HandleKey::readGamepad);
    timer->start(20); // 50Hz 轮询
}

HandleKey::~HandleKey()
{
#ifdef USE_SDL3_GAMEPAD
    shutdownSdl();
#endif
}

void HandleKey::startPolling()
{
#ifdef USE_SDL3_GAMEPAD
    if (!m_sdlInitialized) {
        initSdl();
    }
#endif

    if (!timer->isActive()) {
        timer->start(20);
    }
}

void HandleKey::stopPolling()
{
    timer->stop();
    setConnected(false);
}

void HandleKey::readGamepad()
{
    ControllerState newState = {};
    bool readOk = false;

#ifdef USE_SDL3_GAMEPAD
    readOk = readSdlGamepad(newState);
#endif

#ifdef Q_OS_WIN
    if (!readOk) {
        readOk = readXInputGamepad(newState);
    }
#endif

    if (!readOk) {
        setConnected(false);
        return;
    }

    setConnected(true);
    publishStateIfChanged(newState);
}

void HandleKey::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }

    m_connected = connected;
    if (!connected) {
        state = {};
    }
    emit connectionChanged(connected);
}

void HandleKey::publishStateIfChanged(const ControllerState &newState)
{
    if (std::memcmp(&newState, &state, sizeof(ControllerState)) != 0) {
        state = newState;
        emit getHandleKey(state);
    }
}

#ifdef USE_SDL3_GAMEPAD
bool HandleKey::initSdl()
{
    if (m_sdlInitialized) {
        return true;
    }

#ifdef SDL_HINT_JOYSTICK_HIDAPI
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS4
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS5
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
#endif

    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        qWarning() << "SDL gamepad init failed:" << SDL_GetError();
        return false;
    }

    m_sdlInitialized = true;
    return true;
}

void HandleKey::shutdownSdl()
{
    closeSdlGamepad();
    if (m_sdlInitialized) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        m_sdlInitialized = false;
    }
}

bool HandleKey::openFirstSdlGamepad()
{
    if (!m_sdlInitialized && !initSdl()) {
        return false;
    }
    if (m_sdlGamepad) {
        return true;
    }

    SDL_UpdateGamepads();

    int gamepadCount = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&gamepadCount);
    if (!gamepads) {
        return false;
    }

    for (int i = 0; i < gamepadCount; ++i) {
        if (!SDL_IsGamepad(gamepads[i])) {
            continue;
        }

        m_sdlGamepad = SDL_OpenGamepad(gamepads[i]);
        if (m_sdlGamepad) {
            qDebug() << "SDL gamepad opened:" << SDL_GetGamepadName(m_sdlGamepad);
            SDL_free(gamepads);
            return true;
        }
    }

    SDL_free(gamepads);
    return false;
}

void HandleKey::closeSdlGamepad()
{
    if (m_sdlGamepad) {
        SDL_CloseGamepad(m_sdlGamepad);
        m_sdlGamepad = nullptr;
    }
}

bool HandleKey::readSdlGamepad(ControllerState &newState)
{
    if (!m_sdlGamepad && !openFirstSdlGamepad()) {
        return false;
    }

    if (!SDL_GamepadConnected(m_sdlGamepad)) {
        closeSdlGamepad();
        return false;
    }

    SDL_UpdateGamepads();

    newState.buttonA = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    newState.buttonB = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_EAST);
    newState.buttonX = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_WEST);
    newState.buttonY = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_NORTH);
    newState.buttonStart = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_START);
    newState.buttonBack = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_BACK);
    newState.dpadUp = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    newState.dpadDown = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    newState.dpadLeft = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    newState.dpadRight = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    newState.leftThumb = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    newState.rightThumb = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    newState.leftShoulder = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    newState.rightShoulder = SDL_GetGamepadButton(m_sdlGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    newState.sThumbLX = SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_LEFTX);
    newState.sThumbLY = invertSdlStickAxis(SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_LEFTY));
    newState.sThumbRX = SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    newState.sThumbRY = invertSdlStickAxis(SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_RIGHTY));
    newState.bLeftTrigger = sdlTriggerToByte(SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    newState.bRightTrigger = sdlTriggerToByte(SDL_GetGamepadAxis(m_sdlGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

    return true;
}
#endif

#ifdef Q_OS_WIN
bool HandleKey::readXInputGamepad(ControllerState &newState)
{
    XINPUT_STATE xinputState;
    ZeroMemory(&xinputState, sizeof(XINPUT_STATE));

    DWORD result = XInputGetState(0, &xinputState);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    const XINPUT_GAMEPAD &gp = xinputState.Gamepad;

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

    return true;
}
#endif
