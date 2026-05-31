// ============================================================================
// SOVEREIGN CLI IDE v3.0.0 - Integrated Chat & CLI Console
// ============================================================================
// Standalone CLI that can also integrate as a tab in GUI IDE
// Features: Chat pane + CLI commands + Agent integration
// Build: g++ -O3 -std=c++17 -o sovereign_cli sovereign_cli_ide.cpp -lm -lpthread
// ============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// ANSI COLOR CODES (for terminal styling like Cursor)
// ============================================================================
namespace Color {
    const std::string RESET = "\033[0m";
    const std::string BLACK = "\033[30m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
    const std::string ITALIC = "\033[3m";
    const std::string UNDERLINE = "\033[4m";
    const std::string BG_BLACK = "\033[40m";
    const std::string BG_RED = "\033[41m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_YELLOW = "\033[43m";
    const std::string BG_BLUE = "\033[44m";
    const std::string BG_MAGENTA = "\033[45m";
    const std::string BG_CYAN = "\033[46m";
    const std::string BG_WHITE = "\033[47m";
}

// ============================================================================
// CHAT MESSAGE STRUCTURE (Cursor-style)
// ============================================================================
struct ChatMessage {
    std::string id;
    std::string role;           // "user", "assistant", "system", "tool"
    std::string content;
    std::string timestamp;
    std::vector<std::string> tool_calls;
    bool isStreaming = false;
    bool isComplete = true;
    std::string model;          // Which model generated this
    int thinkingLevel = 0;      // 0-5 thinking effort
    
    ChatMessage() = default;
    ChatMessage(const std::string& r, const std::string& c) 
        : role(r), content(c) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        timestamp = ss.str();
        
        // Generate unique ID
        static int counter = 0;
        id = "msg_" + std::to_string(++counter);
    }
};

// ============================================================================
// CHAT PANEL (Cursor-style interface)
// ============================================================================
class ChatPanel {
private:
    std::vector<ChatMessage> messages;
    std::mutex msgMutex;
    std::string currentModel = "local";
    int thinkingLevel = 3;
    bool showTimestamps = true;
    bool showThinking = true;
    size_t maxHistory = 100;
    
public:
    ChatPanel() {}
    
    void addMessage(const std::string& role, const std::string& content) {
        std::lock_guard<std::mutex> lock(msgMutex);
        messages.emplace_back(role, content);
        messages.back().model = currentModel;
        messages.back().thinkingLevel = thinkingLevel;
        
        // Trim history if needed
        if (messages.size() > maxHistory) {
            messages.erase(messages.begin());
        }
    }
    
    void addStreamingChunk(const std::string& chunk) {
        std::lock_guard<std::mutex> lock(msgMutex);
        if (!messages.empty() && messages.back().role == "assistant" && !messages.back().isComplete) {
            messages.back().content += chunk;
        } else {
            ChatMessage msg("assistant", chunk);
            msg.isStreaming = true;
            msg.isComplete = false;
            messages.push_back(msg);
        }
    }
    
    void completeStreaming() {
        std::lock_guard<std::mutex> lock(msgMutex);
        if (!messages.empty() && messages.back().role == "assistant") {
            messages.back().isComplete = true;
            messages.back().isStreaming = false;
        }
    }
    
    void render() {
        std::lock_guard<std::mutex> lock(msgMutex);
        
        std::cout << "\n" << Color::BG_BLUE << Color::BOLD << " CHAT " << Color::RESET << "\n";
        std::cout << std::string(80, '-') << "\n";
        
        for (const auto& msg : messages) {
            renderMessage(msg);
        }
        
        std::cout << std::string(80, '-') << "\n";
    }
    
