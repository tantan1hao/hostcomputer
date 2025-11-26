#include "yaocao.h"
#include "../parser/parser.h"
#include "../parser/motor_state.h"

YaoCao::YaoCao(QObject *parent)
    : QObject(parent), m_serialPort(nullptr), m_parser(nullptr)
{
    m_serialPort = new QSerialPort(this);

    // 连接信号槽
    connect(m_serialPort, &QSerialPort::readyRead, this, &YaoCao::handleReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &YaoCao::handleError);
}

YaoCao::~YaoCao()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
}

bool YaoCao::openSerialPort(const QString &portName, int baudRate)
{
    if (m_serialPort->isOpen()) {
        closeSerialPort();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "串口" << portName << "打开成功，波特率:" << baudRate;
        emit connectionStatusChanged(true);
        return true;
    } else {
        QString error = QString("无法打开串口 %1: %2")
                       .arg(portName)
                       .arg(m_serialPort->errorString());
        qDebug() << error;
        emit serialPortError(error);
        return false;
    }
}

void YaoCao::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        m_buffer.clear();
        qDebug() << "串口已关闭";
        emit connectionStatusChanged(false);
    }
}

bool YaoCao::isPortOpen() const
{
    return m_serialPort ? m_serialPort->isOpen() : false;
}

QStringList YaoCao::getAvailablePorts()
{
    QStringList portList;

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString portName = info.portName();
        QString description = info.description();
        QString manufacturer = info.manufacturer();

        // 格式化显示串口信息
        QString displayText = QString("%1 (%2 - %3)")
                              .arg(portName)
                              .arg(description)
                              .arg(manufacturer);
        portList.append(displayText);
    }

    return portList;
}

void YaoCao::setParser(Parser *parser)
{
    // 断开之前的parser连接
    if (m_parser) {
        disconnect(m_parser, nullptr, this, nullptr);
    }

    m_parser = parser;

    // 连接新的parser信号
    if (m_parser) {
        connect(m_parser, &Parser::motorStateUpdated,
                this, &YaoCao::onMotorStateUpdated);
        connect(m_parser, &Parser::parseError,
                this, &YaoCao::onParseError);
        connect(m_parser, &Parser::protocolError,
                this, &YaoCao::onProtocolError);

        qDebug() << "YaoCao: Parser已设置并连接信号槽";
    }
}

Parser *YaoCao::getParser() const
{
    return m_parser;
}

void YaoCao::handleReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    m_buffer.append(data);

    qDebug() << "YaoCao: 接收到原始数据:" << data.toHex();

    // 发射原始数据信号
    emit dataReceived(data);

    // 发射字节流信号
    emit byteStreamReceived(data);

    // 处理字节流（包含parser解析）
    processByteStream(data);

    // 兼容性：保留原有的简单解析
    parseData(data);
}

void YaoCao::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    QString errorMessage;
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        errorMessage = "设备未找到";
        break;
    case QSerialPort::PermissionError:
        errorMessage = "权限不足，无法访问设备";
        break;
    case QSerialPort::OpenError:
        errorMessage = "设备已打开或打开失败";
        break;
    case QSerialPort::NotOpenError:
        errorMessage = "设备未打开";
        break;
    case QSerialPort::ReadError:
        errorMessage = "读取数据错误";
        break;
    case QSerialPort::WriteError:
        errorMessage = "写入数据错误";
        break;
    case QSerialPort::ResourceError:
        errorMessage = "设备资源错误，设备可能已被拔出";
        break;
    case QSerialPort::UnsupportedOperationError:
        errorMessage = "不支持的操作";
        break;
    case QSerialPort::TimeoutError:
        errorMessage = "操作超时";
        break;
    default:
        errorMessage = QString("未知错误: %1").arg(m_serialPort->errorString());
        break;
    }

    qDebug() << "串口错误:" << errorMessage;
    emit serialPortError(errorMessage);

    // 如果是严重错误，关闭连接
    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::DeviceNotFoundError) {
        closeSerialPort();
    }
}

void YaoCao::parseData(const QByteArray &data)
{
    // 检查是否是有效的数据包
    if (isValidDataPacket(data)) {
        QString parsedData = extractUsefulData(data);

        if (!parsedData.isEmpty()) {
            qDebug() << "解析后的数据:" << parsedData;
            emit dataParsed(parsedData);
        }
    } else {
        // 如果不是完整数据包，继续等待
        qDebug() << "数据包不完整，继续接收...";
    }

    // 防止缓冲区过大，定期清理
    if (m_buffer.size() > 1024) {
        m_buffer.clear();
        qDebug() << "缓冲区已清理";
    }
}

bool YaoCao::isValidDataPacket(const QByteArray &data)
{
    // 基本的数据包验证逻辑
    // 这里可以根据实际的数据协议格式进行调整

    if (data.isEmpty()) {
        return false;
    }

    // 示例：假设数据包以特定字节开头和结尾
    // 比如：0xAA 0x55 ... 0x0D 0x0A
    if (data.length() >= 4) {
        // 检查帧头
        if (static_cast<unsigned char>(data[0]) == 0xAA &&
            static_cast<unsigned char>(data[1]) == 0x55) {
            return true;
        }
    }

    // 简单的长度检查
    return data.length() >= 2;
}

QString YaoCao::extractUsefulData(const QByteArray &data)
{
    // 根据实际协议解析数据
    // 这里提供一个通用的解析框架

    QString result;

    // 如果是标准的ASCII数据
    bool isAscii = true;
    for (char byte : data) {
        if (byte < 32 || byte > 126) {
            isAscii = false;
            break;
        }
    }

    if (isAscii) {
        result = QString::fromLocal8Bit(data).trimmed();
    } else {
        // 处理二进制数据
        result = QString("二进制数据 [%1字节]: %2")
                .arg(data.length())
                .arg(QString(data.toHex()));
    }

    return result;
}

// 新增的字节流处理方法
void YaoCao::processByteStream(const QByteArray &byteStream)
{
    // 1. 转发给parser进行解析
    forwardToParser(byteStream);

    // 2. 可以在这里添加其他字节流预处理逻辑
    // 例如：数据过滤、缓冲管理、协议检测等
}

void YaoCao::forwardToParser(const QByteArray &byteStream)
{
    if (!m_parser) {
        qDebug() << "YaoCao: Parser未设置，无法转发字节流";
        return;
    }

    // 将字节流转发给parser
    bool success = m_parser->parseByteStream(byteStream);

    // 发射解析结果信号
    emit byteStreamParsed(success);

    if (success) {
        qDebug() << "YaoCao: 字节流已成功转发给parser";
    } else {
        qDebug() << "YaoCao: 字节流转发给parser失败";
    }
}

// Parser槽函数实现
void YaoCao::onMotorStateUpdated(const MotorState &state)
{
    qDebug() << QString("YaoCao: 电机%1状态更新 - 在线:%2 运行:%3 转速:%4")
                .arg(state.motorId)
                .arg(state.isOnline ? "是" : "否")
                .arg(state.isRunning ? "是" : "否")
                .arg(state.currentSpeed);

    // 可以在这里进行进一步的处理
    // 例如：更新UI、记录日志、触发其他操作等
}

void YaoCao::onParseError(const QString &error)
{
    qDebug() << "YaoCao: Parser解析错误:" << error;
    // 可以在这里进行错误处理，例如重试、记录日志等
}

void YaoCao::onProtocolError(const QString &error)
{
    qDebug() << "YaoCao: 协议错误:" << error;
    // 协议错误通常需要更严重的处理，例如重置连接
}