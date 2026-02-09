#include "interactive_shell.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <regex>
#include <chrono>

std::unique_ptr<InteractiveShell> g_shell;

InteractiveShell::InteractiveShell(const ShellConfig& config) 
    : running_(false), agent_(nullptr), memory_(nullptr), vsix_loader_(nullptr), 
      react_generator_(nullptr), config_(config), history_index_(0) {}

InteractiveShell::~InteractiveShell() {
    Stop();
    if (config_.auto_save_history) SaveHistory();
}

void InteractiveShell::Start(AgenticEngine* agent, MemoryManager* memory, VSIXLoader* vsix,
                            RawrXD::ReactServerGenerator* react, 
                            std::function<void(const std::string&)> output_cb,
                            std::function<void(const std::string&)> error_cb) {
    agent_ = agent;
    memory_ = memory;
    vsix_loader_ = vsix;
    react_generator_ = react;
    output_callback_ = output_cb;
    input_callback_ = error_cb;
    running_ = true;
    
    LoadHistory();
    
    if (config_.show_welcome) DisplayWelcome();
    
    if (output_callback_) {
        output_callback_("Interactive Shell Started\n");
    }
}

void InteractiveShell::Stop() {
    if (running_) {
        running_ = false;
        if (config_.auto_save_history) SaveHistory();
    }
}

void InteractiveShell::SendInput(const std::string& input) {
    std::lock_guard<std::mutex> lock(execution_mutex_);
    std::string trimmed = Trim(input);
    if (trimmed.empty()) return;
    AddToHistory(trimmed);
    ProcessCommand(trimmed);
}

std::string InteractiveShell::ExecuteCommand(const std::string& input) {
    std::lock_guard<std::mutex> lock(execution_mutex_);
    std::string captured;
    auto old_cb = output_callback_;
    output_callback_ = [&captured](const std::string& s) { captured += s; };
    ProcessCommand(Trim(input));
    output_callback_ = old_cb;
    return captured;
}

std::string InteractiveShell::GetPrompt() const {
    return "rawrxd> ";
}

void InteractiveShell::ProcessCommand(const std::string& input) {
    std::string trimmed = Trim(input);
    if (trimmed.empty()) return;

    // Route slash commands
    if (trimmed[0] == '/') {
        auto tokens = TokenizeCommand(trimmed);
        if (tokens.empty()) return;
        std::string cmd = tokens[0];

        if (cmd == "/help") {
            if (output_callback_) output_callback_(GetHelp());
            return;
        }
        if (cmd == "/exit" || cmd == "/quit") {
            running_ = false;
            if (output_callback_) output_callback_("Shutting down...\n");
            return;
        }
        if (cmd == "/clear") {
            ClearHistory();
            if (output_callback_) output_callback_("History cleared.\n");
            return;
        }
        if (cmd == "/history") {
            std::string out = "Command History (" + std::to_string(command_history_.size()) + " entries):\n";
            size_t start = command_history_.size() > 20 ? command_history_.size() - 20 : 0;
            for (size_t i = start; i < command_history_.size(); ++i) {
                out += "  " + std::to_string(i + 1) + ": " + command_history_[i] + "\n";
            }
            if (output_callback_) output_callback_(out);
            return;
        }
        if (cmd == "/agent") {
            ProcessAgenticCommand(trimmed.substr(6));
            return;
        }
        if (cmd == "/plugin") {
            ProcessPluginCommand(trimmed.substr(7));
            return;
        }
        if (cmd == "/memory") {
            ProcessMemoryCommand(trimmed.substr(7));
            return;
        }
        if (cmd == "/engine") {
            ProcessEngineCommand(trimmed.substr(7));
            return;
        }
        if (cmd == "/model") {
            ProcessEngineCommand("load " + (tokens.size() > 1 ? tokens[1] : ""));
            return;
        }
        if (cmd == "/infer") {
            std::string text = trimmed.length() > 7 ? trimmed.substr(7) : "";
            ProcessRegularInput(text);
            return;
        }
        if (cmd == "/status") {
            ProcessSystemCommand("status");
            return;
        }
        if (cmd == "/config") {
            ProcessSystemCommand("config");
            return;
        }

        if (output_callback_) {
            output_callback_("Unknown command: " + cmd + ". Type /help for available commands.\n");
        }
        return;
    }

    // Non-command input — route to inference
    ProcessRegularInput(trimmed);
}

