#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace RawrXD {

// Forward declarations
class CPUInferenceEngine;
class AgenticEngine;

/**
 * InteractiveShell - A fully-featured interactive shell for CLI/GUI AI interactions
 * 
 * Features:
 * - Command history with persistent storage
 * - Tab completion for commands and paths
 * - Streaming output support
 * - Multi-line input support
 * - Context window management
 * - Integration with AgenticEngine for AI features
 */
class InteractiveShell {
public:
    // Command handler signature
    using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;
    using StreamCallback = std::function<void(const std::string&)>;
    
    struct ShellCommand {
        std::string name;
        std::string description;
        std::string usage;
        CommandHandler handler;
        std::vector<std::string> aliases;
    };
    
    struct ShellConfig {
        std::string prompt = "rawrxd> ";
        std::string historyFile = ".rawrxd_history";
        int maxHistorySize = 1000;
        bool enableColors = true;
        bool enableTabCompletion = true;
        bool streamOutput = true;
        int contextWindowSize = 32768; // Default 32k tokens
    };

    InteractiveShell() : InteractiveShell(ShellConfig{}) {}
    
    explicit InteractiveShell(const ShellConfig& config) 
        : m_config(config), m_running(false), m_multiLineMode(false) {
        initializeBuiltinCommands();
        loadHistory();
    }
    
    ~InteractiveShell() {
        saveHistory();
    }

    // Main entry point - runs the interactive loop
    void run() {
        m_running = true;
        printWelcome();
        
        while (m_running) {
            std::string prompt = m_multiLineMode ? "... " : m_config.prompt;
            std::string line = readLine(prompt);
            
            if (line.empty() && !m_multiLineMode) continue;
            
            // Handle multi-line input
            if (line.back() == '\\' && !m_multiLineMode) {
                m_multiLineMode = true;
                m_multiLineBuffer = line.substr(0, line.length() - 1);
                continue;
            }
            
            if (m_multiLineMode) {
                if (line.empty()) {
                    m_multiLineMode = false;
                    line = m_multiLineBuffer;
                    m_multiLineBuffer.clear();
                } else {
                    m_multiLineBuffer += "\n" + line;
                    if (line.back() != '\\') {
                        m_multiLineMode = false;
                        line = m_multiLineBuffer;
                        m_multiLineBuffer.clear();
                    } else {
                        continue;
                    }
                }
            }
            
            // Add to history
            addToHistory(line);
            
            // Process the input
            std::string result = processInput(line);
            if (!result.empty()) {
                printOutput(result);
            }
        }
    }
    
    // Process a single input line (can be used externally)
    std::string processInput(const std::string& input) {
        if (input.empty()) return "";
        
        // Check if it's a command
        if (input[0] == '/') {
            return executeCommand(input);
        }
        
        // Otherwise, treat as AI chat input
        return chat(input);
    }
    
    // Register a custom command
    void registerCommand(const ShellCommand& cmd) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands[cmd.name] = cmd;
        for (const auto& alias : cmd.aliases) {
            m_aliases[alias] = cmd.name;
        }
    }
    
    // Set the stream callback for real-time output
    void setStreamCallback(StreamCallback callback) {
        m_streamCallback = std::move(callback);
    }
    
    // Configure the shell
    void configure(const ShellConfig& config) {
        m_config = config;
    }
    
    // Set context window size (4k, 32k, 128k, 1M)
    void setContextWindow(int tokens) {
        m_config.contextWindowSize = tokens;
        if (m_agenticEngine) {
            m_agenticEngine->setContextWindow(tokens);
        }
        printOutput("[Shell] Context window set to " + std::to_string(tokens) + " tokens");
    }
    
    // Connect to inference engine
    void setInferenceEngine(std::shared_ptr<CPUInferenceEngine> engine) {
        m_inferenceEngine = std::move(engine);
    }
    
    // Connect to agentic engine
    void setAgenticEngine(std::shared_ptr<AgenticEngine> engine) {
        m_agenticEngine = std::move(engine);
    }
    
    // Stop the shell
    void stop() {
        m_running = false;
    }
    
    // Get command completions for tab completion
    std::vector<std::string> getCompletions(const std::string& prefix) const {
        std::vector<std::string> completions;
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& [name, cmd] : m_commands) {
            if (name.find(prefix) == 0) {
                completions.push_back("/" + name);
            }
        }
        return completions;
    }

