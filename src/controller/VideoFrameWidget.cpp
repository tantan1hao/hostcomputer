#include "VideoFrameWidget.h"

#include <QPainter>

VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setMinimumSize(160, 90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoFrameWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void VideoFrameWidget::clearFrame()
{
    m_frame = QImage();
    update();
}

void VideoFrameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 10));

    if (m_frame.isNull()) {
        return;
    }

    const QSize scaledSize = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(
        (width() - scaledSize.width()) / 2,
        (height() - scaledSize.height()) / 2,
        scaledSize.width(),
        scaledSize.height());

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, m_frame);
}
