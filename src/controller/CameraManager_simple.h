#ifndef CAMERA_MANAGER_SIMPLE_H
#define CAMERA_MANAGER_SIMPLE_H

#include <QObject>
#include <QPixmap>
#include <QTimer>
#include <QMap>
#include "VideoWidget.h"

/**
 * @brief 简化版摄像头管理器
 * 专门用于管理VideoWidget显示
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

    // 测试视频生成
    void startTestVideo(int cameraId);

signals:
    // 状态信号
    void cameraStarted(int cameraId, const QString& cameraName);
    void cameraStopped(int cameraId);
    void cameraError(int cameraId, const QString& error);

    // 列表更新信号
    void cameraListChanged(const QStringList& cameraNames);

public slots:
    void startAllCameras();
    void restartAllCameras();

private slots:
    void generateTestFrame();
    void checkCameraConnection();

private:
    void setupCameraList();
    QPixmap generateTestPixmap(int cameraId, int frameNumber);

private:
    struct CameraInfo {
        QString name;
        QString description;
        bool isRunning;
        VideoWidget* displayWidget;
    };

    QMap<int, CameraInfo> m_cameras;
    QTimer* m_testTimer;
    QTimer* m_connectionTimer;
    int m_frameCounter;
    bool m_isInitialized;

    static const int TEST_FPS = 10; // 10 FPS 测试视频
};

#endif // CAMERA_MANAGER_SIMPLE_H