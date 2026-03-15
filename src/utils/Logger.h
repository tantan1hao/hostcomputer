#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDir>

namespace Utils {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    Debug = 0,    // 调试信息
    Info = 1,     // 一般信息
    Warning = 2,  // 警告
    Error = 3,    // 错误
    Critical = 4  // 严重错误
};

/**
 * @brief 统一日志管理器
 *
 * 功能：
 * - 多级别日志输出
 * - 文件和控制台双输出
 * - 线程安全
 * - 自动日志文件轮转
 * - 日志过滤
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger& instance();

    // 日志输出接口
    void debug(const QString &module, const QString &message);
    void info(const QString &module, const QString &message);
    void warning(const QString &module, const QString &message);
    void error(const QString &module, const QString &message);
    void critical(const QString &module, const QString &message);

    // 配置接口
    void setLogLevel(LogLevel level);
    void setLogToFile(bool enable);
    void setLogToConsole(bool enable);
    void setLogDirectory(const QString &dir);
    void setMaxFileSize(qint64 maxSize);  // 单位：字节
    void setMaxFileCount(int count);

    // 查询接口
    LogLevel getLogLevel() const { return m_logLevel; }
    QString getLogFilePath() const;
    bool isLogToFile() const { return m_logToFile; }
    bool isLogToConsole() const { return m_logToConsole; }

signals:
    void logMessageGenerated(LogLevel level, const QString &module, const QString &message);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const QString &module, const QString &message);
    void writeToFile(const QString &formattedMessage);
    void rotateLogFile();
    QString formatMessage(LogLevel level, const QString &module, const QString &message);
    QString levelToString(LogLevel level);

private:
    LogLevel m_logLevel;
    bool m_logToFile;
    bool m_logToConsole;
    QString m_logDirectory;
    qint64 m_maxFileSize;
    int m_maxFileCount;

    QFile m_logFile;
    QTextStream m_logStream;
    QMutex m_mutex;

    static const qint64 DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
    static const int DEFAULT_MAX_FILE_COUNT = 5;
};

// 便捷宏定义
#define LOG_DEBUG(module, msg) Utils::Logger::instance().debug(module, msg)
#define LOG_INFO(module, msg) Utils::Logger::instance().info(module, msg)
#define LOG_WARNING(module, msg) Utils::Logger::instance().warning(module, msg)
#define LOG_ERROR(module, msg) Utils::Logger::instance().error(module, msg)
#define LOG_CRITICAL(module, msg) Utils::Logger::instance().critical(module, msg)

} // namespace Utils

#endif // LOGGER_H
