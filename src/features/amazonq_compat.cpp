// ============================================================================
// amazonq_compat.cpp — Amazon Q Compatibility Shim Implementation
// ============================================================================
#include "amazonq_compat.h"
#include "extension_command_bridge.h"
#include <sstream>

AmazonQCompat& AmazonQCompat::instance() {
    static AmazonQCompat s;
    return s;
}

void AmazonQCompat::setChatDelegate(ChatDelegate fn) {
    m_chatFn = std::move(fn);
}

bool AmazonQCompat::isConnected() const { return m_active && (bool)m_chatFn; }
std::string AmazonQCompat::currentModelName() const { return m_modelName; }

// ---- Activation -----------------------------------------------------------
void AmazonQCompat::activate() {
    if (m_active) return;
    m_active = true;

    auto& reg = CommandRegistry::instance();

    // Override the already-registered AWS commands with real handlers
    reg.overrideCommand("aws.amazonq.chat.open", [this](const CommandArgs& a) -> CommandResult {
        handleChatOpen(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.explainCode", [this](const CommandArgs& a) -> CommandResult {
        handleExplainCode(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.refactorCode", [this](const CommandArgs& a) -> CommandResult {
        handleRefactorCode(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.generateTests", [this](const CommandArgs& a) -> CommandResult {
        handleGenerateTests(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.fixCode", [this](const CommandArgs& a) -> CommandResult {
        handleFixCode(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.scanCode", [this](const CommandArgs& a) -> CommandResult {
        handleSecurityScan(a); return { true };
    }, "amazon-q-compat");

    reg.overrideCommand("aws.amazonq.sendToPrompt", [this](const CommandArgs& a) -> CommandResult {
        handleSendToPrompt(a); return { true };
    }, "amazon-q-compat");

    // Additional Amazon Q commands expected by the VSIX side-channel:
    reg.registerCommand({ "aws.amazonq.toggleCodeSuggestions",
        "Amazon Q: Toggle Code Suggestions", "Amazon Q", "",
        "", CMD_ACCESS_ALL, true,
        [this](const CommandArgs&) -> CommandResult {
            // Toggle inline completions
            bool cur = CommandRegistry::instance().hasCommand("ai.inlineComplete");
            CommandRegistry::instance().setEnabled("ai.inlineComplete", !cur);
            return { true };
        }});

    reg.registerCommand({ "aws.amazonq.openSettings",
        "Amazon Q: Open Settings", "Amazon Q", "",
        "", CMD_ACCESS_ALL, true,
        [](const CommandArgs&) -> CommandResult {
            CommandRegistry::instance().execute("workbench.action.openSettings");
            return { true };
        }});

    reg.registerCommand({ "aws.amazonq.showOutput",
        "Amazon Q: Show Output", "Amazon Q", "",
        "", CMD_ACCESS_ALL, true,
        [](const CommandArgs&) -> CommandResult {
            CommandRegistry::instance().execute("view.toggleOutput");
            return { true };
        }});

    // Notify the extension bridge that Amazon Q is active
    ExtensionCommandBridge::instance().onExtensionActivate({
        "amazon-q-compat", "", "", "1.0.0" });
}

void AmazonQCompat::deactivate() {
    if (!m_active) return;
    m_active = false;

    auto& reg = CommandRegistry::instance();
    const char* cmds[] = {
        "aws.amazonq.chat.open", "aws.amazonq.explainCode",
        "aws.amazonq.refactorCode", "aws.amazonq.generateTests",
        "aws.amazonq.fixCode", "aws.amazonq.scanCode",
        "aws.amazonq.sendToPrompt"
    };
    for (const char* c : cmds)
        reg.releaseOverride(c, "amazon-q-compat");

    ExtensionCommandBridge::instance().onExtensionDeactivate("amazon-q-compat");
}

// ---- Handlers -------------------------------------------------------------
void AmazonQCompat::handleChatOpen(const CommandArgs& /*args*/) {
    // Delegate to RawrXD AI chat panel command
    CommandRegistry::instance().execute("ai.openChat");
}

void AmazonQCompat::handleExplainCode(const CommandArgs& args) {
    if (!m_chatFn) {
        CommandRegistry::instance().execute("ai.explainCode", args);
        return;
    }
    auto text = vscode::window::getActiveEditorText();
    if (!text) return;
    std::string result = m_chatFn({}, "Explain the following code:\n\n" + *text);
    vscode::window::showInformationMessage(result);
}

void AmazonQCompat::handleRefactorCode(const CommandArgs& args) {
    if (!m_chatFn) { CommandRegistry::instance().execute("ai.refactorCode", args); return; }
    auto text = vscode::window::getActiveEditorText();
    if (!text) { vscode::window::showWarningMessage("No active editor."); return; }
    std::string result = m_chatFn({}, "Refactor the following code for clarity and correctness:\n\n" + *text);
    vscode::window::showInformationMessage(result);
}

void AmazonQCompat::handleGenerateTests(const CommandArgs& args) {
    if (!m_chatFn) { CommandRegistry::instance().execute("ai.generateTests", args); return; }
    auto text = vscode::window::getActiveEditorText();
    if (!text) { vscode::window::showWarningMessage("No active editor."); return; }
    std::string result = m_chatFn({}, "Generate unit tests for the following code:\n\n" + *text);
    vscode::window::showInformationMessage(result);
}

void AmazonQCompat::handleFixCode(const CommandArgs& args) {
    if (!m_chatFn) { CommandRegistry::instance().execute("ai.fixErrors", args); return; }
    auto text = vscode::window::getActiveEditorText();
    if (!text) { vscode::window::showWarningMessage("No active editor."); return; }
    std::string result = m_chatFn({}, "Fix any bugs in the following code:\n\n" + *text);
    vscode::window::showInformationMessage(result);
}

void AmazonQCompat::handleSecurityScan(const CommandArgs& args) {
    if (!m_chatFn) { CommandRegistry::instance().execute("ai.optimizeCode", args); return; }
    auto text = vscode::window::getActiveEditorText();
    if (!text) { vscode::window::showWarningMessage("No active editor."); return; }
    std::string result = m_chatFn({},
        "Perform a security analysis of the following code. "
        "Identify OWASP Top-10 risks and suggest mitigations:\n\n" + *text);
    vscode::window::showInformationMessage(result);
}

void AmazonQCompat::handleSendToPrompt(const CommandArgs& args) {
    // Sends selected text to the AI chat prompt
    auto text = vscode::window::getActiveEditorText();
    if (!text || text->empty()) return;
    CommandArgs chatArgs = args;
    chatArgs.jsonPayload = "{\"prompt\":\"" + *text + "\"}";
    CommandRegistry::instance().execute("ai.openChat", chatArgs);
}