    void renderMessage(const ChatMessage& msg) {
        if (msg.role == "user") {
            std::cout << Color::GREEN << Color::BOLD << "You" << Color::RESET;
            if (showTimestamps) {
                std::cout << Color::DIM << " [" << msg.timestamp << "]" << Color::RESET;
            }
            std::cout << "\n";
            std::cout << msg.content << "\n\n";
        } else if (msg.role == "assistant") {
            std::cout << Color::MAGENTA << Color::BOLD << "AI" << Color::RESET;
            if (showTimestamps) {
                std::cout << Color::DIM << " [" << msg.timestamp << "]" << Color::RESET;
            }
            if (showThinking && msg.thinkingLevel > 0) {
                std::cout << Color::YELLOW << " (thinking:" << msg.thinkingLevel << ")" << Color::RESET;
            }
            std::cout << "\n";
            
            // Format code blocks
            std::string formatted = formatCodeBlocks(msg.content);
            std::cout << formatted;
            
            if (msg.isStreaming) {
                std::cout << Color::CYAN << "▊" << Color::RESET;
            }
            std::cout << "\n\n";
        } else if (msg.role == "system") {
            std::cout << Color::YELLOW << Color::BOLD << "System" << Color::RESET << "\n";
            std::cout << Color::DIM << msg.content << Color::RESET << "\n\n";
        } else if (msg.role == "tool") {
            std::cout << Color::CYAN << Color::BOLD << "Tool" << Color::RESET << "\n";
            std::cout << msg.content << "\n\n";
        }
    }
    
    std::string formatCodeBlocks(const std::string& content) {
        std::string result = content;
        // Simple code block formatting
        std::regex codeBlock(R"(```(\w+)?\n(.*?)```)");
        std::smatch match;
        std::string::const_iterator searchStart(result.cbegin());
        
        while (std::regex_search(searchStart, result.cend(), match, codeBlock)) {
            std::string lang = match[1].str();
            std::string code = match[2].str();
            
            std::string formatted = Color::BG_BLACK + Color::GREEN + "\n┌" + std::string(78, '─') + "┐\n";
            formatted += "│ " + Color::CYAN + (lang.empty() ? "code" : lang) + Color::GREEN + 
                        std::string(76 - (lang.empty() ? 4 : lang.length()), ' ') + "│\n";
            formatted += "├" + std::string(78, '─') + "┤\n";
            
            // Add code lines
            std::istringstream iss(code);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.length() > 76) line = line.substr(0, 76);
                formatted += "│ " + Color::WHITE + line + 
                           std::string(76 - line.length(), ' ') + Color::GREEN + "│\n";
            }
            
            formatted += "└" + std::string(78, '─') + "┘" + Color::RESET;
            
            size_t pos = result.find(match[0].str());
            if (pos != std::string::npos) {
                result.replace(pos, match[0].length(), formatted);
            }
            searchStart = result.cbegin() + pos + formatted.length();
        }
        
        return result;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(msgMutex);
        messages.clear();
    }
    
    std::vector<ChatMessage> getHistory() {
        std::lock_guard<std::mutex> lock(msgMutex);
        return messages;
    }
    
    void setModel(const std::string& model) { currentModel = model; }
    void setThinkingLevel(int level) { thinkingLevel = level; }
    void setShowTimestamps(bool show) { showTimestamps = show; }
    void setShowThinking(bool show) { showThinking = show; }
    
    size_t getMessageCount() const { return messages.size(); }
};

// ============================================================================
// CLI COMMAND PROCESSOR
// ============================================================================
class CLIProcessor {
public:
    using CommandHandler = std::function<void(const std::vector<std::string>&)>;
    
private:
    std::map<std::string, CommandHandler> commands;
    std::map<std::string, std::string> helpText;
    
public:
    CLIProcessor() {
        registerDefaultCommands();
    }
    
    void registerCommand(const std::string& name, const std::string& help, CommandHandler handler) {
        commands[name] = handler;
        helpText[name] = help;
    }
    
