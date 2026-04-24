#include "videowidget.h"

#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
}

void VideoWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void VideoWidget::clearFrame()
{
    m_frame = QImage();
    update();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_frame.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("请选择一个本地 MP4 文件"));
        return;
    }

    const QSize scaledSize = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(QPoint((width() - scaledSize.width()) / 2,
                                  (height() - scaledSize.height()) / 2),
                           scaledSize);
    painter.drawImage(targetRect, m_frame);
}
