/**
 * \file agentic_text_edit.cpp
 * \brief Agentic text editor implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "agentic_text_edit.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>

namespace RawrXD {

AgenticTextEdit::AgenticTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
{
    // Lightweight constructor - defer widget creation to initialize()
}

void AgenticTextEdit::initialize() {
    // Create ghost text renderer overlay
    m_ghostRenderer = new GhostTextRenderer(this, this);
    m_ghostRenderer->initialize();
    
    // Create completion timer
    m_completionTimer = new QTimer(this);
    m_completionTimer->setSingleShot(true);
    m_completionTimer->setInterval(m_completionDelay);
    
    // Connect signals
    connect(this, &QPlainTextEdit::textChanged, this, &AgenticTextEdit::onTextChanged);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &AgenticTextEdit::onCursorPositionChanged);
    connect(m_completionTimer, &QTimer::timeout, this, &AgenticTextEdit::onCompletionTimeout);
    
    connect(m_ghostRenderer, &GhostTextRenderer::ghostTextAccepted,
            this, &AgenticTextEdit::onGhostTextAccepted);
    connect(m_ghostRenderer, &GhostTextRenderer::ghostTextDismissed,
            this, &AgenticTextEdit::onGhostTextDismissed);
    
    qDebug() << "[AgenticTextEdit] Initialized with ghost text renderer";
}

void AgenticTextEdit::setLSPClient(LSPClient* client) {
    if (m_lspClient) {
        // Disconnect old client
        disconnect(m_lspClient, nullptr, this, nullptr);
        
        // Close document if opened
        if (m_documentOpened && !m_documentUri.isEmpty()) {
            m_lspClient->closeDocument(m_documentUri);
            m_documentOpened = false;
        }
    }
    
    m_lspClient = client;
    
    if (m_lspClient) {
        // Connect to LSP client signals
        connect(m_lspClient, &LSPClient::completionsReceived,
                this, &AgenticTextEdit::onCompletionsReceived);
        connect(m_lspClient, &LSPClient::serverReady, this, [this]() {
            qDebug() << "[AgenticTextEdit] LSP server ready, opening document";
            syncDocumentToLSP();
        });
        
        // Open document if server is already running
        if (m_lspClient->isRunning() && !m_documentUri.isEmpty()) {
            syncDocumentToLSP();
        }
    }
}

void AgenticTextEdit::setDocumentUri(const QString& uri) {
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
    
    qDebug() << "[AgenticTextEdit] Document URI set:" << uri << "Language:" << m_languageId;
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
        if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
            m_ghostRenderer->acceptGhostText();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            m_ghostRenderer->clearGhostText();
            emit completionDismissed();
            return;
        }
    }
    
    // Pass to base class
    QPlainTextEdit::keyPressEvent(event);
    
    // Trigger completion on typing
    if (m_autoCompletionsEnabled && event->text().length() > 0) {
        QString lineText = getCurrentLineText();
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
    
    QString lineText = getCurrentLineText();
    if (shouldTriggerCompletion(lineText)) {
        triggerCompletion();
    }
}

void AgenticTextEdit::onCompletionsReceived(const QString& uri, int line, int character, 
                                             const QVector<CompletionItem>& items) {
    if (uri != m_documentUri || items.isEmpty()) return;
    
    // Use first completion item
    const CompletionItem& item = items.first();
    
    qDebug() << "[AgenticTextEdit] Showing ghost text:" << item.insertText;
    m_ghostRenderer->showGhostText(item.insertText, "completion");
}

void AgenticTextEdit::onGhostTextAccepted(const QString& text) {
    qDebug() << "[AgenticTextEdit] Ghost text accepted:" << text;
    emit completionAccepted(text);
}

void AgenticTextEdit::onGhostTextDismissed() {
    qDebug() << "[AgenticTextEdit] Ghost text dismissed";
    emit completionDismissed();
}

void AgenticTextEdit::triggerCompletion() {
    // Priority: AI completions > LSP completions
    // AI completions provide better context-aware suggestions
    
    if (m_aiCompletionsEnabled && m_aiProvider) {
        QTextCursor cursor = textCursor();
        QString currentLine = getCurrentLineText();
        int cursorColumn = cursor.columnNumber();
        
        // Get some context lines (5 before current line)
        QTextBlock block = cursor.block();
        QStringList contextLines;
        QTextBlock contextBlock = block.previous();
        for (int i = 0; i < 5 && contextBlock.isValid(); ++i) {
            contextLines.prepend(contextBlock.text());
            contextBlock = contextBlock.previous();
        }
        
        qDebug() << "[AgenticTextEdit] Requesting AI completions at column" << cursorColumn;
        
        // Extract prefix/suffix
        QString prefix = currentLine.left(cursorColumn);
        QString suffix = currentLine.mid(cursorColumn);
        
        // Determine file type from language ID
        QString fileType = m_languageId;
        
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
        
        qDebug() << "[AgenticTextEdit] Requesting LSP completions at" << line << ":" << character;
        m_lspClient->requestCompletions(m_documentUri, line, character);
    }
}

void AgenticTextEdit::syncDocumentToLSP() {
    if (!m_lspClient || m_documentUri.isEmpty()) return;
    
    QString text = toPlainText();
    m_documentVersion++;
    
    if (!m_documentOpened) {
        m_lspClient->openDocument(m_documentUri, m_languageId, text);
        m_documentOpened = true;
        qDebug() << "[AgenticTextEdit] Document opened in LSP";
    } else {
        m_lspClient->updateDocument(m_documentUri, text, m_documentVersion);
    }
}

QString AgenticTextEdit::getCurrentLineText() const {
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    return block.text();
}

bool AgenticTextEdit::shouldTriggerCompletion(const QString& lineText) const {
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
    QString trimmed = lineText.trimmed();
    if (trimmed.length() >= 2) {
        return true;
    }
    
    return false;
}

void AgenticTextEdit::setAICompletionProvider(AICompletionProvider* provider) {
    if (m_aiProvider) {
        // Disconnect old provider
        disconnect(m_aiProvider, nullptr, this, nullptr);
    }
    
    m_aiProvider = provider;
    
    if (m_aiProvider) {
        // Connect to AI provider signals
        connect(m_aiProvider, &AICompletionProvider::completionsReady,
                this, &AgenticTextEdit::onAICompletionsReceived);
        connect(m_aiProvider, &AICompletionProvider::error,
                this, &AgenticTextEdit::onAICompletionError);
        connect(m_aiProvider, &AICompletionProvider::latencyReported,
                this, [](double ms) {
                    qDebug() << "[AgenticTextEdit] AI completion latency:" << ms << "ms";
                });
        
        qDebug() << "[AgenticTextEdit] AI completion provider connected";
    }
}

void AgenticTextEdit::setAICompletionsEnabled(bool enabled) {
    m_aiCompletionsEnabled = enabled;
    qDebug() << "[AgenticTextEdit] AI completions" << (enabled ? "enabled" : "disabled");
}

void AgenticTextEdit::onAICompletionsReceived(const QVector<AICompletion>& completions) {
    if (completions.isEmpty()) {
        qDebug() << "[AgenticTextEdit] No AI completions received";
        return;
    }
    
    // Use the best completion (first one, already sorted by confidence)
    const AICompletion& best = completions.first();
    
    qDebug() << "[AgenticTextEdit] Showing AI completion (confidence:" 
             << (best.confidence * 100) << "%)" 
             << "Text:" << best.text.left(50);
    
    // Show as ghost text
    m_ghostRenderer->showGhostText(best.text, "ai-completion");
}

void AgenticTextEdit::onAICompletionError(const QString& error) {
    qWarning() << "[AgenticTextEdit] AI completion error:" << error;
    
    // Fallback to LSP if AI fails
    if (m_lspClient && m_lspClient->isRunning()) {
        qDebug() << "[AgenticTextEdit] Falling back to LSP completions";
        QTextCursor cursor = textCursor();
        m_lspClient->requestCompletions(m_documentUri, cursor.blockNumber(), cursor.columnNumber());
    }
}

} // namespace RawrXD