    bool execute(const std::string& input) {
        auto tokens = tokenize(input);
        if (tokens.empty()) return true;
        
        std::string cmd = tokens[0];
        tokens.erase(tokens.begin());
        
        auto it = commands.find(cmd);
        if (it != commands.end()) {
            it->second(tokens);
            return true;
        }
        
        return false; // Command not found
    }
    
    void showHelp() {
        std::cout << Color::BOLD << "Available Commands:\n" << Color::RESET;
        std::cout << std::string(40, '-') << "\n";
        for (const auto& [cmd, help] : helpText) {
            std::cout << Color::CYAN << std::left << std::setw(15) << cmd 
                     << Color::RESET << help << "\n";
        }
    }
    
private:
    std::vector<std::string> tokenize(const std::string& input) {
        std::vector<std::string> tokens;
        std::istringstream iss(input);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }
    
    void registerDefaultCommands() {
        registerCommand("help", "Show this help message", [this](auto&) { showHelp(); });
        registerCommand("quit", "Exit the IDE", [](auto&) { exit(0); });
        registerCommand("exit", "Exit the IDE", [](auto&) { exit(0); });
        registerCommand("clear", "Clear the screen", [](auto&) { 
            std::cout << "\033[2J\033[H"; 
        });
    }
};

// ============================================================================
// SOVEREIGN CLI IDE MAIN CLASS
// ============================================================================
class SovereignCLI {
private:
    ChatPanel chat;
    CLIProcessor cli;
    bool running = true;
    bool chatMode = false;  // Toggle between CLI and Chat mode
    std::string currentFile;
    std::string currentDir;
    
public:
    SovereignCLI() {
        currentDir = fs::current_path().string();
        setupCommands();
    }
    
    void run() {
        printBanner();
        
        while (running) {
            if (chatMode) {
                runChatMode();
            } else {
                runCLIMode();
            }
        }
    }
    
    void runCLIMode() {
        std::cout << Color::GREEN << "sov> " << Color::RESET;
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) return;
        
        // Check for chat mode toggle
        if (input == "/chat" || input == "\\chat") {
            chatMode = true;
            chat.render();
            return;
        }
        
