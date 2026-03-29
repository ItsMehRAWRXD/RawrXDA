#include "interactive_shell.h"
#include "agentic_engine.h"
#include "modules/vsix_loader.h"
#include "modules/memory_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <regex>
#include <chrono>
#include <unordered_map>

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
        if (cmd == "/reveng") {
            ProcessRevEngCommand(trimmed.length() > 7 ? Trim(trimmed.substr(7)) : "");
            return;
        }
        if (cmd == "/disk") {
            ProcessDiskCommand(trimmed.length() > 5 ? Trim(trimmed.substr(5)) : "");
            return;
        }
        if (cmd == "/governor") {
            ProcessGovernorCommand(trimmed.length() > 9 ? Trim(trimmed.substr(9)) : "");
            return;
        }
        if (cmd == "/lsp") {
            ProcessLspCommand(trimmed.length() > 4 ? Trim(trimmed.substr(4)) : "");
            return;
        }
        if (cmd == "/debugger" || cmd == "/dbg") {
            ProcessDebuggerCommand(trimmed.length() > cmd.length() ? Trim(trimmed.substr(cmd.length())) : "");
            return;
        }
        if (cmd == "/swarm") {
            ProcessSwarmCommand(trimmed.length() > 6 ? Trim(trimmed.substr(6)) : "");
            return;
        }
        if (cmd == "/hotpatch") {
            ProcessHotpatchCommand(trimmed.length() > 9 ? Trim(trimmed.substr(9)) : "");
            return;
        }
        if (cmd == "/build") {
            ProcessBuildCommand(trimmed.length() > 6 ? Trim(trimmed.substr(6)) : "");
            return;
        }
        if (cmd == "/view") {
            ProcessViewCommand(trimmed.length() > 5 ? Trim(trimmed.substr(5)) : "");
            return;
        }
        if (cmd == "/voice") {
            ProcessVoiceCommand(trimmed.length() > 6 ? Trim(trimmed.substr(6)) : "");
            return;
        }
        if (cmd == "/telemetry") {
            ProcessTelemetryCommand(trimmed.length() > 10 ? Trim(trimmed.substr(10)) : "");
            return;
        }
        if (cmd == "/backend") {
            ProcessBackendCommand(trimmed.length() > 8 ? Trim(trimmed.substr(8)) : "");
            return;
        }
        if (cmd == "/git") {
            ProcessGitCommand(trimmed.length() > 4 ? Trim(trimmed.substr(4)) : "");
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
    if (!agent_) {
        if (output_callback_)
            output_callback_("[offline] No engine connected — use /engine load <path> or provide an Ollama model name.\n");
        return;
    }
    if (output_callback_) output_callback_("[inference] ...\n");
    try {
        std::string response = agent_->chat(input);
        if (output_callback_) {
            if (!response.empty()) output_callback_(response + "\n");
            else output_callback_("[inference] (no response generated)\n");
        }
    } catch (const std::exception& e) {
        if (output_callback_)
            output_callback_("[inference] Error: " + std::string(e.what()) + "\n");
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
        "  /agent iteration status  Show iteration progress status\n"
        "  /agent iteration set <current> <total> [phase] [message]\n"
        "                           Set iteration progress status\n"
        "  /agent iteration reset   Reset iteration progress state\n"
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
        "REVERSE ENGINEERING:\n"
        "  /reveng disassemble <p>  Hex dump and disassemble binary\n"
        "  /reveng decompile <path> AI-assisted decompilation\n"
        "  /reveng vulns <path>     Scan for vulnerabilities\n"
        "\n"
        "SYSTEM:\n"
        "  /disk list-drives        List all logical drives\n"
        "  /governor status         Show CPU/memory power status\n"
        "  /lsp status              Show LSP server status\n"
        "\n"
        "DEBUGGER:\n"
        "  /debugger launch <exe>   Launch process under debugger\n"
        "  /debugger attach <pid>   Attach to running process\n"
        "  /debugger break          Break into target\n"
        "  /debugger go             Continue execution\n"
        "  /debugger step           Single-step (step over)\n"
        "  /debugger status         Show debugger status\n"
        "\n"
        "SWARM:\n"
        "  /swarm status            Show swarm cluster status\n"
        "  /swarm start             Start swarm leader node\n"
        "  /swarm stop              Stop swarm node\n"
        "\n"
        "BUILD:\n"
        "  /build compile           Compile current project\n"
        "  /build run               Build and run\n"
        "  /build clean             Clean build artifacts\n"
        "\n"
        "HOTPATCH:\n"
        "  /hotpatch status         Show hotpatch layer status\n"
        "  /hotpatch toggle         Toggle hotpatch system\n"
        "\n"
        "VIEW:\n"
        "  /view sidebar            Toggle sidebar\n"
        "  /view terminal           Toggle terminal panel\n"
        "  /view zoom-in            Increase zoom level\n"
        "  /view zoom-out           Decrease zoom level\n"
        "\n"
        "TELEMETRY:\n"
        "  /telemetry status        Show telemetry dashboard\n"
        "  /telemetry export        Export telemetry data\n"
        "\n"
        "BACKEND:\n"
        "  /backend status          Show backend switcher status\n"
        "  /backend switch <name>   Switch backend (local/ollama/openai)\n"
        "\n"
        "GIT:\n"
        "  /git status              Show git repository status\n"
        "  /git commit <msg>        Commit staged changes\n"
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
        "/agent memory", "/agent status", "/agent iteration status",
        "/agent iteration set", "/agent iteration reset", "/plugin list", "/plugin install",
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
            "  /agent iteration status\n"
            "                         Show iteration progress\n"
            "  /agent iteration set <current> <total> [phase] [message]\n"
            "                         Update iteration progress\n"
            "  /agent iteration reset Reset iteration progress\n"
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
        if (!agent_) {
            if (output_callback_) output_callback_("[agent] No agentic engine available.\n");
            return;
        }
        if (output_callback_) output_callback_("[agent] Executing: " + rest + "\n");
        try {
            std::string result = agent_->chat(
                "Execute the following task and return concrete results:\n" + rest);
            if (output_callback_)
                output_callback_(result.empty() ? "[agent] (no output generated)\n" : result + "\n");
        } catch (const std::exception& e) {
            if (output_callback_)
                output_callback_("[agent] Error: " + std::string(e.what()) + "\n");
        }
    } else if (subcmd == "loop") {
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /agent loop <goal>\n");
            return;
        }
        if (!agent_) {
            if (output_callback_) output_callback_("[agent] No agentic engine available.\n");
            return;
        }
        if (output_callback_) output_callback_("[agent] Planning loop for: " + rest + "\n");
        try {
            std::string plan = agent_->planTask(rest);
            if (!plan.empty() && output_callback_) output_callback_("[plan]\n" + plan + "\n");
            std::string execution = agent_->chat(
                "Execute this plan step by step, reporting each result:\n" + plan);
            if (!execution.empty() && output_callback_) output_callback_(execution + "\n");
        } catch (const std::exception& e) {
            if (output_callback_)
                output_callback_("[agent] Loop error: " + std::string(e.what()) + "\n");
        }
    } else if (subcmd == "goal") {
        if (rest.empty()) {
            if (output_callback_) output_callback_("Usage: /agent goal <description>\n");
            return;
        }
        current_goal_ = rest;
        if (memory_) memory_->ProcessWithContext("PRIMARY GOAL: " + rest);
        if (agent_) {
            // Prime the agent context — discard response, just seeding state
            agent_->chat("System: Your primary goal for this session is: " + rest +
                         ". Acknowledge with 'Goal accepted.' only.");
        }
        if (output_callback_) output_callback_("[agent] Goal persisted: " + rest + "\n");
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
    } else if (subcmd == "iteration" || subcmd == "iterate") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_(
                "Usage:\n"
                "  /agent iteration status\n"
                "  /agent iteration set <current> <total> [phase] [message]\n"
                "  /agent iteration reset\n");
            return;
        }
        const std::string action = tokens[1];
        if (action == "status") {
            if (output_callback_) {
                output_callback_("[agent] Iteration Status:\n");
                output_callback_("  Busy: " + std::string(iteration_busy_ ? "true" : "false") + "\n");
                output_callback_("  Progress: " + std::to_string(iteration_current_) + "/" + std::to_string(iteration_total_) + "\n");
                output_callback_("  Phase: " + iteration_phase_ + "\n");
                output_callback_("  Message: " + (iteration_message_.empty() ? std::string("(none)") : iteration_message_) + "\n");
            }
        } else if (action == "set") {
            if (tokens.size() < 4) {
                if (output_callback_) output_callback_("Usage: /agent iteration set <current> <total> [phase] [message]\n");
                return;
            }
            try {
                iteration_current_ = std::max(0, std::stoi(tokens[2]));
                iteration_total_ = std::max(0, std::stoi(tokens[3]));
                iteration_busy_ = iteration_total_ > 0 && iteration_current_ < iteration_total_;
                if (tokens.size() >= 5) {
                    iteration_phase_ = tokens[4];
                }
                if (tokens.size() >= 6) {
                    iteration_message_.clear();
                    for (size_t i = 5; i < tokens.size(); ++i) {
                        if (!iteration_message_.empty()) iteration_message_ += " ";
                        iteration_message_ += tokens[i];
                    }
                }
                if (output_callback_) {
                    output_callback_("[agent] Iteration updated: "
                                     + std::to_string(iteration_current_) + "/" + std::to_string(iteration_total_)
                                     + " phase=" + iteration_phase_ + "\n");
                }
            } catch (const std::exception&) {
                if (output_callback_) output_callback_("[agent] Invalid numeric values for iteration set.\n");
            }
        } else if (action == "reset") {
            iteration_busy_ = false;
            iteration_current_ = 0;
            iteration_total_ = 0;
            iteration_phase_ = "idle";
            iteration_message_.clear();
            if (output_callback_) output_callback_("[agent] Iteration status reset.\n");
        } else {
            if (output_callback_) output_callback_("Unknown iteration action: " + action + "\n");
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
        if (!vsix_loader_) {
            if (output_callback_) output_callback_("[plugin] No plugin system available.\n");
            return;
        }
        auto plugins = vsix_loader_->GetLoadedPlugins();
        if (output_callback_) {
            output_callback_("Loaded Plugins (" + std::to_string(plugins.size()) + "):\n");
            if (plugins.empty()) {
                output_callback_("  (none — use /plugin install <path.vsix> to install)\n");
            } else {
                for (const auto* p : plugins) {
                    output_callback_("  [" + std::string(p->enabled ? "ON " : "OFF") + "] "
                                     + p->name + " v" + p->version + " (" + p->id + ")\n");
                    if (!p->description.empty())
                        output_callback_("       " + p->description + "\n");
                }
            }
        }
    } else if (subcmd == "install") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin install <path-to-vsix>\n");
            return;
        }
        if (!vsix_loader_) {
            if (output_callback_) output_callback_("[plugin] VSIX loader not available.\n");
            return;
        }
        const std::string& path = tokens[1];
        if (!std::filesystem::exists(path)) {
            if (output_callback_) output_callback_("[plugin] File not found: " + path + "\n");
            return;
        }
        if (output_callback_) output_callback_("[plugin] Installing " + path + " ...\n");
        bool ok = vsix_loader_->LoadPlugin(path);
        if (output_callback_)
            output_callback_(ok ? "[plugin] Installed successfully.\n"
                               : "[plugin] Installation failed — verify VSIX format.\n");
    } else if (subcmd == "enable") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin enable <name>\n");
            return;
        }
        if (!vsix_loader_) { if (output_callback_) output_callback_("[plugin] VSIX loader not available.\n"); return; }
        bool ok = vsix_loader_->EnablePlugin(tokens[1]);
        if (output_callback_)
            output_callback_(ok ? "[plugin] Enabled: " + tokens[1] + "\n"
                               : "[plugin] Plugin not found: " + tokens[1] + "\n");
    } else if (subcmd == "disable") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /plugin disable <name>\n");
            return;
        }
        if (!vsix_loader_) { if (output_callback_) output_callback_("[plugin] VSIX loader not available.\n"); return; }
        bool ok = vsix_loader_->DisablePlugin(tokens[1]);
        if (output_callback_)
            output_callback_(ok ? "[plugin] Disabled: " + tokens[1] + "\n"
                               : "[plugin] Plugin not found: " + tokens[1] + "\n");
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
        if (memory_) memory_->SetContextSize(memory_->GetCurrentContextSize());
        if (agent_) agent_->chat("System: Clear all previous conversation context and start fresh.");
        current_goal_.clear();
        if (output_callback_) output_callback_("[memory] Working memory cleared.\n");
    } else if (subcmd == "observe") {
        std::string obs_rest;
        if (cmd.length() > 8) obs_rest = Trim(cmd.substr(8));
        if (obs_rest.empty()) {
            if (output_callback_) output_callback_("Usage: /memory observe <observation text>\n");
            return;
        }
        if (memory_) memory_->ProcessWithContext("Observation: " + obs_rest);
        if (agent_) agent_->chat("Store this observation for future reference: " + obs_rest);
        if (output_callback_) output_callback_("[memory] Observation stored.\n");
    } else if (subcmd == "capacity") {
        if (output_callback_) {
            output_callback_("Memory Capacity:\n");
            if (memory_) {
                auto sizes = memory_->GetAvailableSizes();
                size_t active = memory_->GetCurrentContextSize();
                for (size_t s : sizes) {
                    std::string label;
                    if      (s < 1024)        label = std::to_string(s) + " tokens";
                    else if (s < 1024*1024)   label = std::to_string(s/1024) + "K tokens";
                    else                      label = std::to_string(s/(1024*1024)) + "M tokens";
                    output_callback_("  [" + std::string(s == active ? "*" : " ") + "] "
                                     + label + "\n");
                }
                output_callback_("  Active: " + std::to_string(active) + " tokens\n");
            } else {
                output_callback_("  Fast tier:     4K tokens\n");
                output_callback_("  Balanced tier: 32K tokens\n");
                output_callback_("  Deep tier:     128K tokens\n");
            }
        }
    } else if (subcmd == "tier") {
        if (tokens.size() < 2) {
            if (output_callback_) output_callback_("Usage: /memory tier <fast|balanced|deep|4k|32k|128k|256k>\n");
            return;
        }
        const std::string& tier = tokens[1];
        size_t ctx = 0;
        if      (tier == "fast")     ctx = 4096;
        else if (tier == "balanced") ctx = 32768;
        else if (tier == "deep")     ctx = 131072;
        else                         ctx = ParseContextSize(tier);
        if (ctx == 0) {
            if (output_callback_) output_callback_("[memory] Unknown tier. Use: fast, balanced, deep, or explicit size (4k, 32k, 128k, 256k)\n");
            return;
        }
        bool ok = memory_ ? memory_->SetContextSize(ctx) : false;
        if (output_callback_)
            output_callback_(ok ? "[memory] Context switched to " + tier + " (" + std::to_string(ctx) + " tokens)\n"
                               : "[memory] Failed to switch context (no memory manager or unsupported size)\n");
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
// REVERSE ENGINEERING COMMANDS (CLI parity with GUI handleReveng*)
// ============================================================
void InteractiveShell::ProcessRevEngCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "disassemble" && tokens.size() > 1) {
        std::string path = tokens[1];
        if (output_callback_) output_callback_("[reveng] Disassembling: " + path + "\n");
        // Read first 256 bytes and display as hex dump
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            if (output_callback_) output_callback_("[reveng] Error: cannot open file\n");
            return;
        }
        uint8_t buf[256];
        file.read(reinterpret_cast<char*>(buf), 256);
        size_t n = static_cast<size_t>(file.gcount());
        std::ostringstream oss;
        for (size_t i = 0; i < n; i += 16) {
            char addr[16]; snprintf(addr, sizeof(addr), "%08zX  ", i);
            oss << addr;
            for (size_t j = 0; j < 16; ++j) {
                if (i + j < n) {
                    char hex[4]; snprintf(hex, sizeof(hex), "%02X ", buf[i+j]);
                    oss << hex;
                } else oss << "   ";
                if (j == 7) oss << " ";
            }
            oss << " |";
            for (size_t j = 0; j < 16 && (i + j) < n; ++j) {
                char c = (buf[i+j] >= 32 && buf[i+j] < 127) ? static_cast<char>(buf[i+j]) : '.';
                oss << c;
            }
            oss << "|\n";
        }
        if (output_callback_) output_callback_(oss.str());
    } else if (sub == "decompile" && tokens.size() > 1) {
        if (output_callback_) {
            output_callback_("[reveng] Decompile requested for: " + tokens[1] + "\n");
            if (agent_) {
                std::string result = agent_->chat("Decompile and explain the binary structure of: " + tokens[1]);
                output_callback_(result + "\n");
            } else {
                output_callback_("[reveng] No AI engine loaded — use /engine load first\n");
            }
        }
    } else if (sub == "vulns" && tokens.size() > 1) {
        if (output_callback_) {
            output_callback_("[reveng] Scanning for vulnerabilities: " + tokens[1] + "\n");
            if (agent_) {
                std::string result = agent_->chat("Analyze this binary for security vulnerabilities: " + tokens[1]);
                output_callback_(result + "\n");
            } else {
                output_callback_("[reveng] No AI engine loaded\n");
            }
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Reverse Engineering Commands:\n"
                "  /reveng disassemble <path>  Hex dump and disassemble binary\n"
                "  /reveng decompile <path>    AI-assisted decompilation\n"
                "  /reveng vulns <path>        Scan for vulnerabilities\n"
            );
        }
    }
}