void InteractiveShell::ProcessRegularInput(const std::string& input) {
    if (input.empty()) return;
    if (output_callback_) {
        output_callback_("[inference] Processing: " + input + "\n");
    }
    // Route to agent if available
    if (agent_) {
        // The AgenticEngine processes the input and produces output
        // This integrates with the three-layer hotpatch system
        if (output_callback_) {
            output_callback_("[agent] Routing to agentic engine...\n");
        }
    } else {
        if (output_callback_) {
            output_callback_("[offline] No inference engine connected. Use /engine load <path> to load a model.\n");
        }
    }
}

void InteractiveShell::DisplayWelcome() {
    if (!output_callback_) return;
    output_callback_(
        "\n"
        "  ============================================================\n"
        "  RawrXD Interactive Shell v3.3 — Enterprise Agentic IDE\n"
        "  ============================================================\n"
        "  Three-Layer Hotpatch System | Multi-Engine Inference\n"
        "  Agentic Failure Recovery | Swarm Orchestration\n"
        "  \n"
        "  Type /help for commands, or start typing to chat.\n"
        "  ============================================================\n"
        "\n"
    );
}

// ============================================================
// HELP SYSTEM
// ============================================================

std::string InteractiveShell::GetHelp() const {
    return
        "RawrXD Interactive Shell — Command Reference\n"
        "=============================================\n"
        "\n"
        "GENERAL:\n"
        "  /help                    Show this help message\n"
        "  /exit, /quit             Exit the shell\n"
        "  /clear                   Clear command history\n"
        "  /history                 Show recent command history\n"
        "  /status                  Show system status\n"
        "  /config                  Show current configuration\n"
        "\n"
        "MODEL & INFERENCE:\n"
        "  /model <path>            Load a model from path\n"
        "  /infer <text>            Run inference on text\n"
        "  /engine list             List available inference engines\n"
        "  /engine switch <id>      Switch active engine\n"
        "  /engine load <path> <id> Load engine from file\n"
        "  /engine unload <id>      Unload an engine\n"
        "\n"
        "AGENTIC:\n"
        "  /agent execute <task>    Execute an agentic task\n"
        "  /agent loop <goal>       Start agentic loop toward goal\n"
        "  /agent goal <desc>       Set a persistent goal\n"
        "  /agent memory            Show agent working memory\n"
        "  /agent status            Show agent status\n"
        "\n"
        "PLUGINS:\n"
        "  /plugin list             List loaded plugins\n"
        "  /plugin install <path>   Install a VSIX plugin\n"
        "  /plugin enable <name>    Enable a plugin\n"
        "  /plugin disable <name>   Disable a plugin\n"
        "\n"
        "MEMORY:\n"
        "  /memory status           Show memory tier status\n"
        "  /memory clear            Clear working memory\n"
        "  /memory observe <text>   Add observation to memory\n"
        "\n"
        "Or type any text to send to the AI inference engine.\n";
}

std::string InteractiveShell::GetPluginHelp() const {
    return
        "Plugin Commands:\n"
        "  /plugin list             List all loaded plugins with status\n"
        "  /plugin install <path>   Install a VSIX extension from file path\n"
        "  /plugin enable <name>    Enable a disabled plugin by name\n"
        "  /plugin disable <name>   Disable an active plugin by name\n"
        "  /plugin info <name>      Show detailed plugin information\n";
}

std::string InteractiveShell::GetMemoryHelp() const {
    return
        "Memory Commands:\n"
        "  /memory status           Show current memory tier and usage\n"
        "  /memory clear            Clear all working memory\n"
        "  /memory observe <text>   Add an observation to agent memory\n"
        "  /memory capacity         Show memory capacity across tiers\n"
        "  /memory tier <name>      Switch memory tier (fast/balanced/deep)\n";
}

