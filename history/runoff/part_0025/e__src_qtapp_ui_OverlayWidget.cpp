#include "OverlayWidget.h"
#include <QPainter>
#include <QStyle>
#include <QEvent>

OverlayWidget::OverlayWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setVisible(true);

    if (parent) {
        // Track parent size/resize to keep overlay covering editor
        parent->installEventFilter(this);
        resize(parent->size());
        move(0, 0);
    }
}

void OverlayWidget::setGhostText(const QString& text) {
    ghostText_ = text;
    update();
}

void OverlayWidget::clear() {
    ghostText_.clear();
    update();
}

void OverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    if (ghostText_.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Derive font from parent (editor) for alignment
    if (parentWidget()) {
        p.setFont(parentWidget()->font());
    }

    QColor ghostColor = palette().color(QPalette::Text);
    ghostColor.setAlpha(120); // semi-transparent
    p.setPen(ghostColor);

    // Basic rendering: draw at top-left; editor-aware layout can be added later
    const int margin = 8;
    const QRect textRect = rect().adjusted(margin, margin, -margin, -margin);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, ghostText_);
}

bool OverlayWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize) {
            resize(parentWidget()->size());
            move(0, 0);
        } else if (event->type() == QEvent::Show) {
            resize(parentWidget()->size());
            move(0, 0);
        }
    }
    return QWidget::eventFilter(watched, event);
}
