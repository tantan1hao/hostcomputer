#include "RobotAttitudeWidget.h"

#include "RobotViewModel.h"

#include <QQmlContext>
#include <QQuickWidget>
#include <QVBoxLayout>
#include <QDebug>

RobotAttitudeWidget::RobotAttitudeWidget(QWidget *parent)
    : QWidget(parent)
    , m_robotView(new QQuickWidget(this))
    , m_viewModel(new RobotViewModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_robotView);

    m_robotView->rootContext()->setContextProperty("robotViewModel", m_viewModel);
    m_robotView->setSource(QUrl("qrc:/resources/qml/RobotView.qml"));
    m_robotView->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(m_robotView, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status == QQuickWidget::Error) {
            for (const auto &err : m_robotView->errors()) {
                qWarning() << "[RobotView]" << err.toString();
            }
        }
    });
}

void RobotAttitudeWidget::updateAttitude(double roll, double pitch, double yaw)
{
    m_viewModel->updateAttitude(roll, pitch, yaw);
}

void RobotAttitudeWidget::updateLegs(double leg1, double leg2, double leg3, double leg4)
{
    m_viewModel->updateLegs(leg1, leg2, leg3, leg4);
}

void RobotAttitudeWidget::resetView()
{
    updateAttitude(0.0, 0.0, 0.0);
    updateLegs(0.0, 0.0, 0.0, 0.0);
}
