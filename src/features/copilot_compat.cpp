// ============================================================================
// copilot_compat.cpp — GitHub Copilot Compatibility Shim Implementation
// ============================================================================
#include "copilot_compat.h"
#include <sstream>

CopilotCompat& CopilotCompat::instance() {
    static CopilotCompat s;
    return s;
}

void CopilotCompat::setInlineDelegate(InlineDelegate fn) { m_inlineFn = std::move(fn); }
void CopilotCompat::setChatDelegate(ChatDelegate fn)     { m_chatFn   = std::move(fn); }

void CopilotCompat::activate() {
    if (m_active) return;
    m_active = true;

    auto& reg = CommandRegistry::instance();

    // Override stub handlers with real Copilot-routing handlers
    reg.overrideCommand("github.copilot.generate", [this](const CommandArgs& a) {
        return handleGenerate(a);
    }, "copilot-compat");

    reg.overrideCommand("github.copilot.chat.open", [this](const CommandArgs& a) {
        return handleChatOpen(a);
    }, "copilot-compat");

    reg.overrideCommand("github.copilot.explain", [this](const CommandArgs& a) {
        return handleExplain(a);
    }, "copilot-compat");

    reg.overrideCommand("github.copilot.fix", [this](const CommandArgs& a) {
        return handleFix(a);
    }, "copilot-compat");

    reg.overrideCommand("github.copilot.tests", [this](const CommandArgs& a) {
        return handleTests(a);
    }, "copilot-compat");

    reg.overrideCommand("github.copilot.review", [this](const CommandArgs& a) {
        return handleReview(a);
    }, "copilot-compat");

    // Additional Copilot commands (chat panel protocol)
    reg.registerCommand({ "github.copilot.acceptSuggestion",
        "Copilot: Accept Suggestion", "Copilot", "Tab", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs& a) -> CommandResult {
            return CommandRegistry::instance().execute("ai.acceptSuggestion", a);
        }});

    reg.registerCommand({ "github.copilot.dismissSuggestion",
        "Copilot: Dismiss Suggestion", "Copilot", "Escape", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs& a) -> CommandResult {
            return CommandRegistry::instance().execute("ai.dismissSuggestion", a);
        }});

    reg.registerCommand({ "github.copilot.nextSuggestion",
        "Copilot: Next Suggestion", "Copilot", "Alt+]", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs& a) -> CommandResult {
            return CommandRegistry::instance().execute("ai.nextSuggestion", a);
        }});

    reg.registerCommand({ "github.copilot.prevSuggestion",
        "Copilot: Previous Suggestion", "Copilot", "Alt+[", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs& a) -> CommandResult {
            return CommandRegistry::instance().execute("ai.prevSuggestion", a);
        }});

    reg.registerCommand({ "github.copilot.toggleSuggestions",
        "Copilot: Toggle Suggestions", "Copilot", "", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs&) -> CommandResult {
            static bool enabled = true;
            enabled = !enabled;
            CommandRegistry::instance().setEnabled("ai.inlineComplete", enabled);
            return { true, enabled ? "\"enabled\"" : "\"disabled\"", "" };
        }});

    // Copilot Chat: @workspace, @vscode slash commands
    reg.registerCommand({ "github.copilot.chat.insertAtCursor",
        "Copilot Chat: Insert at Cursor", "Copilot", "", "",
        CMD_ACCESS_ALL, true, [](const CommandArgs&) -> CommandResult { return {true}; }});

    reg.registerCommand({ "github.copilot.chat.copyToClipboard",
        "Copilot Chat: Copy to Clipboard", "Copilot", "", "",
        CMD_ACCESS_ALL, true, [](const CommandArgs&) -> CommandResult { return {true}; }});

    reg.registerCommand({ "github.copilot.chat.runInTerminal",
        "Copilot Chat: Run in Terminal", "Copilot", "", "",
        CMD_ACCESS_ALL, true,
        [](const CommandArgs& a) -> CommandResult {
            return CommandRegistry::instance().execute("workbench.action.terminal.new", a);
        }});

    // Register inline provider through the extension language API
    if (m_inlineFn) {
        vscode::languages::registerInlineCompletionItemProvider(
            "*",
            [this](const std::string& docText, size_t pos) -> std::vector<std::string> {
                auto items = m_inlineFn(docText, pos, "");
                std::vector<std::string> texts;
                texts.reserve(items.size());
                for (auto& i : items) texts.push_back(i.text);
                return texts;
            });
    }

    ExtensionCommandBridge::instance().onExtensionActivate({
        "copilot-compat", "", "", "1.0.0" });
}

void CopilotCompat::deactivate() {
    if (!m_active) return;
    m_active = false;

    auto& reg = CommandRegistry::instance();
    const char* overrides[] = {
        "github.copilot.generate",    "github.copilot.chat.open",
        "github.copilot.explain",     "github.copilot.fix",
        "github.copilot.tests",       "github.copilot.review"
    };
    for (const char* c : overrides)
        reg.releaseOverride(c, "copilot-compat");

    ExtensionCommandBridge::instance().onExtensionDeactivate("copilot-compat");
}

// ---- Handlers ---------------------------------------------------------------
CommandResult CopilotCompat::handleGenerate(const CommandArgs& args) {
    return CommandRegistry::instance().execute("ai.inlineComplete", args);
}

CommandResult CopilotCompat::handleChatOpen(const CommandArgs& args) {
    return CommandRegistry::instance().execute("ai.openChat", args);
}

CommandResult CopilotCompat::handleExplain(const CommandArgs& args) {
    if (!m_chatFn) return CommandRegistry::instance().execute("ai.explainCode", args);
    auto text = vscode::window::getActiveEditorText();
    if (!text) return { false, "", "No active editor" };
    std::string result = m_chatFn(
        "You are GitHub Copilot. Explain the following code clearly.",
        *text);
    vscode::window::showInformationMessage(result);
    return { true, "{}", "" };
}

CommandResult CopilotCompat::handleFix(const CommandArgs& args) {
    if (!m_chatFn) return CommandRegistry::instance().execute("ai.fixErrors", args);
    auto text = vscode::window::getActiveEditorText();
    if (!text) return { false, "", "No active editor" };
    std::string result = m_chatFn(
        "You are GitHub Copilot. Fix bugs in the following code.",
        *text);
    vscode::window::showInformationMessage(result);
    return { true, "{}", "" };
}

CommandResult CopilotCompat::handleTests(const CommandArgs& args) {
    if (!m_chatFn) return CommandRegistry::instance().execute("ai.generateTests", args);
    auto text = vscode::window::getActiveEditorText();
    if (!text) return { false, "", "No active editor" };
    std::string result = m_chatFn(
        "You are GitHub Copilot. Write comprehensive unit tests for the following code.",
        *text);
    vscode::window::showInformationMessage(result);
    return { true, "{}", "" };
}

CommandResult CopilotCompat::handleReview(const CommandArgs& args) {
    if (!m_chatFn) return CommandRegistry::instance().execute("ai.explainCode", args);
    auto text = vscode::window::getActiveEditorText();
    if (!text) return { false, "", "No active editor" };
    std::string result = m_chatFn(
        "You are GitHub Copilot. Review the following code for correctness, "
        "style, security and performance.",
        *text);
    vscode::window::showInformationMessage(result);
    return { true, "{}", "" };
}
