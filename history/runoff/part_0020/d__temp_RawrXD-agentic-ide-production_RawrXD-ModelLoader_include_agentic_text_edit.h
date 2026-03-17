/**
 * \file agentic_text_edit.h
 * \brief Agentic text editor with LSP completions and ghost text
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

#include <QPlainTextEdit>
#include <QTimer>
#include "lsp_client.h"
#include "ghost_text_renderer.h"

namespace RawrXD {

/**
 * \brief Enhanced text editor with LSP integration and ghost text
 * 
 * Features:
 * - Two-phase initialization (lightweight constructor + explicit initialize())
 * - LSP client integration for real-time completions
 * - Ghost text overlay for inline suggestions
 * - Tab to accept, Esc to dismiss
 * - Auto-trigger completions on typing pause (300ms debounce)
 * - Multi-language support (C++, Python, JavaScript, etc.)
 */
class AgenticTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit AgenticTextEdit(QWidget* parent = nullptr);
    ~AgenticTextEdit() override = default;

    /**
     * Two-phase initialization
     * Call after QApplication is ready
     */
    void initialize();

    /**
     * Set LSP client for this editor
     */
    void setLSPClient(LSPClient* client);

    /**
     * Get current LSP client
     */
    LSPClient* lspClient() const { return m_lspClient; }

    /**
     * Get ghost text renderer
     */
    GhostTextRenderer* ghostRenderer() const { return m_ghostRenderer; }

    /**
     * Set document URI (for LSP communication)
     */
    void setDocumentUri(const QString& uri);

    /**
     * Get document URI
     */
    QString documentUri() const { return m_documentUri; }

    /**
     * Enable/disable auto-completions
     */
    void setAutoCompletionsEnabled(bool enabled);

    /**
     * Check if auto-completions are enabled
     */
    bool autoCompletionsEnabled() const { return m_autoCompletionsEnabled; }

    /**
     * Set completion debounce delay (milliseconds)
     */
    void setCompletionDelay(int ms);

signals:
    /**
     * Emitted when ghost text is accepted
     */
    void completionAccepted(const QString& text);

    /**
     * Emitted when ghost text is dismissed
     */
    void completionDismissed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTextChanged();
    void onCursorPositionChanged();
    void onCompletionTimeout();
    void onCompletionsReceived(const QString& uri, int line, int character, const QVector<CompletionItem>& items);
    void onGhostTextAccepted(const QString& text);
    void onGhostTextDismissed();

private:
    void triggerCompletion();
    void syncDocumentToLSP();
    QString getCurrentLineText() const;
    bool shouldTriggerCompletion(const QString& lineText) const;

    LSPClient* m_lspClient{};
    GhostTextRenderer* m_ghostRenderer{};
    
    QString m_documentUri;
    QString m_languageId = "cpp";
    int m_documentVersion = 0;
    
    QTimer* m_completionTimer{};
    int m_completionDelay = 300;  // 300ms debounce
    bool m_autoCompletionsEnabled = true;
    
    bool m_documentOpened = false;
};

} // namespace RawrXD
