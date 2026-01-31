/**
 * \file agentic_text_edit.cpp
 * \brief Agentic text editor implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "agentic_text_edit.h"


namespace RawrXD {

AgenticTextEdit::AgenticTextEdit(void* parent)
    : QPlainTextEdit(parent)
{
    // Lightweight constructor - defer widget creation to initialize()
}

void AgenticTextEdit::initialize() {
    // Create ghost text renderer overlay
    m_ghostRenderer = new GhostTextRenderer(this, this);
    m_ghostRenderer->initialize();
    
    // Create completion timer
    m_completionTimer = new void*(this);
    m_completionTimer->setSingleShot(true);
    m_completionTimer->setInterval(m_completionDelay);
    
    // Connect signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void AgenticTextEdit::setLSPClient(LSPClient* client) {
    if (m_lspClient) {
        // Disconnect old client
// Qt disconnect removed
        // Close document if opened
        if (m_documentOpened && !m_documentUri.isEmpty()) {
            m_lspClient->closeDocument(m_documentUri);
            m_documentOpened = false;
        }
    }
    
    m_lspClient = client;
    
    if (m_lspClient) {
        // Connect to LSP client signals
// Qt connect removed
// Qt connect removed
            syncDocumentToLSP();
        });
        
        // Open document if server is already running
        if (m_lspClient->isRunning() && !m_documentUri.isEmpty()) {
            syncDocumentToLSP();
        }
    }
}

void AgenticTextEdit::setDocumentUri(const std::string& uri) {
    m_documentUri = uri;
    
    // Infer language ID from file extension
    if (uri.endsWith(".cpp") || uri.endsWith(".h") || uri.endsWith(".cc") || uri.endsWith(".hpp")) {
        m_languageId = "cpp";
    } else if (uri.endsWith(".py")) {
        m_languageId = "python";
    } else if (uri.endsWith(".js")) {
        m_languageId = "javascript";
    } else if (uri.endsWith(".ts")) {
        m_languageId = "typescript";
    } else {
        m_languageId = "plaintext";
    }
    
}

void AgenticTextEdit::setAutoCompletionsEnabled(bool enabled) {
    m_autoCompletionsEnabled = enabled;
    if (!enabled) {
        m_completionTimer->stop();
        m_ghostRenderer->clearGhostText();
    }
}

void AgenticTextEdit::setCompletionDelay(int ms) {
    m_completionDelay = ms;
    m_completionTimer->setInterval(ms);
}

void AgenticTextEdit::keyPressEvent(QKeyEvent* event) {
    // Let ghost renderer handle Tab/Esc first
    if (m_ghostRenderer && m_ghostRenderer->hasGhostText()) {
        if (event->key() == //Key_Tab && event->modifiers() == //NoModifier) {
            m_ghostRenderer->acceptGhostText();
            return;
        }
        if (event->key() == //Key_Escape) {
            m_ghostRenderer->clearGhostText();
            completionDismissed();
            return;
        }
    }
    
    // Pass to base class
    QPlainTextEdit::keyPressEvent(event);
    
    // Trigger completion on typing
    if (m_autoCompletionsEnabled && event->text().length() > 0) {
        std::string lineText = getCurrentLineText();
        if (shouldTriggerCompletion(lineText)) {
            m_completionTimer->start();
        }
    }
}

void AgenticTextEdit::onTextChanged() {
    // Sync document to LSP
    if (m_lspClient && m_documentOpened) {
        syncDocumentToLSP();
    }
    
    // Clear ghost text on manual edit
    if (m_ghostRenderer && m_ghostRenderer->hasGhostText()) {
        m_ghostRenderer->clearGhostText();
    }
}

void AgenticTextEdit::onCursorPositionChanged() {
    // Clear ghost text when cursor moves
    if (m_ghostRenderer && m_ghostRenderer->hasGhostText()) {
        m_ghostRenderer->clearGhostText();
    }
    
    // Restart completion timer
    if (m_autoCompletionsEnabled) {
        m_completionTimer->start();
    }
}

void AgenticTextEdit::onCompletionTimeout() {
    if (!m_autoCompletionsEnabled) return;
    
    std::string lineText = getCurrentLineText();
    if (shouldTriggerCompletion(lineText)) {
        triggerCompletion();
    }
}

void AgenticTextEdit::onCompletionsReceived(const std::string& uri, int line, int character, 
                                             const std::vector<CompletionItem>& items) {
    if (uri != m_documentUri || items.isEmpty()) return;
    
    // Use first completion item
    const CompletionItem& item = items.first();
    
    m_ghostRenderer->showGhostText(item.insertText, "completion");
}

void AgenticTextEdit::onGhostTextAccepted(const std::string& text) {
    completionAccepted(text);
}

void AgenticTextEdit::onGhostTextDismissed() {
    completionDismissed();
}

void AgenticTextEdit::triggerCompletion() {
    // Priority: AI completions > LSP completions
    // AI completions provide better context-aware suggestions
    
    if (m_aiCompletionsEnabled && m_aiProvider) {
        QTextCursor cursor = textCursor();
        std::string currentLine = getCurrentLineText();
        int cursorColumn = cursor.columnNumber();
        
        // Get some context lines (5 before current line)
        QTextBlock block = cursor.block();
        std::vector<std::string> contextLines;
        QTextBlock contextBlock = block.previous();
        for (int i = 0; i < 5 && contextBlock.isValid(); ++i) {
            contextLines.prepend(contextBlock.text());
            contextBlock = contextBlock.previous();
        }
        
        
        // Extract prefix/suffix
        std::string prefix = currentLine.left(cursorColumn);
        std::string suffix = currentLine.mid(cursorColumn);
        
        // Determine file type from language ID
        std::string fileType = m_languageId;
        
        // Request completions
        m_aiProvider->requestCompletions(
            prefix,
            suffix,
            m_documentUri,
            fileType,
            contextLines
        );
        
        return;  // Skip LSP if AI is enabled
    }
    
    // Fallback to LSP completions
    if (m_lspClient && m_lspClient->isRunning()) {
        QTextCursor cursor = textCursor();
        int line = cursor.blockNumber();
        int character = cursor.columnNumber();
        
        m_lspClient->requestCompletions(m_documentUri, line, character);
    }
}

void AgenticTextEdit::syncDocumentToLSP() {
    if (!m_lspClient || m_documentUri.isEmpty()) return;
    
    std::string text = toPlainText();
    m_documentVersion++;
    
    if (!m_documentOpened) {
        m_lspClient->openDocument(m_documentUri, m_languageId, text);
        m_documentOpened = true;
    } else {
        m_lspClient->updateDocument(m_documentUri, text, m_documentVersion);
    }
}

std::string AgenticTextEdit::getCurrentLineText() const {
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    return block.text();
}

bool AgenticTextEdit::shouldTriggerCompletion(const std::string& lineText) const {
    if (lineText.isEmpty()) return false;
    
    // Trigger on dot/arrow operators (C++)
    if (lineText.endsWith('.') || lineText.endsWith("->")) {
        return true;
    }
    
    // Trigger on colon (Python, C++)
    if (lineText.endsWith(':') || lineText.endsWith("::")) {
        return true;
    }
    
    // Trigger after typing 2+ characters
    std::string trimmed = lineText.trimmed();
    if (trimmed.length() >= 2) {
        return true;
    }
    
    return false;
}

void AgenticTextEdit::setAICompletionProvider(AICompletionProvider* provider) {
    if (m_aiProvider) {
        // Disconnect old provider
// Qt disconnect removed
    }
    
    m_aiProvider = provider;
    
    if (m_aiProvider) {
        // Connect to AI provider signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
                });
        
    }
}

void AgenticTextEdit::setAICompletionsEnabled(bool enabled) {
    m_aiCompletionsEnabled = enabled;
}

void AgenticTextEdit::onAICompletionsReceived(const std::vector<AICompletion>& completions) {
    if (completions.isEmpty()) {
        return;
    }
    
    // Use the best completion (first one, already sorted by confidence)
    const AICompletion& best = completions.first();
    
             << (best.confidence * 100) << "%)" 
             << "Text:" << best.text.left(50);
    
    // Show as ghost text
    m_ghostRenderer->showGhostText(best.text, "ai-completion");
}

void AgenticTextEdit::onAICompletionError(const std::string& error) {
    
    // Fallback to LSP if AI fails
    if (m_lspClient && m_lspClient->isRunning()) {
        QTextCursor cursor = textCursor();
        m_lspClient->requestCompletions(m_documentUri, cursor.blockNumber(), cursor.columnNumber());
    }
}

} // namespace RawrXD

