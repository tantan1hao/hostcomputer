// YaoCao类使用示例
// 此文件展示如何使用YaoCao类进行串口通信

#include "src/controller/yaocao.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

class YaoCaoUsageExample : public QObject
{
    Q_OBJECT

public:
    YaoCaoUsageExample(QObject *parent = nullptr) : QObject(parent)
    {
        m_yaoCao = new YaoCao(this);

        // 连接信号槽
        connect(m_yaoCao, &YaoCao::dataReceived, this, &YaoCaoUsageExample::onDataReceived);
        connect(m_yaoCao, &YaoCao::dataParsed, this, &YaoCaoUsageExample::onDataParsed);
        connect(m_yaoCao, &YaoCao::serialPortError, this, &YaoCaoUsageExample::onSerialError);
        connect(m_yaoCao, &YaoCao::connectionStatusChanged, this, &YaoCaoUsageExample::onConnectionChanged);

        // 设置定时器进行演示
        m_demoTimer = new QTimer(this);
        connect(m_demoTimer, &QTimer::timeout, this, &YaoCaoUsageExample::demonstrateFeatures);
    }

    void startDemo()
    {
        qDebug() << "=== YaoCao串口通信演示开始 ===";

        // 1. 显示可用串口
        demonstrateAvailablePorts();

        // 2. 5秒后开始连接演示
        QTimer::singleShot(5000, this, [this]() {
            demonstrateConnection();
        });

        // 3. 启动定时器进行定期演示
        m_demoTimer->start(10000); // 每10秒演示一次功能
    }

private slots:
    void onDataReceived(const QByteArray &data)
    {
        qDebug() << "接收到原始数据:" << data.toHex();
    }

    void onDataParsed(const QString &parsedData)
    {
        qDebug() << "解析后的数据:" << parsedData;

        // 在这里可以处理具体的业务逻辑
        // 例如：更新UI、存储数据、触发其他操作等
    }

    void onSerialError(const QString &error)
    {
        qDebug() << "串口错误:" << error;
    }

    void onConnectionChanged(bool connected)
    {
        qDebug() << "连接状态变化:" << (connected ? "已连接" : "已断开");
    }

    void demonstrateAvailablePorts()
    {
        qDebug() << "\n--- 可用串口列表 ---";
        QStringList ports = YaoCao::getAvailablePorts();

        if (ports.isEmpty()) {
            qDebug() << "未发现可用串口";
        } else {
            for (int i = 0; i < ports.size(); ++i) {
                qDebug() << QString("%1. %2").arg(i + 1).arg(ports[i]);
            }
        }
    }

    void demonstrateConnection()
    {
        qDebug() << "\n--- 串口连接演示 ---";

        // 尝试连接第一个可用串口（如果存在）
        QStringList ports = YaoCao::getAvailablePorts();

        if (!ports.isEmpty()) {
            // 提取串口名称（去掉描述信息）
            QString firstPort = ports.first().split(" ").first();

            qDebug() << QString("尝试连接串口: %1").arg(firstPort);

            bool success = m_yaoCao->openSerialPort(firstPort, 115200);

            if (success) {
                qDebug() << "串口连接成功！";

                // 10秒后自动断开
                QTimer::singleShot(10000, this, [this]() {
                    qDebug() << "断开串口连接...";
                    m_yaoCao->closeSerialPort();
                });
            } else {
                qDebug() << "串口连接失败（这是正常的，可能没有实际设备）";
            }
        } else {
            qDebug() << "没有可用串口，跳过连接演示";
        }
    }

    void demonstrateFeatures()
    {
        qDebug() << "\n--- 功能特性演示 ---";
        qDebug() << "1. 串口自动检测和枚举";
        qDebug() << "2. 多种波特率支持 (默认115200)";
        qDebug() << "3. 实时数据接收和解析";
        qDebug() << "4. 完善的错误处理机制";
        qDebug() << "5. 信号槽异步通信";
        qDebug() << "6. 数据包验证和缓冲管理";
        qDebug() << "7. ASCII和二进制数据处理";

        if (m_yaoCao->isPortOpen()) {
            qDebug() << "当前串口状态: 已连接";
        } else {
            qDebug() << "当前串口状态: 未连接";
        }
    }

private:
    YaoCao *m_yaoCao;
    QTimer *m_demoTimer;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    YaoCaoUsageExample example;
    example.startDemo();

    return app.exec();
}

#include "yaocao_usage_example.moc"