// ============================================================================
// copilot_compat.h — GitHub Copilot Extension Compatibility Shim
// ============================================================================
// Registers the command IDs and LSP/inline-completion API surface that
// GitHub Copilot and GitHub Copilot Chat extensions expect, routing all
// calls through RawrXD's command registry and AI backend.
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../ui/command_registry.h"
#include "extension_command_bridge.h"

// ---- Copilot completion item -------------------------------------------------
struct CopilotCompletionItem {
    std::string text;         // Full insertion text
    std::string prefix;       // Already typed prefix (for display)
    std::string range;        // JSON range to replace
};

// ---- Copilot compat API ------------------------------------------------------
class CopilotCompat {
public:
    static CopilotCompat& instance();

    // Call at startup after CommandRegistry::registerBuiltins()
    void activate();
    void deactivate();

    // Wire up your AI inference backend
    using InlineDelegate = std::function<std::vector<CopilotCompletionItem>(
        const std::string& fileContent,
        size_t             cursorOffset,
        const std::string& languageId)>;
    void setInlineDelegate(InlineDelegate fn);

    using ChatDelegate = std::function<std::string(
        const std::string& systemPrompt,
        const std::string& userMessage)>;
    void setChatDelegate(ChatDelegate fn);

    bool isActive() const { return m_active; }

private:
    CopilotCompat() = default;
    InlineDelegate m_inlineFn;
    ChatDelegate   m_chatFn;
    bool           m_active = false;

    CommandResult handleGenerate(const CommandArgs& args);
    CommandResult handleChatOpen(const CommandArgs& args);
    CommandResult handleExplain(const CommandArgs& args);
    CommandResult handleFix(const CommandArgs& args);
    CommandResult handleTests(const CommandArgs& args);
    CommandResult handleReview(const CommandArgs& args);
};