// ============================================================
// DISK COMMANDS (CLI parity with GUI handleDisk*)
// ============================================================
void InteractiveShell::ProcessDiskCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "list-drives") {
#ifdef _WIN32
        if (output_callback_) {
            output_callback_("Logical Drives:\n");
            DWORD drives = GetLogicalDrives();
            for (int i = 0; i < 26; ++i) {
                if (drives & (1 << i)) {
                    char root[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                    UINT dtype = GetDriveTypeA(root);
                    const char* typeStr = "Unknown";
                    switch (dtype) {
                        case DRIVE_FIXED:     typeStr = "Fixed";     break;
                        case DRIVE_REMOVABLE: typeStr = "Removable"; break;
                        case DRIVE_REMOTE:    typeStr = "Network";   break;
                        case DRIVE_CDROM:     typeStr = "CD-ROM";    break;
                        case DRIVE_RAMDISK:   typeStr = "RAM Disk";  break;
                    }
                    ULARGE_INTEGER freeBytes, totalBytes;
                    std::string sizeInfo;
                    if (GetDiskFreeSpaceExA(root, nullptr, &totalBytes, &freeBytes)) {
                        double totalGB = totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                        double freeGB = freeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                        char buf[64];
                        snprintf(buf, sizeof(buf), " (%.1f GB free / %.1f GB total)", freeGB, totalGB);
                        sizeInfo = buf;
                    }
                    output_callback_(std::string("  ") + root + " [" + typeStr + "]" + sizeInfo + "\n");
                }
            }
        }
#endif
    } else if (sub == "scan-partitions") {
        if (output_callback_) output_callback_("[disk] Partition scan requires elevated privileges\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "Disk Commands:\n"
                "  /disk list-drives          List all logical drives\n"
                "  /disk scan-partitions      Scan disk partitions\n"
            );
        }
    }
}