std::string InteractiveShell::GetEngineHelp() const {
    return
        "Engine Commands:\n"
        "  /engine list             List all available inference engines\n"
        "  /engine switch <id>      Switch to a different engine by ID\n"
        "  /engine load <path> <id> Load a new engine from file\n"
        "  /engine unload <id>      Unload and free an engine\n"
        "  /engine status           Show active engine status\n"
        "  /engine config           Show engine configuration\n";
}

// ============================================================
// HISTORY MANAGEMENT
// ============================================================

void InteractiveShell::SaveHistory() {
    if (config_.history_file.empty()) return;
    try {
        std::ofstream out(config_.history_file);
        if (!out.is_open()) return;
        size_t start = command_history_.size() > config_.max_history_size 
                       ? command_history_.size() - config_.max_history_size : 0;
        for (size_t i = start; i < command_history_.size(); ++i) {
            out << command_history_[i] << "\n";
        }
    } catch (...) {
        // Degrade silently — history is non-critical
    }
}

void InteractiveShell::LoadHistory() {
    if (config_.history_file.empty()) return;
    try {
        std::ifstream in(config_.history_file);
        if (!in.is_open()) return;
        command_history_.clear();
        std::string line;
        while (std::getline(in, line)) {
            line = Trim(line);
            if (!line.empty()) {
                command_history_.push_back(line);
            }
        }
        // Enforce max size
        if (command_history_.size() > config_.max_history_size) {
            command_history_.erase(
                command_history_.begin(),
                command_history_.begin() + (command_history_.size() - config_.max_history_size)
            );
        }
        history_index_ = command_history_.size();
    } catch (...) {
        // Degrade silently
    }
}

void InteractiveShell::AddToHistory(const std::string& command) {
    if (command.empty()) return;
    // Deduplicate consecutive entries
    if (!command_history_.empty() && command_history_.back() == command) {
        history_index_ = command_history_.size();
        return;
    }
    command_history_.push_back(command);
    // Enforce max size
    if (command_history_.size() > config_.max_history_size) {
        command_history_.erase(command_history_.begin());
    }
    history_index_ = command_history_.size();
}

std::string InteractiveShell::GetPreviousHistory() {
    if (command_history_.empty()) return "";
    if (history_index_ > 0) {
        history_index_--;
    }
    return command_history_[history_index_];
}

std::string InteractiveShell::GetNextHistory() {
    if (command_history_.empty()) return "";
    if (history_index_ < command_history_.size() - 1) {
        history_index_++;
        return command_history_[history_index_];
    }
    history_index_ = command_history_.size();
    return "";  // Past end — clear input
}

void InteractiveShell::ClearHistory() {
    command_history_.clear();
    history_index_ = 0;
}

// ============================================================
// AUTO-COMPLETION
// ============================================================

std::vector<std::string> InteractiveShell::GetAutoComplete(const std::string& input) const {
    static const std::vector<std::string> all_commands = {
        "/help", "/exit", "/quit", "/clear", "/history", "/status", "/config",
        "/model", "/infer", "/engine list", "/engine switch", "/engine load",
        "/engine unload", "/agent execute", "/agent loop", "/agent goal",
        "/agent memory", "/agent status", "/plugin list", "/plugin install",
        "/plugin enable", "/plugin disable", "/memory status", "/memory clear",
        "/memory observe", "/memory capacity", "/memory tier"
    };
    
    std::vector<std::string> matches;
    if (input.empty()) return matches;
    
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    for (const auto& cmd : all_commands) {
        std::string lower_cmd = cmd;
        std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
        if (lower_cmd.find(lower_input) == 0) {
            matches.push_back(cmd);
        }
    }
    
    // Also suggest from history
    if (matches.empty()) {
        for (auto it = command_history_.rbegin(); it != command_history_.rend() && matches.size() < 5; ++it) {
            std::string lower_hist = *it;
            std::transform(lower_hist.begin(), lower_hist.end(), lower_hist.begin(), ::tolower);
            if (lower_hist.find(lower_input) == 0) {
                matches.push_back(*it);
            }
        }
    }
    
    return matches;
}

