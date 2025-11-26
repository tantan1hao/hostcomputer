#include "handlekey.h"

HandleKey::HandleKey(QObject *parent)
    : QObject{parent}
{
    UDPSocket = new QUdpSocket(this);
    UDPSocket->bind(9700, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    connect(UDPSocket, &QUdpSocket::readyRead, this, &HandleKey::receiveData);
}

void HandleKey::receiveData(){
    QByteArray array;
    array.resize(UDPSocket->bytesAvailable());
    UDPSocket->readDatagram(array.data(),array.size());
    decode(array);
    //qDebug("decode");
    emit getHandleKey(state);
}

void HandleKey::decode(const QByteArray& array) {


    // 检查数据长度是否足够（7 字节 = 56 位）
    if (array.size() < 7) {
        return; // 数据不足，退出
    }

    for (int i = 0; i < 4; ++i) {
        int bit_start = i * 8;
        bool isNegative = (array[(bit_start + 3) / 8] >> (7 - ((bit_start + 3) % 8))) & 1;
        int magnitude = ((array[(bit_start + 4) / 8] >> (7 - ((bit_start + 4) % 8))) & 1) << 3 |
                        ((array[(bit_start + 5) / 8] >> (7 - ((bit_start + 5) % 8))) & 1) << 2 |
                        ((array[(bit_start + 6) / 8] >> (7 - ((bit_start + 6) % 8))) & 1) << 1 |
                        ((array[(bit_start + 7) / 8] >> (7 - ((bit_start + 7) % 8))) & 1);
        int value = (isNegative ? -magnitude : magnitude) * 32767 / 15;
        int16_t* axes[] = { &state.sThumbLX, &state.sThumbLY, &state.sThumbRX, &state.sThumbRY };
        *axes[i] = static_cast<int16_t>(value);
    }

    for (int i = 0; i < 2; ++i) {
        int bit_start = 32 + i * 4;
        int level = ((array[bit_start / 8] >> (7 - (bit_start % 8))) & 1) << 3 |
                    ((array[(bit_start + 1) / 8] >> (7 - ((bit_start + 1) % 8))) & 1) << 2 |
                    ((array[(bit_start + 2) / 8] >> (7 - ((bit_start + 2) % 8))) & 1) << 1 |
                    ((array[(bit_start + 3) / 8] >> (7 - ((bit_start + 3) % 8))) & 1);
        uint8_t value = level * 255 / 15;
        (i == 0 ? state.bLeftTrigger : state.bRightTrigger) = value;
    }

    bool* buttons[] = { &state.dpadUp, &state.dpadDown, &state.dpadLeft, &state.dpadRight,
                       &state.buttonY, &state.buttonA, &state.buttonX, &state.buttonB,
                       &state.leftThumb, &state.rightThumb, &state.leftShoulder,
                       &state.rightShoulder, &state.buttonBack, &state.buttonStart };
    for (int i = 0; i < 14; ++i) {
        buttons[i][0] = (array[(40 + i) / 8] >> (7 - ((40 + i) % 8))) & 1;
    }

    state.modeIndex = ((array[54 / 8] >> (7 - (54 % 8))) & 1) << 1 | ((array[55 / 8] >> (7 - (55 % 8))) & 1);
}

