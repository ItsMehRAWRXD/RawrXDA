/**
 * \file ghost_text_renderer.cpp
 * \brief Cursor-style inline ghost text implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "ghost_text_renderer.h"
#include <QKeyEvent>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QScrollBar>
#include <QThread>

namespace RawrXD {

GhostTextRenderer::GhostTextRenderer(QPlainTextEdit* editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
{
    // Lightweight constructor
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
}

void GhostTextRenderer::initialize() {
    if (!m_editor) return;
    
    // Set up overlay
    setParent(m_editor->viewport());
    updateOverlayGeometry();
    
    // Install event filter on editor
    m_editor->viewport()->installEventFilter(this);
    
    // Fade timer for animations
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(30);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        if (m_fading) {
            m_opacity -= 0.1;
            if (m_opacity <= 0.0) {
                m_opacity = 0.0;
                m_fading = false;
                m_fadeTimer->stop();
                clearGhostText();
            }
            update();
        }
    });
    
    // Track editor changes
    connect(m_editor, &QPlainTextEdit::textChanged, this, &GhostTextRenderer::updateOverlayGeometry);
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, &GhostTextRenderer::updateOverlayGeometry);
    connect(m_editor->horizontalScrollBar(), &QScrollBar::valueChanged, this, &GhostTextRenderer::updateOverlayGeometry);
}

void GhostTextRenderer::showGhostText(const QString& text, const QString& type) {
    if (text.isEmpty()) {
        clearGhostText();
        return;
    }
    
    m_currentGhostText = text;
    
    QTextCursor cursor = m_editor->textCursor();
    m_ghostDecoration.line = cursor.blockNumber();
    m_ghostDecoration.column = cursor.columnNumber();
    m_ghostDecoration.text = text;
    m_ghostDecoration.type = type;
    m_ghostDecoration.multiline = false;
    
    // Color based on type
    if (type == "completion") {
        m_ghostDecoration.color = m_ghostColor;
    } else if (type == "suggestion") {
        m_ghostDecoration.color = QColor(100, 150, 255, 180);  // Light blue
    } else {
        m_ghostDecoration.color = m_ghostColor;
    }
    
    m_opacity = 1.0;
    m_fading = false;
    show();
    raise();
    update();
}

void GhostTextRenderer::showMultilineGhost(const QStringList& lines) {
    if (lines.isEmpty()) {
        clearGhostText();
        return;
    }
    
    m_currentGhostText = lines.join('\n');
    
    QTextCursor cursor = m_editor->textCursor();
    m_ghostDecoration.line = cursor.blockNumber();
    m_ghostDecoration.column = cursor.columnNumber();
    m_ghostDecoration.multiline = true;
    m_ghostDecoration.lines = lines;
    m_ghostDecoration.color = m_ghostColor;
    
    m_opacity = 1.0;
    m_fading = false;
    show();
    raise();
    update();
}

void GhostTextRenderer::updateGhostText(const QString& additionalText) {
    if (!hasGhostText()) return;
    
    m_currentGhostText += additionalText;
    m_ghostDecoration.text = m_currentGhostText;
    
    // Check if multiline
    if (m_currentGhostText.contains('\n')) {
        m_ghostDecoration.multiline = true;
        m_ghostDecoration.lines = m_currentGhostText.split('\n');
    }
    
    update();
}

void GhostTextRenderer::showDiffPreview(int startLine, int endLine, const QString& oldText, const QString& newText) {
    DiffDecoration diff;
    diff.startLine = startLine;
    diff.endLine = endLine;
    diff.oldText = oldText;
    diff.newText = newText;
    
    // Determine diff type
    if (oldText.isEmpty()) {
        diff.type = "add";
    } else if (newText.isEmpty()) {
        diff.type = "remove";
    } else {
        diff.type = "modify";
    }
    
    m_diffDecorations.append(diff);
    show();
    raise();
    update();
}

void GhostTextRenderer::clearGhostText() {
    m_currentGhostText.clear();
    m_ghostDecoration = GhostTextDecoration{};
    hide();
    update();
}

void GhostTextRenderer::clearDiffPreview() {
    m_diffDecorations.clear();
    update();
}

void GhostTextRenderer::acceptGhostText() {
    if (!hasGhostText()) return;
    
    QString text = m_currentGhostText;
    m_editor->textCursor().insertText(text);
    
    emit ghostTextAccepted(text);
    clearGhostText();
}

void GhostTextRenderer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    
    renderGhostText(painter);
    renderDiffPreview(painter);
}

void GhostTextRenderer::renderGhostText(QPainter& painter) {
    if (!hasGhostText()) return;
    
    QTextCursor cursor = m_editor->textCursor();
    QPoint cursorPos = getCursorPosition();
    int lineHeight = getLineHeight();
    QFont font = getEditorFont();
    
    painter.setFont(font);
    painter.setPen(m_ghostDecoration.color);
    
    if (m_ghostDecoration.multiline) {
        // Render multi-line ghost text
        int y = cursorPos.y();
        for (const QString& line : m_ghostDecoration.lines) {
            painter.drawText(cursorPos.x(), y, line);
            y += lineHeight;
        }
    } else {
        // Render single-line ghost text
        painter.drawText(cursorPos.x(), cursorPos.y(), m_ghostDecoration.text);
    }
}

void GhostTextRenderer::renderDiffPreview(QPainter& painter) {
    if (m_diffDecorations.isEmpty()) return;
    
    int lineHeight = getLineHeight();
    QFont font = getEditorFont();
    painter.setFont(font);
    
    for (const DiffDecoration& diff : m_diffDecorations) {
        QTextCursor cursor(m_editor->document()->findBlockByNumber(diff.startLine));
        QPoint pos = getCursorPosition();
        
        // Highlight background
        QColor bgColor;
        if (diff.type == "add") {
            bgColor = m_addColor;
        } else if (diff.type == "remove") {
            bgColor = m_removeColor;
        } else {
            bgColor = QColor(255, 255, 0, 100);  // Yellow for modify
        }
        
        int numLines = diff.endLine - diff.startLine + 1;
        QRect highlightRect(0, pos.y(), width(), numLines * lineHeight);
        painter.fillRect(highlightRect, bgColor);
        
        // Draw diff text
        if (diff.type == "remove") {
            painter.setPen(Qt::red);
            painter.drawText(pos.x(), pos.y(), "- " + diff.oldText);
        }
        if (diff.type == "add" || diff.type == "modify") {
            painter.setPen(Qt::darkGreen);
            int offset = (diff.type == "modify") ? lineHeight : 0;
            painter.drawText(pos.x(), pos.y() + offset, "+ " + diff.newText);
        }
    }
}

bool GhostTextRenderer::eventFilter(QObject* obj, QEvent* event) {
    if (obj != m_editor->viewport()) return false;
    
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        if (hasGhostText()) {
            // Tab to accept
            if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier) {
                acceptGhostText();
                return true;  // Consume event
            }
            
            // Esc to dismiss
            if (keyEvent->key() == Qt::Key_Escape) {
                emit ghostTextDismissed();
                fadeOut();
                return true;
            }
            
            // Any other key dismisses ghost text
            if (keyEvent->text().length() > 0) {
                fadeOut();
            }
        }
    }
    
    if (event->type() == QEvent::Resize || event->type() == QEvent::Paint) {
        updateOverlayGeometry();
    }
    
    return false;
}

void GhostTextRenderer::updateOverlayGeometry() {
    if (!m_editor) return;
    setGeometry(m_editor->viewport()->rect());
    update();
}

QPoint GhostTextRenderer::getCursorPosition() const {
    if (!m_editor) return QPoint(0, 0);
    
    QTextCursor cursor = m_editor->textCursor();
    QRect cursorRect = m_editor->cursorRect(cursor);
    
    return QPoint(cursorRect.x(), cursorRect.y() + cursorRect.height());
}

int GhostTextRenderer::getLineHeight() const {
    if (!m_editor) return 14;
    
    QFontMetrics fm(m_editor->font());
    return fm.lineSpacing();
}

QFont GhostTextRenderer::getEditorFont() const {
    if (!m_editor) return QFont("Consolas", 10);
    return m_editor->font();
}

void GhostTextRenderer::fadeIn() {
    m_opacity = 0.0;
    m_fading = false;
    m_fadeTimer->start();
    
    // Fade in animation
    QTimer::singleShot(0, this, [this]() {
        while (m_opacity < 1.0) {
            m_opacity += 0.1;
            update();
            QThread::msleep(30);
        }
        m_opacity = 1.0;
    });
}

void GhostTextRenderer::fadeOut() {
    m_fading = true;
    m_fadeTimer->start();
}

} // namespace RawrXD
