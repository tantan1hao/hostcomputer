#include "MotorRuntimeCarouselWidget.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QString enabledText(bool enabled)
{
    return enabled ? QStringLiteral("已使能") : QStringLiteral("未使能");
}

QString stateText(const Communication::JointRuntimeState &state)
{
    return state.lifecycleState.isEmpty() ? QStringLiteral("--") : state.lifecycleState;
}

} // namespace

MotorRuntimeCarouselWidget::MotorRuntimeCarouselWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto *topLayout = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("12 电机生命状态"), this);
    title->setStyleSheet("font-weight: 700; color: #111827;");
    m_summaryLabel = new QLabel(QStringLiteral("等待运行态"), this);
    m_summaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_summaryLabel->setStyleSheet("color: #64748b; font-size: 12px;");
    topLayout->addWidget(title);
    topLayout->addStretch();
    topLayout->addWidget(m_summaryLabel);

    m_pages = new QStackedWidget(this);
    m_pages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *navLayout = new QHBoxLayout();
    m_prevButton = new QPushButton(QStringLiteral("‹"), this);
    m_nextButton = new QPushButton(QStringLiteral("›"), this);
    m_pageLabel = new QLabel(QStringLiteral("0 / 0"), this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_prevButton->setFixedSize(28, 24);
    m_nextButton->setFixedSize(28, 24);
    navLayout->addStretch();
    navLayout->addWidget(m_prevButton);
    navLayout->addWidget(m_pageLabel);
    navLayout->addWidget(m_nextButton);
    navLayout->addStretch();

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_pages, 1);
    mainLayout->addLayout(navLayout);

    connect(m_prevButton, &QPushButton::clicked, this, &MotorRuntimeCarouselWidget::showPreviousPage);
    connect(m_nextButton, &QPushButton::clicked, this, &MotorRuntimeCarouselWidget::showNextPage);

    setStyleSheet(
        "MotorRuntimeCarouselWidget { background: #ffffff; border: 1px solid #dbe3ef; border-radius: 6px; }"
        "MotorRuntimeCarouselWidget QLabel { color: #111827; }"
        "MotorRuntimeCarouselWidget QPushButton { min-height: 22px; padding: 0 8px; }");

    rebuildPages();
}

void MotorRuntimeCarouselWidget::setRuntimeStates(const Communication::JointRuntimeStateList &states)
{
    m_states = states;
    rebuildPages();
}

void MotorRuntimeCarouselWidget::showPreviousPage()
{
    if (!m_pages || m_pages->count() <= 1) {
        return;
    }
    const int current = m_pages->currentIndex();
    m_pages->setCurrentIndex((current + m_pages->count() - 1) % m_pages->count());
    updatePageLabel();
    updateNavigation();
}

void MotorRuntimeCarouselWidget::showNextPage()
{
    if (!m_pages || m_pages->count() <= 1) {
        return;
    }
    const int current = m_pages->currentIndex();
    m_pages->setCurrentIndex((current + 1) % m_pages->count());
    updatePageLabel();
    updateNavigation();
}

QWidget *MotorRuntimeCarouselWidget::createPage(int pageIndex)
{
    auto *page = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(createHeaderRow(page));

    for (int row = 0; row < kPageSize; ++row) {
        layout->addWidget(createMotorRow(page, pageIndex * kPageSize + row));
    }

    layout->addStretch();
    return page;
}

QWidget *MotorRuntimeCarouselWidget::createHeaderRow(QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QGridLayout(row);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setHorizontalSpacing(8);
    layout->addWidget(new QLabel(QString(), row), 0, 0);
    layout->addWidget(new QLabel(QStringLiteral("关节"), row), 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("生命周期"), row), 0, 2);
    layout->addWidget(new QLabel(QStringLiteral("使能"), row), 0, 3);
    layout->setColumnStretch(1, 1);
    row->setStyleSheet("QLabel { color: #64748b; font-size: 11px; font-weight: 700; }");
    return row;
}

