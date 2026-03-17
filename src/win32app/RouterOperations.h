// ============================================================================
// RouterOperations.h — Command Routing with Real Win32 Integration
// ============================================================================
// FIX #6: Real IDE Operations
// Purpose:
//   - Route IDE commands to appropriate subsystems (editor, build, debug)
//   - Handle command execution with proper error handling
//   - Maintain command history and enablement state
//   - Integrate with native Win32 APIs for system operations
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Win32App {

// ============================================================================
// RouterOperations — Command dispatch and routing system
// ============================================================================
class RouterOperations {
public:
    static RouterOperations& Instance();

    // Command execution
    struct CommandContext {
        std::string id;
        std::string title;
        std::string description;
        std::string category;
        bool enabled = true;
        std::string keybinding;
    };

    struct CommandResult {
        bool success;
        std::string message;
        std::string errorCode;
        int exitCode = 0;
    };

    // Core command operations
    CommandResult Execute(const std::string& commandId);
    CommandResult ExecuteWithArgs(const std::string& commandId, const std::string& args);
    
    // Command registration
    using CommandHandler = std::function<CommandResult()>;
    using CommandHandlerWithArgs = std::function<CommandResult(const std::string&)>;
    
    bool RegisterCommand(const CommandContext& context, CommandHandler handler);
    bool RegisterCommandWithArgs(const CommandContext& context, CommandHandlerWithArgs handler);
    bool UnregisterCommand(const std::string& commandId);

    // Command queries
    bool IsCommandEnabled(const std::string& commandId) const;
    bool CommandExists(const std::string& commandId) const;
    std::vector<CommandContext> GetAllCommands() const;
    CommandContext GetCommandContext(const std::string& commandId) const;

    // Enable/disable commands
    void SetCommandEnabled(const std::string& commandId, bool enabled);

    // History
    struct HistoryEntry {
        std::string commandId;
        std::string title;
        int64_t timestamp;
        CommandResult result;
    };
    std::vector<HistoryEntry> GetCommandHistory(int maxEntries = 100);
    void ClearHistory();

    // Native Win32 integration
    bool ExecuteShellCommand(const std::string& command);
    bool LaunchProcess(const std::string& programPath, const std::string& args);
    std::string GetClipboardText();
    bool SetClipboardText(const std::string& text);

    // File system operations
    bool OpenFileDialog(std::string& outPath);
    bool SaveFileDialog(std::string& outPath);
    bool SelectFolderDialog(std::string& outPath);

private:
    RouterOperations();
    ~RouterOperations();

    struct CommandEntry {
        CommandContext context;
        CommandHandler handler;
        CommandHandlerWithArgs handlerWithArgs;
        bool hasArgs = false;
    };

    std::unordered_map<std::string, CommandEntry> m_commands;
    std::vector<HistoryEntry> m_history;
    mutable std::mutex m_commandsMutex;
    mutable std::mutex m_historyMutex;
    
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    void AddToHistory(const std::string& commandId, const std::string& title, const CommandResult& result);
};

} // namespace Win32App
} // namespace RawrXD