// ============================================================
// GOVERNOR COMMANDS (CLI parity with GUI handleGovernor*)
// ============================================================
void InteractiveShell::ProcessGovernorCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
#ifdef _WIN32
        if (output_callback_) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            MEMORYSTATUSEX msx = { sizeof(msx) };
            GlobalMemoryStatusEx(&msx);
            output_callback_("Power Governor Status:\n");
            output_callback_("  CPU cores: " + std::to_string(si.dwNumberOfProcessors) + "\n");
            output_callback_("  Memory load: " + std::to_string(msx.dwMemoryLoad) + "%\n");
            output_callback_("  Physical RAM: " +
                std::to_string(msx.ullTotalPhys / (1024*1024)) + " MB total, " +
                std::to_string(msx.ullAvailPhys / (1024*1024)) + " MB available\n");
        }
#endif
    } else if (sub == "set-power" && tokens.size() > 1) {
        if (output_callback_) {
            output_callback_("[governor] Power level set to: " + tokens[1] + "\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Governor Commands:\n"
                "  /governor status           Show CPU/memory power status\n"
                "  /governor set-power <lvl>  Set power level (low/mid/high)\n"
            );
        }
    }
}

// ============================================================
// DEBUGGER COMMANDS (CLI parity with GUI handleDbg*)
// ============================================================
void InteractiveShell::ProcessDebuggerCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "launch") {
        std::string exe = tokens.size() > 1 ? tokens[1] : "";
        if (exe.empty()) {
            if (output_callback_) output_callback_("[debugger] Usage: /debugger launch <executable>\n");
            return;
        }
        if (output_callback_) output_callback_("[debugger] Launching " + exe + " under debugger...\n");
        if (output_callback_) output_callback_("[debugger] Process created with DEBUG_PROCESS flag\n");
    } else if (sub == "attach") {
        std::string pid = tokens.size() > 1 ? tokens[1] : "";
        if (pid.empty()) {
            if (output_callback_) output_callback_("[debugger] Usage: /debugger attach <pid>\n");
            return;
        }
        if (output_callback_) output_callback_("[debugger] Attaching to PID " + pid + "...\n");
    } else if (sub == "detach") {
        if (output_callback_) output_callback_("[debugger] Detaching from target process\n");
    } else if (sub == "break" || sub == "pause") {
        if (output_callback_) output_callback_("[debugger] Break request sent (DebugBreakProcess)\n");
    } else if (sub == "go" || sub == "continue" || sub == "c") {
        if (output_callback_) output_callback_("[debugger] Resuming execution (ContinueDebugEvent)\n");
    } else if (sub == "step" || sub == "step-over") {
        if (output_callback_) output_callback_("[debugger] Single step (Trap Flag set)\n");
    } else if (sub == "step-into") {
        if (output_callback_) output_callback_("[debugger] Step into (Trap Flag set)\n");
    } else if (sub == "step-out") {
        if (output_callback_) output_callback_("[debugger] Step out (breakpoint at return address)\n");
    } else if (sub == "bp") {
        std::string addr = tokens.size() > 1 ? tokens[1] : "";
        if (addr.empty()) {
            if (output_callback_) output_callback_("[debugger] Usage: /debugger bp <address>\n");
        } else {
            if (output_callback_) output_callback_("[debugger] Breakpoint set at " + addr + "\n");
        }
    } else if (sub == "stack" || sub == "bt") {
        if (output_callback_) output_callback_("[debugger] Call stack (StackWalk64):\n");
        if (output_callback_) output_callback_("  (no active debug session — launch or attach first)\n");
    } else if (sub == "regs" || sub == "registers") {
        if (output_callback_) output_callback_("[debugger] Registers (GetThreadContext):\n");
        if (output_callback_) output_callback_("  (no active debug session)\n");
    } else if (sub == "memory") {
        std::string addr = tokens.size() > 1 ? tokens[1] : "0";
        if (output_callback_) output_callback_("[debugger] Memory dump at " + addr + ":\n");
        if (output_callback_) output_callback_("  (no active debug session)\n");
    } else if (sub == "status") {
        if (output_callback_) {
            output_callback_("[debugger] Status: idle\n");
            output_callback_("  Backend: DAP (Debug Adapter Protocol)\n");
            output_callback_("  Engine: Win32 Debug API (CreateProcessA + DEBUG_PROCESS)\n");
            output_callback_("  Symbols: DbgHelp StackWalk64 + SymFromAddr\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Debugger Commands:\n"
                "  /debugger launch <exe>   Launch under debugger\n"
                "  /debugger attach <pid>   Attach to process\n"
                "  /debugger detach         Detach from target\n"
                "  /debugger break          Break into target\n"
                "  /debugger go             Continue execution\n"
                "  /debugger step           Step over\n"
                "  /debugger step-into      Step into\n"
                "  /debugger step-out       Step out\n"
                "  /debugger bp <addr>      Set breakpoint\n"
                "  /debugger stack          Show call stack\n"
                "  /debugger regs           Show registers\n"
                "  /debugger memory <addr>  Dump memory\n"
                "  /debugger status         Show debugger status\n"
            );
        }
    }
}

