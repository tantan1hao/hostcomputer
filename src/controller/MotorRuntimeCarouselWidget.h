#ifndef MOTORRUNTIMECAROUSELWIDGET_H
#define MOTORRUNTIMECAROUSELWIDGET_H

#include <QWidget>
#include "SharedStructs.h"

class QLabel;
class QPushButton;
class QStackedWidget;

class MotorRuntimeCarouselWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorRuntimeCarouselWidget(QWidget *parent = nullptr);

    void setRuntimeStates(const Communication::JointRuntimeStateList &states);

private slots:
    void showPreviousPage();
    void showNextPage();

private:
    QWidget *createPage(int pageIndex);
    QWidget *createHeaderRow(QWidget *parent);
    QWidget *createMotorRow(QWidget *parent, int localIndex);
    void rebuildPages();
    void updatePageLabel();
    void updateNavigation();
    void setHealthStyle(QLabel *dot, const Communication::JointRuntimeState &state);

    static constexpr int kPageSize = 4;

    Communication::JointRuntimeStateList m_states;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_prevButton = nullptr;
    QPushButton *m_nextButton = nullptr;
};

#endif // MOTORRUNTIMECAROUSELWIDGET_H