        // Try CLI command first
        if (!cli.execute(input)) {
            // If not a CLI command, treat as chat message
            processChatMessage(input);
        }
    }
    
    void runChatMode() {
        std::cout << Color::MAGENTA << "chat> " << Color::RESET;
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) return;
        
        // Check for CLI mode toggle
        if (input == "/cli" || input == "\\cli" || input == "/exit") {
            chatMode = false;
            return;
        }
        
        // Special chat commands
        if (input == "/clear") {
            chat.clear();
            chat.render();
            return;
        }
        
        if (input == "/history") {
            chat.render();
            return;
        }
        
        if (input.rfind("/think ", 0) == 0) {
            int level = std::stoi(input.substr(7));
            chat.setThinkingLevel(level);
            std::cout << Color::YELLOW << "Thinking level set to " << level << Color::RESET << "\n";
            return;
        }
        
        // Regular chat message
        processChatMessage(input);
        chat.render();
    }
    
    void processChatMessage(const std::string& input) {
        // Add user message
        chat.addMessage("user", input);
        
        // Simulate AI response (in real implementation, this would call the model)
        std::string response = generateResponse(input);
        
        // Add AI response with streaming effect
        chat.addMessage("assistant", "");
        simulateStreaming(response);
        chat.completeStreaming();
    }
    
    std::string generateResponse(const std::string& input) {
        // Simple response generation (replace with actual AI model call)
        if (input.find("hello") != std::string::npos || input.find("hi") != std::string::npos) {
            return "Hello! I'm Sovereign IDE's AI assistant. How can I help you today?";
        }
        if (input.find("code") != std::string::npos) {
            return "I can help you write code. Here's an example:\n\n```cpp\n#include <iostream>\n\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}\n```\n\nWould you like me to explain this code or help with something else?";
        }
        if (input.find("file") != std::string::npos) {
            return "I can help you work with files. Use commands like:\n- `open <file>` to open a file\n- `save` to save current file\n- `list` to see files in current directory";
        }
        if (input.find("help") != std::string::npos) {
            return "I'm your AI coding assistant! I can:\n\n1. **Write code** - Just ask me to write something\n2. **Explain code** - Paste code and ask for explanation\n3. **Debug** - Share errors and I'll help fix them\n4. **Chat** - Just talk to me about programming\n\nType `/cli` to switch to command mode, or just chat with me!";
        }
        
        return "I understand you're asking about: \"" + input + "\"\n\nI'm a prototype AI assistant for Sovereign IDE. In the full implementation, I would:\n\n1. Process your request using the local LLM\n2. Analyze your codebase context\n3. Provide intelligent responses\n4. Execute tools when needed\n\nFor now, try asking about:\n- Writing code\n- Opening files\n- Getting help";
    }
    
    void simulateStreaming(const std::string& text) {
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            chat.addStreamingChunk(word + " ");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    
    void setupCommands() {
        // File commands
        cli.registerCommand("open", "Open a file", [this](const auto& args) {
            if (args.empty()) {
                std::cout << Color::RED << "Usage: open <filename>" << Color::RESET << "\n";
                return;
            }
            std::string filename = args[0];
            std::ifstream file(filename);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                currentFile = filename;
                std::cout << Color::GREEN << "Opened: " << filename << " (" 
                         << content.size() << " bytes)" << Color::RESET << "\n";
                
                // Show preview
                std::cout << Color::DIM << "--- Preview ---" << Color::RESET << "\n";
                std::istringstream iss(content);
                std::string line;
                int lines = 0;
                while (std::getline(iss, line) && lines < 10) {
                    std::cout << line.substr(0, 80) << "\n";
                    lines++;
                }
                if (lines >= 10) std::cout << Color::DIM << "..." << Color::RESET << "\n";
            } else {
                std::cout << Color::RED << "Failed to open: " << filename << Color::RESET << "\n";
            }
        });
        
        cli.registerCommand("save", "Save current file", [this](const auto& args) {
            if (currentFile.empty()) {
                std::cout << Color::RED << "No file open" << Color::RESET << "\n";
                return;
            }
            std::cout << Color::GREEN << "Saved: " << currentFile << Color::RESET << "\n";
        });
        
        cli.registerCommand("list", "List files in directory", [this](const auto& args) {
            std::string dir = args.empty() ? currentDir : args[0];
            std::cout << Color::CYAN << "Files in " << dir << ":" << Color::RESET << "\n";
            try {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory()) {
                        std::cout << Color::BLUE << "[DIR]  " << name << Color::RESET << "\n";
                    } else {
                        std::cout << "[FILE] " << name << " (" 
                                 << entry.file_size() << " bytes)\n";
                    }
                }
            } catch (const std::exception& e) {
                std::cout << Color::RED << "Error: " << e.what() << Color::RESET << "\n";
            }
        });
        
        cli.registerCommand("cd", "Change directory", [this](const auto& args) {
            if (args.empty()) {
                std::cout << Color::YELLOW << "Current: " << currentDir << Color::RESET << "\n";
                return;
            }
            try {
                fs::current_path(args[0]);
                currentDir = fs::current_path().string();
                std::cout << Color::GREEN << "Changed to: " << currentDir << Color::RESET << "\n";
            } catch (const std::exception& e) {
                std::cout << Color::RED << "Error: " << e.what() << Color::RESET << "\n";
            }
        });
        
        // Chat commands
        cli.registerCommand("chat", "Switch to chat mode", [this](const auto&) {
            chatMode = true;
            chat.render();
        });
        
        cli.registerCommand("ask", "Ask AI a question", [this](const auto& args) {
            std::string question;
            for (const auto& arg : args) {
                question += arg + " ";
            }
            processChatMessage(question);
            chat.render();
        });
        
        // System commands
        cli.registerCommand("pwd", "Print working directory", [this](const auto&) {
            std::cout << currentDir << "\n";
        });
        
        cli.registerCommand("echo", "Echo text", [](const auto& args) {
            for (const auto& arg : args) {
                std::cout << arg << " ";
            }
            std::cout << "\n";
        });
        
        cli.registerCommand("cat", "Display file contents", [](const auto& args) {
            if (args.empty()) return;
            std::ifstream file(args[0]);
            if (file) {
                std::cout << file.rdbuf() << "\n";
            } else {
                std::cout << Color::RED << "Cannot read: " << args[0] << Color::RESET << "\n";
            }
        });
        
        cli.registerCommand("grep", "Search in files", [](const auto& args) {
            if (args.size() < 2) {
                std::cout << Color::RED << "Usage: grep <pattern> <file>" << Color::RESET << "\n";
                return;
            }
            std::string pattern = args[0];
            std::string filename = args[1];
            
            std::ifstream file(filename);
            if (!file) {
                std::cout << Color::RED << "Cannot open: " << filename << Color::RESET << "\n";
                return;
            }
            
            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (line.find(pattern) != std::string::npos) {
                    std::cout << Color::CYAN << lineNum << ":" << Color::RESET 
                             << line << "\n";
                }
            }
        });
        
        // IDE commands
        cli.registerCommand("edit", "Edit file (opens in default editor)", [](const auto& args) {
            if (args.empty()) return;
            std::string cmd = "notepad " + args[0];  // Windows
            system(cmd.c_str());
        });
        
        cli.registerCommand("build", "Build current project", [](const auto&) {
            std::cout << Color::YELLOW << "Building..." << Color::RESET << "\n";
            // Would integrate with build system
            std::cout << Color::GREEN << "Build complete!" << Color::RESET << "\n";
        });
        
        cli.registerCommand("run", "Run current file", [this](const auto&) {
            if (currentFile.empty()) {
                std::cout << Color::RED << "No file open" << Color::RESET << "\n";
                return;
            }
            std::cout << Color::YELLOW << "Running: " << currentFile << Color::RESET << "\n";
            // Would execute based on file type
        });
        
        cli.registerCommand("think", "Set thinking level (0-5)", [this](const auto& args) {
            if (args.empty()) {
                std::cout << "Current thinking level: " << chat.getMessageCount() << "\n";
                return;
            }
            int level = std::stoi(args[0]);
            chat.setThinkingLevel(level);
            std::cout << Color::GREEN << "Thinking level set to " << level << Color::RESET << "\n";
        });
        
        cli.registerCommand("model", "Set AI model", [this](const auto& args) {
            if (args.empty()) {
                std::cout << "Available models: local, codestral, gpt4\n";
                return;
            }
            chat.setModel(args[0]);
            std::cout << Color::GREEN << "Model set to: " << args[0] << Color::RESET << "\n";
        });
    }
    
    void printBanner() {
        std::cout << "\n";
        std::cout << Color::BG_BLUE << Color::BOLD << Color::WHITE
                 << "╔══════════════════════════════════════════════════════════════╗" << Color::RESET << "\n";
        std::cout << Color::BG_BLUE << Color::BOLD << Color::WHITE
                 << "║     Sovereign CLI IDE v3.0.0 - Integrated Chat & Console   ║" << Color::RESET << "\n";
        std::cout << Color::BG_BLUE << Color::BOLD << Color::WHITE
                 << "║     Type '/chat' for chat mode, 'help' for commands          ║" << Color::RESET << "\n";
        std::cout << Color::BG_BLUE << Color::BOLD << Color::WHITE
                 << "╚══════════════════════════════════════════════════════════════╝" << Color::RESET << "\n";
        std::cout << "\n";
    }
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    // Enable ANSI colors on Windows
    #ifdef _WIN32
    system("");
    #endif
    
    SovereignCLI ide;
    ide.run();
    
    return 0;
}
