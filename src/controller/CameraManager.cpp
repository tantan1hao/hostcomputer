#include "CameraManager.h"
#include <QDebug>
#include <QTimer>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QApplication>

QT_BEGIN_NAMESPACE

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_connectionTimer(new QTimer(this))
    , m_isInitialized(false)
{
    setupCameraList();

    // 设置连接状态检查定时器
    m_connectionTimer->setInterval(CAMERA_REFRESH_INTERVAL);
    connect(m_connectionTimer, &QTimer::timeout, this, &CameraManager::checkCameraConnection);
    m_connectionTimer->start();

    qDebug() << "CameraManager initialized";
}

CameraManager::~CameraManager()
{
    stopAllCameras();
    qDebug() << "CameraManager destroyed";
}

void CameraManager::setupCameraList()
{
    refreshCameraList();
}

QStringList CameraManager::getAvailableCameras() const
{
    QStringList names;
    for (auto it = m_cameras.constBegin(); it != m_cameras.constEnd(); ++it) {
        names << it.value().name;
    }
    return names;
}

void CameraManager::refreshCameraList()
{
    // 简化的摄像头列表生成（基于假设的摄像头数量）
    QStringList currentNames = getAvailableCameras();
    QStringList newNames;

    // 生成6个默认摄像头（假设都有）
    for (int i = 1; i <= 6; i++) {
        QString cameraName = QString("Camera %1").arg(i);
        if (!currentNames.contains(cameraName)) {
            newNames << cameraName;
        }
    }

    // 发送摄像头列表更新信号
    emit cameraListChanged(newNames);

    qDebug() << "Available cameras:" << newNames;
}

bool CameraManager::startCamera(int cameraId, VideoWidget* displayWidget)
{
    if (!displayWidget) {
        qWarning() << "Display widget is null for camera" << cameraId;
        return false;
    }

    if (m_cameras.contains(cameraId)) {
        qWarning() << "Camera" << cameraId << "already started";
        return true;
    }

    // 创建工作线程
    CameraWorker* worker = new CameraWorker(cameraId, this);

    // 连接信号槽（跨线程安全）
    connectWorkerSignals(worker, cameraId);

    // 设置分辨率
    if (displayWidget->width() > 0 && displayWidget->height() > 0) {
        worker->setResolution(displayWidget->width(), displayWidget->height());
    }

    // 启动摄像头
    worker->start();

    // 更新信息
    CameraInfo info;
    info.isRunning = true;
    info.worker = worker;
    info.displayWidget = displayWidget;

    // 查找摄像头名称
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (cameraId < cameras.size()) {
        info.name = cameras[cameraId].description();
        if (info.name.isEmpty()) {
            info.name = QString("Camera %1").arg(cameraId + 1);
        }
    } else {
        info.name = QString("Camera %1").arg(cameraId + 1);
    }

    m_cameras[cameraId] = info;

    // 显示启动信息
    displayWidget->setText(QString("%1\n启动中...").arg(info.name));

    qDebug() << "Starting camera" << cameraId << ":" << info.name;

    return true;
}

void CameraManager::stopCamera(int cameraId)
{
    if (!m_cameras.contains(cameraId)) {
        return;
    }

    CameraInfo& info = m_cameras[cameraId];

    if (info.worker) {
        info.worker->stop();
        info.worker->deleteLater();
        info.worker = nullptr;
    }

    if (info.displayWidget) {
        info.displayWidget->clear();
        info.displayWidget->setText(QString("%1\n已停止").arg(info.name));
    }

    info.isRunning = false;

    emit cameraStopped(cameraId);
    qDebug() << "Stopped camera" << cameraId;

    m_cameras.remove(cameraId);
}

