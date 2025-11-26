#include "CameraManager_simple.h"
#include <QDateTime>
#include <QPainter>
#include <QDebug>

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_testTimer(new QTimer(this))
    , m_connectionTimer(new QTimer(this))
    , m_frameCounter(0)
    , m_isInitialized(false)
{
    setupCameraList();

    // 设置测试视频生成定时器
    m_testTimer->setInterval(1000 / TEST_FPS);
    connect(m_testTimer, &QTimer::timeout, this, &CameraManager::generateTestFrame);

    // 设置连接状态检查定时器
    m_connectionTimer->setInterval(3000);
    connect(m_connectionTimer, &QTimer::timeout, this, &CameraManager::checkCameraConnection);
    m_connectionTimer->start();

    qDebug() << "CameraManager (simplified) initialized";
}

CameraManager::~CameraManager()
{
    stopAllCameras();
    qDebug() << "CameraManager (simplified) destroyed";
}

void CameraManager::setupCameraList()
{
    // 生成6个默认摄像头
    for (int i = 1; i <= 6; i++) {
        CameraInfo info;
        info.name = QString("Camera %1").arg(i);
        info.description = QString("Test camera %1").arg(i);
        info.isRunning = false;
        info.displayWidget = nullptr;

        m_cameras[i] = info;
    }

    QStringList cameraNames = getAvailableCameras();
    emit cameraListChanged(cameraNames);

    qDebug() << "Setup cameras:" << cameraNames;
}

QStringList CameraManager::getAvailableCameras() const
{
    QStringList names;
    for (auto it = m_cameras.constBegin(); it != m_cameras.constEnd(); ++it) {
        names << it.value().name;
    }
    return names;
}

bool CameraManager::startCamera(int cameraId, VideoWidget* displayWidget)
{
    if (!displayWidget) {
        qWarning() << "Display widget is null for camera" << cameraId;
        return false;
    }

    if (!m_cameras.contains(cameraId)) {
        qWarning() << "Camera" << cameraId << "not found";
        return false;
    }

    // 停止已有的摄像头
    if (m_cameras[cameraId].isRunning) {
        stopCamera(cameraId);
    }

    // 配置摄像头信息
    m_cameras[cameraId].displayWidget = displayWidget;
    m_cameras[cameraId].isRunning = true;

    // 清空显示控件
    displayWidget->clearFrame();

    qDebug() << "Camera" << cameraId << "started";
    emit cameraStarted(cameraId, m_cameras[cameraId].name);

    // 如果这是第一个摄像头，启动测试视频生成
    if (m_testTimer && !m_testTimer->isActive()) {
        m_testTimer->start();
    }

    return true;
}

void CameraManager::stopCamera(int cameraId)
{
    if (!m_cameras.contains(cameraId)) {
        return;
    }

    if (m_cameras[cameraId].isRunning) {
        // 清空显示控件
        if (m_cameras[cameraId].displayWidget) {
            m_cameras[cameraId].displayWidget->clearFrame();
        }

        m_cameras[cameraId].isRunning = false;
        m_cameras[cameraId].displayWidget = nullptr;

        qDebug() << "Camera" << cameraId << "stopped";
        emit cameraStopped(cameraId);
    }

    // 如果没有摄像头在运行，停止测试视频生成
    bool hasRunningCamera = false;
    for (auto it = m_cameras.constBegin(); it != m_cameras.constEnd(); ++it) {
        if (it.value().isRunning) {
            hasRunningCamera = true;
            break;
        }
    }

    if (!hasRunningCamera && m_testTimer->isActive()) {
        m_testTimer->stop();
    }
}

void CameraManager::stopAllCameras()
{
    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        stopCamera(it.key());
    }
}

bool CameraManager::isCameraRunning(int cameraId) const
{
    return m_cameras.contains(cameraId) && m_cameras[cameraId].isRunning;
}

int CameraManager::getCameraCount() const
{
    return m_cameras.size();
}

void CameraManager::startAllCameras()
{
    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        if (it.value().displayWidget && !it.value().isRunning) {
            startCamera(it.key(), it.value().displayWidget);
        }
    }
}

void CameraManager::restartAllCameras()
{
    stopAllCameras();
    QTimer::singleShot(500, this, &CameraManager::startAllCameras);
}

void CameraManager::generateTestFrame()
{
    m_frameCounter++;

    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        int cameraId = it.key();
        CameraInfo& info = it.value();

        if (info.isRunning && info.displayWidget) {
            QPixmap testFrame = generateTestPixmap(cameraId, m_frameCounter);
            info.displayWidget->updateFrame(testFrame);
        }
    }
}

QPixmap CameraManager::generateTestPixmap(int cameraId, int frameNumber)
{
    QSize size(390, 290); // 与UI控件大小一致
    QPixmap pixmap(size);
    pixmap.fill(Qt::black);

    QPainter painter(&pixmap);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16, QFont::Bold));

    // 绘制摄像头ID
    QString cameraText = QString("Camera %1").arg(cameraId);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, cameraText);

    // 绘制时间戳和帧计数
    painter.setFont(QFont("Arial", 10));
    QString infoText = QString("Frame: %1\nTime: %2")
                      .arg(frameNumber)
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    painter.drawText(10, size.height() - 40, infoText);

    // 绘制简单的动画效果
    int radius = (frameNumber * 2) % 100 + 20;
    painter.setPen(QPen(QColor(255, 255, 0), 2));
    painter.drawEllipse(size.width() / 2 - radius, size.height() / 2 - radius,
                       radius * 2, radius * 2);

    return pixmap;
}

void CameraManager::checkCameraConnection()
{
    // 简化版本：只是检查连接状态
    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        int cameraId = it.key();
        CameraInfo& info = it.value();

        if (info.isRunning && !info.displayWidget) {
            qWarning() << "Camera" << cameraId << "lost display widget, stopping";
            stopCamera(cameraId);
            emit cameraError(cameraId, "Display widget disconnected");
        }
    }
}