#ifndef CAMERAGRIDWIDGET_H
#define CAMERAGRIDWIDGET_H

#include <QWidget>
#include <QString>
#include <array>

class DisplayLayoutManager;
class QLabel;
class QComboBox;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
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

signals:
    void cameraListRefreshRequested();

private slots:
    void selectCamera(int cameraId);
    void showGridMode();
    void showFocusMode();

private:
    void moveFocusedCameraBackToGrid();
    void moveSelectedCameraToFocus();
    void updateModeButtons();

    QStackedWidget *m_viewStack = nullptr;
    DisplayLayoutManager *m_displayLayout = nullptr;
    QWidget *m_gridPage = nullptr;
    QWidget *m_focusPage = nullptr;
    QVBoxLayout *m_focusLayout = nullptr;
    QComboBox *m_cameraSelector = nullptr;
    QPushButton *m_gridButton = nullptr;
    QPushButton *m_focusButton = nullptr;
    std::array<RtspPlayerWidget*, kCameraCount> m_rtspWidgets = {};
    QWidget *m_auxiliaryWidget = nullptr;
    int m_selectedCameraId = 0;
    int m_focusedCameraId = -1;
    bool m_focusMode = false;
};

#endif // CAMERAGRIDWIDGET_H
