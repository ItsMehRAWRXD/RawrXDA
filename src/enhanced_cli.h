#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include "ide_orchestrator.h"
#include "utils/Expected.h"

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
    std::function<RawrXD::Expected<std::string, CLIError>(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    )> handler;
    std::string usage;
    std::string helpText;
};

class EnhancedCLI {
public:
    EnhancedCLI();
    ~EnhancedCLI() = default;

    RawrXD::Expected<std::string, CLIError> executeCommand(
        const std::string& input,
        IDEOrchestrator* ide = nullptr
    );

    RawrXD::Expected<void, CLIError> registerCommand(const CLICommand& cmd);

    // Interactive mode
    RawrXD::Expected<void, CLIError> runInteractive(IDEOrchestrator* ide = nullptr);
    RawrXD::Expected<void, CLIError> runBatch(
        const std::string& scriptPath,
        IDEOrchestrator* ide = nullptr
    );

    // Automation
    RawrXD::Expected<void, CLIError> executeScript(
        const std::string& content,
        IDEOrchestrator* ide = nullptr
    );

    // Status and History
    std::vector<std::string> getHistory() const { return m_history; }
    void clearHistory() { m_history.clear(); }
    
    // Config management
    void setPrompt(const std::string& prompt) { m_prompt = prompt; }
    RawrXD::Expected<void, CLIError> loadHistory(const std::string& path);
    RawrXD::Expected<void, CLIError> saveHistory(const std::string& path);

private:
    std::map<std::string, CLICommand> m_commands;
    std::vector<std::string> m_history;
    std::string m_prompt;
    bool m_running;

    // Helper methods
    void registerBuiltInCommands();
    RawrXD::Expected<CLICommand*, CLIError> findCommand(const std::string& name);
    RawrXD::Expected<std::vector<std::string>, CLIError> parseArguments(
        const std::string& input
    );
    void printHelp();
    void printWelcome();

    // Command implementations
    RawrXD::Expected<std::string, CLIError> cmdHelp(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdStatus(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdGenerate(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdSwarm(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdChain(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdTokenize(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdLoadModel(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdDebug(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdOptimize(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdTest(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdDocs(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdLSP(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdFile(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdEdit(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );

    RawrXD::Expected<std::string, CLIError> cmdExit(
        const std::vector<std::string>& args,
        IDEOrchestrator* ide
    );
};

} // namespace RawrXD
