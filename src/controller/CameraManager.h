#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <QObject>
#include <QPixmap>
#include <QLabel>
#include <QThread>
#include <QTimer>
#include <QMap>
#include "VideoWidget.h"

// OpenCV 支持（可选）
#ifdef OPENCV_FOUND
#include <opencv2/opencv.hpp>
#endif

QT_BEGIN_NAMESPACE

class CameraWorker;

/**
 * @brief 摄像头管理器 - 管理多个摄像头的非阻塞显示
 *
 * 特点：
 * - 每个摄像头在独立线程中运行
 * - 使用Qt信号槽确保线程安全
 * - 支持热插拔和动态重连
 * - 自动适配分辨率
 */
class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    // 摄像头管理
    bool startCamera(int cameraId, VideoWidget* displayWidget);
    void stopCamera(int cameraId);
    void stopAllCameras();

    // 状态查询
    bool isCameraRunning(int cameraId) const;
    int getCameraCount() const;
    QStringList getAvailableCameras() const;

    // 刷新摄像头列表
    void refreshCameraList();

    // 手动请求帧
    void requestFrame(int cameraId);

signals:
    // 视频信号
    void frameReady(int cameraId, const QPixmap& frame);
    void frameAvailable(int cameraId, const cv::Mat& frame);
    void videoFrameReady(int cameraId, VideoWidget* widget);

    // 状态信号
    void cameraStarted(int cameraId, const QString& cameraName);
    void cameraStopped(int cameraId);
    void cameraError(int cameraId, const QString& error);

    // 列表更新信号
    void cameraListChanged(const QStringList& cameraNames);

public slots:
    // 全局控制
    void startAllCameras();
    void stopAllCameras();
    void restartAllCameras();

private slots:
    void onFrameReady(int cameraId, const QPixmap& frame);
    void onCameraError(int cameraId, const QString& error);
    void onCameraStatusChanged(int cameraId, bool isRunning);
    void checkCameraConnection();

private:
    void setupCameraList();
    void connectWorkerSignals(CameraWorker* worker, int cameraId);
    void disconnectWorkerSignals(CameraWorker* worker);

private:
    struct CameraInfo {
        QString name;
        QString description;
        bool isRunning;
        CameraWorker* worker;
        VideoWidget* displayWidget;
    };

    QMap<int, CameraInfo> m_cameras;
    QTimer* m_connectionTimer;
    bool m_isInitialized;

    static const int CAMERA_REFRESH_INTERVAL = 3000; // 3秒检查一次连接状态
};

/**
 * @brief 摄像头工作线程 - 在独立线程中处理摄像头数据
 *
 * 这个类在独立线程中运行，确保视频处理不会阻塞主线程
 */
class CameraWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraWorker(int cameraId, QObject *parent = nullptr);
    ~CameraWorker();

public slots:
    void start();
    void stop();
    void captureFrame();
    void setResolution(int width, int height);
    void setFps(int fps);

signals:
    void frameReady(const QPixmap& frame);
    void frameAvailable(const cv::Mat& frame);
    void errorOccurred(const QString& error);
    void statusChanged(bool isRunning);
    void finished();

private slots:
    void onTimerTimeout();
    void handleCameraError(QCamera::Error error);

private:
    void processFrame(cv::Mat& frame);
    cv::Mat matToPixmap(const cv::Mat& frame);

    int m_cameraId;
    bool m_isRunning;

    // OpenCV Camera (可选)
#ifdef OPENCV_FOUND
    cv::VideoCapture* m_cvCapture;
#endif

    // 控制参数
    QTimer* m_timer;
    int m_targetFps;

    // 线程安全
    QMutex m_mutex;
};

QT_END_NAMESPACE

#endif // CAMERA_MANAGER_H