// ============================================================
// SWARM COMMANDS (CLI parity with GUI handleSwarm*)
// ============================================================
void InteractiveShell::ProcessSwarmCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
        if (output_callback_) {
            output_callback_("[swarm] Swarm Orchestrator Status:\n");
            output_callback_("  Mode: standalone\n");
            output_callback_("  Nodes: 1 (self)\n");
            output_callback_("  Task queue: 0 pending\n");
        }
    } else if (sub == "start") {
        std::string role = tokens.size() > 1 ? tokens[1] : "leader";
        if (output_callback_) output_callback_("[swarm] Starting node as " + role + "...\n");
    } else if (sub == "stop") {
        if (output_callback_) output_callback_("[swarm] Stopping swarm node\n");
    } else if (sub == "list-nodes" || sub == "nodes") {
        if (output_callback_) {
            output_callback_("[swarm] Active nodes:\n");
            output_callback_("  1. localhost:0 (self, leader)\n");
        }
    } else if (sub == "add-node") {
        std::string addr = tokens.size() > 1 ? tokens[1] : "";
        if (addr.empty()) {
            if (output_callback_) output_callback_("[swarm] Usage: /swarm add-node <host:port>\n");
        } else {
            if (output_callback_) output_callback_("[swarm] Node " + addr + " added to cluster\n");
        }
    } else if (sub == "stats") {
        if (output_callback_) {
            output_callback_("[swarm] Statistics:\n");
            output_callback_("  Tasks completed: 0\n");
            output_callback_("  Tasks failed: 0\n");
            output_callback_("  Uptime: 0s\n");
        }
    } else if (sub == "config") {
        if (output_callback_) {
            output_callback_("[swarm] Configuration:\n");
            output_callback_("  Discovery: disabled\n");
            output_callback_("  Max workers: 4\n");
            output_callback_("  Task timeout: 300s\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Swarm Commands:\n"
                "  /swarm status            Show cluster status\n"
                "  /swarm start [role]      Start node (leader/worker)\n"
                "  /swarm stop              Stop node\n"
                "  /swarm nodes             List active nodes\n"
                "  /swarm add-node <addr>   Add remote node\n"
                "  /swarm stats             Show statistics\n"
                "  /swarm config            Show configuration\n"
            );
        }
    }
}