void CameraManager::stopAllCameras()
{
    QList<int> cameraIds = m_cameras.keys();
    for (int cameraId : cameraIds) {
        stopCamera(cameraId);
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

void CameraManager::requestFrame(int cameraId)
{
    if (m_cameras.contains(cameraId) && m_cameras[cameraId].worker) {
        // 发送请求帧信号（如果工作线程支持）
        QMetaObject::invokeMethod(m_cameras[cameraId].worker, "captureFrame",
                                  Qt::QueuedConnection);
    }
}

void CameraManager::startAllCameras()
{
    // 自动启动所有可用摄像头（最多6个）
    QList<QCameraInfo> availableCameras = QCameraInfo::availableCameras();
    int maxCameras = qMin(6, availableCameras.size());

    for (int i = 0; i < maxCameras; ++i) {
        QLabel* cameraWidget = nullptr;
        // 尝试从UI查找camera_1, camera_2等
        QString widgetName = QString("camera_%1").arg(i + 1);
        cameraWidget = findChild<QLabel*>(widgetName);

        if (cameraWidget) {
            startCamera(i, cameraWidget);
        }
    }
}

void CameraManager::restartAllCameras()
{
    stopAllCameras();

    // 延迟500ms后重启，确保资源释放
    QTimer::singleShot(500, this, &CameraManager::startAllCameras);
}

void CameraManager::onFrameReady(int cameraId, const QPixmap& frame)
{
    if (!m_cameras.contains(cameraId)) {
        return;
    }

    VideoWidget* display = m_cameras[cameraId].displayWidget;
    if (display) {
        if (!frame.isNull()) {
            display->updateFrame(frame);
        } else {
            display->clearFrame();
        }
        // 转发信号用于通知UI层
        emit videoFrameReady(cameraId, display);
    }

    // 转发信号
    emit frameReady(cameraId, frame);
}

void CameraManager::onCameraError(int cameraId, const QString& error)
{
    qWarning() << "Camera" << cameraId << "error:" << error;

    // 尝试自动重连
    QTimer::singleShot(2000, [this, cameraId]() {
        if (m_cameras.contains(cameraId)) {
            CameraInfo& info = m_cameras[cameraId];
            if (info.displayWidget) {
                startCamera(cameraId, info.displayWidget);
            }
        }
    });

    emit cameraError(cameraId, error);
}

void CameraManager::onCameraStatusChanged(int cameraId, bool isRunning)
{
    if (!m_cameras.contains(cameraId)) {
        return;
    }

    m_cameras[cameraId].isRunning = isRunning;

    if (isRunning) {
        emit cameraStarted(cameraId, m_cameras[cameraId].name);
    } else {
        emit cameraStopped(cameraId);
    }
}

void CameraManager::checkCameraConnection()
{
    // 检查摄像头连接状态
    refreshCameraList();

    // 检查已启动摄像头的状态
    QStringList currentNames = getAvailableCameras();
    QList<int> camerasToRemove;

    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        int cameraId = it.key();
        QString cameraName = it.value().name;

        if (!currentNames.contains(cameraName)) {
            camerasToRemove << cameraId;
        }
    }

    // 停止断开的摄像头
    for (int cameraId : camerasToRemove) {
        qDebug() << "Camera" << cameraId << "disconnected, stopping...";
        stopCamera(cameraId);
    }
}

void CameraManager::connectWorkerSignals(CameraWorker* worker, int cameraId)
{
    connect(worker, &CameraWorker::frameReady,
            this, [this, cameraId](const QPixmap& frame) {
                onFrameReady(cameraId, frame);
            }, Qt::QueuedConnection);

    connect(worker, &CameraWorker::errorOccurred,
            this, [this, cameraId](const QString& error) {
                onCameraError(cameraId, error);
            }, Qt::QueuedConnection);

    connect(worker, &CameraWorker::statusChanged,
            this, [this, cameraId](bool isRunning) {
                onCameraStatusChanged(cameraId, isRunning);
            }, Qt::QueuedConnection);

    connect(worker, &CameraWorker::finished,
            worker, &CameraWorker::deleteLater,
            Qt::QueuedConnection);
}

void CameraManager::disconnectWorkerSignals(CameraWorker* worker)
{
    disconnect(worker, nullptr, nullptr, nullptr);
}

// CameraWorker 实现
CameraWorker::CameraWorker(int cameraId, QObject *parent)
    : QObject(parent)
    , m_cameraId(cameraId)
    , m_isRunning(false)
    , m_useQtCamera(true)
    , m_qtCamera(nullptr)
    , m_viewfinder(nullptr)
    , m_cvCapture(nullptr)
    , m_timer(new QTimer(this))
    , m_targetFps(30)
    , m_resolution(640, 480)
{
    m_timer->setInterval(1000 / m_targetFps);
    connect(m_timer, &QTimer::timeout, this, &CameraWorker::onTimerTimeout);

    qDebug() << "CameraWorker" << m_cameraId << "created";
}

CameraWorker::~CameraWorker()
{
    stop();
    qDebug() << "CameraWorker" << m_cameraId << "destroyed";
}

void CameraWorker::start()
{
    if (m_isRunning) {
        return;
    }

    qDebug() << "Starting camera" << m_cameraId;

    // 优先使用Qt Camera
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (m_cameraId < cameras.size()) {
        m_useQtCamera = true;
        m_qtCamera = new QCamera(cameras[m_cameraId]);
        m_viewfinder = new QCameraViewfinder();

        // 连接信号
        connect(m_qtCamera, QOverload<QCamera::Error>::of(&QCamera::error),
                this, &CameraWorker::handleCameraError);
        connect(m_viewfinder, &QCameraViewfinder::videoChanged,
                this, [this](const QVideoFrame& frame) {
                    if (!m_isRunning) return;

                    QImage image = frame.image();
                    if (!image.isNull()) {
                        QPixmap pixmap = QPixmap::fromImage(image);
                        emit frameReady(pixmap);
                    }
                });

        m_qtCamera->setViewfinder(m_viewfinder);
        m_qtCamera->start();

        m_isRunning = true;
        emit statusChanged(true);
        emit frameReady(QPixmap()); // 发送空帧表示启动成功

        return;
    }

    // 备用方案：使用OpenCV
    m_useQtCamera = false;
    m_cvCapture = new cv::VideoCapture(m_cameraId);

    if (!m_cvCapture->isOpened()) {
        // 尝试使用索引方式
        m_cvCapture = new cv::VideoCapture();
        if (!m_cvCapture->open(m_cameraId)) {
            emit errorOccurred(QString("无法打开摄像头 %1").arg(m_cameraId));
            return;
        }
    }

    m_isRunning = true;
    m_timer->start();
    emit statusChanged(true);
    emit frameReady(QPixmap()); // 发送空帧表示启动成功
}

void CameraWorker::stop()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    m_timer->stop();

    if (m_qtCamera) {
        m_qtCamera->stop();
        m_qtCamera->deleteLater();
        m_qtCamera = nullptr;
    }

    if (m_viewfinder) {
        m_viewfinder->deleteLater();
        m_viewfinder = nullptr;
    }

    if (m_cvCapture) {
        if (m_cvCapture->isOpened()) {
            m_cvCapture->release();
        }
        delete m_cvCapture;
        m_cvCapture = nullptr;
    }

    emit statusChanged(false);
    emit finished();

    qDebug() << "CameraWorker" << m_cameraId << "stopped";
}

