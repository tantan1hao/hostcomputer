#include "CameraGridWidget.h"

#include "DisplayLayoutManager.h"
#include "RtspPlayerWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

CameraGridWidget::CameraGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_viewStack(new QStackedWidget(this))
    , m_displayLayout(new DisplayLayoutManager(2, 3))
    , m_gridPage(new QWidget(this))
    , m_focusPage(new QWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(6);

    m_cameraSelector = new QComboBox(this);
    for (int i = 0; i < kCameraCount; ++i) {
        m_cameraSelector->addItem(QString("摄像头 %1").arg(i + 1), i);
    }

    m_gridButton = new QPushButton("网格", this);
    m_focusButton = new QPushButton("单路", this);
    auto *refreshButton = new QPushButton("刷新源", this);
    auto *fullscreenButton = new QPushButton("区域全屏", this);

    toolbar->addWidget(new QLabel("视频源", this));
    toolbar->addWidget(m_cameraSelector, 1);
    toolbar->addWidget(m_gridButton);
    toolbar->addWidget(m_focusButton);
    toolbar->addWidget(refreshButton);
    toolbar->addStretch();
    toolbar->addWidget(fullscreenButton);

    auto *gridPageLayout = new QVBoxLayout(m_gridPage);
    gridPageLayout->setContentsMargins(0, 0, 0, 0);
    gridPageLayout->addWidget(m_displayLayout);

    m_focusLayout = new QVBoxLayout(m_focusPage);
    m_focusLayout->setContentsMargins(0, 0, 0, 0);

    m_viewStack->addWidget(m_gridPage);
    m_viewStack->addWidget(m_focusPage);

    layout->addLayout(toolbar);
    layout->addWidget(m_viewStack, 1);

    for (int i = 0; i < kCameraCount; ++i) {
        m_rtspWidgets[i] = new RtspPlayerWidget(i);
        m_displayLayout->setWidget(i, m_rtspWidgets[i]);
    }

    connect(m_cameraSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        selectCamera(m_cameraSelector->itemData(index).toInt());
    });
    connect(m_gridButton, &QPushButton::clicked, this, &CameraGridWidget::showGridMode);
    connect(m_focusButton, &QPushButton::clicked, this, &CameraGridWidget::showFocusMode);
    connect(fullscreenButton, &QPushButton::clicked, this, &CameraGridWidget::showFocusMode);
    connect(refreshButton, &QPushButton::clicked, this, &CameraGridWidget::cameraListRefreshRequested);

    updateModeButtons();
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

void CameraGridWidget::selectCamera(int cameraId)
{
    if (cameraId < 0 || cameraId >= kCameraCount) {
        return;
    }

    m_selectedCameraId = cameraId;
    const int selectorIndex = m_cameraSelector->findData(cameraId);
    if (selectorIndex >= 0 && m_cameraSelector->currentIndex() != selectorIndex) {
        m_cameraSelector->setCurrentIndex(selectorIndex);
    }

    if (m_focusMode) {
        moveFocusedCameraBackToGrid();
        moveSelectedCameraToFocus();
    }
}

void CameraGridWidget::showGridMode()
{
    m_focusMode = false;
    moveFocusedCameraBackToGrid();
    m_viewStack->setCurrentWidget(m_gridPage);
    updateModeButtons();
}

void CameraGridWidget::showFocusMode()
{
    m_focusMode = true;
    moveSelectedCameraToFocus();
    m_viewStack->setCurrentWidget(m_focusPage);
    updateModeButtons();
}

void CameraGridWidget::moveFocusedCameraBackToGrid()
{
    if (m_focusedCameraId < 0 || m_focusedCameraId >= kCameraCount) {
        return;
    }

    auto *widget = m_rtspWidgets[m_focusedCameraId];
    if (!widget) {
        m_focusedCameraId = -1;
        return;
    }

    m_focusLayout->removeWidget(widget);
    m_displayLayout->setWidget(m_focusedCameraId, widget);
    m_focusedCameraId = -1;
}

void CameraGridWidget::moveSelectedCameraToFocus()
{
    if (m_selectedCameraId < 0 || m_selectedCameraId >= kCameraCount) {
        return;
    }
    if (m_focusedCameraId == m_selectedCameraId) {
        return;
    }

    moveFocusedCameraBackToGrid();
    QWidget *widget = m_displayLayout->removeWidget(m_selectedCameraId);
    if (!widget) {
        widget = m_rtspWidgets[m_selectedCameraId];
    }
    if (!widget) {
        return;
    }

    m_focusLayout->addWidget(widget);
    m_focusedCameraId = m_selectedCameraId;
}

void CameraGridWidget::updateModeButtons()
{
    m_gridButton->setEnabled(m_focusMode);
    m_focusButton->setEnabled(!m_focusMode);
}
