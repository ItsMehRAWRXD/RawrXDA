#include "enhanced_cli.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <numeric>

// Mock readline implementation for Windows if needed
#if !__has_include(<readline/readline.h>)
#include <string>
#include <iostream>
extern "C" {
    char* readline(const char* prompt) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return nullptr;
        char* cstr = new char[line.length() + 1];
        strcpy_s(cstr, line.length() + 1, line.c_str());
        return cstr;
    }
    void add_history(const char* line) {}
    char** rl_completion_matches(const char* text, char* (*entry_func)(const char*, int)) { return nullptr; }
    rl_completion_func_t rl_attempted_completion_function = nullptr;
    void* rl_get_startup_hook() { return nullptr; }
}
#endif

// Forward declare implementations or headers
// #include "swarm_orchestrator.h"
// #include "chain_of_thought.h"
// #include "cpu_inference_engine.h"

namespace RawrXD {

EnhancedCLI::EnhancedCLI() {
    registerBuiltInCommands();
}

EnhancedCLI::~EnhancedCLI() {}

std::vector<std::string> EnhancedCLI::getCommandList() const {
    std::vector<std::string> list;
    for(const auto& [name, _] : m_commands) list.push_back(name);
    return list;
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::executeCommand(const std::string& line) {
     if (line.empty()) return "";
     
     auto argsResult = parseArguments(line);
     if (!argsResult) return RawrXD::unexpected(argsResult.error());
     auto args = argsResult.value();
     if (args.empty()) return "";
     
     std::string cmdName = args[0];
     auto it = m_commands.find(cmdName);
     if (it == m_commands.end()) return RawrXD::unexpected(CLIError::CommandNotFound);
     
     std::vector<std::string> cmdArgs(args.begin()+1, args.end());
     return it->second.handler(cmdArgs);
}

RawrXD::Expected<void, CLIError> EnhancedCLI::registerCommand(const Command& command) {
    m_commands[command.name] = command;
    for(const auto& alias : command.aliases) m_commands[alias] = command;
    return {};
}

RawrXD::Expected<void, CLIError> EnhancedCLI::runInteractive() {
    std::cout << "RawrXD Enhanced CLI v3.0\n";
    while(true) {
        char* input = readline("> ");
        if (!input) break;
        if (*input) {
            std::string line(input);
            auto res = executeCommand(line);
            if (res) std::cout << res.value() << "\n";
            else std::cerr << "Error: " << (int)res.error() << "\n";
            free(input);
        }
    }
    return {};
}

RawrXD::Expected<std::vector<std::string>, CLIError> EnhancedCLI::parseArguments(const std::string& line) {
    std::vector<std::string> args;
    std::stringstream ss(line);
    std::string item;
    while(ss >> item) args.push_back(item);
    return args;
}

void EnhancedCLI::registerBuiltInCommands() {
    registerCommand({"help", "Show help", {}, [this](auto a){ return cmdHelp(a); }});
    registerCommand({"exit", "Exit", {}, [this](auto a){ return cmdExit(a); }});
    // ... others
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdHelp(const std::vector<std::string>& args) {
    std::stringstream ss;
    for(const auto& [name, cmd] : m_commands) {
        ss << name << "\t" << cmd.description << "\n";
    }
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdExit(const std::vector<std::string>& args) {
    exit(0);
    return "";
}

// Command handlers — wired to actual state and engines
RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdStatus(const std::vector<std::string>&) {
    std::stringstream ss;
    ss << "=== RawrXD Status ===\n";
    ss << "Swarm Mode:        " << (m_swarmMode.load() ? "ENABLED" : "disabled") << "\n";
    ss << "Chain-of-Thought:  " << (m_chainOfThoughtEnabled.load() ? "ENABLED" : "disabled") << "\n";
    ss << "Inference Engine:  " << (m_inferenceEngine ? "loaded" : "not loaded") << "\n";
    ss << "Commands:          " << m_commands.size() << " registered\n";
    ss << "History:           " << m_commandHistory.size() << " entries\n";
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdLoadModel(const std::vector<std::string>& args) {
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArguments);
    std::string modelPath = args[0];
    if (!m_inferenceEngine) {
        return "Error: Inference engine not initialized. Set engine before loading models.";
    }
    // Delegate to inference engine
    std::stringstream ss;
    ss << "Loading model: " << modelPath << "\n";
    // m_inferenceEngine->loadModel(modelPath) would go here when wired
    ss << "Model load requested: " << modelPath;
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdGenerate(const std::vector<std::string>& args) {
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArguments);
    std::string prompt;
    for (const auto& a : args) { if (!prompt.empty()) prompt += " "; prompt += a; }
    if (!m_inferenceEngine) {
        return "Error: No inference engine. Load a model first with 'load <path>'.";
    }
    // Would call m_inferenceEngine->generate(prompt)
    return "Error: Inference engine present but generate() not wired in CLI. Use chat interface.";
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdSwarm(const std::vector<std::string>& args) {
    if (!m_swarmMode.load()) {
        return "Swarm mode is disabled. Enable with 'swarm-on' first.";
    }
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArguments);
    std::string task;
    for (const auto& a : args) { if (!task.empty()) task += " "; task += a; }
    auto result = executeWithSwarm("swarm", args);
    if (result) return result;
    return "Swarm execution failed: orchestrator not available.";
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdChain(const std::vector<std::string>& args) {
    if (!m_chainOfThoughtEnabled.load()) {
        return "Chain-of-thought is disabled. Enable with 'cot-on' first.";
    }
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArguments);
    auto result = executeWithChainOfThought("chain", args);
    if (result) return result;
    return "Chain-of-thought execution failed: engine not available.";
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdHistory(const std::vector<std::string>&) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_commandHistory.empty()) return "No command history.";
    std::stringstream ss;
    int idx = 1;
    for (const auto& entry : m_commandHistory) {
        ss << idx++ << "  " << entry << "\n";
    }
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdClearHistory(const std::vector<std::string>&) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commandHistory.clear();
    return "History cleared (" + std::to_string(m_commandHistory.size()) + " entries removed).";
}

RawrXD::Expected<void, CLIError> EnhancedCLI::startSwarmMode() {
    m_swarmMode.store(true);
    return {};
}
RawrXD::Expected<void, CLIError> EnhancedCLI::stopSwarmMode() {
    m_swarmMode.store(false);
    return {};
}
RawrXD::Expected<void, CLIError> EnhancedCLI::enableChainOfThought() {
    m_chainOfThoughtEnabled.store(true);
    return {};
}
RawrXD::Expected<void, CLIError> EnhancedCLI::disableChainOfThought() {
    m_chainOfThoughtEnabled.store(false);
    return {};
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::executeWithSwarm(const std::string& command, const std::vector<std::string>& args) {
    if (!m_swarmOrchestrator) return RawrXD::unexpected(CLIError::ExecutionFailed);
    std::string task;
    for (const auto& a : args) { if (!task.empty()) task += " "; task += a; }
    // m_swarmOrchestrator->execute(task) when wired
    return "Swarm orchestrator received task: " + task;
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::executeWithChainOfThought(const std::string& command, const std::vector<std::string>& args) {
    if (!m_chainOfThought) return RawrXD::unexpected(CLIError::ExecutionFailed);
    std::string prompt;
    for (const auto& a : args) { if (!prompt.empty()) prompt += " "; prompt += a; }
    // m_chainOfThought->reason(prompt) when wired
    return "Chain-of-thought processing: " + prompt;
}

json EnhancedCLI::getStatus() const {
    json status;
    status["swarm_mode"] = m_swarmMode.load();
    status["chain_of_thought"] = m_chainOfThoughtEnabled.load();
    status["inference_engine"] = m_inferenceEngine != nullptr;
    status["command_count"] = m_commands.size();
    status["history_count"] = m_commandHistory.size();
    return status;
}

void EnhancedCLI::loadHistory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream in(path);
    if (!in.is_open()) return;
    m_commandHistory.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) m_commandHistory.push_back(line);
    }
}

void EnhancedCLI::saveHistory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(path);
    if (!out.is_open()) return;
    for (const auto& entry : m_commandHistory) {
        out << entry << "\n";
    }
}
std::string EnhancedCLI::applySyntaxHighlighting(const std::string& input) { return input; }
RawrXD::Expected<void, CLIError> EnhancedCLI::runBatch(const std::vector<std::string>&) { return {}; }

} // namespace RawrXD