void CameraWorker::captureFrame()
{
    if (!m_isRunning) {
        return;
    }

    if (m_useQtCamera && m_qtCamera && m_viewfinder) {
        // Qt Camera会自动发送帧，这里不需要手动捕获
        return;
    }

    if (m_cvCapture && m_cvCapture->isOpened()) {
        cv::Mat frame;
        if (m_cvCapture->read(frame) && !frame.empty()) {
            // 处理帧并发送
            processFrame(frame);

            QPixmap pixmap = matToPixmap(frame);
            if (!pixmap.isNull()) {
                emit frameReady(pixmap);
                emit frameAvailable(frame);
            }
        }
    }
}

void CameraWorker::setResolution(int width, int height)
{
    m_resolution = cv::Size(width, height);

    if (m_qtCamera) {
        // Qt Camera会在启动时自动适配分辨率
    }
}

void CameraWorker::setFps(int fps)
{
    m_targetFps = fps;
    if (m_timer->interval() != 1000 / m_targetFps) {
        m_timer->setInterval(1000 / m_targetFps);
    }
}

void CameraWorker::onTimerTimeout()
{
    captureFrame();
}

void CameraWorker::handleCameraError(QCamera::Error error)
{
    QString errorMessage;
    switch (error) {
        case QCamera::NoError:
            errorMessage = "No error";
            break;
        case QCamera::CameraError:
            errorMessage = "Camera error";
            break;
        case QCamera::InvalidRequestError:
            errorMessage = "Invalid request";
            break;
        case QCamera::ServiceMissingError:
            errorMessage = "Camera service missing";
            break;
        case QCamera::NotSupportedFeatureError:
            errorMessage = "Feature not supported";
            break;
        default:
            errorMessage = QString("Unknown camera error: %1").arg(error);
            break;
    }

    qWarning() << "Camera" << m_cameraId << "error:" << errorMessage;
    emit errorOccurred(errorMessage);
}

void CameraWorker::processFrame(cv::Mat& frame)
{
    if (frame.empty()) {
        return;
    }

    // 可以在这里添加图像处理代码
    // 例如：降噪、增强、格式转换等
}

cv::Mat CameraWorker::matToPixmap(const cv::Mat& frame)
{
    if (frame.empty()) {
        return cv::Mat();
    }

    cv::Mat rgbFrame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    } else if (frame.channels() == 1) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_GRAY2RGB);
    } else {
        return cv::Mat();
    }

    QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
               rgbFrame.step(), QImage::Format_RGB888);

    return QPixmap::fromImage(img);
}

QT_END_NAMESPACE