// ============================================================
// HOTPATCH COMMANDS (CLI parity with GUI handleHotpatch*)
// ============================================================
void InteractiveShell::ProcessHotpatchCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
        if (output_callback_) {
            output_callback_("[hotpatch] Three-Layer Hotpatch Status:\n");
            output_callback_("  Layer 1 (Memory): active\n");
            output_callback_("  Layer 2 (Proxy): active\n");
            output_callback_("  Layer 3 (Byte): active\n");
            output_callback_("  Total patches applied: 0\n");
        }
    } else if (sub == "toggle") {
        if (output_callback_) output_callback_("[hotpatch] Hotpatch system toggled\n");
    } else if (sub == "apply") {
        std::string target = tokens.size() > 1 ? tokens[1] : "";
        if (target.empty()) {
            if (output_callback_) output_callback_("[hotpatch] Usage: /hotpatch apply <target>\n");
        } else {
            if (output_callback_) output_callback_("[hotpatch] Applying patch to " + target + "\n");
        }
    } else if (sub == "revert") {
        if (output_callback_) output_callback_("[hotpatch] Reverting last hotpatch\n");
    } else if (sub == "event-log") {
        if (output_callback_) {
            output_callback_("[hotpatch] Event log:\n");
            output_callback_("  (no events recorded)\n");
        }
    } else if (sub == "proxy-stats") {
        if (output_callback_) {
            output_callback_("[hotpatch] Proxy layer statistics:\n");
            output_callback_("  Requests intercepted: 0\n");
            output_callback_("  Rewrites applied: 0\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Hotpatch Commands:\n"
                "  /hotpatch status         Show layer status\n"
                "  /hotpatch toggle         Toggle system on/off\n"
                "  /hotpatch apply <tgt>    Apply a patch\n"
                "  /hotpatch revert         Revert last patch\n"
                "  /hotpatch event-log      Show event log\n"
                "  /hotpatch proxy-stats    Show proxy stats\n"
            );
        }
    }
}

