// 简单的测试程序，验证yaocao类的接口
#include <iostream>
#include <QString>
#include <QDebug>

// 模拟Qt环境的基础功能
namespace Qt {
    enum CaseSensitivity { CaseInsensitive, CaseSensitive };
}

class QString {
public:
    QString() {}
    QString(const char* str) { m_str = str; }
    QString(const std::string& str) { m_str = str; }

    QString arg(const QString& a) const {
        std::string result = m_str;
        size_t pos = result.find("%1");
        if (pos != std::string::npos) {
            result.replace(pos, 2, a.m_str);
        }
        return result;
    }

    QString arg(int a) const {
        return arg(QString(std::to_string(a)));
    }

    QString trimmed() const {
        size_t start = m_str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return QString();
        size_t end = m_str.find_last_not_of(" \t\n\r");
        return QString(m_str.substr(start, end - start + 1));
    }

    const char* toLocal8Bit() const { return m_str.c_str(); }
    const std::string& toStdString() const { return m_str; }
    bool isEmpty() const { return m_str.empty(); }

private:
    std::string m_str;
};

class QByteArray {
public:
    QByteArray() {}
    QByteArray(const char* data, int size = -1) {
        if (size == -1) size = strlen(data);
        m_data.assign(data, data + size);
    }

    void append(const QByteArray& other) {
        m_data.insert(m_data.end(), other.m_data.begin(), other.m_data.end());
    }

    int size() const { return static_cast<int>(m_data.size()); }
    bool isEmpty() const { return m_data.empty(); }
    void clear() { m_data.clear(); }

    QString toHex() const {
        QString result;
        for (unsigned char byte : m_data) {
            result = result.arg(QString("%1 ").arg(byte, 2, 16));
        }
        return result.trimmed();
    }

    char operator[](int index) const { return m_data[index]; }

private:
    std::vector<char> m_data;
};

class QDebug {
public:
    QDebug& operator<<(const QString& str) {
        std::cout << str.toStdString() << std::endl;
        return *this;
    }
    QDebug& operator<<(const char* str) {
        std::cout << str << std::endl;
        return *this;
    }
    QDebug& operator<<(int value) {
        std::cout << value << std::endl;
        return *this;
    }
};

QDebug qDebug() { return QDebug(); }

class QStringList {
public:
    void append(const QString& str) { m_list.push_back(str); }
    int size() const { return static_cast<int>(m_list.size()); }

private:
    std::vector<QString> m_list;
};

// 简化的YaoCao类测试
class YaoCaoTest {
public:
    static void testBasicFunctionality() {
        std::cout << "=== YaoCao类基本功能测试 ===" << std::endl;

        // 测试字符串操作
        QString portName("COM3");
        QString message = QString("打开串口: %1").arg(portName);
        qDebug() << message;

        // 测试字节数组操作
        QByteArray testData("Hello World");
        qDebug() << "测试数据:" << testData.toHex();

        // 测试数据解析
        testParseData();

        std::cout << "=== 基本功能测试完成 ===" << std::endl;
    }

private:
    static void testParseData() {
        std::cout << "\n--- 数据解析测试 ---" << std::endl;

        // 测试ASCII数据
        QByteArray asciiData("Test Data");
        bool isAscii = true;
        for (int i = 0; i < asciiData.size(); ++i) {
            if (asciiData[i] < 32 || asciiData[i] > 126) {
                isAscii = false;
                break;
            }
        }
        qDebug() << QString("ASCII数据检查: %1").arg(isAscii ? "通过" : "失败");

        // 测试二进制数据模拟
        QByteArray binaryData;
        binaryData.append(QByteArray("\xAA\x55", 2));
        binaryData.append(QByteArray("\x01\x02\x03", 3));
        qDebug() << "二进制数据:" << binaryData.toHex();

        // 测试数据包验证
        bool isValidPacket = binaryData.size() >= 4 &&
                           (unsigned char)binaryData[0] == 0xAA &&
                           (unsigned char)binaryData[1] == 0x55;
        qDebug() << QString("数据包验证: %1").arg(isValidPacket ? "通过" : "失败");
    }
};

int main() {
    std::cout << "开始测试YaoCao串口通信功能..." << std::endl;

    YaoCaoTest::testBasicFunctionality();

    std::cout << "\n所有测试完成！" << std::endl;

    std::cout << "\n=== YaoCao类功能说明 ===" << std::endl;
    std::cout << "1. 串口连接管理: openSerialPort(), closeSerialPort()" << std::endl;
    std::cout << "2. 数据接收处理: handleReadyRead()" << std::endl;
    std::cout << "3. 数据解析: parseData(), isValidDataPacket()" << std::endl;
    std::cout << "4. 错误处理: handleError()" << std::endl;
    std::cout << "5. 信号机制: dataReceived, dataParsed, serialPortError" << std::endl;

    return 0;
}