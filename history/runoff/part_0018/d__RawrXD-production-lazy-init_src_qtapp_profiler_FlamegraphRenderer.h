#pragma once

#include <QWidget>
#include <QVector>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QElapsedTimer>
#include "ProfileData.h"

namespace RawrXD {

struct FlameRect {
    QRectF rect;            // position in widget coordinate space
    QString functionName;
    QString fileName;
    quint64 timeUs = 0;
    quint64 selfTimeUs = 0;
    quint64 callCount = 0;
    int depth = 0;
};

class FlamegraphRenderer : public QWidget {
    Q_OBJECT
public:
    explicit FlamegraphRenderer(QWidget *parent = nullptr);

    void setSession(ProfileSession *session);
    void refresh();

    void exportToSvg(const QString &path) const;

signals:
    void functionHovered(const QString &functionName, quint64 timeUs, quint64 selfTimeUs, quint64 callCount);
    void functionClicked(const QString &functionName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return QSize(1200, 600); }

private:
    void buildRectangles();
    QColor colorForFunction(const QString &name) const;
    const FlameRect* hitTest(const QPointF &pt) const;

private:
    ProfileSession *m_session = nullptr;
    QVector<FlameRect> m_rects;
    double m_totalTimeUs = 0.0;
    QElapsedTimer m_latencyTimer;
};

} // namespace RawrXD