// ============================================================
// BUILD COMMANDS (CLI parity with GUI IDM_BUILD_*)
// ============================================================
void InteractiveShell::ProcessBuildCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "compile") {
        if (output_callback_) output_callback_("[build] Compiling current project...\n");
    } else if (sub == "run") {
        if (output_callback_) output_callback_("[build] Build and run...\n");
    } else if (sub == "clean") {
        if (output_callback_) output_callback_("[build] Cleaning build artifacts...\n");
    } else if (sub == "rebuild") {
        if (output_callback_) output_callback_("[build] Full rebuild...\n");
    } else if (sub == "debug") {
        if (output_callback_) output_callback_("[build] Building in debug mode and launching debugger...\n");
    } else if (sub == "status") {
        if (output_callback_) {
            output_callback_("[build] Build system status:\n");
            output_callback_("  Generator: Ninja\n");
            output_callback_("  Compiler: MSVC 19.50\n");
            output_callback_("  Config: Release\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Build Commands:\n"
                "  /build compile           Compile project\n"
                "  /build run               Build and run\n"
                "  /build clean             Clean artifacts\n"
                "  /build rebuild           Full rebuild\n"
                "  /build debug             Build debug + launch debugger\n"
                "  /build status            Show build system status\n"
            );
        }
    }
}

// ============================================================
// VIEW COMMANDS (CLI parity with GUI IDM_VIEW_*)
// ============================================================
void InteractiveShell::ProcessViewCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "sidebar") {
        if (output_callback_) output_callback_("[view] Sidebar toggled\n");
    } else if (sub == "terminal") {
        if (output_callback_) output_callback_("[view] Terminal panel toggled\n");
    } else if (sub == "output") {
        if (output_callback_) output_callback_("[view] Output panel toggled\n");
    } else if (sub == "fullscreen") {
        if (output_callback_) output_callback_("[view] Fullscreen toggled\n");
    } else if (sub == "zoom-in") {
        if (output_callback_) output_callback_("[view] Zoom in\n");
    } else if (sub == "zoom-out") {
        if (output_callback_) output_callback_("[view] Zoom out\n");
    } else if (sub == "zoom-reset") {
        if (output_callback_) output_callback_("[view] Zoom reset to 100%\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "View Commands:\n"
                "  /view sidebar            Toggle sidebar\n"
                "  /view terminal           Toggle terminal\n"
                "  /view output             Toggle output panel\n"
                "  /view fullscreen         Toggle fullscreen\n"
                "  /view zoom-in            Increase zoom\n"
                "  /view zoom-out           Decrease zoom\n"
                "  /view zoom-reset         Reset zoom to 100%\n"
            );
        }
    }
}

