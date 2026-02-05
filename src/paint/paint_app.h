#pragma once

#include <QMainWindow>
#include <QImage>
#include <QColor>
#include <QPoint>

class PaintApp : public QMainWindow
{
    Q_OBJECT

public:
    PaintApp(QWidget *parent = nullptr);
    ~PaintApp();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void drawLineTo(const QPoint &endPoint);
    void resizeImage(QImage *image, const QSize &newSize);

    bool m_drawing = false;
    QPoint m_lastPoint;
    QImage m_image;
    QColor m_penColor = Qt::blue;
    int m_penWidth = 2;
};