#include "HostProtocol.h"

#include <QDateTime>
#include <QJsonArray>

namespace Communication {

qint64 HostProtocol::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QJsonObject HostProtocol::makeOperatorInput(const OperatorInputState &state,
                                            quint64 seq,
                                            qint64 timestampMs)
{
    QJsonObject message = makeEnvelope(QStringLiteral("operator_input"), seq, timestampMs);
    message[QStringLiteral("ttl_ms")] = state.ttlMs;
    message[QStringLiteral("mode")] = state.mode;

    QJsonArray pressedKeys;
    for (const QString &key : state.keyboard.pressedKeys) {
        pressedKeys.append(key);
    }

    QJsonObject keyboard;
    keyboard[QStringLiteral("pressed_keys")] = pressedKeys;
    message[QStringLiteral("keyboard")] = keyboard;

    QJsonObject buttons;
    buttons[QStringLiteral("a")] = state.gamepad.buttons.a;
    buttons[QStringLiteral("b")] = state.gamepad.buttons.b;
    buttons[QStringLiteral("x")] = state.gamepad.buttons.x;
    buttons[QStringLiteral("y")] = state.gamepad.buttons.y;
    buttons[QStringLiteral("start")] = state.gamepad.buttons.start;
    buttons[QStringLiteral("back")] = state.gamepad.buttons.back;
    buttons[QStringLiteral("lb")] = state.gamepad.buttons.lb;
    buttons[QStringLiteral("rb")] = state.gamepad.buttons.rb;
    buttons[QStringLiteral("l3")] = state.gamepad.buttons.l3;
    buttons[QStringLiteral("r3")] = state.gamepad.buttons.r3;
    buttons[QStringLiteral("dpad_up")] = state.gamepad.buttons.dpadUp;
    buttons[QStringLiteral("dpad_down")] = state.gamepad.buttons.dpadDown;
    buttons[QStringLiteral("dpad_left")] = state.gamepad.buttons.dpadLeft;
    buttons[QStringLiteral("dpad_right")] = state.gamepad.buttons.dpadRight;

    QJsonObject axes;
    axes[QStringLiteral("left_x")] = state.gamepad.axes.leftX;
    axes[QStringLiteral("left_y")] = state.gamepad.axes.leftY;
    axes[QStringLiteral("right_x")] = state.gamepad.axes.rightX;
    axes[QStringLiteral("right_y")] = state.gamepad.axes.rightY;
    axes[QStringLiteral("lt")] = state.gamepad.axes.lt;
    axes[QStringLiteral("rt")] = state.gamepad.axes.rt;

    QJsonObject gamepad;
    gamepad[QStringLiteral("connected")] = state.gamepad.connected;
    gamepad[QStringLiteral("buttons")] = buttons;
    gamepad[QStringLiteral("axes")] = axes;
    message[QStringLiteral("gamepad")] = gamepad;

    return message;
}

QJsonObject HostProtocol::makeHeartbeat(quint64 seq, qint64 timestampMs)
{
    return makeEnvelope(QStringLiteral("heartbeat"), seq, timestampMs);
}

QJsonObject HostProtocol::makeSyncRequest(quint64 seq, const QString &reason, qint64 timestampMs)
{
    QJsonObject params;
    params[QStringLiteral("reason")] = reason;
    return makeCommand(QStringLiteral("sync_request"), seq, params, timestampMs);
}

QJsonObject HostProtocol::makeCameraListRequest(quint64 seq, qint64 timestampMs)
{
    return makeEnvelope(QStringLiteral("camera_list_request"), seq, timestampMs);
}

QJsonObject HostProtocol::makeCommand(const QString &type,
                                      quint64 seq,
                                      const QJsonObject &params,
                                      qint64 timestampMs)
{
    QJsonObject message = makeEnvelope(type, seq, timestampMs);
    message[QStringLiteral("params")] = params;
    return message;
}

QJsonObject HostProtocol::makeEnvelope(const QString &type, quint64 seq, qint64 timestampMs)
{
    QJsonObject message;
    message[QStringLiteral("type")] = type;
    message[QStringLiteral("protocol_version")] = ProtocolVersion;
    message[QStringLiteral("seq")] = static_cast<qint64>(seq);
    message[QStringLiteral("timestamp_ms")] = timestampMs;
    return message;
}

} // namespace Communication
