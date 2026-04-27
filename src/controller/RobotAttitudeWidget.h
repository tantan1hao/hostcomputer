#ifndef ROBOTATTITUDEWIDGET_H
#define ROBOTATTITUDEWIDGET_H

#include <QWidget>

class QQuickWidget;
class RobotViewModel;

class RobotAttitudeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RobotAttitudeWidget(QWidget *parent = nullptr);

    void updateAttitude(double roll, double pitch, double yaw);
    void updateLegs(double leg1, double leg2, double leg3, double leg4);
    void resetView();

private:
    QQuickWidget *m_robotView = nullptr;
    RobotViewModel *m_viewModel = nullptr;
};

#endif // ROBOTATTITUDEWIDGET_H
