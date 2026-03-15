#include "Logger.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <iostream>

namespace Utils {

Logger::Logger()
    : m_logLevel(LogLevel::Debug)
    , m_logToFile(true)
    , m_logToConsole(true)
    , m_maxFileSize(DEFAULT_MAX_FILE_SIZE)
    , m_maxFileCount(DEFAULT_MAX_FILE_COUNT)
{
    // 默认日志目录：应用程序目录/logs
    QString appDir = QCoreApplication::applicationDirPath();
    m_logDirectory = appDir + "/logs";

    // 创建日志目录
    QDir dir;
    if (!dir.exists(m_logDirectory)) {
        dir.mkpath(m_logDirectory);
    }

    // 打开日志文件
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logFileName = QString("%1/hostcomputer_%2.log").arg(m_logDirectory).arg(timestamp);
    m_logFile.setFileName(logFileName);

    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
    }
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
}

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

void Logger::debug(const QString &module, const QString &message)
{
    log(LogLevel::Debug, module, message);
}

void Logger::info(const QString &module, const QString &message)
{
    log(LogLevel::Info, module, message);
}

void Logger::warning(const QString &module, const QString &message)
{
    log(LogLevel::Warning, module, message);
}

void Logger::error(const QString &module, const QString &message)
{
    log(LogLevel::Error, module, message);
}

void Logger::critical(const QString &module, const QString &message)
{
    log(LogLevel::Critical, module, message);
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::setLogToFile(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_logToFile = enable;
}

void Logger::setLogToConsole(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_logToConsole = enable;
}

void Logger::setLogDirectory(const QString &dir)
{
    QMutexLocker locker(&m_mutex);
    m_logDirectory = dir;

    // 创建目录
    QDir qdir;
    if (!qdir.exists(m_logDirectory)) {
        qdir.mkpath(m_logDirectory);
    }
}

void Logger::setMaxFileSize(qint64 maxSize)
{
    QMutexLocker locker(&m_mutex);
    m_maxFileSize = maxSize;
}

void Logger::setMaxFileCount(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxFileCount = count;
}

QString Logger::getLogFilePath() const
{
    return m_logFile.fileName();
}

void Logger::log(LogLevel level, const QString &module, const QString &message)
{
    // 过滤低于设定级别的日志
    if (level < m_logLevel) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    QString formattedMessage = formatMessage(level, module, message);

    // 输出到控制台
    if (m_logToConsole) {
        if (level >= LogLevel::Error) {
            std::cerr << formattedMessage.toStdString() << std::endl;
        } else {
            std::cout << formattedMessage.toStdString() << std::endl;
        }
    }

    // 输出到文件
    if (m_logToFile && m_logFile.isOpen()) {
        writeToFile(formattedMessage);
    }

    // 发送信号
    emit logMessageGenerated(level, module, message);
}

void Logger::writeToFile(const QString &formattedMessage)
{
    m_logStream << formattedMessage << "\n";
    m_logStream.flush();

    // 检查文件大小，必要时轮转
    if (m_logFile.size() > m_maxFileSize) {
        rotateLogFile();
    }
}

void Logger::rotateLogFile()
{
    m_logStream.flush();
    m_logFile.close();

    // 获取当前日志文件列表
    QDir logDir(m_logDirectory);
    QStringList filters;
    filters << "hostcomputer_*.log";
    QFileInfoList logFiles = logDir.entryInfoList(filters, QDir::Files, QDir::Time);

    // 删除超出数量的旧日志
    while (logFiles.size() >= m_maxFileCount) {
        QFile::remove(logFiles.last().absoluteFilePath());
        logFiles.removeLast();
    }

    // 创建新日志文件
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logFileName = QString("%1/hostcomputer_%2.log").arg(m_logDirectory).arg(timestamp);
    m_logFile.setFileName(logFileName);

    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
    }
}

QString Logger::formatMessage(LogLevel level, const QString &module, const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString levelStr = levelToString(level);

    return QString("[%1] [%2] [%3] %4")
        .arg(timestamp)
        .arg(levelStr)
        .arg(module)
        .arg(message);
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:    return "DEBUG";
    case LogLevel::Info:     return "INFO ";
    case LogLevel::Warning:  return "WARN ";
    case LogLevel::Error:    return "ERROR";
    case LogLevel::Critical: return "CRIT ";
    default:                 return "UNKN ";
    }
}

} // namespace Utils
