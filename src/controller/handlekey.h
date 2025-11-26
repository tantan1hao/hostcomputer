#ifndef HANDLEKEY_H
#define HANDLEKEY_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include <QCoreApplication>

struct ControllerState {
    int16_t sThumbLX;      // 左摇杆 X 轴 (-32767 到 32767)
    int16_t sThumbLY;      // 左摇杆 Y 轴
    int16_t sThumbRX;      // 右摇杆 X 轴
    int16_t sThumbRY;      // 右摇杆 Y 轴
    uint8_t bLeftTrigger;  // 左扳机 (0-255)
    uint8_t bRightTrigger; // 右扳机
    bool dpadUp;           // DPad 上
    bool dpadDown;         // DPad 下
    bool dpadLeft;         // DPad 左
    bool dpadRight;        // DPad 右
    bool buttonY;          // Y 按钮
    bool buttonA;          // A 按钮
    bool buttonX;          // X 按钮
    bool buttonB;          // B 按钮
    bool leftThumb;        // 左摇杆按键
    bool rightThumb;       // 右摇杆按键
    bool leftShoulder;     // 左肩键 (LB)
    bool rightShoulder;    // 右肩键 (RB)
    bool buttonBack;       // Back 按钮
    bool buttonStart;      // Start 按钮
    uint8_t modeIndex;     // 模式索引 (0-3，对应 Mode1-Mode4)
};


class HandleKey : public QObject
{
    Q_OBJECT
public:
    explicit HandleKey(QObject *parent = nullptr);

    void decode(const QByteArray& array);

signals:
    void getHandleKey(ControllerState);

public slots:
    void receiveData();

private:
    QUdpSocket* UDPSocket;//套接字


    ControllerState state = {};

};

#endif // HANDLEKEY_H
