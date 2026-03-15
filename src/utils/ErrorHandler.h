#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <functional>

namespace Utils {

/**
 * @brief 错误代码定义
 */
enum class ErrorCode {
    // 通用错误 (0-99)
    NoError = 0,
    UnknownError = 1,
    InvalidParameter = 2,
    Timeout = 3,

    // 网络错误 (100-199)
    NetworkConnectionFailed = 100,
    NetworkDisconnected = 101,
    NetworkTimeout = 102,
    NetworkSendFailed = 103,
    NetworkReceiveFailed = 104,

    // 串口错误 (200-299)
    SerialPortOpenFailed = 200,
    SerialPortNotOpen = 201,
    SerialPortWriteFailed = 202,
    SerialPortReadFailed = 203,
    SerialPortTimeout = 204,

    // 协议错误 (300-399)
    ProtocolParseError = 300,
    ProtocolChecksumError = 301,
    ProtocolInvalidFrame = 302,
    ProtocolVersionMismatch = 303,

    // 设备错误 (400-499)
    DeviceNotFound = 400,
    DeviceNotReady = 401,
    DeviceOverload = 402,
    DeviceEmergencyStop = 403,

    // 摄像头错误 (500-599)
    CameraOpenFailed = 500,
    CameraNotAvailable = 501,
    CameraCaptureFailed = 502,

    // 文件错误 (600-699)
    FileOpenFailed = 600,
    FileWriteFailed = 601,
    FileReadFailed = 602,
    FileNotFound = 603
};

/**
 * @brief 错误信息结构
 */
struct ErrorInfo {
    ErrorCode code;
    QString module;
    QString message;
    QString details;
    QDateTime timestamp;

    ErrorInfo()
        : code(ErrorCode::NoError)
        , timestamp(QDateTime::currentDateTime())
    {}

    ErrorInfo(ErrorCode c, const QString &mod, const QString &msg, const QString &det = QString())
        : code(c)
        , module(mod)
        , message(msg)
        , details(det)
        , timestamp(QDateTime::currentDateTime())
    {}
};

/**
 * @brief 统一错误处理器
 *
 * 功能：
 * - 错误码管理
 * - 错误信息记录
 * - 错误回调处理
 * - 错误统计
 */
class ErrorHandler : public QObject
{
    Q_OBJECT

public:
    static ErrorHandler& instance();

    // 错误处理接口
    void handleError(const ErrorInfo &error);
    void handleError(ErrorCode code, const QString &module, const QString &message, const QString &details = QString());

    // 注册错误回调
    using ErrorCallback = std::function<void(const ErrorInfo&)>;
    void registerErrorCallback(ErrorCode code, ErrorCallback callback);
    void registerGlobalErrorCallback(ErrorCallback callback);

    // 错误查询
    ErrorInfo getLastError() const;
    QList<ErrorInfo> getErrorHistory(int maxCount = 100) const;
    int getErrorCount(ErrorCode code) const;
    void clearErrorHistory();

    // 错误描述
    static QString getErrorDescription(ErrorCode code);

signals:
    void errorOccurred(const ErrorInfo &error);
    void criticalErrorOccurred(const ErrorInfo &error);

private:
    ErrorHandler();
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;

    void logError(const ErrorInfo &error);
    bool isCriticalError(ErrorCode code) const;

private:
    QList<ErrorInfo> m_errorHistory;
    QMap<ErrorCode, int> m_errorCounts;
    QMap<ErrorCode, QList<ErrorCallback>> m_errorCallbacks;
    QList<ErrorCallback> m_globalCallbacks;
    ErrorInfo m_lastError;

    static const int MAX_HISTORY_SIZE = 1000;
};

// 便捷宏定义
#define HANDLE_ERROR(code, module, msg) \
    Utils::ErrorHandler::instance().handleError(code, module, msg)

#define HANDLE_ERROR_DETAIL(code, module, msg, detail) \
    Utils::ErrorHandler::instance().handleError(code, module, msg, detail)

} // namespace Utils

#endif // ERROR_HANDLER_H
