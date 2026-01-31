/**
 * \file agentic_text_edit.h
 * \brief Agentic text editor with LSP completions and ghost text
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once


#include "lsp_client.h"
#include "ghost_text_renderer.h"
#include "ai_completion_provider.h"

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

public:
    explicit AgenticTextEdit(void* parent = nullptr);
    ~AgenticTextEdit() override = default;

    /**
     * Two-phase initialization
     * Call after void is ready
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
     * Set AI completion provider for this editor
     * Enables Cursor-style AI completions powered by local GGUF models
     */
    void setAICompletionProvider(AICompletionProvider* provider);

    /**
     * Get current AI completion provider
     */
    AICompletionProvider* aiCompletionProvider() const { return m_aiProvider; }

    /**
     * Enable/disable AI completions (separate from LSP)
     */
    void setAICompletionsEnabled(bool enabled);

    /**
     * Check if AI completions are enabled
     */
    bool aiCompletionsEnabled() const { return m_aiCompletionsEnabled; }

    /**
     * Get ghost text renderer
     */
    GhostTextRenderer* ghostRenderer() const { return m_ghostRenderer; }

    /**
     * Set document URI (for LSP communication)
     */
    void setDocumentUri(const std::string& uri);

    /**
     * Get document URI
     */
    std::string documentUri() const { return m_documentUri; }

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


    /**
     * Emitted when ghost text is accepted
     */
    void completionAccepted(const std::string& text);

    /**
     * Emitted when ghost text is dismissed
     */
    void completionDismissed();

protected:
    void keyPressEvent(void*  event) override;

private:
    void onTextChanged();
    void onCursorPositionChanged();
    void onCompletionTimeout();
    void onCompletionsReceived(const std::string& uri, int line, int character, const std::vector<CompletionItem>& items);
    void onAICompletionsReceived(const std::vector<AICompletion>& completions);
    void onAICompletionError(const std::string& error);
    void onGhostTextAccepted(const std::string& text);
    void onGhostTextDismissed();

private:
    void triggerCompletion();
    void syncDocumentToLSP();
    std::string getCurrentLineText() const;
    bool shouldTriggerCompletion(const std::string& lineText) const;

    LSPClient* m_lspClient{};
    AICompletionProvider* m_aiProvider{};
    GhostTextRenderer* m_ghostRenderer{};
    
    std::string m_documentUri;
    std::string m_languageId = "cpp";
    int m_documentVersion = 0;
    
    void** m_completionTimer{};
    int m_completionDelay = 300;  // 300ms debounce
    bool m_autoCompletionsEnabled = true;
    bool m_aiCompletionsEnabled = true;  // AI completions enabled by default
    
    bool m_documentOpened = false;
};

} // namespace RawrXD

