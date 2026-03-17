#include "AISuggestionOverlay.h"
#include <QPlainTextEdit>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>

AISuggestionOverlay::AISuggestionOverlay(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setVisible(false);

    m_cursorBlinkTimer.setInterval(500);
    connect(&m_cursorBlinkTimer, &QTimer::timeout, [this]() {
        m_cursorOn = !m_cursorOn;
        update();
    });
}

void AISuggestionOverlay::attachToEditor(QPlainTextEdit* editor) {
    m_editor = editor;
    if (!m_editor) return;
    setParent(m_editor);
    setGeometry(m_editor->viewport()->geometry());
    setVisible(true);
    raise();
    m_cursorBlinkTimer.start();
}

void AISuggestionOverlay::setSuggestionText(const QString& text) {
    m_text = text;
    if (!isVisible()) setVisible(true);
    update();
}

void AISuggestionOverlay::setGhostText(const QString& text) {
    setSuggestionText(text);
}

void AISuggestionOverlay::clearSuggestion() {
    m_text.clear();
    setVisible(false);
    update();
}

void AISuggestionOverlay::clear() {
    clearSuggestion();
}

void AISuggestionOverlay::setOpacity(qreal opacity) {
    m_opacity = qBound<qreal>(0.1, opacity, 0.9);
    update();
}

void AISuggestionOverlay::onStreamChunk(const QString& chunk) {
    m_text += chunk;
    update();
}

void AISuggestionOverlay::onStreamCompleted() {
    // Keep suggestion visible; user can accept.
}

void AISuggestionOverlay::onStreamError(const QString& message) {
    // Optionally render an error hint; for now just clear.
    Q_UNUSED(message);
    clearSuggestion();
}

void AISuggestionOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (!m_editor || m_text.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Determine caret position in editor to align ghost text.
    auto cursor = m_editor->textCursor();
    auto block = cursor.block();
    auto layout = block.layout();
    if (!layout) return;

    const int cursorPosInBlock = cursor.position() - block.position();
    const QRectF lineRect = layout->boundingRect();

    // Map to viewport coordinates
    QPointF topLeft = m_editor->cursorRect().topLeft();
    // Apply scroll offsets
    topLeft.setY(topLeft.y() - m_editor->verticalScrollBar()->value());

    QFont f = m_editor->font();
    p.setFont(f);

    QColor ghost = m_editor->palette().text().color();
    ghost.setAlphaF(m_opacity);
    p.setPen(ghost);

    // Draw suggestion next to caret
    p.drawText(topLeft + QPointF(1, 0), m_text);

    // Optional thin underline / cursor indicator
    if (m_cursorOn) {
        QColor caret = ghost;
        caret.setAlphaF(std::min(1.0, m_opacity + 0.2));
        p.setPen(caret);
        p.drawLine(topLeft, topLeft + QPointF(8, 0));
    }
}

void AISuggestionOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_editor) setGeometry(m_editor->viewport()->geometry());
}
