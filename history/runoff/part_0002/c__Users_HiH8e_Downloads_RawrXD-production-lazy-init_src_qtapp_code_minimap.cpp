#include "code_minimap.h"
#include <QPlainTextEdit>
#include <QPlainTextDocumentLayout>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScopedValueRollback>
#include <QResizeEvent>
#include <QAbstractTextDocumentLayout>
#include <QTimer>
#include <QDebug>
#include <QApplication>

namespace RawrXD {

CodeMinimap::CodeMinimap(QPlainTextEdit* editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setFixedWidth(m_minimapWidth);
    setStyleSheet("QWidget { background-color: #1e1e1e; border-left: 1px solid #3e3e42; }");

    // Setup update timer for debounced redraws
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        update();
    });

    if (m_editor) {
        // Connect editor signals
        connect(m_editor->document(), &QTextDocument::contentsChanged,
                this, &CodeMinimap::onEditorTextChanged);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &CodeMinimap::onEditorScrolled);
        m_cachedLineCount = getLineCountFromEditor();
    }
}

void CodeMinimap::setEditor(QPlainTextEdit* editor)
{
    if (m_editor) {
        disconnect(m_editor->document(), nullptr, this, nullptr);
        disconnect(m_editor->verticalScrollBar(), nullptr, this, nullptr);
    }

    m_editor = editor;

    if (m_editor) {
        connect(m_editor->document(), &QTextDocument::contentsChanged,
                this, &CodeMinimap::onEditorTextChanged);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &CodeMinimap::onEditorScrolled);
        m_cachedLineCount = getLineCountFromEditor();
        update();
    }
}

void CodeMinimap::setWidth(int width)
{
    m_minimapWidth = width;
    setFixedWidth(width);
}

void CodeMinimap::setEnabled(bool enabled)
{
    m_enabled = enabled;
    setVisible(enabled);
}

void CodeMinimap::paintEvent(QPaintEvent* event)
{
    if (!m_enabled || !m_editor) {
        QWidget::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.drawLine(0, 0, 0, height());

    drawMinimap(painter);
    drawViewportIndicator(painter);
}

void CodeMinimap::drawMinimap(QPainter& painter)
{
    if (!m_editor) return;

    QTextDocument* doc = m_editor->document();
    int totalLines = doc->blockCount();
    int visibleArea = m_editor->viewport()->height() / m_editor->fontMetrics().lineSpacing();

    if (totalLines == 0) return;

    int minimapHeight = height();
    double pixelsPerLine = static_cast<double>(minimapHeight) / totalLines;

    painter.setFont(QFont("Consolas", 6));
    painter.setPen(m_textColor);

    QTextBlock block = doc->firstBlock();
    int lineNum = 0;

    while (block.isValid() && lineNum < totalLines) {
        int y = static_cast<int>(lineNum * pixelsPerLine);

        if (y > minimapHeight) break;

        // Draw a small representation of the line
        QString lineText = block.text().left(40);  // Limit to 40 chars for width
        if (!lineText.isEmpty()) {
            // Draw simplified representation - just a small colored bar
            int lineLength = lineText.length();
            int colorIntensity = qBound(50, 150 + (lineLength % 100), 255);
            painter.fillRect(5, y, m_minimapWidth - 10, qMax(1, static_cast<int>(pixelsPerLine)),
                           QColor(colorIntensity, colorIntensity * 0.7, colorIntensity * 0.5, 200));
        }

        block = block.next();
        lineNum++;
    }
}

void CodeMinimap::drawViewportIndicator(QPainter& painter)
{
    if (!m_editor) return;

    QTextDocument* doc = m_editor->document();
    int totalLines = doc->blockCount();
    if (totalLines == 0) return;

    int minimapHeight = height();
    double pixelsPerLine = static_cast<double>(minimapHeight) / totalLines;

    int firstVisibleLine = getFirstVisibleLine();
    int lastVisibleLine = getLastVisibleLine();

    int viewportStart = static_cast<int>(firstVisibleLine * pixelsPerLine);
    int viewportEnd = static_cast<int>(lastVisibleLine * pixelsPerLine);
    int viewportHeight = qMax(10, viewportEnd - viewportStart);

    // Draw semi-transparent viewport indicator
    painter.fillRect(0, viewportStart, width(), viewportHeight, m_viewportAlpha);
    painter.setPen(QPen(m_viewportColor, 1));
    painter.drawRect(0, viewportStart, width() - 1, viewportHeight - 1);
}

void CodeMinimap::mousePressEvent(QMouseEvent* event)
{
    if (m_enabled && m_editor && event->button() == Qt::LeftButton) {
        handleNavigationClick(event->y());
    }
}

void CodeMinimap::mouseMoveEvent(QMouseEvent* event)
{
    if (m_enabled && m_editor && event->buttons() & Qt::LeftButton) {
        handleNavigationClick(event->y());
    }
}

void CodeMinimap::handleNavigationClick(int y)
{
    if (!m_editor) return;

    QTextDocument* doc = m_editor->document();
    int totalLines = doc->blockCount();
    if (totalLines == 0) return;

    double minimapHeight = height();
    double pixelsPerLine = minimapHeight / totalLines;
    int targetLine = static_cast<int>(y / pixelsPerLine);
    targetLine = qBound(0, targetLine, totalLines - 1);

    QTextCursor cursor(doc->findBlockByLineNumber(targetLine));
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
}

void CodeMinimap::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void CodeMinimap::wheelEvent(QWheelEvent* event)
{
    // Forward wheel events to editor for scrolling; guard reentry to avoid loops
    if (m_editor && !m_forwardingWheel) {
        QScopedValueRollback<bool> guard(m_forwardingWheel, true);
        QWheelEvent editorEvent(event->position(), event->globalPosition(),
                               event->pixelDelta(), event->angleDelta(),
                               event->buttons(), event->modifiers(),
                               event->phase(), event->inverted());
        QApplication::sendEvent(m_editor, &editorEvent);
    }
}

void CodeMinimap::onEditorTextChanged()
{
    int newLineCount = getLineCountFromEditor();
    if (newLineCount != m_cachedLineCount) {
        m_cachedLineCount = newLineCount;
        m_updateTimer->start();
    }
}

void CodeMinimap::onEditorScrolled()
{
    update();
}

void CodeMinimap::updateViewport()
{
    update();
}

int CodeMinimap::getLineCountFromEditor() const
{
    if (!m_editor || !m_editor->document()) return 0;
    return m_editor->document()->blockCount();
}

int CodeMinimap::getFirstVisibleLine() const
{
    if (!m_editor) return 0;

    QTextCursor startCursor = m_editor->cursorForPosition(QPoint(0, 0));
    return startCursor.blockNumber();
}

int CodeMinimap::getLastVisibleLine() const
{
    if (!m_editor) return 0;

    QTextCursor endCursor = m_editor->cursorForPosition(
        QPoint(0, m_editor->viewport()->height())
    );
    return endCursor.blockNumber();
}

} // namespace RawrXD