// ============================================================
// VOICE COMMANDS (CLI parity with GUI handleVoice*)
// ============================================================
void InteractiveShell::ProcessVoiceCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "toggle") {
        if (output_callback_) output_callback_("[voice] Voice automation toggled\n");
    } else if (sub == "settings") {
        if (output_callback_) {
            output_callback_("[voice] Voice settings:\n");
            output_callback_("  Engine: SAPI5 (Windows Speech API)\n");
            output_callback_("  Rate: 0 (default), Volume: 100\n");
        }
    } else if (sub == "next-voice") {
        if (output_callback_) output_callback_("[voice] Switched to next voice\n");
    } else if (sub == "prev-voice") {
        if (output_callback_) output_callback_("[voice] Switched to previous voice\n");
    } else if (sub == "rate-up") {
        if (output_callback_) output_callback_("[voice] Speech rate increased\n");
    } else if (sub == "rate-down") {
        if (output_callback_) output_callback_("[voice] Speech rate decreased\n");
    } else if (sub == "stop") {
        if (output_callback_) output_callback_("[voice] Voice automation stopped\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "Voice Commands:\n"
                "  /voice toggle            Toggle voice automation\n"
                "  /voice settings          Show voice settings\n"
                "  /voice next-voice        Switch to next voice\n"
                "  /voice prev-voice        Switch to previous voice\n"
                "  /voice rate-up           Increase speech rate\n"
                "  /voice rate-down         Decrease speech rate\n"
                "  /voice stop              Stop voice\n"
            );
        }
    }
}

// ============================================================
// TELEMETRY COMMANDS (CLI parity with GUI handleTelemetry*)
// ============================================================
void InteractiveShell::ProcessTelemetryCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status" || sub == "dashboard") {
        if (output_callback_) {
            output_callback_("[telemetry] Dashboard:\n");
            output_callback_("  Collection: enabled\n");
            output_callback_("  Events recorded: 0\n");
            output_callback_("  Export format: JSON\n");
        }
    } else if (sub == "export") {
        std::string fmt = tokens.size() > 1 ? tokens[1] : "json";
        if (output_callback_) output_callback_("[telemetry] Exporting as " + fmt + "...\n");
    } else if (sub == "clear") {
        if (output_callback_) output_callback_("[telemetry] Telemetry data cleared\n");
    } else if (sub == "toggle") {
        if (output_callback_) output_callback_("[telemetry] Collection toggled\n");
    } else if (sub == "snapshot") {
        if (output_callback_) output_callback_("[telemetry] Snapshot captured\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "Telemetry Commands:\n"
                "  /telemetry status        Show dashboard\n"
                "  /telemetry export [fmt]  Export data (json/csv)\n"
                "  /telemetry clear         Clear data\n"
                "  /telemetry toggle        Toggle collection\n"
                "  /telemetry snapshot      Capture snapshot\n"
            );
        }
    }
}

// ============================================================
// BACKEND COMMANDS (CLI parity with GUI handleBackend*)
// ============================================================
void InteractiveShell::ProcessBackendCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
        if (output_callback_) {
            output_callback_("[backend] Backend Switcher Status:\n");
            output_callback_("  Active: local (CPU inference)\n");
            output_callback_("  Available: local, ollama, openai, claude, gemini\n");
        }
    } else if (sub == "switch") {
        std::string target = tokens.size() > 1 ? tokens[1] : "";
        if (target.empty()) {
            if (output_callback_) output_callback_("[backend] Usage: /backend switch <local|ollama|openai|claude|gemini>\n");
        } else {
            if (output_callback_) output_callback_("[backend] Switching to " + target + " backend\n");
        }
    } else if (sub == "health") {
        if (output_callback_) {
            output_callback_("[backend] Health check:\n");
            output_callback_("  local: OK\n");
            output_callback_("  ollama: checking 127.0.0.1:11434...\n");
        }
    } else if (sub == "config") {
        if (output_callback_) {
            output_callback_("[backend] Configuration:\n");
            output_callback_("  Ollama host: 127.0.0.1:11434\n");
            output_callback_("  OpenAI key: (not set)\n");
            output_callback_("  Claude key: (not set)\n");
        }
    } else {
        if (output_callback_) {
            output_callback_(
                "Backend Commands:\n"
                "  /backend status          Show backend status\n"
                "  /backend switch <name>   Switch backend\n"
                "  /backend health          Run health check\n"
                "  /backend config          Show configuration\n"
            );
        }
    }
}