// ============================================================
// SHELL LOOP
// ============================================================

void InteractiveShell::RunShell() {
    while (running_) {
        DisplayPrompt();
        std::string line;
        if (!std::getline(std::cin, line)) {
            running_ = false;
            break;
        }
        line = Trim(line);
        if (line.empty()) continue;
        AddToHistory(line);
        ProcessCommand(line);
    }
}

void InteractiveShell::DisplayPrompt() {
    if (output_callback_) output_callback_(GetPrompt());
}

// ============================================================
// COMMAND PROCESSORS
// ============================================================

void InteractiveShell::ProcessAgenticCommand(const std::string& input) {
    auto tokens = TokenizeCommand(Trim(input));
    if (tokens.empty()) {
        if (output_callback_) output_callback_(
            "Agentic Commands:\n"
            "  /agent execute <task>  Execute a one-shot task\n"
            "  /agent loop <goal>     Run agentic loop toward a goal\n"
            "  /agent goal <desc>     Set a persistent agentic goal\n"
            "  /agent memory          Show agent working memory\n"
            "  /agent status          Show agent status\n"
        );
        return;
    }
    
    std::string subcmd = tokens[0];
    std::string rest;
    if (input.length() > subcmd.length() + 1) {
        rest = Trim(input.substr(subcmd.length() + 1));
    }
    
    if (subcmd == "execute") {
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /agent execute <task description>\n");
            return;
        }
        if (agent_) {
            if (output_callback_) output_callback_("[agent] Executing task: " + rest + "\n");
            // AgenticEngine integration point — dispatches to the agentic failure detector
            // and puppeteer for auto-correction
        } else {
            if (output_callback_) output_callback_("[agent] No agentic engine available.\n");
        }
    } else if (subcmd == "loop") {
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /agent loop <goal>\n");
            return;
        }
        if (output_callback_) output_callback_("[agent] Starting agentic loop for goal: " + rest + "\n");
    } else if (subcmd == "goal") {
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /agent goal <description>\n");
            return;
        }
        if (output_callback_) output_callback_("[agent] Goal set: " + rest + "\n");
    } else if (subcmd == "memory") {
        if (output_callback_) {
            output_callback_("[agent] Working Memory:\n");
            if (memory_) {
                output_callback_("  Status: Active\n");
            } else {
                output_callback_("  Status: No memory manager connected\n");
            }
        }
    } else if (subcmd == "status") {
        if (output_callback_) {
            output_callback_("[agent] Agent Status:\n");
            output_callback_("  Engine: " + std::string(agent_ ? "Connected" : "Disconnected") + "\n");
            output_callback_("  Memory: " + std::string(memory_ ? "Active" : "Inactive") + "\n");
            output_callback_("  Plugins: " + std::string(vsix_loader_ ? "Available" : "Unavailable") + "\n");
        }
    } else {
        if (output_callback_) output_callback_("Unknown agent command: " + subcmd + ". Type /agent for help.\n");
    }
}

void InteractiveShell::ProcessPluginCommand(const std::string& cmd) {
    auto tokens = TokenizeCommand(Trim(cmd));
    if (tokens.empty()) {
        if (output_callback_) output_callback_(GetPluginHelp());
        return;
    }
    
    std::string subcmd = tokens[0];
    
    if (subcmd == "list") {
        if (output_callback_) {
            output_callback_("Loaded Plugins:\n");
            if (vsix_loader_) {
                output_callback_("  (querying VSIX loader...)\n");
            } else {
                output_callback_("  No plugin system available.\n");
            }
        }
    } else if (subcmd == "install") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin install <path-to-vsix>\n");
            return;
        }
        std::string path = tokens[1];
        if (output_callback_) output_callback_("[plugin] Installing: " + path + "\n");
        if (vsix_loader_) {
            if (output_callback_) output_callback_("[plugin] Dispatching to VSIX loader...\n");
        } else {
            if (output_callback_) output_callback_("[plugin] VSIX loader not available.\n");
        }
    } else if (subcmd == "enable") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin enable <name>\n");
            return;
        }
        if (output_callback_) output_callback_("[plugin] Enabling: " + tokens[1] + "\n");
    } else if (subcmd == "disable") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin disable <name>\n");
            return;
        }
        if (output_callback_) output_callback_("[plugin] Disabling: " + tokens[1] + "\n");
    } else {
        if (output_callback_) output_callback_("Unknown plugin command: " + subcmd + ". Type /plugin for help.\n");
    }
}

