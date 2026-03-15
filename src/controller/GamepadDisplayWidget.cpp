#include "GamepadDisplayWidget.h"
#include <QPainterPath>
#include <QFont>
#include <QtMath>

// ============================================================
//  StickVisualWidget - 摇杆圆盘可视化
// ============================================================

StickVisualWidget::StickVisualWidget(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title)
{
    setMinimumSize(50, 60);
    setMaximumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAttribute(Qt::WA_StyledBackground, false);
    setStyleSheet("background: transparent; border: none;");
}

void StickVisualWidget::setPosition(float x, float y)
{
    m_x = qBound(-1.0f, x, 1.0f);
    m_y = qBound(-1.0f, y, 1.0f);
    update();
}

void StickVisualWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int side = qMin(width(), height() - 22);
    const int radius = side / 2 - 4;
    const QPointF center(width() / 2.0, 12 + side / 2.0);

    // --- 标题 ---
    QFont titleFont;
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor("#555"));
    p.drawText(QRectF(0, 0, width(), 14), Qt::AlignCenter, m_title);

    // --- 外圈底盘 ---
    QRadialGradient bgGrad(center, radius);
    bgGrad.setColorAt(0.0, QColor("#e8e8e8"));
    bgGrad.setColorAt(1.0, QColor("#d0d0d0"));
    p.setBrush(bgGrad);
    p.setPen(QPen(QColor("#bbb"), 1.5));
    p.drawEllipse(center, radius, radius);

    // --- 十字准线 ---
    p.setPen(QPen(QColor("#ccc"), 1, Qt::DashLine));
    p.drawLine(QPointF(center.x() - radius, center.y()),
               QPointF(center.x() + radius, center.y()));
    p.drawLine(QPointF(center.x(), center.y() - radius),
               QPointF(center.x(), center.y() + radius));

    // --- 摇杆小圆点 ---
    const float dotX = center.x() + m_x * (radius - 5);
    const float dotY = center.y() - m_y * (radius - 5);   // Y轴取反：上为正
    const int dotR = 5;

    // 距离中心越远颜色越红
    float dist = qSqrt(m_x * m_x + m_y * m_y);
    dist = qMin(dist, 1.0f);
    int r = static_cast<int>(80 + 175 * dist);
    int g = static_cast<int>(180 - 130 * dist);
    int b = static_cast<int>(220 - 170 * dist);
    QColor dotColor(r, g, b);

    // 发光阴影
    QRadialGradient glowGrad(QPointF(dotX, dotY), dotR + 4);
    glowGrad.setColorAt(0.0, QColor(dotColor.red(), dotColor.green(), dotColor.blue(), 100));
    glowGrad.setColorAt(1.0, QColor(dotColor.red(), dotColor.green(), dotColor.blue(), 0));
    p.setBrush(glowGrad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(dotX, dotY), dotR + 4, dotR + 4);

    // 实心圆点
    QRadialGradient dotGrad(QPointF(dotX - 2, dotY - 2), dotR);
    dotGrad.setColorAt(0.0, dotColor.lighter(140));
    dotGrad.setColorAt(1.0, dotColor);
    p.setBrush(dotGrad);
    p.setPen(QPen(dotColor.darker(130), 1.2));
    p.drawEllipse(QPointF(dotX, dotY), dotR, dotR);

    // --- 数值文字 ---
    QFont valFont;
    valFont.setPointSize(8);
    p.setFont(valFont);
    p.setPen(QColor("#666"));
    QString valText = QString("(%1, %2)").arg(m_x, 0, 'f', 2).arg(m_y, 0, 'f', 2);
    p.drawText(QRectF(0, center.y() + radius + 2, width(), 16), Qt::AlignCenter, valText);
}

// ============================================================
//  TriggerBarWidget - 扳机进度条
// ============================================================

TriggerBarWidget::TriggerBarWidget(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title)
{
    setMinimumSize(60, 24);
    setMaximumHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, false);
    setStyleSheet("background: transparent; border: none;");
}

void TriggerBarWidget::setValue(float value)
{
    m_value = qBound(0.0f, value, 1.0f);
    update();
}

void TriggerBarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int margin = 2;
    const int titleW = 22;
    const int valW = 30;
    const int barX = titleW + 4;
    const int barY = margin;
    const int barW = width() - titleW - valW - 8;
    const int barH = height() - margin * 2;

    // --- 标题 ---
    QFont titleFont;
    titleFont.setPointSize(8);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor("#555"));
    p.drawText(QRectF(0, 0, titleW, height()), Qt::AlignCenter, m_title);

    // --- 背景槽 ---
    p.setBrush(QColor("#ddd"));
    p.setPen(QPen(QColor("#bbb"), 1));
    p.drawRoundedRect(barX, barY, barW, barH, 4, 4);

    // --- 填充 (从左往右) ---
    int fillW = static_cast<int>(barW * m_value);
    if (fillW > 0) {
        QLinearGradient fillGrad(barX, barY, barX + fillW, barY);
        int r = static_cast<int>(100 + 155 * m_value);
        int g = static_cast<int>(200 - 120 * m_value);
        fillGrad.setColorAt(0.0, QColor(r, g, 80));
        fillGrad.setColorAt(1.0, QColor(r, g, 60).darker(110));
        p.setBrush(fillGrad);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barX, barY, fillW, barH, 4, 4);
    }

    // --- 数值 ---
    QFont valFont;
    valFont.setPointSize(8);
    p.setFont(valFont);
    p.setPen(QColor("#666"));
    p.drawText(QRectF(barX + barW + 4, 0, valW, height()),
               Qt::AlignCenter, QString::number(m_value, 'f', 2));
}

// ============================================================
//  GamepadDisplayWidget - 组合控件
// ============================================================

GamepadDisplayWidget::GamepadDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(1);

    // --- 标题 ---
    m_titleLabel = new QLabel("手柄映射", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "font-size: 12px; font-weight: bold; color: #333; "
        "border: none; background: transparent;");
    mainLayout->addWidget(m_titleLabel);

    // --- 摇杆 水平布局 ---
    auto *stickLayout = new QHBoxLayout();
    stickLayout->setSpacing(2);

    m_leftStick = new StickVisualWidget("左摇杆", this);
    m_rightStick = new StickVisualWidget("右摇杆", this);

    stickLayout->addWidget(m_leftStick, 1);
    stickLayout->addWidget(m_rightStick, 1);

    mainLayout->addLayout(stickLayout);

    // --- 扳机 水平布局 ---
    auto *triggerLayout = new QHBoxLayout();
    triggerLayout->setSpacing(2);

    m_ltBar = new TriggerBarWidget("LT", this);
    m_rtBar = new TriggerBarWidget("RT", this);

    triggerLayout->addWidget(m_ltBar, 1);
    triggerLayout->addWidget(m_rtBar, 1);

    mainLayout->addLayout(triggerLayout);

    // --- 按钮状态 ---
    m_buttonStatusLabel = new QLabel("按钮: --", this);
    QFont btnFont;
    btnFont.setPointSize(8);
    m_buttonStatusLabel->setFont(btnFont);
    m_buttonStatusLabel->setAlignment(Qt::AlignCenter);
    m_buttonStatusLabel->setMaximumHeight(18);
    m_buttonStatusLabel->setStyleSheet(
        "color: #666; border: none; background: transparent;");
    mainLayout->addWidget(m_buttonStatusLabel);

    setStyleSheet(
        "GamepadDisplayWidget { "
        "  background-color: #f5f5f5; "
        "  border: 1px solid #ddd; "
        "  border-radius: 6px; "
        "}"
        "GamepadDisplayWidget QLabel { "
        "  background: transparent; "
        "  border: none; "
        "}"
        "GamepadDisplayWidget StickVisualWidget { "
        "  background: transparent; "
        "  border: none; "
        "}"
        "GamepadDisplayWidget TriggerBarWidget { "
        "  background: transparent; "
        "  border: none; "
        "}");
}

void GamepadDisplayWidget::updateAxis(const QString &axisName, float value)
{
    if (axisName == "LX" || axisName == "左摇杆X") {
        m_lx = value;
        m_leftStick->setPosition(m_lx, m_ly);
    } else if (axisName == "LY" || axisName == "左摇杆Y") {
        m_ly = value;
        m_leftStick->setPosition(m_lx, m_ly);
    } else if (axisName == "RX" || axisName == "右摇杆X") {
        m_rx = value;
        m_rightStick->setPosition(m_rx, m_ry);
    } else if (axisName == "RY" || axisName == "右摇杆Y") {
        m_ry = value;
        m_rightStick->setPosition(m_rx, m_ry);
    } else if (axisName == "LT") {
        m_ltBar->setValue(value);
    } else if (axisName == "RT") {
        m_rtBar->setValue(value);
    }
}

void GamepadDisplayWidget::updateButton(const QString &buttonName, bool pressed)
{
    QString status = pressed ? QString("按钮: %1 [按下]").arg(buttonName)
                             : QString("按钮: %1 [释放]").arg(buttonName);
    m_buttonStatusLabel->setText(status);
    m_buttonStatusLabel->setStyleSheet(
        pressed ? "color: #4CAF50; font-weight: bold; border: none; background: transparent;"
                : "color: #666; border: none; background: transparent;");
}

void GamepadDisplayWidget::updateAll(float lx, float ly, float rx, float ry, float lt, float rt)
{
    m_lx = lx; m_ly = ly;
    m_rx = rx; m_ry = ry;
    m_leftStick->setPosition(m_lx, m_ly);
    m_rightStick->setPosition(m_rx, m_ry);
    m_ltBar->setValue(lt);
    m_rtBar->setValue(rt);
}
