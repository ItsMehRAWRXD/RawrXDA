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

// Stubs for other commands mentioned in header
RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdStatus(const std::vector<std::string>&) {
    std::stringstream ss;
    ss << "RawrXD CLI Status\n";
    ss << "  Swarm Mode: " << (m_swarmMode.load() ? "enabled" : "disabled") << "\n";
    ss << "  Chain-of-Thought: " << (m_chainOfThoughtEnabled.load() ? "enabled" : "disabled") << "\n";
    ss << "  Commands registered: " << m_commands.size() << "\n";
    ss << "  History entries: " << m_commandHistory.size() << "\n";
    ss << "  Inference Engine: " << (m_inferenceEngine ? "loaded" : "not loaded") << "\n";
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdLoadModel(const std::vector<std::string>& args) {
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArgument);
    if (!m_inferenceEngine) return RawrXD::unexpected(CLIError::NotInitialized);
    bool ok = m_inferenceEngine->loadModel(args[0]);
    return ok ? ("Model loaded: " + args[0]) : ("Failed to load: " + args[0]);
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdGenerate(const std::vector<std::string>& args) {
    if (args.empty()) return RawrXD::unexpected(CLIError::InvalidArgument);
    if (!m_inferenceEngine) return RawrXD::unexpected(CLIError::NotInitialized);
    std::string prompt;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) prompt += " ";
        prompt += args[i];
    }
    return m_inferenceEngine->generate(prompt);
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdSwarm(const std::vector<std::string>& args) {
    if (!m_swarmMode.load()) {
        auto r = startSwarmMode();
        if (!r) return RawrXD::unexpected(r.error());
    }
    if (args.empty()) return "Swarm mode active. Pass a prompt to execute.";
    std::string prompt;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) prompt += " ";
        prompt += args[i];
    }
    return executeWithSwarm(prompt, args);
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdChain(const std::vector<std::string>& args) {
    if (!m_chainOfThoughtEnabled.load()) {
        auto r = enableChainOfThought();
        if (!r) return RawrXD::unexpected(r.error());
    }
    if (args.empty()) return "Chain-of-thought enabled. Pass a prompt.";
    std::string prompt;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) prompt += " ";
        prompt += args[i];
    }
    return executeWithChainOfThought(prompt, args);
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdHistory(const std::vector<std::string>&) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_commandHistory.empty()) return "No history.";
    std::stringstream ss;
    int idx = 1;
    for (const auto& entry : m_commandHistory) {
        ss << idx++ << ". " << entry << "\n";
    }
    return ss.str();
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::cmdClearHistory(const std::vector<std::string>&) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commandHistory.clear();
    return "History cleared";
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

RawrXD::Expected<std::string, CLIError> EnhancedCLI::executeWithSwarm(const std::string& prompt, const std::vector<std::string>&) {
    if (!m_swarmOrchestrator) return RawrXD::unexpected(CLIError::NotInitialized);
    // Delegate to swarm orchestrator — it manages multi-agent reasoning
    return m_swarmOrchestrator->execute(prompt);
}

RawrXD::Expected<std::string, CLIError> EnhancedCLI::executeWithChainOfThought(const std::string& prompt, const std::vector<std::string>&) {
    if (!m_chainOfThought) return RawrXD::unexpected(CLIError::NotInitialized);
    return m_chainOfThought->reason(prompt);
}

json EnhancedCLI::getStatus() const {
    return json{
        {"swarm_mode", m_swarmMode.load()},
        {"chain_of_thought", m_chainOfThoughtEnabled.load()},
        {"commands_registered", m_commands.size()},
        {"history_size", m_commandHistory.size()},
        {"inference_engine", m_inferenceEngine != nullptr}
    };
}

void EnhancedCLI::loadHistory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream file(path);
    if (!file.is_open()) return;
    m_commandHistory.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) m_commandHistory.push_back(line);
    }
}

void EnhancedCLI::saveHistory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(path);
    if (!file.is_open()) return;
    for (const auto& entry : m_commandHistory) {
        file << entry << "\n";
    }
}

std::string EnhancedCLI::applySyntaxHighlighting(const std::string& input) {
    // ANSI color codes for terminal syntax highlighting
    std::string result = input;
    // Highlight keywords in cyan
    static const char* keywords[] = {"help", "exit", "status", "load", "generate", "swarm", "chain"};
    for (const char* kw : keywords) {
        size_t pos = 0;
        std::string kwStr(kw);
        while ((pos = result.find(kwStr, pos)) != std::string::npos) {
            // Only highlight at word boundaries
            bool atStart = (pos == 0 || result[pos-1] == ' ');
            bool atEnd = (pos + kwStr.size() >= result.size() || result[pos+kwStr.size()] == ' ');
            if (atStart && atEnd) {
                result.insert(pos + kwStr.size(), "\033[0m");
                result.insert(pos, "\033[36m");
                pos += kwStr.size() + 9; // skip past the color codes
            } else {
                pos += kwStr.size();
            }
        }
    }
    return result;
}

RawrXD::Expected<void, CLIError> EnhancedCLI::runBatch(const std::vector<std::string>& commands) {
    for (const auto& cmd : commands) {
        auto result = executeCommand(cmd);
        if (!result) return RawrXD::unexpected(result.error());
        std::cout << result.value() << "\n";
    }
    return {};
}

} // namespace RawrXD

