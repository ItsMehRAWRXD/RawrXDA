// ============================================================================
// command_registry.h — RawrXD Unified Command Registry
// ============================================================================
// Central command bus accessible from:
//   - Win32 menu bar (WM_COMMAND)
//   - Command palette (Ctrl+Shift+P)
//   - Extension API (vscode.commands.executeCommand)
//   - Agentic / REST API (POST /api/commands/execute)
//   - IPC pipe (JSON-RPC)
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <optional>

// ---- Command access modes (bitmask) ----------------------------------------
enum CommandAccess : unsigned int {
    CMD_ACCESS_MENU      = 1 << 0,   // Visible in menu bar
    CMD_ACCESS_PALETTE   = 1 << 1,   // Visible in command palette
    CMD_ACCESS_API       = 1 << 2,   // Callable from REST/IPC API
    CMD_ACCESS_EXTENSION = 1 << 3,   // Callable from extension host
    CMD_ACCESS_AGENTIC   = 1 << 4,   // Callable by agentic/autonomous code
    CMD_ACCESS_ALL       = 0x1F
};

// ---- Command argument/result envelope -------------------------------------
struct CommandArgs {
    std::string jsonPayload;     // JSON string with named arguments
    std::string sourceExtId;     // Extension ID that triggered (empty = native)
    bool        silent = false;  // Suppress UI feedback
};

struct CommandResult {
    bool        success  = false;
    std::string jsonResult;      // JSON-encoded return value (may be empty)
    std::string errorMessage;
};

// ---- Command descriptor ---------------------------------------------------
struct CommandDescriptor {
    std::string  id;            // "file.newFile"
    std::string  displayName;   // "New File"
    std::string  category;      // "File", "Edit", "View", "AI", "Git", …
    std::string  keybinding;    // "Ctrl+N" (informational only)
    std::string  iconId;        // Optional icon identifier
    unsigned int accessModes = CMD_ACCESS_ALL;
    bool         enabled = true;

    std::function<CommandResult(const CommandArgs&)> handler;
};

// ---- Central registry -------------------------------------------------------
class CommandRegistry {
public:
    // Singleton access
    static CommandRegistry& instance();

    // Register a command. Returns false if id already registered.
    bool registerCommand(CommandDescriptor desc);

    // Override handler for an existing command (e.g., extension override).
    // Saves previous handler as fallback.
    bool overrideCommand(const std::string& id,
                         std::function<CommandResult(const CommandArgs&)> handler,
                         const std::string& ownerExtId = "");

    // Remove extension override and restore previous handler
    void releaseOverride(const std::string& id, const std::string& ownerExtId);

    // Execute by string ID
    CommandResult execute(const std::string& id, const CommandArgs& args = {});

    // Execute by numeric Win32 menu ID (WM_COMMAND dispatch)
    CommandResult executeByMenuId(int menuId, const CommandArgs& args = {});

    // Query
    bool                            hasCommand(const std::string& id) const;
    std::optional<CommandDescriptor> getDescriptor(const std::string& id) const;

    // Enumerate commands visible in a given access mode
    std::vector<CommandDescriptor> enumerate(unsigned int accessFilter = CMD_ACCESS_ALL) const;

    // Enable / disable a command
    void setEnabled(const std::string& id, bool enabled);

    // Map numeric menu ID ↔ string command ID
    void registerMenuMapping(int menuId, const std::string& commandId);
    std::string commandIdForMenuId(int menuId) const;

    // Register built-in RawrXD commands (called once at startup)
    static void registerBuiltins();

private:
    CommandRegistry() = default;
    mutable std::mutex m_mutex;

    std::unordered_map<std::string, CommandDescriptor>          m_commands;
    std::unordered_map<std::string, std::vector<std::pair<std::string,
        std::function<CommandResult(const CommandArgs&)>>>>     m_overrideStack; // id → [(extId,handler)]
    std::unordered_map<int, std::string>                        m_menuMap;       // menuId → commandId
};

// ---- Convenience macro for quick registration ------------------------------
#define RAWRXD_REGISTER_CMD(id_, name_, cat_, key_, access_, fn_) \
    CommandRegistry::instance().registerCommand({ \
        (id_), (name_), (cat_), (key_), "", (access_), true, (fn_) })
