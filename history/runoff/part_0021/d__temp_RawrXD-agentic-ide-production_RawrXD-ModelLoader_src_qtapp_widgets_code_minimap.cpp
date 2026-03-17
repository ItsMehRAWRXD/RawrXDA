#include "code_minimap.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QAbstractTextDocumentLayout>
#include <QDebug>

namespace RawrXD {

CodeMinimap::CodeMinimap(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(100);
    setMaximumWidth(150);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    
    // Set dark theme styling
    setStyleSheet(
        "CodeMinimap {"
        "  background-color: #1e1e1e;"
        "  border-left: 1px solid #3e3e42;"
        "}"
    );
    
    setMouseTracking(true);
}

void CodeMinimap::setEditor(QPlainTextEdit* editor)
{
    if (m_editor == editor) {
        return;
    }
    
    // Disconnect old editor
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
        disconnect(m_editor->verticalScrollBar(), nullptr, this, nullptr);
        if (m_document) {
            disconnect(m_document, nullptr, this, nullptr);
        }
    }
    
    m_editor = editor;
    
    if (m_editor) {
        m_document = m_editor->document();
        
        // Connect signals for updates
        connect(m_editor, &QPlainTextEdit::textChanged, this, &CodeMinimap::updateContent);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, 
                this, &CodeMinimap::onEditorScroll);
        
        if (m_document) {
            connect(m_document, &QTextDocument::contentsChanged, this, &CodeMinimap::updateContent);
        }
        
        updateContent();
    }
}

void CodeMinimap::setMinimapVisible(bool visible)
{
    m_visible = visible;
    setVisible(visible);
}

void CodeMinimap::setScale(double scale)
{
    if (scale > 0 && scale <= 1.0) {
        m_scale = scale;
        update();
    }
}

void CodeMinimap::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    if (!m_editor || !m_document || !m_visible) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // Faster rendering
    
    // Draw background
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    // Calculate dimensions
    const int totalLines = m_document->blockCount();
    if (totalLines == 0) {
        return;
    }
    
    const double lineHeight = m_scale * 12.0; // Scaled line height
    const double totalHeight = totalLines * lineHeight;
    const double scaleY = static_cast<double>(height()) / totalHeight;
    
    // Draw document content
    QFont font = m_editor->font();
    font.setPointSizeF(font.pointSizeF() * m_scale);
    painter.setFont(font);
    
    int lineNumber = 0;
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        const double y = lineNumber * lineHeight * scaleY;
        
        if (y > height()) {
            break; // Past visible area
        }
        
        if (y + lineHeight * scaleY >= 0) { // Within visible area
            QString text = block.text();
            
            // Color based on content hints
            QColor textColor(180, 180, 180); // Default light gray
            
            if (text.trimmed().startsWith("//") || text.trimmed().startsWith("#")) {
                textColor = QColor(106, 153, 85); // Green for comments
            } else if (text.contains("class ") || text.contains("struct ") || 
                       text.contains("public:") || text.contains("private:")) {
                textColor = QColor(86, 156, 214); // Blue for keywords
            } else if (text.contains("{") || text.contains("}")) {
                textColor = QColor(220, 220, 170); // Yellow for braces
            }
            
            painter.setPen(textColor);
            
            // Draw simplified text (replace tabs and trim)
            text.replace('\t', "    ");
            if (text.length() > 50) {
                text = text.left(50);
            }
            
            QRectF textRect(2, y, width() - 4, lineHeight * scaleY + 1);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, text);
        }
        
        lineNumber++;
    }
    
    // Draw viewport indicator (visible area in editor)
    QRect visibleRect = calculateVisibleRect();
    if (!visibleRect.isEmpty()) {
        painter.setPen(QPen(QColor(0, 122, 204, 100), 1));
        painter.setBrush(QColor(0, 122, 204, 30));
        painter.drawRect(visibleRect);
    }
}

void CodeMinimap::mousePressEvent(QMouseEvent* event)
{
    if (!m_editor || event->button() != Qt::LeftButton) {
        return;
    }
    
    m_dragging = true;
    m_dragStartY = event->pos().y();
    scrollToPosition(event->pos().y());
    event->accept();
}

void CodeMinimap::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_editor) {
        return;
    }
    
    if (m_dragging) {
        scrollToPosition(event->pos().y());
        event->accept();
    } else {
        // Change cursor to indicate draggable area
        QRect visibleRect = calculateVisibleRect();
        if (visibleRect.contains(event->pos())) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void CodeMinimap::wheelEvent(QWheelEvent* event)
{
    if (!m_editor) {
        return;
    }
    
    // Forward wheel events to editor's scrollbar
    QScrollBar* scrollBar = m_editor->verticalScrollBar();
    if (scrollBar) {
        const int delta = -event->angleDelta().y() / 8;
        scrollBar->setValue(scrollBar->value() + delta);
        event->accept();
    }
}

void CodeMinimap::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void CodeMinimap::updateContent()
{
    update();
}

void CodeMinimap::updateViewport()
{
    update();
}

void CodeMinimap::onEditorScroll(int value)
{
    Q_UNUSED(value);
    update(); // Redraw viewport indicator
}

void CodeMinimap::scrollToPosition(int y)
{
    if (!m_editor || !m_document) {
        return;
    }
    
    const int totalLines = m_document->blockCount();
    if (totalLines == 0) {
        return;
    }
    
    // Calculate which line the user clicked
    const double lineHeight = m_scale * 12.0;
    const double totalHeight = totalLines * lineHeight;
    const double scaleY = static_cast<double>(height()) / totalHeight;
    
    const int targetLine = static_cast<int>(y / (lineHeight * scaleY));
    
    // Scroll editor to that line
    QScrollBar* scrollBar = m_editor->verticalScrollBar();
    if (scrollBar) {
        const double ratio = static_cast<double>(targetLine) / totalLines;
        const int scrollValue = static_cast<int>(ratio * scrollBar->maximum());
        scrollBar->setValue(scrollValue);
    }
    
    m_dragging = false;
}

QRect CodeMinimap::calculateVisibleRect() const
{
    if (!m_editor || !m_document) {
        return QRect();
    }
    
    const int totalLines = m_document->blockCount();
    if (totalLines == 0) {
        return QRect();
    }
    
    QScrollBar* scrollBar = m_editor->verticalScrollBar();
    if (!scrollBar) {
        return QRect();
    }
    
    const double lineHeight = m_scale * 12.0;
    const double totalHeight = totalLines * lineHeight;
    const double scaleY = static_cast<double>(height()) / totalHeight;
    
    // Calculate visible range
    const int firstVisibleLine = m_editor->firstVisibleBlock().blockNumber();
    const int visibleLineCount = m_editor->height() / m_editor->fontMetrics().lineSpacing();
    
    const double startY = firstVisibleLine * lineHeight * scaleY;
    const double rectHeight = visibleLineCount * lineHeight * scaleY;
    
    return QRect(0, static_cast<int>(startY), width(), static_cast<int>(rectHeight));
}

} // namespace RawrXD
