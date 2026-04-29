#ifndef RTSPPLAYERWIDGET_H
#define RTSPPLAYERWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

class FfmpegRtspDecoder;
class VideoFrameWidget;

class RtspPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RtspPlayerWidget(int cameraId = 0, QWidget *parent = nullptr);
    ~RtspPlayerWidget() override;

    int cameraId() const { return m_cameraId; }

    // 接收下位机JSON推送的摄像头信息
    void setCameraInfo(const QString &rtspUrl, bool online,
                       const QString &codec, int width, int height, int fps, int bitrateKbps);

public slots:
    void startVideo();
    void stopVideo();

private slots:
    void updateLocalClock();
    void updateVideoFrame();
    void onDecoderStarted();
    void onDecoderStopped();
    void onDecoderFailed(const QString &message);

private:
    int m_cameraId;
    QString m_rtspUrl;
    int m_streamWidth = 0;
    int m_streamHeight = 0;
    int m_streamFps = 0;
    bool m_streamOnline = false;

    FfmpegRtspDecoder *m_decoder;
    VideoFrameWidget *m_videoWidget;
    QLabel *m_waitLabel;
    QLabel *m_statusLabel;
    QLabel *m_titleLabel;
    QLabel *m_infoLabel;
    QLabel *m_localClockLabel;   // 叠加在视频上的本地时钟
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QTimer *m_clockTimer;        // 刷新本地时钟的定时器
    QTimer *m_frameTimer;        // 从解码线程取最新帧的定时器
};

#endif // RTSPPLAYERWIDGET_H
