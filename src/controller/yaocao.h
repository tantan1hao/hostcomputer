#ifndef YAOCAO_H
#define YAOCAO_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QString>
#include <QDebug>

// 前向声明
class Parser;
struct MotorState;

class YaoCao : public QObject
{
    Q_OBJECT

public:
    explicit YaoCao(QObject *parent = nullptr);
    ~YaoCao();

    // 串口操作
    bool openSerialPort(const QString &portName, int baudRate = 115200);
    void closeSerialPort();
    bool isPortOpen() const;

    // 获取可用串口列表
    static QStringList getAvailablePorts();

    // Parser集成
    void setParser(Parser *parser);
    Parser *getParser() const;

signals:
    // 数据接收信号
    void dataReceived(const QByteArray &data);
    void dataParsed(const QString &parsedData);
    void serialPortError(const QString &error);
    void connectionStatusChanged(bool connected);

    // 字节流处理信号 (新增)
    void byteStreamReceived(const QByteArray &byteStream);
    void byteStreamParsed(bool success);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

    // Parser处理槽函数 (新增)
    void onMotorStateUpdated(const MotorState &state);
    void onParseError(const QString &error);
    void onProtocolError(const QString &error);

private:
    QSerialPort *m_serialPort;
    QByteArray m_buffer;
    Parser *m_parser;  // 新增parser成员

    // 数据解析方法
    void parseData(const QByteArray &data);
    bool isValidDataPacket(const QByteArray &data);
    QString extractUsefulData(const QByteArray &data);

    // 字节流处理方法 (新增)
    void processByteStream(const QByteArray &byteStream);
    void forwardToParser(const QByteArray &byteStream);
};

#endif // YAOCAO_H
