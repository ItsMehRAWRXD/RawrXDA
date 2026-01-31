/**
 * \file ghost_text_renderer.cpp
 * \brief Cursor-style inline ghost text implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "ghost_text_renderer.h"


namespace RawrXD {

GhostTextRenderer::GhostTextRenderer(QPlainTextEdit* editor, void* parent)
    : void(parent)
    , m_editor(editor)
{
    // Lightweight constructor
    setWindowFlags(//Widget);
    setAttribute(//WA_TransparentForMouseEvents);
    setAttribute(//WA_TranslucentBackground);
}

void GhostTextRenderer::initialize() {
    if (!m_editor) return;
    
    // Set up overlay
    setParent(m_editor->viewport());
    updateOverlayGeometry();
    
    // Install event filter on editor
    m_editor->viewport()->installEventFilter(this);
    
    // Fade timer for animations
    m_fadeTimer = new void*(this);
    m_fadeTimer->setInterval(30);
// Qt connect removed
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
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void GhostTextRenderer::showGhostText(const std::string& text, const std::string& type) {
    if (text.empty()) {
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
        m_ghostDecoration.color = uint32_t(100, 150, 255, 180);  // Light blue
    } else {
        m_ghostDecoration.color = m_ghostColor;
    }
    
    m_opacity = 1.0;
    m_fading = false;
    show();
    raise();
    update();
}

void GhostTextRenderer::showMultilineGhost(const std::vector<std::string>& lines) {
    if (lines.empty()) {
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

void GhostTextRenderer::updateGhostText(const std::string& additionalText) {
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

void GhostTextRenderer::showDiffPreview(int startLine, int endLine, const std::string& oldText, const std::string& newText) {
    DiffDecoration diff;
    diff.startLine = startLine;
    diff.endLine = endLine;
    diff.oldText = oldText;
    diff.newText = newText;
    
    // Determine diff type
    if (oldText.empty()) {
        diff.type = "add";
    } else if (newText.empty()) {
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
    
    std::string text = m_currentGhostText;
    m_editor->textCursor().insertText(text);
    
    ghostTextAccepted(text);
    clearGhostText();
}

void GhostTextRenderer::paintEvent(void*  event) {
    (event);
    
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    
    renderGhostText(painter);
    renderDiffPreview(painter);
}

void GhostTextRenderer::renderGhostText(QPainter& painter) {
    if (!hasGhostText()) return;
    
    QTextCursor cursor = m_editor->textCursor();
    void* cursorPos = getCursorPosition();
    int lineHeight = getLineHeight();
    std::string font = getEditorFont();
    
    painter.setFont(font);
    painter.setPen(m_ghostDecoration.color);
    
    if (m_ghostDecoration.multiline) {
        // Render multi-line ghost text
        int y = cursorPos.y();
        for (const std::string& line : m_ghostDecoration.lines) {
            painter.drawText(cursorPos.x(), y, line);
            y += lineHeight;
        }
    } else {
        // Render single-line ghost text
        painter.drawText(cursorPos.x(), cursorPos.y(), m_ghostDecoration.text);
    }
}

void GhostTextRenderer::renderDiffPreview(QPainter& painter) {
    if (m_diffDecorations.empty()) return;
    
    int lineHeight = getLineHeight();
    std::string font = getEditorFont();
    painter.setFont(font);
    
    for (const DiffDecoration& diff : m_diffDecorations) {
        QTextCursor cursor(m_editor->document()->findBlockByNumber(diff.startLine));
        void* pos = getCursorPosition();
        
        // Highlight background
        uint32_t bgColor;
        if (diff.type == "add") {
            bgColor = m_addColor;
        } else if (diff.type == "remove") {
            bgColor = m_removeColor;
        } else {
            bgColor = uint32_t(255, 255, 0, 100);  // Yellow for modify
        }
        
        int numLines = diff.endLine - diff.startLine + 1;
        void* highlightRect(0, pos.y(), width(), numLines * lineHeight);
        painter.fillRect(highlightRect, bgColor);
        
        // Draw diff text
        if (diff.type == "remove") {
            painter.setPen(//red);
            painter.drawText(pos.x(), pos.y(), "- " + diff.oldText);
        }
        if (diff.type == "add" || diff.type == "modify") {
            painter.setPen(//darkGreen);
            int offset = (diff.type == "modify") ? lineHeight : 0;
            painter.drawText(pos.x(), pos.y() + offset, "+ " + diff.newText);
        }
    }
}

bool GhostTextRenderer::eventFilter(void* obj, QEvent* event) {
    if (obj != m_editor->viewport()) return false;
    
    if (event->type() == QEvent::KeyPress) {
        void*  keyEvent = static_cast<void* >(event);
        
        if (hasGhostText()) {
            // Tab to accept
            if (keyEvent->key() == //Key_Tab && keyEvent->modifiers() == //NoModifier) {
                acceptGhostText();
                return true;  // Consume event
            }
            
            // Esc to dismiss
            if (keyEvent->key() == //Key_Escape) {
                ghostTextDismissed();
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

void* GhostTextRenderer::getCursorPosition() const {
    if (!m_editor) return void*(0, 0);
    
    QTextCursor cursor = m_editor->textCursor();
    void* cursorRect = m_editor->cursorRect(cursor);
    
    return void*(cursorRect.x(), cursorRect.y() + cursorRect.height());
}

int GhostTextRenderer::getLineHeight() const {
    if (!m_editor) return 14;
    
    QFontMetrics fm(m_editor->font());
    return fm.lineSpacing();
}

std::string GhostTextRenderer::getEditorFont() const {
    if (!m_editor) return std::string("Consolas", 10);
    return m_editor->font();
}

void GhostTextRenderer::fadeIn() {
    m_opacity = 0.0;
    m_fading = false;
    m_fadeTimer->start();
    
    // Fade in animation
    void*::singleShot(0, this, [this]() {
        while (m_opacity < 1.0) {
            m_opacity += 0.1;
            update();
            std::thread::msleep(30);
        }
        m_opacity = 1.0;
    });
}

void GhostTextRenderer::fadeOut() {
    m_fading = true;
    m_fadeTimer->start();
}

} // namespace RawrXD


