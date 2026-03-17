#include "FlamegraphRenderer.h"
#include <QDebug>
#include <QPainter>
#include <QFile>
#include <QTextStream>

// Qt SVG is optional - check if available and module is included
#ifdef QT_SVG_LIB
#include <QSvgGenerator>
#define HAS_SVG_SUPPORT 1
#else
#define HAS_SVG_SUPPORT 0
#endif

using namespace RawrXD;

FlamegraphRenderer::FlamegraphRenderer(QWidget *parent)
    : QWidget(parent) {
    setMouseTracking(true);
}

void FlamegraphRenderer::setSession(ProfileSession *session) {
    m_session = session;
    m_latencyTimer.start();
    buildRectangles();
    update();
}

void FlamegraphRenderer::refresh() {
    buildRectangles();
    update();
}

void FlamegraphRenderer::exportToSvg(const QString &path) const {
#if HAS_SVG_SUPPORT
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(size());
    gen.setViewBox(QRect(0,0,size().width(), size().height()));
    QPainter p;
    p.begin(&gen);
    p.fillRect(rect(), Qt::white);
    for (const auto &fr : m_rects) {
        p.setBrush(colorForFunction(fr.functionName));
        p.setPen(Qt::NoPen);
        p.drawRect(fr.rect);
    }
    p.end();
#else
    // Fallback: write a simple text representation when SVG support is not available
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "<!-- Flamegraph export (SVG module not available) -->\n";
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width() << "\" height=\"" << height() << "\">\n";
        for (const auto &fr : m_rects) {
            QColor c = colorForFunction(fr.functionName);
            out << QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"%5\" />\n")
                   .arg(fr.rect.x()).arg(fr.rect.y()).arg(fr.rect.width()).arg(fr.rect.height()).arg(c.name());
        }
        out << "</svg>\n";
        file.close();
    }
#endif
}

void FlamegraphRenderer::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    for (const auto &fr : m_rects) {
        painter.setBrush(colorForFunction(fr.functionName));
        painter.setPen(Qt::NoPen);
        painter.drawRect(fr.rect);

        // Draw function name if enough width
        if (fr.rect.width() > 80) {
            painter.setPen(Qt::black);
            painter.drawText(fr.rect.adjusted(3, 2, -3, -2), Qt::AlignLeft | Qt::AlignVCenter,
                             fr.functionName);
        }
    }

    // Structured logging for render latency
    qint64 latencyUs = m_latencyTimer.nsecsElapsed() / 1000;
    qDebug() << "FlamegraphRenderer paint latency(us)=" << latencyUs << " rects=" << m_rects.size();
    m_latencyTimer.restart();
}

void FlamegraphRenderer::mouseMoveEvent(QMouseEvent *event) {
    if (const FlameRect *hit = hitTest(event->pos())) {
        QString tip = QString("%1\nTime: %2 ms (self %3 ms)\nCalls: %4")
                .arg(hit->functionName)
                .arg(hit->timeUs / 1000.0, 0, 'f', 2)
                .arg(hit->selfTimeUs / 1000.0, 0, 'f', 2)
                .arg(hit->callCount);
        QToolTip::showText(event->globalPos(), tip, this);
        emit functionHovered(hit->functionName, hit->timeUs, hit->selfTimeUs, hit->callCount);
    } else {
        QToolTip::hideText();
    }
}

void FlamegraphRenderer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (const FlameRect *hit = hitTest(event->pos())) {
            emit functionClicked(hit->functionName);
        }
    }
}

void FlamegraphRenderer::buildRectangles() {
    m_rects.clear();
    if (!m_session) return;

    // Aggregate function stats from session
    QList<FunctionStat> stats = m_session->functionStats().values();
    // total time for width normalization
    m_totalTimeUs = 0.0;
    for (const auto &fs : stats) m_totalTimeUs += fs.totalTimeUs;
    if (m_totalTimeUs <= 0.0) return;

    // Sort by total time descending
    std::sort(stats.begin(), stats.end(), [](const FunctionStat &a, const FunctionStat &b){
        return a.totalTimeUs > b.totalTimeUs;
    });

    // Lay out as single-level flamegraph grouped by time (simple yet informative)
    const double W = width();
    const double H = height();
    const double BARH = 24.0;
    double x = 0.0;
    int depth = 0;

    for (const auto &fs : stats) {
        double w = (fs.totalTimeUs / m_totalTimeUs) * W;
        FlameRect fr;
        fr.rect = QRectF(x, depth * BARH, w, BARH - 2);
        fr.functionName = fs.functionName;
        fr.fileName = QString();
        fr.timeUs = fs.totalTimeUs;
        fr.selfTimeUs = fs.selfTimeUs;
        fr.callCount = fs.callCount;
        fr.depth = depth;
        m_rects.append(fr);
        x += w;
        if (x > W) { x = 0.0; depth++; }
        if ((depth+1) * BARH > H) break; // no more space
    }
}

QColor FlamegraphRenderer::colorForFunction(const QString &name) const {
    // Hash-based color palette
    uint h = qHash(name);
    int r = 50 + (h & 0xFF) % 205;
    int g = 50 + ((h >> 8) & 0xFF) % 205;
    int b = 50 + ((h >> 16) & 0xFF) % 205;
    return QColor(r, g, b, 220);
}

const FlameRect* FlamegraphRenderer::hitTest(const QPointF &pt) const {
    for (const auto &fr : m_rects) {
        if (fr.rect.contains(pt)) return &fr;
    }
    return nullptr;
}
