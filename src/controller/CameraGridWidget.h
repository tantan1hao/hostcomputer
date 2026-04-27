#ifndef CAMERAGRIDWIDGET_H
#define CAMERAGRIDWIDGET_H

#include <QWidget>
#include <QString>
#include <array>

class DisplayLayoutManager;
class RtspPlayerWidget;

class CameraGridWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kCameraCount = 5;

    explicit CameraGridWidget(QWidget *parent = nullptr);

    void setCameraInfo(int cameraId, const QString &rtspUrl, bool online,
                       const QString &codec, int width, int height,
                       int fps, int bitrateKbps);
    void setAuxiliaryWidget(QWidget *widget);

private:
    DisplayLayoutManager *m_displayLayout = nullptr;
    std::array<RtspPlayerWidget*, kCameraCount> m_rtspWidgets = {};
    QWidget *m_auxiliaryWidget = nullptr;
};

#endif // CAMERAGRIDWIDGET_H