QWidget *MotorRuntimeCarouselWidget::createMotorRow(QWidget *parent, int stateIndex)
{
    auto *row = new QFrame(parent);
    row->setFrameShape(QFrame::NoFrame);
    auto *layout = new QGridLayout(row);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setHorizontalSpacing(8);

    auto *dot = new QLabel(row);
    dot->setFixedSize(9, 9);
    auto *name = new QLabel(QStringLiteral("--"), row);
    auto *lifecycle = new QLabel(QStringLiteral("--"), row);
    auto *enabled = new QLabel(QStringLiteral("--"), row);

    name->setStyleSheet("font-weight: 700; color: #111827;");
    lifecycle->setStyleSheet("color: #475569; font-weight: 600;");
    enabled->setStyleSheet("color: #475569; font-weight: 600;");
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    if (stateIndex >= 0 && stateIndex < m_states.size()) {
        const auto &state = m_states[stateIndex];
        name->setText(state.jointName.isEmpty() ? QStringLiteral("--") : state.jointName);
        lifecycle->setText(stateText(state));
        enabled->setText(enabledText(state.enabled));
        setHealthStyle(dot, state);
    } else {
        dot->setStyleSheet("border-radius: 4px; background: #cbd5e1;");
        row->setEnabled(false);
    }

    layout->addWidget(dot, 0, 0, Qt::AlignCenter);
    layout->addWidget(name, 0, 1);
    layout->addWidget(lifecycle, 0, 2);
    layout->addWidget(enabled, 0, 3);
    layout->setColumnStretch(1, 1);

    row->setStyleSheet("QFrame { background: #fbfcfe; border: 1px solid #e2e8f0; border-radius: 5px; }");
    return row;
}

void MotorRuntimeCarouselWidget::rebuildPages()
{
    const int previousIndex = m_pages ? m_pages->currentIndex() : 0;
    while (m_pages->count() > 0) {
        QWidget *page = m_pages->widget(0);
        m_pages->removeWidget(page);
        page->deleteLater();
    }

    const int pageCount = qMax(1, (m_states.size() + kPageSize - 1) / kPageSize);
    for (int page = 0; page < pageCount; ++page) {
        m_pages->addWidget(createPage(page));
    }
    m_pages->setCurrentIndex(qBound(0, previousIndex, m_pages->count() - 1));

    int onlineCount = 0;
    int faultCount = 0;
    for (const auto &state : m_states) {
        if (state.online) {
            ++onlineCount;
        }
        if (state.fault) {
            ++faultCount;
        }
    }
    m_summaryLabel->setText(m_states.isEmpty()
                                ? QStringLiteral("等待运行态")
                                : QStringLiteral("%1 在线 / %2 fault").arg(onlineCount).arg(faultCount));
    updatePageLabel();
    updateNavigation();
}

void MotorRuntimeCarouselWidget::updatePageLabel()
{
    if (!m_pageLabel || !m_pages) {
        return;
    }
    m_pageLabel->setText(QStringLiteral("%1 / %2").arg(m_pages->currentIndex() + 1).arg(m_pages->count()));
}

void MotorRuntimeCarouselWidget::updateNavigation()
{
    const bool hasPages = m_pages && m_pages->count() > 1;
    m_prevButton->setEnabled(hasPages);
    m_nextButton->setEnabled(hasPages);
}

void MotorRuntimeCarouselWidget::setHealthStyle(QLabel *dot, const Communication::JointRuntimeState &state)
{
    if (state.fault) {
        dot->setStyleSheet("border-radius: 4px; background: #c92a2a;");
        return;
    }
    if (!state.online) {
        dot->setStyleSheet("border-radius: 4px; background: #b7791f;");
        return;
    }
    dot->setStyleSheet("border-radius: 4px; background: #12805c;");
}