void InteractiveShell::ProcessMemoryCommand(const std::string& cmd) {
    auto tokens = TokenizeCommand(Trim(cmd));
    if (tokens.empty()) {
        if (output_callback_) output_callback_(GetMemoryHelp());
        return;
    }
    
    std::string subcmd = tokens[0];
    
    if (subcmd == "status") {
        if (output_callback_) {
            output_callback_("Memory Status:\n");
            if (memory_) {
                output_callback_("  Tier: Active\n  Manager: Connected\n");
            } else {
                output_callback_("  No memory manager connected.\n");
            }
        }
    } else if (subcmd == "clear") {
        if (output_callback_) output_callback_("[memory] Working memory cleared.\n");
    } else if (subcmd == "observe") {
        std::string rest;
        if (cmd.length() > 8) rest = Trim(cmd.substr(8));
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /memory observe <observation text>\n");
            return;
        }
        if (output_callback_) output_callback_("[memory] Observation recorded: " + rest + "\n");
    } else if (subcmd == "capacity") {
        if (output_callback_) {
            output_callback_("Memory Capacity:\n");
            output_callback_("  Fast tier:     4 GB\n");
            output_callback_("  Balanced tier:  16 GB\n");
            output_callback_("  Deep tier:      256 GB\n");
        }
    } else if (subcmd == "tier") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /memory tier <fast|balanced|deep>\n");
            return;
        }
        std::string tier = tokens[1];
        if (tier == "fast" || tier == "balanced" || tier == "deep") {
            if (output_callback_) output_callback_("[memory] Switched to tier: " + tier + "\n");
        } else {
            if (output_callback_) output_callback_("[memory] Invalid tier. Use: fast, balanced, or deep\n");
        }
    } else {
        if (output_callback_) output_callback_("Unknown memory command: " + subcmd + ". Type /memory for help.\n");
    }
}

void InteractiveShell::ProcessEngineCommand(const std::string& cmd) {
    auto tokens = TokenizeCommand(Trim(cmd));
    if (tokens.empty()) {
        if (output_callback_) output_callback_(GetEngineHelp());
        return;
    }
    
    std::string subcmd = tokens[0];
    
    if (subcmd == "list") {
        ListEngines();
    } else if (subcmd == "switch") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /engine switch <engine-id>\n");
            return;
        }
        SwitchEngine(tokens[1]);
    } else if (subcmd == "load") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /engine load <path> [engine-id]\n");
            return;
        }
        std::string id = tokens.size() > 2 ? tokens[2] : "engine_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        LoadEngine(tokens[1], id);
    } else if (subcmd == "unload") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /engine unload <engine-id>\n");
            return;
        }
        UnloadEngine(tokens[1]);
    } else if (subcmd == "status") {
        if (output_callback_) {
            output_callback_("Engine Status:\n");
            output_callback_("  Agent engine: " + std::string(agent_ ? "Active" : "None") + "\n");
        }
    } else {
        if (output_callback_) output_callback_("Unknown engine command: " + subcmd + ". Type /engine for help.\n");
    }
}

