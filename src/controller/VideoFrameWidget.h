#ifndef VIDEOFRAMEWIDGET_H
#define VIDEOFRAMEWIDGET_H

#include <QImage>
#include <QWidget>

class VideoFrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
};

#endif // VIDEOFRAMEWIDGET_H