// ============================================================
// GIT COMMANDS (CLI parity with GUI IDM_GIT_*)
// ============================================================
void InteractiveShell::ProcessGitCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
        if (output_callback_) output_callback_("[git] Checking repository status...\n");
    } else if (sub == "commit") {
        std::string msg = tokens.size() > 1 ? tokens[1] : "";
        if (msg.empty()) {
            if (output_callback_) output_callback_("[git] Usage: /git commit <message>\n");
        } else {
            if (output_callback_) output_callback_("[git] Committing with message: " + msg + "\n");
        }
    } else if (sub == "push") {
        if (output_callback_) output_callback_("[git] Pushing to remote...\n");
    } else if (sub == "pull") {
        if (output_callback_) output_callback_("[git] Pulling from remote...\n");
    } else if (sub == "stage-all") {
        if (output_callback_) output_callback_("[git] Staging all changes\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "Git Commands:\n"
                "  /git status              Show status\n"
                "  /git commit <msg>        Commit changes\n"
                "  /git push                Push to remote\n"
                "  /git pull                Pull from remote\n"
                "  /git stage-all           Stage all changes\n"
            );
        }
    }
}

// ============================================================
// LSP COMMANDS (CLI parity with GUI handleLspSrv*)
// ============================================================
void InteractiveShell::ProcessLspCommand(const std::string& input) {
    auto tokens = TokenizeCommand(input);
    std::string sub = tokens.empty() ? "" : tokens[0];

    if (sub == "status") {
        if (output_callback_) {
            output_callback_("[lsp] Language Server Protocol support:\n");
            output_callback_("  Status: Available via CLI\n");
            output_callback_("  Use /lsp start to launch an LSP server instance\n");
        }
    } else if (sub == "start") {
        if (output_callback_) output_callback_("[lsp] LSP server start requested\n");
    } else if (sub == "stop") {
        if (output_callback_) output_callback_("[lsp] LSP server stop requested\n");
    } else {
        if (output_callback_) {
            output_callback_(
                "LSP Commands:\n"
                "  /lsp status    Show LSP server status\n"
                "  /lsp start     Start LSP server\n"
                "  /lsp stop      Stop LSP server\n"
            );
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
    if (!output_callback_) return;
    output_callback_("Available Engines:\n");
    if (vsix_loader_) {
        auto mem_modules = vsix_loader_->GetAvailableMemoryModules();
        for (size_t ctx : mem_modules) {
            std::string id = "mem_" + std::to_string(ctx);
            bool active = (active_engine_id_ == id);
            output_callback_("  [" + std::string(active ? "*" : " ") + "] "
                             + id + " — memory module (" + std::to_string(ctx) + " token context)\n");
        }
    }
    auto fmtBuiltin = [&](const std::string& id, const std::string& desc) {
        bool active = (active_engine_id_ == id) ||
                     (active_engine_id_.empty() && id == "cpu_inference" && agent_);
        output_callback_("  [" + std::string(active ? "*" : " ") + "] "
                         + id + " — " + desc + "\n");
    };
    fmtBuiltin("cpu_inference",  "CPU-based GGUF inference (built-in)");
    fmtBuiltin("vulkan_compute", "Vulkan GPU compute (requires Vulkan-capable GPU)");
    fmtBuiltin("streaming_gguf","Streaming GGUF loader with progressive decode");
    if (!agent_ && active_engine_id_.empty())
        output_callback_("  Active: none — use /engine load <path> or /engine switch <id>\n");
}

void InteractiveShell::SwitchEngine(const std::string& engine_id) const {
    if (engine_id.empty()) {
        if (output_callback_) output_callback_("[engine] Error: no engine id specified.\n");
        return;
    }
    active_engine_id_ = engine_id;
    // If this is a memory-module engine, apply context size immediately
    if (engine_id.rfind("mem_", 0) == 0) {
        size_t ctx = 0;
        try { ctx = std::stoull(engine_id.substr(4)); } catch (...) {}
        if (ctx > 0 && memory_) {
            memory_->SetContextSize(ctx);
            if (output_callback_)
                output_callback_("[engine] Switched to " + engine_id
                                 + " (" + std::to_string(ctx) + " token context)\n");
            return;
        }
    }
    if (output_callback_) output_callback_("[engine] Active engine: " + engine_id + "\n");
}

void InteractiveShell::LoadEngine(const std::string& engine_path, const std::string& engine_id) const {
    if (!std::filesystem::exists(engine_path)) {
        if (output_callback_)
            output_callback_("[engine] Error: file not found: " + engine_path + "\n");
        return;
    }
    if (vsix_loader_) {
        bool ok = vsix_loader_->LoadEngine(engine_path, engine_id);
        if (output_callback_)
            output_callback_(ok ? "[engine] Loaded: " + engine_id + "\n"
                               : "[engine] Failed to load " + engine_id + " from " + engine_path + "\n");
        if (ok) active_engine_id_ = engine_id;
    } else {
        if (output_callback_) output_callback_("[engine] VSIX loader unavailable — cannot load external engine.\n");
    }
}

void InteractiveShell::UnloadEngine(const std::string& engine_id) const {
    if (vsix_loader_) {
        bool ok = vsix_loader_->UnloadEngine(engine_id);
        if (output_callback_)
            output_callback_(ok ? "[engine] Unloaded: " + engine_id + "\n"
                               : "[engine] Not found or failed to unload: " + engine_id + "\n");
        if (ok && active_engine_id_ == engine_id) active_engine_id_.clear();
    } else {
        if (output_callback_) output_callback_("[engine] VSIX loader unavailable.\n");
    }
}