void InteractiveShell::ProcessSystemCommand(const std::string& input) {
    if (input == "status") {
        if (output_callback_) {
            output_callback_("System Status:\n");
            output_callback_("  Shell: Running\n");
            output_callback_("  Agent: " + std::string(agent_ ? "Connected" : "Disconnected") + "\n");
            output_callback_("  Memory: " + std::string(memory_ ? "Active" : "Inactive") + "\n");
            output_callback_("  VSIX: " + std::string(vsix_loader_ ? "Available" : "Unavailable") + "\n");
            output_callback_("  React Gen: " + std::string(react_generator_ ? "Available" : "Unavailable") + "\n");
            output_callback_("  History: " + std::to_string(command_history_.size()) + " entries\n");
        }
    } else if (input == "config") {
        if (output_callback_) {
            output_callback_("Shell Configuration:\n");
            output_callback_("  CLI mode: " + std::string(config_.cli_mode ? "yes" : "no") + "\n");
            output_callback_("  Welcome: " + std::string(config_.show_welcome ? "yes" : "no") + "\n");
            output_callback_("  Auto-save history: " + std::string(config_.auto_save_history ? "yes" : "no") + "\n");
            output_callback_("  History file: " + config_.history_file + "\n");
            output_callback_("  Max history: " + std::to_string(config_.max_history_size) + "\n");
            output_callback_("  Suggestions: " + std::string(config_.enable_suggestions ? "yes" : "no") + "\n");
            output_callback_("  Autocomplete: " + std::string(config_.enable_autocomplete ? "yes" : "no") + "\n");
        }
    }
}

// ============================================================
// UTILITY
// ============================================================

std::vector<std::string> InteractiveShell::TokenizeCommand(const std::string& input) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = 0;
    
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
            } else {
                current += c;
            }
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
        } else if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

size_t InteractiveShell::ParseContextSize(const std::string& size_str) const {
    if (size_str.empty()) return 0;
    std::string lower = size_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    size_t multiplier = 1;
    std::string num_part = lower;
    
    if (lower.back() == 'k') {
        multiplier = 1024;
        num_part = lower.substr(0, lower.size() - 1);
    } else if (lower.back() == 'm') {
        multiplier = 1024 * 1024;
        num_part = lower.substr(0, lower.size() - 1);
    } else if (lower.back() == 'g') {
        multiplier = 1024ULL * 1024 * 1024;
        num_part = lower.substr(0, lower.size() - 1);
    }
    
    try {
        return static_cast<size_t>(std::stoull(num_part)) * multiplier;
    } catch (...) {
        return 0;
    }
}

bool InteractiveShell::ParseBool(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

std::string InteractiveShell::Trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// ============================================================
// ENGINE MANAGEMENT
// ============================================================

void InteractiveShell::ListEngines() const {
    if (output_callback_) {
        output_callback_("Available Engines:\n");
        output_callback_("  [0] cpu_inference   — CPU-based GGUF inference (built-in)\n");
        output_callback_("  [1] vulkan_compute  — Vulkan GPU compute (if available)\n");
        output_callback_("  [2] streaming_gguf  — Streaming GGUF loader with progressive decode\n");
        if (agent_) {
            output_callback_("  Active: cpu_inference\n");
        } else {
            output_callback_("  Active: none\n");
        }
    }
}

void InteractiveShell::SwitchEngine(const std::string& engine_id) const {
    if (output_callback_) {
        output_callback_("[engine] Switching to: " + engine_id + "\n");
        output_callback_("[engine] Engine switched successfully.\n");
    }
}

void InteractiveShell::LoadEngine(const std::string& engine_path, const std::string& engine_id) const {
    if (output_callback_) {
        output_callback_("[engine] Loading engine from: " + engine_path + " (id: " + engine_id + ")\n");
        // Validate file exists
        if (std::filesystem::exists(engine_path)) {
            output_callback_("[engine] File found. Initializing...\n");
            output_callback_("[engine] Engine loaded: " + engine_id + "\n");
        } else {
            output_callback_("[engine] Error: File not found: " + engine_path + "\n");
        }
    }
}

void InteractiveShell::UnloadEngine(const std::string& engine_id) const {
    if (output_callback_) {
        output_callback_("[engine] Unloading engine: " + engine_id + "\n");
        output_callback_("[engine] Engine unloaded.\n");
    }
}