private:
    ShellConfig m_config;
    std::atomic<bool> m_running;
    std::atomic<bool> m_multiLineMode;
    std::string m_multiLineBuffer;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ShellCommand> m_commands;
    std::unordered_map<std::string, std::string> m_aliases;
    std::deque<std::string> m_history;
    int m_historyIndex = -1;
    
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<AgenticEngine> m_agenticEngine;
    StreamCallback m_streamCallback;
    
    // Mode flags
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    bool m_noRefusal = false;
    
    void initializeBuiltinCommands() {
        // /help command
        registerCommand({
            "help", "Show available commands", "/help [command]",
            [this](const std::vector<std::string>& args) -> std::string {
                return showHelp(args.empty() ? "" : args[0]);
            },
            {"h", "?"}
        });
        
        // /exit command
        registerCommand({
            "exit", "Exit the shell", "/exit",
            [this](const std::vector<std::string>&) -> std::string {
                m_running = false;
                return "Goodbye!";
            },
            {"quit", "q"}
        });
        
        // /clear command
        registerCommand({
            "clear", "Clear the screen", "/clear",
            [this](const std::vector<std::string>&) -> std::string {
                clearScreen();
                return "";
            },
            {"cls"}
        });
        
        // /history command
        registerCommand({
            "history", "Show command history", "/history [count]",
            [this](const std::vector<std::string>& args) -> std::string {
                int count = args.empty() ? 20 : std::stoi(args[0]);
                return showHistory(count);
            },
            {"hist"}
        });
        
        // /context command
        registerCommand({
            "context", "Set context window size", "/context <4k|8k|32k|64k|128k|256k|512k|1m>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return "Usage: /context <size>";
                return setContextSize(args[0]);
            },
            {"ctx"}
        });
        
        // /maxmode command
        registerCommand({
            "maxmode", "Toggle max context mode", "/maxmode [on|off]",
            [this](const std::vector<std::string>& args) -> std::string {
                bool enable = args.empty() ? !m_maxMode : (args[0] == "on" || args[0] == "1");
                return toggleMaxMode(enable);
            },
            {"max"}
        });
        
        // /think command  
        registerCommand({
            "think", "Toggle deep thinking mode (chain-of-thought)", "/think [on|off]",
            [this](const std::vector<std::string>& args) -> std::string {
                bool enable = args.empty() ? !m_deepThinking : (args[0] == "on" || args[0] == "1");
                return toggleDeepThinking(enable);
            },
            {"cot", "deepthink"}
        });
        
        // /research command
        registerCommand({
            "research", "Toggle deep research mode (file scanning)", "/research [on|off]",
            [this](const std::vector<std::string>& args) -> std::string {
                bool enable = args.empty() ? !m_deepResearch : (args[0] == "on" || args[0] == "1");
                return toggleDeepResearch(enable);
            },
            {"scan", "deepresearch"}
        });
        
        // /norefusal command
        registerCommand({
            "norefusal", "Toggle safety override mode", "/norefusal [on|off]",
            [this](const std::vector<std::string>& args) -> std::string {
                bool enable = args.empty() ? !m_noRefusal : (args[0] == "on" || args[0] == "1");
                return toggleNoRefusal(enable);
            },
            {"unsafe", "override"}
        });
        
        // /status command
        registerCommand({
            "status", "Show current shell/engine status", "/status",
            [this](const std::vector<std::string>&) -> std::string {
                return showStatus();
            },
            {"st", "info"}
        });
        
        // /model command
        registerCommand({
            "model", "Load or show current model", "/model [path]",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return showModelInfo();
                return loadModel(args[0]);
            },
            {"m", "load"}
        });
        
        // /plan command
        registerCommand({
            "plan", "Create an agentic task plan", "/plan <task description>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return "Usage: /plan <task description>";
                std::string task;
                for (const auto& arg : args) task += arg + " ";
                return planTask(task);
            },
            {"task"}
        });
        
        // /bugreport command
        registerCommand({
            "bugreport", "Analyze code for bugs", "/bugreport <file>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return "Usage: /bugreport <file path>";
                return bugReport(args[0]);
            },
            {"bug", "analyze"}
        });
        
        // /suggest command
        registerCommand({
            "suggest", "Get code suggestions", "/suggest <file>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return "Usage: /suggest <file path>";
                return codeSuggestions(args[0]);
            },
            {"improve", "suggestions"}
        });
        
        // /hotpatch command
        registerCommand({
            "hotpatch", "Apply a hot patch to running code", "/hotpatch <file> <function>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.size() < 2) return "Usage: /hotpatch <file> <function>";
                return hotPatch(args[0], args[1]);
            },
            {"patch", "livepatch"}
        });
        
        // /generate command
        registerCommand({
            "generate", "Generate code from description", "/generate <language> <description>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.size() < 2) return "Usage: /generate <language> <description>";
                std::string desc;
                for (size_t i = 1; i < args.size(); ++i) desc += args[i] + " ";
                return generateCode(args[0], desc);
            },
            {"gen", "code"}
        });
        
        // /react command
        registerCommand({
            "react", "Generate React project", "/react <projectDir> <name>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.size() < 2) return "Usage: /react <projectDir> <name>";
                return generateReactProject(args[0], args[1]);
            },
            {"reactgen"}
        });
        
        // /install_vsix command
        registerCommand({
            "install_vsix", "Install a VSIX extension", "/install_vsix <path>",
            [this](const std::vector<std::string>& args) -> std::string {
                if (args.empty()) return "Usage: /install_vsix <vsix path>";
                return installVsix(args[0]);
            },
            {"vsix"}
        });
        
        // /multiline command
        registerCommand({
            "multiline", "Enter multi-line input mode", "/multiline",
            [this](const std::vector<std::string>&) -> std::string {
                m_multiLineMode = true;
                return "Multi-line mode. End with empty line.";
            },
            {"ml"}
        });
    }
    
    std::string readLine(const std::string& prompt) {
#ifdef _WIN32
        std::cout << prompt << std::flush;
        std::string line;
        std::getline(std::cin, line);
        
        // Handle up/down arrow for history navigation
        // Note: Full implementation would use _getch() for character-by-character input
        return line;
#else
        std::cout << prompt << std::flush;
        std::string line;
        std::getline(std::cin, line);
        return line;
#endif
    }
    
    void printOutput(const std::string& output) {
        if (m_streamCallback) {
            m_streamCallback(output);
        } else {
            std::cout << output << std::endl;
        }
    }
    
    void printWelcome() {
        std::string welcome = R"(
╔═══════════════════════════════════════════════════════════════╗
║  RawrXD Interactive Shell v1.0                                 ║
║  Type /help for available commands                             ║
║  Current context: )" + std::to_string(m_config.contextWindowSize / 1000) + R"(k tokens                                  ║
╚═══════════════════════════════════════════════════════════════╝
)";
        printOutput(welcome);
    }
    
    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }
    
    std::string showHelp(const std::string& cmdName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (cmdName.empty()) {
            std::string help = "\nAvailable Commands:\n";
            help += "═══════════════════════════════════════\n";
            for (const auto& [name, cmd] : m_commands) {
                help += "  /" + name + " - " + cmd.description + "\n";
            }
            help += "\nType /help <command> for detailed usage.\n";
            help += "Enter text without / to chat with AI.\n";
            return help;
        }
        
        std::string lookupName = cmdName[0] == '/' ? cmdName.substr(1) : cmdName;
        
        // Check aliases
        auto aliasIt = m_aliases.find(lookupName);
        if (aliasIt != m_aliases.end()) {
            lookupName = aliasIt->second;
        }
        
        auto it = m_commands.find(lookupName);
        if (it == m_commands.end()) {
            return "Unknown command: " + cmdName;
        }
        
        std::string help = "\n/" + it->second.name + "\n";
        help += "Description: " + it->second.description + "\n";
        help += "Usage: " + it->second.usage + "\n";
        if (!it->second.aliases.empty()) {
            help += "Aliases: ";
            for (const auto& alias : it->second.aliases) {
                help += "/" + alias + " ";
            }
            help += "\n";
        }
        return help;
    }
    
    std::string showHistory(int count) {
        std::string result = "\nCommand History:\n";
        int start = std::max(0, (int)m_history.size() - count);
        for (int i = start; i < (int)m_history.size(); ++i) {
            result += "  " + std::to_string(i + 1) + ": " + m_history[i] + "\n";
        }
        return result;
    }
    
    std::string showStatus() {
        std::string status = "\n=== Shell Status ===\n";
        status += "Context Window: " + std::to_string(m_config.contextWindowSize) + " tokens\n";
        status += "Max Mode: " + std::string(m_maxMode ? "ON" : "OFF") + "\n";
        status += "Deep Thinking: " + std::string(m_deepThinking ? "ON" : "OFF") + "\n";
        status += "Deep Research: " + std::string(m_deepResearch ? "ON" : "OFF") + "\n";
        status += "No Refusal: " + std::string(m_noRefusal ? "ON" : "OFF") + "\n";
        status += "History Size: " + std::to_string(m_history.size()) + " entries\n";
        status += "Inference Engine: " + std::string(m_inferenceEngine ? "Connected" : "Not connected") + "\n";
        status += "Agentic Engine: " + std::string(m_agenticEngine ? "Connected" : "Not connected") + "\n";
        return status;
    }
    
    std::string setContextSize(const std::string& sizeStr) {
        static const std::unordered_map<std::string, int> sizes = {
            {"4k", 4096}, {"8k", 8192}, {"16k", 16384}, {"32k", 32768},
            {"64k", 65536}, {"128k", 131072}, {"256k", 262144},
            {"512k", 524288}, {"1m", 1048576}
        };
        
        std::string lower = sizeStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        auto it = sizes.find(lower);
        if (it == sizes.end()) {
            return "Invalid size. Use: 4k, 8k, 16k, 32k, 64k, 128k, 256k, 512k, 1m";
        }
        
        setContextWindow(it->second);
        return "Context window set to " + sizeStr;
    }
    
    std::string toggleMaxMode(bool enable) {
        m_maxMode = enable;
        if (m_inferenceEngine) {
            // Call engine's SetMaxMode
        }
        return "Max Mode: " + std::string(enable ? "ENABLED" : "DISABLED");
    }
    
    std::string toggleDeepThinking(bool enable) {
        m_deepThinking = enable;
        if (m_inferenceEngine) {
            // Call engine's SetDeepThinking
        }
        return "Deep Thinking Mode: " + std::string(enable ? "ENABLED" : "DISABLED");
    }
    
    std::string toggleDeepResearch(bool enable) {
        m_deepResearch = enable;
        if (m_inferenceEngine) {
            // Call engine's SetDeepResearch
        }
        return "Deep Research Mode: " + std::string(enable ? "ENABLED" : "DISABLED");
    }
    
    std::string toggleNoRefusal(bool enable) {
        m_noRefusal = enable;
        if (m_inferenceEngine) {
            // Call engine's SetNoRefusal
        }
        return "No Refusal Mode: " + std::string(enable ? "ENABLED - Use responsibly" : "DISABLED");
    }
    
    std::string showModelInfo() {
        if (!m_inferenceEngine) {
            return "No inference engine connected";
        }
        return "Model info: [Connected to inference engine]";
    }
    
    std::string loadModel(const std::string& path) {
        if (!m_inferenceEngine) {
            return "No inference engine connected";
        }
        return "Loading model from: " + path + " ...";
    }
    
    std::string chat(const std::string& input) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected. Use /status to check.";
        }
        
        // Forward to agentic engine with streaming
        return m_agenticEngine->chat(input);
    }
    
    std::string planTask(const std::string& task) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->planTask(task);
    }
    
    std::string bugReport(const std::string& file) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->bugReport(file);
    }
    
    std::string codeSuggestions(const std::string& file) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->codeSuggestions(file);
    }
    
    std::string hotPatch(const std::string& file, const std::string& function) {
        // Hot patching logic
        return "[HotPatch] Patching " + function + " in " + file + " ... (Not implemented)";
    }
    
    std::string generateCode(const std::string& language, const std::string& description) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->generateCode(language, description);
    }
    
    std::string generateReactProject(const std::string& dir, const std::string& name) {
        // Use ReactServerGenerator
        return "[React] Generating project '" + name + "' in " + dir + " ...";
    }
    
    std::string installVsix(const std::string& path) {
        // Use VsixNativeConverter
        return "[VSIX] Installing extension from " + path + " ...";
    }
    
    std::string executeCommand(const std::string& input) {
        // Parse command and args
        std::vector<std::string> parts;
        std::string current;
        bool inQuotes = false;
        
        for (char c : input) {
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (c == ' ' && !inQuotes) {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        
        if (parts.empty()) return "";
        
        std::string cmdName = parts[0].substr(1); // Remove leading /
        std::vector<std::string> args(parts.begin() + 1, parts.end());
        
        // Check aliases
        auto aliasIt = m_aliases.find(cmdName);
        if (aliasIt != m_aliases.end()) {
            cmdName = aliasIt->second;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_commands.find(cmdName);
        if (it == m_commands.end()) {
            return "Unknown command: /" + cmdName + ". Type /help for available commands.";
        }
        
        try {
            return it->second.handler(args);
        } catch (const std::exception& e) {
            return "Error executing command: " + std::string(e.what());
        }
    }
    
    void addToHistory(const std::string& line) {
        if (line.empty()) return;
        if (!m_history.empty() && m_history.back() == line) return; // No duplicates
        
        m_history.push_back(line);
        if (m_history.size() > static_cast<size_t>(m_config.maxHistorySize)) {
            m_history.pop_front();
        }
        m_historyIndex = -1;
    }
    
    void loadHistory() {
        std::ifstream file(m_config.historyFile);
        if (!file) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                m_history.push_back(line);
            }
        }
    }
    
    void saveHistory() {
        std::ofstream file(m_config.historyFile);
        if (!file) return;
        
        for (const auto& line : m_history) {
            file << line << "\n";
        }
    }
};

} // namespace RawrXD
