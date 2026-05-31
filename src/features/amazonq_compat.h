// ============================================================================
// amazonq_compat.h — Amazon Q Extension Compatibility Shim
// ============================================================================
// Registers all Amazon Q command IDs that the extension expects so that:
//   1. Existing Amazon Q VSIX activates without modification.
//   2. RawrXD's AI backend answers Amazon Q API calls.
//   3. The same commands appear in the command palette for end users.
//   4. Agentic code can call amazon Q actions by ID.
// ============================================================================
#pragma once
#include <string>
#include <functional>
#include "../ui/command_registry.h"

// ---- Activation events Amazon Q expects -----------------------------------
//  "onCommand:aws.amazonq.chat.open"
//  "onCommand:aws.amazonq.*"

// ---- Amazon Q chat message structure --------------------------------------
struct AmazonQMessage {
    std::string role;     // "user" | "assistant"
    std::string content;
};

// ---- Amazon Q compat API --------------------------------------------------
class AmazonQCompat {
public:
    static AmazonQCompat& instance();

    // Must be called at IDE startup (after CommandRegistry::registerBuiltins)
    void activate();
    void deactivate();

    // Set the backend AI delegate (called by RawrXD AI subsystem)
    using ChatDelegate = std::function<std::string(
        const std::vector<AmazonQMessage>& history,
        const std::string& newMessage)>;
    void setChatDelegate(ChatDelegate fn);

    // Query current status for palette/menu enablement
    bool isConnected() const;
    std::string currentModelName() const;

private:
    AmazonQCompat() = default;
    ChatDelegate m_chatFn;
    bool         m_active = false;
    std::string  m_modelName = "Amazon Q (RawrXD Backend)";

    void handleChatOpen(const CommandArgs& args);
    void handleExplainCode(const CommandArgs& args);
    void handleRefactorCode(const CommandArgs& args);
    void handleGenerateTests(const CommandArgs& args);
    void handleFixCode(const CommandArgs& args);
    void handleSecurityScan(const CommandArgs& args);
    void handleSendToPrompt(const CommandArgs& args);
};
