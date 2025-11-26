#ifndef VIDEO_WIDGET_H
#define VIDEO_WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

// OpenCV 支持（可选）
#ifdef OPENCV_FOUND
#include <opencv2/opencv.hpp>
#endif

/**
 * @brief 优化的视频显示控件
 *
 * 特点：
 * - 双缓冲减少闪烁
 * - 自适应缩放
 * - 高性能绘制
 * - 支持OpenCV Mat直接显示
 */
class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    // 更新帧数据
    void updateFrame(const QPixmap& frame);
#ifdef OPENCV_FOUND
    void updateFrame(const cv::Mat& frame);
#endif
    void clearFrame();

    // 显示设置
    void setAspectRatioMode(Qt::AspectRatioMode mode);
    void setScaleMode(bool keepAspectRatio = true);

    // 状态查询
    bool hasFrame() const { return !m_currentFrame.isNull(); }
    QSize getFrameSize() const { return m_currentFrame.size(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateScaledFrame();
    void calculateScaledRect(const QSize& frameSize, const QSize& widgetSize);

private:
    QPixmap m_currentFrame;        // 原始帧
    QPixmap m_scaledFrame;         // 缩放后的帧（双缓冲）
    QRect m_targetRect;            // 目标绘制区域

    Qt::AspectRatioMode m_aspectRatioMode;
    bool m_keepAspectRatio;
    bool m_needRescale;

    // 性能优化
    QTimer* m_updateTimer;
    static const int UPDATE_INTERVAL_MS = 33; // ~30 FPS
};

#endif // VIDEO_WIDGET_H