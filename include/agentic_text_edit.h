<<<<<<< HEAD
/**
 * \file agentic_text_edit.h
 * \brief Agentic text editor with LSP completions and ghost text — C++20, no Qt
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

// ============================================================================
// AgenticTextEdit — Qt-free stub. Use Win32 Rich Edit / custom control in impl.
// ============================================================================

#include <functional>
#include <string>

namespace RawrXD {

class AICompletionProvider;
class LSPClient;
class GhostTextRenderer;

/**
 * Enhanced text editor with LSP integration and ghost text (Qt-free).
 * Two-phase init; LSP client; ghost text overlay; completion debounce (e.g. 300 ms).
 */
class AgenticTextEdit {
public:
    using CompletionAcceptedFn = std::function<void(const std::string&)>;

    AgenticTextEdit() = default;
    explicit AgenticTextEdit(void* /*parent*/) {}
    ~AgenticTextEdit() = default;

    void initialize();

    void setLSPClient(LSPClient* client);
    LSPClient* lspClient() const { return m_lspClient; }

    void setAICompletionProvider(AICompletionProvider* provider);
    AICompletionProvider* aiCompletionProvider() const { return m_aiProvider; }

    GhostTextRenderer* ghostRenderer() const { return m_ghostRenderer; }

    void setDocumentUri(const std::string& uri);
    std::string documentUri() const { return m_documentUri; }

    void setAutoCompletionsEnabled(bool enabled);
    bool autoCompletionsEnabled() const { return m_autoCompletionsEnabled; }

    void setCompletionDelay(int ms);

    void setOnCompletionAccepted(CompletionAcceptedFn fn) { m_onCompletionAccepted = std::move(fn); }

protected:
    std::string getCurrentLineText() const;
    bool shouldTriggerCompletion(const std::string& lineText) const;

private:
    void triggerCompletion();
    void syncDocumentToLSP();

    LSPClient* m_lspClient = nullptr;
    AICompletionProvider* m_aiProvider = nullptr;
    GhostTextRenderer* m_ghostRenderer = nullptr;

    std::string m_documentUri;
    std::string m_languageId = "cpp";
    int m_documentVersion = 0;

    int m_completionDelay = 300;
    bool m_autoCompletionsEnabled = true;
    bool m_documentOpened = false;

    CompletionAcceptedFn m_onCompletionAccepted;
};

} // namespace RawrXD
=======
/**
 * \file agentic_text_edit.h
 * \brief Agentic text editor with LSP completions and ghost text — C++20, no Qt
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

// ============================================================================
// AgenticTextEdit — Qt-free; use Win32 Rich Edit / custom control in impl.
// ============================================================================

#include <functional>
#include <string>

namespace RawrXD {

class AICompletionProvider;
class LSPClient;
class GhostTextRenderer;

/**
 * Enhanced text editor with LSP integration and ghost text (Qt-free).
 * Two-phase init; LSP client; ghost text overlay; completion debounce (e.g. 300 ms).
 */
class AgenticTextEdit {
public:
    using CompletionAcceptedFn = std::function<void(const std::string&)>;

    AgenticTextEdit() = default;
    explicit AgenticTextEdit(void* /*parent*/) {}
    ~AgenticTextEdit() = default;

    void initialize();

    void setLSPClient(LSPClient* client);
    LSPClient* lspClient() const { return m_lspClient; }

    void setAICompletionProvider(AICompletionProvider* provider);
    AICompletionProvider* aiCompletionProvider() const { return m_aiProvider; }

    GhostTextRenderer* ghostRenderer() const { return m_ghostRenderer; }

    void setDocumentUri(const std::string& uri);
    std::string documentUri() const { return m_documentUri; }

    void setAutoCompletionsEnabled(bool enabled);
    bool autoCompletionsEnabled() const { return m_autoCompletionsEnabled; }

    void setCompletionDelay(int ms);

    void setOnCompletionAccepted(CompletionAcceptedFn fn) { m_onCompletionAccepted = std::move(fn); }

protected:
    std::string getCurrentLineText() const;
    bool shouldTriggerCompletion(const std::string& lineText) const;

private:
    void triggerCompletion();
    void syncDocumentToLSP();

    LSPClient* m_lspClient = nullptr;
    AICompletionProvider* m_aiProvider = nullptr;
    GhostTextRenderer* m_ghostRenderer = nullptr;

    std::string m_documentUri;
    std::string m_languageId = "cpp";
    int m_documentVersion = 0;

    int m_completionDelay = 300;
    bool m_autoCompletionsEnabled = true;
    bool m_documentOpened = false;

    CompletionAcceptedFn m_onCompletionAccepted;
};

} // namespace RawrXD
>>>>>>> origin/main
