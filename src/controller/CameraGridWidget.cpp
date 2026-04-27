#include "CameraGridWidget.h"

#include "DisplayLayoutManager.h"
#include "RtspPlayerWidget.h"

#include <QLabel>
#include <QVBoxLayout>

CameraGridWidget::CameraGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_displayLayout(new DisplayLayoutManager(2, 3, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_displayLayout);

    for (int i = 0; i < kCameraCount; ++i) {
        m_rtspWidgets[i] = new RtspPlayerWidget(i);
        m_displayLayout->setWidget(i, m_rtspWidgets[i]);
    }

    auto *placeholder = new QLabel("状态面板预留", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #777; background-color: #1a1a1a; border: 1px solid #333;");
    setAuxiliaryWidget(placeholder);
}

void CameraGridWidget::setCameraInfo(int cameraId, const QString &rtspUrl, bool online,
                                     const QString &codec, int width, int height,
                                     int fps, int bitrateKbps)
{
    if (cameraId < 0 || cameraId >= kCameraCount || !m_rtspWidgets[cameraId]) {
        return;
    }

    m_rtspWidgets[cameraId]->setCameraInfo(rtspUrl, online, codec, width, height, fps, bitrateKbps);
}

void CameraGridWidget::setAuxiliaryWidget(QWidget *widget)
{
    if (!widget || widget == m_auxiliaryWidget) {
        return;
    }

    QWidget *old = m_displayLayout->removeWidget(kCameraCount);
    if (old && old != widget) {
        old->deleteLater();
    }

    m_auxiliaryWidget = widget;
    m_displayLayout->setWidget(kCameraCount, widget);
}
