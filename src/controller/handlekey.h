#ifndef HANDLEKEY_H
#define HANDLEKEY_H

#include <QObject>
#include <QTimer>

struct ControllerState {
    bool buttonA;
    bool buttonB;
    bool buttonX;
    bool buttonY;

    bool buttonStart;
    bool buttonBack;

    bool dpadUp;
    bool dpadDown;
    bool dpadLeft;
    bool dpadRight;

    bool leftThumb;
    bool rightThumb;
    bool leftShoulder;
    bool rightShoulder;

    int16_t sThumbLX;      // 左摇杆 X 轴 (-32768 到 32767)
    int16_t sThumbLY;      // 左摇杆 Y 轴
    int16_t sThumbRX;      // 右摇杆 X 轴
    int16_t sThumbRY;      // 右摇杆 Y 轴

    uint8_t bLeftTrigger;  // 左扳机 (0-255)
    uint8_t bRightTrigger; // 右扳机 (0-255)

    uint8_t modeIndex;     // 模式索引
};

class HandleKey : public QObject
{
    Q_OBJECT

public:
    explicit HandleKey(QObject *parent = nullptr);
    bool isConnected() const { return m_connected; }
    void startPolling();
    void stopPolling();

signals:
    void getHandleKey(const ControllerState &state);
    void connectionChanged(bool connected);

private slots:
    void readGamepad();

private:
    QTimer *timer;
    ControllerState state = {};
    bool m_connected = false;
};

#endif // HANDLEKEY_H
