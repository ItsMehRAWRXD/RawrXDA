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
    if (!m_lspClient || !m_lspClient->isRunning()) return;
    
    QTextCursor cursor = textCursor();
    int line = cursor.blockNumber();
    int character = cursor.columnNumber();
    
    qDebug() << "[AgenticTextEdit] Requesting completions at" << line << ":" << character;
    m_lspClient->requestCompletions(m_documentUri, line, character);
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

} // namespace RawrXD
