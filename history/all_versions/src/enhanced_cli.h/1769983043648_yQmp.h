#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <expected>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include "ide_orchestrator.h"

namespace RawrXD {

enum class CLIError {
    Success = 0,
    CommandNotFound,
    InvalidArguments,
    ExecutionFailed,
    FileNotFound,
    PermissionDenied,
    Timeout,
    CancellationRequested
};

struct CLICommand {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    std::function<std::expected<std::string, CLIError>(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    )> handler;
    std::string usage;
    std::string help;
    bool requiresIDE = false;
};

class EnhancedCLI {
public:
    EnhancedCLI();
    ~EnhancedCLI();
    
    // Real command processing
    std::expected<std::string, CLIError> executeCommand(
        const std::string& line,
        IDEOrchestrator* ide = nullptr
    );
    
    std::expected<void, CLIError> registerCommand(const CLICommand& cmd);
    
    // Real interactive mode
    std::expected<void, CLIError> runInteractive(IDEOrchestrator* ide = nullptr);
    std::expected<void, CLIError> runBatch(
        const std::vector<std::string>& commands,
        IDEOrchestrator* ide = nullptr
    );
    
    // Real scripting
    std::expected<void, CLIError> executeScript(
        const std::string& scriptPath,
        IDEOrchestrator* ide = nullptr
    );
    
    // Real completion
    void setupCompletion();
    std::vector<std::string> getCompletions(const std::string& input) const;
    
    // History management
    std::expected<void, CLIError> loadHistory(const std::string& path);
    std::expected<void, CLIError> saveHistory(const std::string& path);
    void clearHistory();
    
    // Status
    json getStatus() const;
    
private:
    std::unordered_map<std::string, CLICommand> m_commands;
    std::vector<std::string> m_history;
    std::vector<std::string> m_completions;
    mutable std::mutex m_mutex;
    
    // Real command processing
    std::expected<CLICommand*, CLIError> findCommand(const std::string& name);
    std::expected<std::vector<std::string>, CLIError> parseArguments(
        const std::string& line
    );
    
    // Built-in commands
    void registerBuiltInCommands();
    
    // Command implementations
    std::expected<std::string, CLIError> cmdHelp(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdStatus(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdGenerate(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdSwarm(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdChain(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdTokenize(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdLoadModel(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdDebug(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdOptimize(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdTest(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdDocs(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdLSP(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdFile(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdEdit(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
    
    std::expected<std::string, CLIError> cmdExit(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
};

} // namespace RawrXD
