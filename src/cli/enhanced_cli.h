#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include "utils/Expected.h"

#include <unordered_map>
#include <nlohmann/json.hpp>

// Assuming readline is available or mocked
// If on windows with MSVC, we might need a port or mock
#if __has_include(<readline/readline.h>)
#include <readline/readline.h>
#include <readline/history.h>
#else
// Mock declarations for windows if readline not present in include path
extern "C" {
    char* readline(const char* prompt);
    void add_history(const char* line);
    char** rl_completion_matches(const char* text, char* (*entry_func)(const char*, int));
    typedef char** (*rl_completion_func_t)(const char*, int, int);
    extern rl_completion_func_t rl_attempted_completion_function;
    void* rl_get_startup_hook(); // Approximating hook fetch
}
#endif

using json = nlohmann::json;

namespace RawrXD {

class SwarmOrchestrator; 
class ChainOfThought;
class CPUInferenceEngine;

enum class CLIError {
    Success = 0,
    CommandNotFound,
    InvalidArguments,
    ExecutionFailed,
    HistoryLoadFailed,
    HistorySaveFailed
};

struct Command {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    std::function<RawrXD::Expected<std::string, CLIError>(
        const std::vector<std::string>& args
    )> handler;
    std::string usage;
    std::string helpText;
};

class EnhancedCLI {
public:
    EnhancedCLI();
    ~EnhancedCLI();
    
    // Real command processing
    RawrXD::Expected<std::string, CLIError> executeCommand(const std::string& line);
    RawrXD::Expected<void, CLIError> registerCommand(const Command& command);
    
    // Real interactive mode
    RawrXD::Expected<void, CLIError> runInteractive();
    RawrXD::Expected<void, CLIError> runBatch(const std::vector<std::string>& commands);
    
    // Status
    json getStatus() const;
    std::vector<std::string> getCommandList() const;
    
    // Swarm integration
    RawrXD::Expected<void, CLIError> startSwarmMode();
    RawrXD::Expected<void, CLIError> stopSwarmMode();
    
    // Chain-of-thought integration
    RawrXD::Expected<void, CLIError> enableChainOfThought();
    RawrXD::Expected<void, CLIError> disableChainOfThought();
    
private:
    std::unordered_map<std::string, Command> m_commands;
    std::vector<std::string> m_commandHistory;
    std::atomic<bool> m_swarmMode{false};
    std::atomic<bool> m_chainOfThoughtEnabled{false};
    std::unique_ptr<SwarmOrchestrator> m_swarmOrchestrator;
    std::unique_ptr<ChainOfThought> m_chainOfThought;
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    mutable std::mutex m_mutex;
    
    RawrXD::Expected<Command*, CLIError> findCommand(const std::string& name);
    RawrXD::Expected<std::vector<std::string>, CLIError> parseArguments(
        const std::string& line
    );
    
    RawrXD::Expected<std::string, CLIError> executeWithSwarm(
        const std::string& command,
        const std::vector<std::string>& args
    );
    
    RawrXD::Expected<std::string, CLIError> executeWithChainOfThought(
        const std::string& command,
        const std::vector<std::string>& args
    );
    
    void registerBuiltInCommands();
    void loadHistory(const std::string& path);
    void saveHistory(const std::string& path);
    std::string applySyntaxHighlighting(const std::string& input);

    // Command handlers
    RawrXD::Expected<std::string, CLIError> cmdHelp(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdStatus(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdLoadModel(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdGenerate(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdSwarm(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdChain(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdHistory(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdClearHistory(const std::vector<std::string>& args);
    RawrXD::Expected<std::string, CLIError> cmdExit(const std::vector<std::string>& args);
};

} // namespace RawrXD

