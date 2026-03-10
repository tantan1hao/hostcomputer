#ifndef GAMEPADDISPLAYWIDGET_H
#define GAMEPADDISPLAYWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPointF>

/**
 * @brief 摇杆可视化圆盘控件
 *
 * 绘制一个圆形底盘，中间有一个跟随摇杆位置移动的小圆点
 * x, y 范围 -1.0 ~ 1.0
 */
class StickVisualWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StickVisualWidget(const QString &title, QWidget *parent = nullptr);

    void setPosition(float x, float y);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    float m_x = 0.0f;
    float m_y = 0.0f;
};

/**
 * @brief 扳机进度条控件
 *
 * 竖直进度条，值范围 0.0 ~ 1.0
 */
class TriggerBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TriggerBarWidget(const QString &title, QWidget *parent = nullptr);

    void setValue(float value);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    float m_value = 0.0f;
};

/**
 * @brief 手柄映射显示控件
 *
 * 包含：
 *   - 左右摇杆可视化圆盘（圆点跟随摇杆位置实时移动）
 *   - LT/RT 扳机进度条
 *   - 按钮状态文字显示
 */
class GamepadDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GamepadDisplayWidget(QWidget *parent = nullptr);

    void updateAxis(const QString &axisName, float value);
    void updateButton(const QString &buttonName, bool pressed);
    void updateAll(float lx, float ly, float rx, float ry, float lt, float rt);

private:
    QLabel *m_titleLabel;

    // 摇杆可视化
    StickVisualWidget *m_leftStick;
    StickVisualWidget *m_rightStick;

    // 扳机进度条
    TriggerBarWidget *m_ltBar;
    TriggerBarWidget *m_rtBar;

    // 按钮状态标签
    QLabel *m_buttonStatusLabel;

    // 缓存摇杆值用于组合更新
    float m_lx = 0.0f, m_ly = 0.0f;
    float m_rx = 0.0f, m_ry = 0.0f;
};

#endif // GAMEPADDISPLAYWIDGET_H
