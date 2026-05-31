// ============================================================================
// extension_command_bridge.h — VS Code-Compatible Extension Command Bridge
// ============================================================================
// Provides the `vscode.commands.*` and `vscode.window.*` surface that
// extensions (Amazon Q, GitHub Copilot, etc.) expect. All calls are routed
// through CommandRegistry so that every extension action is visible in the
// menu and command palette, and callable from agentic code.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <any>
#include <optional>
#include "../ui/command_registry.h"

// ---- Extension context passed to activate() ------------------------------
struct ExtensionContext {
    std::string extensionId;       // "github.copilot"
    std::string extensionPath;     // Filesystem path to extension root
    std::string storagePath;       // Per-extension storage directory
    std::string version;
};

// ---- VS Code-compatible Disposable ----------------------------------------
struct Disposable {
    std::function<void()> dispose;
    ~Disposable() { if (dispose) dispose(); }
};

// ============================================================================
// Extension Commands API — vscode.commands namespace
// ============================================================================
namespace vscode {
namespace commands {

// Register a command callable by any surface (palette, menu, API, agentic).
Disposable registerCommand(const std::string& commandId,
                           std::function<CommandResult(const CommandArgs&)> handler,
                           const std::string& ownerExtId = "");

// Execute a registered command.
CommandResult executeCommand(const std::string& commandId,
                             const CommandArgs& args = {});

// List all registered commands (optionally including internal-only).
std::vector<std::string> getCommands(bool includeInternal = false);

} // namespace commands

// ============================================================================
// Window API — vscode.window namespace (minimal surface needed by extensions)
// ============================================================================
namespace window {

// Show a message to the user via status bar / notification popup.
void showInformationMessage(const std::string& message);
void showWarningMessage(const std::string& message);
void showErrorMessage(const std::string& message);

// Modal input dialog. Returns empty optional if cancelled.
std::optional<std::string> showInputBox(
    const std::string& prompt,
    const std::string& placeholder    = "",
    const std::string& defaultValue   = "",
    bool               password       = false);

// Quick-pick dropdown. Returns selected item or empty optional.
std::optional<std::string> showQuickPick(
    const std::vector<std::string>& items,
    const std::string& placeHolder = "",
    bool               canPickMany = false);

// Get current editor text
std::optional<std::string> getActiveEditorText();

// Set status bar message (returns a Disposable that clears it)
Disposable setStatusBarMessage(const std::string& text,
                                int timeoutMs = 0);

} // namespace window

// ============================================================================
// Workspace API — vscode.workspace namespace
// ============================================================================
namespace workspace {

std::optional<std::string> getWorkspaceRoot();
std::vector<std::string>   findFiles(const std::string& include,
                                     const std::string& exclude = "");
std::optional<std::string> readFile(const std::string& path);
bool                       writeFile(const std::string& path,
                                     const std::string& content);
} // namespace workspace

// ============================================================================
// Languages API — vscode.languages namespace
// ============================================================================
namespace languages {

// Register inline completion provider (co-pilot style ghost text)
Disposable registerInlineCompletionItemProvider(
    const std::string& languageId,
    std::function<std::vector<std::string>(const std::string& docText,
                                           size_t cursorPos)> provider);

// Register hover provider
Disposable registerHoverProvider(
    const std::string& languageId,
    std::function<std::string(const std::string& docText, size_t pos)> provider);

} // namespace languages

} // namespace vscode

// ============================================================================
// ExtensionCommandBridge — manages extension lifecycle + registration
// ============================================================================
class ExtensionCommandBridge {
public:
    static ExtensionCommandBridge& instance();

    // Called by the extension host when an extension activates
    void onExtensionActivate(const ExtensionContext& ctx);

    // Called when an extension deactivates (cleans up registered commands)
    void onExtensionDeactivate(const std::string& extensionId);

    // Check if an extension is active
    bool isActive(const std::string& extensionId) const;

    // List active extension IDs
    std::vector<std::string> activeExtensions() const;

    // Expose all commands registered by an extension
    std::vector<std::string> commandsForExtension(const std::string& extId) const;

    // JSON serialization of command surface (for REST API / agentic callers)
    std::string dumpCommandSurfaceJson() const;

private:
    ExtensionCommandBridge() = default;
    struct ExtState {
        ExtensionContext             ctx;
        std::vector<std::string>     registeredCommandIds;
        std::vector<Disposable*>     disposables;
    };
    mutable std::mutex                          m_mutex;
    std::unordered_map<std::string, ExtState>  m_extensions;
};
