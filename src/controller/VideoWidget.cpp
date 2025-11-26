#include "VideoWidget.h"
#include <QDebug>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_aspectRatioMode(Qt::KeepAspectRatio)
    , m_keepAspectRatio(true)
    , m_needRescale(false)
    , m_updateTimer(new QTimer(this))
{
    // 设置背景颜色
    setStyleSheet("background-color: #000000;");

    // 配置更新定时器
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        if (!m_scaledFrame.isNull() && isVisible()) {
            update(); // 触发重绘
        }
    });

    // 启动定时器
    m_updateTimer->start();

    qDebug() << "VideoWidget created with update interval:" << UPDATE_INTERVAL_MS << "ms";
}

VideoWidget::~VideoWidget()
{
    qDebug() << "VideoWidget destroyed";
}

void VideoWidget::updateFrame(const QPixmap &frame)
{
    if (frame.isNull()) {
        qWarning() << "VideoWidget: Received null frame";
        return;
    }

    m_currentFrame = frame;
    m_needRescale = true;

    // 如果控件可见，立即更新
    if (isVisible()) {
        updateScaledFrame();
        update(); // 触发paintEvent
    }
}

#ifdef OPENCV_FOUND
void VideoWidget::updateFrame(const cv::Mat &frame)
{
    if (frame.empty()) {
        qWarning() << "VideoWidget: Received empty OpenCV frame";
        return;
    }

    // 转换OpenCV Mat为QPixmap
    QImage::Format format;
    if (frame.channels() == 3) {
        format = QImage::Format_RGB888;
    } else if (frame.channels() == 1) {
        format = QImage::Format_Grayscale8;
    } else {
        format = QImage::Format_ARGB32;
    }

    QImage img(frame.data, frame.cols, frame.rows, frame.step, format);
    if (frame.channels() == 3) {
        img = img.rgbSwapped(); // OpenCV uses BGR, Qt uses RGB
    }

    QPixmap pixmap = QPixmap::fromImage(img);
    updateFrame(pixmap);
}
#endif

void VideoWidget::clearFrame()
{
    m_currentFrame = QPixmap();
    m_scaledFrame = QPixmap();
    update(); // 清空显示
}

void VideoWidget::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_aspectRatioMode = mode;
    m_keepAspectRatio = (mode == Qt::KeepAspectRatio || mode == Qt::KeepAspectRatioByExpanding);
    m_needRescale = true;

    if (!m_currentFrame.isNull()) {
        updateScaledFrame();
    }
}

void VideoWidget::setScaleMode(bool keepAspectRatio)
{
    m_keepAspectRatio = keepAspectRatio;
    m_needRescale = true;

    if (!m_currentFrame.isNull()) {
        updateScaledFrame();
    }
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_scaledFrame.isNull()) {
        // 显示占位符文本
        painter.fillRect(rect(), QColor(48, 48, 48));
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(rect(), Qt::AlignCenter, "等待视频输入...");
        return;
    }

    // 绘制缩放后的帧
    painter.drawPixmap(m_targetRect, m_scaledFrame);
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    m_needRescale = true;

    if (!m_currentFrame.isNull()) {
        updateScaledFrame();
    }
}

void VideoWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    // 控件变为可见时，重新处理帧
    if (!m_currentFrame.isNull()) {
        m_needRescale = true;
        updateScaledFrame();
    }
}

void VideoWidget::updateScaledFrame()
{
    if (m_currentFrame.isNull() || !m_needRescale) {
        return;
    }

    QSize frameSize = m_currentFrame.size();
    QSize widgetSize = size();

    if (frameSize.isEmpty() || widgetSize.isEmpty()) {
        return;
    }

    // 计算目标尺寸
    calculateScaledRect(frameSize, widgetSize);

    // 缩放帧到目标尺寸
    if (m_keepAspectRatio) {
        // 保持宽高比，填充到目标矩形
        m_scaledFrame = m_currentFrame.scaled(
            m_targetRect.size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    } else {
        // 拉伸填充整个控件
        m_scaledFrame = m_currentFrame.scaled(
            widgetSize,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation
        );
        m_targetRect = rect();
    }

    m_needRescale = false;
}

void VideoWidget::calculateScaledRect(const QSize &frameSize, const QSize &widgetSize)
{
    if (!m_keepAspectRatio) {
        // 不保持宽高比，直接使用控件尺寸
        m_targetRect = QRect(0, 0, widgetSize.width(), widgetSize.height());
        return;
    }

    // 保持宽高比的计算
    double frameRatio = static_cast<double>(frameSize.width()) / frameSize.height();
    double widgetRatio = static_cast<double>(widgetSize.width()) / widgetSize.height();

    QSize targetSize;

    if (frameRatio > widgetRatio) {
        // 视频更宽，以控件宽度为准
        targetSize.setWidth(widgetSize.width());
        targetSize.setHeight(static_cast<int>(widgetSize.width() / frameRatio));
    } else {
        // 视频更高，以控件高度为准
        targetSize.setHeight(widgetSize.height());
        targetSize.setWidth(static_cast<int>(widgetSize.height() * frameRatio));
    }

    // 居中对齐
    int x = (widgetSize.width() - targetSize.width()) / 2;
    int y = (widgetSize.height() - targetSize.height()) / 2;

    m_targetRect = QRect(x, y, targetSize.width(), targetSize.height());
}