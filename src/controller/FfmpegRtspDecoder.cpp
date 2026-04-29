#include "FfmpegRtspDecoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>

namespace {

constexpr int kBytesPerPixel = 3;
constexpr int kMaxFrameBytes = 3840 * 2160 * kBytesPerPixel;
constexpr int kStderrLimit = 4000;

} // namespace

FfmpegRtspDecoder::FfmpegRtspDecoder(QObject *parent)
    : QObject(parent)
{
}

FfmpegRtspDecoder::~FfmpegRtspDecoder()
{
    stop();
}

bool FfmpegRtspDecoder::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}

void FfmpegRtspDecoder::start(const QString &rtspUrl, int width, int height, int fps)
{
    stop();

    if (rtspUrl.trimmed().isEmpty()) {
        emit failed(QStringLiteral("RTSP URL 为空"));
        return;
    }
    if (width <= 0 || height <= 0) {
        emit failed(QStringLiteral("视频尺寸无效: %1x%2").arg(width).arg(height));
        return;
    }

    const qint64 frameBytes = static_cast<qint64>(width) * height * kBytesPerPixel;
    if (frameBytes <= 0 || frameBytes > kMaxFrameBytes) {
        emit failed(QStringLiteral("视频帧过大: %1x%2").arg(width).arg(height));
        return;
    }

    m_width = width;
    m_height = height;
    m_fps = fps > 0 ? fps : 30;
    m_frameBytes = static_cast<int>(frameBytes);
    m_stopRequested.store(false);
    {
        QMutexLocker locker(&m_frameMutex);
        m_latestFrame = QImage();
        m_hasNewFrame = false;
    }

    const QString program = ffmpegProgram();
    const QStringList arguments = ffmpegArguments(rtspUrl);
    m_workerThread = QThread::create([this, program, arguments, width, height, frameBytes = m_frameBytes]() {
        decoderLoop(program, arguments, width, height, frameBytes);
    });
    m_workerThread->start();
}

void FfmpegRtspDecoder::stop()
{
    m_stopRequested.store(true);
    if (m_workerThread) {
        m_workerThread->requestInterruption();
        m_workerThread->wait(2000);
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    {
        QMutexLocker locker(&m_frameMutex);
        m_latestFrame = QImage();
        m_hasNewFrame = false;
    }

    emit stopped();
}

bool FfmpegRtspDecoder::takeLatestFrame(QImage *frame)
{
    if (!frame) {
        return false;
    }

    QMutexLocker locker(&m_frameMutex);
    if (!m_hasNewFrame || m_latestFrame.isNull()) {
        return false;
    }

    *frame = m_latestFrame;
    m_hasNewFrame = false;
    return true;
}

QString FfmpegRtspDecoder::ffmpegProgram() const
{
    const QString configured = qEnvironmentVariable("HOSTCOMPUTER_FFMPEG_PATH");
    if (!configured.trimmed().isEmpty()) {
        return configured.trimmed();
    }

    const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString bundled = QDir(appDir).filePath(QStringLiteral("ffmpeg.exe"));
#else
    const QString bundled = QDir(appDir).filePath(QStringLiteral("ffmpeg"));
#endif
    if (QFileInfo::exists(bundled)) {
        return bundled;
    }

    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return inPath.isEmpty() ? QStringLiteral("ffmpeg") : inPath;
}

QStringList FfmpegRtspDecoder::ffmpegArguments(const QString &rtspUrl) const
{
    return {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("warning"),
        QStringLiteral("-rtsp_transport"),
        QStringLiteral("tcp"),
        QStringLiteral("-fflags"),
        QStringLiteral("nobuffer"),
        QStringLiteral("-flags"),
        QStringLiteral("low_delay"),
        QStringLiteral("-avioflags"),
        QStringLiteral("direct"),
        QStringLiteral("-max_delay"),
        QStringLiteral("0"),
        QStringLiteral("-reorder_queue_size"),
        QStringLiteral("0"),
        QStringLiteral("-analyzeduration"),
        QStringLiteral("0"),
        QStringLiteral("-probesize"),
        QStringLiteral("32768"),
        QStringLiteral("-i"),
        rtspUrl,
        QStringLiteral("-an"),
        QStringLiteral("-vf"),
        QStringLiteral("scale=%1:%2").arg(m_width).arg(m_height),
        QStringLiteral("-f"),
        QStringLiteral("rawvideo"),
        QStringLiteral("-pix_fmt"),
        QStringLiteral("rgb24"),
        QStringLiteral("-")
    };
}

void FfmpegRtspDecoder::decoderLoop(const QString &program, const QStringList &arguments,
                                    int width, int height, int frameBytes)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(program, arguments, QIODevice::ReadOnly);

    if (!process.waitForStarted(3000)) {
        if (!m_stopRequested.load()) {
            emit failed(QStringLiteral("无法启动 ffmpeg：请将 ffmpeg 放在上位机可执行文件同目录，或设置 HOSTCOMPUTER_FFMPEG_PATH"));
        }
        return;
    }

    emit started();

    QByteArray stdoutBuffer;
    QString stderrBuffer;

    while (!m_stopRequested.load() && !QThread::currentThread()->isInterruptionRequested()) {
        const bool hasData = process.waitForReadyRead(20);
        if (hasData) {
            stdoutBuffer += process.readAllStandardOutput();
        }

        const QByteArray stderrChunk = process.readAllStandardError();
        if (!stderrChunk.isEmpty()) {
            stderrBuffer += QString::fromLocal8Bit(stderrChunk);
            if (stderrBuffer.size() > kStderrLimit) {
                stderrBuffer = stderrBuffer.right(kStderrLimit);
            }
        }

        const int completeFrames = stdoutBuffer.size() / frameBytes;
        if (completeFrames > 0) {
            const int latestOffset = (completeFrames - 1) * frameBytes;
            const QByteArray latestFrame = stdoutBuffer.mid(latestOffset, frameBytes);
            stdoutBuffer.remove(0, completeFrames * frameBytes);
            storeLatestFrame(latestFrame, width, height);
        }

        if (process.state() == QProcess::NotRunning) {
            break;
        }
    }

    if (process.state() != QProcess::NotRunning) {
        process.terminate();
        if (!process.waitForFinished(800)) {
            process.kill();
            process.waitForFinished(800);
        }
    }

    if (!m_stopRequested.load()) {
        const QString status = process.exitStatus() == QProcess::CrashExit
            ? QStringLiteral("崩溃")
            : QStringLiteral("退出");
        emit failed(QStringLiteral("ffmpeg %1，code=%2%3")
                        .arg(status)
                        .arg(process.exitCode())
                        .arg(stderrTail(stderrBuffer)));
    }
}

void FfmpegRtspDecoder::storeLatestFrame(const QByteArray &rawFrame, int width, int height)
{
    QImage frame(reinterpret_cast<const uchar *>(rawFrame.constData()),
                 width,
                 height,
                 width * kBytesPerPixel,
                 QImage::Format_RGB888);

    QMutexLocker locker(&m_frameMutex);
    m_latestFrame = frame.copy();
    m_hasNewFrame = true;
}

QString FfmpegRtspDecoder::stderrTail(const QString &stderrBuffer)
{
    const QString tail = stderrBuffer.trimmed();
    if (tail.isEmpty()) {
        return QString();
    }
    return QStringLiteral(": %1").arg(tail.right(500));
}
