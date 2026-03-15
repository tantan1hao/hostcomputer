#include "ErrorHandler.h"
#include "Logger.h"

namespace Utils {

ErrorHandler::ErrorHandler()
{
}

ErrorHandler& ErrorHandler::instance()
{
    static ErrorHandler instance;
    return instance;
}

void ErrorHandler::handleError(const ErrorInfo &error)
{
    // 记录错误
    logError(error);

    // 更新统计
    m_errorCounts[error.code]++;
    m_lastError = error;

    // 添加到历史记录
    m_errorHistory.prepend(error);
    if (m_errorHistory.size() > MAX_HISTORY_SIZE) {
        m_errorHistory.removeLast();
    }

    // 触发回调
    if (m_errorCallbacks.contains(error.code)) {
        for (const auto &callback : m_errorCallbacks[error.code]) {
            callback(error);
        }
    }

    // 触发全局回调
    for (const auto &callback : m_globalCallbacks) {
        callback(error);
    }

    // 发送信号
    emit errorOccurred(error);

    // 严重错误单独发送信号
    if (isCriticalError(error.code)) {
        emit criticalErrorOccurred(error);
    }
}

void ErrorHandler::handleError(ErrorCode code, const QString &module, const QString &message, const QString &details)
{
    ErrorInfo error(code, module, message, details);
    handleError(error);
}

void ErrorHandler::registerErrorCallback(ErrorCode code, ErrorCallback callback)
{
    m_errorCallbacks[code].append(callback);
}

void ErrorHandler::registerGlobalErrorCallback(ErrorCallback callback)
{
    m_globalCallbacks.append(callback);
}

ErrorInfo ErrorHandler::getLastError() const
{
    return m_lastError;
}

QList<ErrorInfo> ErrorHandler::getErrorHistory(int maxCount) const
{
    if (maxCount <= 0 || maxCount >= m_errorHistory.size()) {
        return m_errorHistory;
    }
    return m_errorHistory.mid(0, maxCount);
}

int ErrorHandler::getErrorCount(ErrorCode code) const
{
    return m_errorCounts.value(code, 0);
}

void ErrorHandler::clearErrorHistory()
{
    m_errorHistory.clear();
    m_errorCounts.clear();
}

QString ErrorHandler::getErrorDescription(ErrorCode code)
{
    static QMap<ErrorCode, QString> descriptions = {
        // 通用错误
        {ErrorCode::NoError, "无错误"},
        {ErrorCode::UnknownError, "未知错误"},
        {ErrorCode::InvalidParameter, "无效参数"},
        {ErrorCode::Timeout, "操作超时"},

        // 网络错误
        {ErrorCode::NetworkConnectionFailed, "网络连接失败"},
        {ErrorCode::NetworkDisconnected, "网络连接断开"},
        {ErrorCode::NetworkTimeout, "网络超时"},
        {ErrorCode::NetworkSendFailed, "网络发送失败"},
        {ErrorCode::NetworkReceiveFailed, "网络接收失败"},

        // 串口错误
        {ErrorCode::SerialPortOpenFailed, "串口打开失败"},
        {ErrorCode::SerialPortNotOpen, "串口未打开"},
        {ErrorCode::SerialPortWriteFailed, "串口写入失败"},
        {ErrorCode::SerialPortReadFailed, "串口读取失败"},
        {ErrorCode::SerialPortTimeout, "串口超时"},

        // 协议错误
        {ErrorCode::ProtocolParseError, "协议解析错误"},
        {ErrorCode::ProtocolChecksumError, "协议校验和错误"},
        {ErrorCode::ProtocolInvalidFrame, "无效协议帧"},
        {ErrorCode::ProtocolVersionMismatch, "协议版本不匹配"},

        // 设备错误
        {ErrorCode::DeviceNotFound, "设备未找到"},
        {ErrorCode::DeviceNotReady, "设备未就绪"},
        {ErrorCode::DeviceOverload, "设备过载"},
        {ErrorCode::DeviceEmergencyStop, "设备急停"},

        // 摄像头错误
        {ErrorCode::CameraOpenFailed, "摄像头打开失败"},
        {ErrorCode::CameraNotAvailable, "摄像头不可用"},
        {ErrorCode::CameraCaptureFailed, "摄像头捕获失败"},

        // 文件错误
        {ErrorCode::FileOpenFailed, "文件打开失败"},
        {ErrorCode::FileWriteFailed, "文件写入失败"},
        {ErrorCode::FileReadFailed, "文件读取失败"},
        {ErrorCode::FileNotFound, "文件未找到"}
    };

    return descriptions.value(code, "未定义错误");
}

void ErrorHandler::logError(const ErrorInfo &error)
{
    QString logMessage = QString("%1: %2").arg(getErrorDescription(error.code)).arg(error.message);
    if (!error.details.isEmpty()) {
        logMessage += QString(" | 详情: %1").arg(error.details);
    }

    // 根据错误严重程度选择日志级别
    if (isCriticalError(error.code)) {
        LOG_CRITICAL(error.module, logMessage);
    } else if (static_cast<int>(error.code) >= 100) {
        LOG_ERROR(error.module, logMessage);
    } else {
        LOG_WARNING(error.module, logMessage);
    }
}

bool ErrorHandler::isCriticalError(ErrorCode code) const
{
    // 定义严重错误
    return code == ErrorCode::DeviceEmergencyStop ||
           code == ErrorCode::DeviceOverload ||
           code == ErrorCode::ProtocolVersionMismatch;
}

} // namespace Utils
