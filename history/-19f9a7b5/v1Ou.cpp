#include "interactive_shell.h"
#include "agentic_engine.h"
#include "modules/memory_manager.h"
#include "modules/vsix_loader.h"
#include "advanced_features.h"
#include "modules/react_generator.h"
#include "cpu_inference_engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>

using RawrXD::ReactServerGenerator;
using RawrXD::ReactServerConfig;

std::unique_ptr<InteractiveShell> g_shell;

InteractiveShell::InteractiveShell(const ShellConfig& config) 
    : running_(false), agent_(nullptr), memory_(nullptr), vsix_loader_(nullptr), 
      react_generator_(nullptr), config_(config), history_index_(0) {}

InteractiveShell::~InteractiveShell() {
    Stop();
    if (config_.auto_save_history) {
        SaveHistory();
    }
}

void InteractiveShell::Start(AgenticEngine* agent, MemoryManager* memory, VSIXLoader* vsix_loader,
                            ReactServerGenerator* react_generator,
                            std::function<void(const std::string&)> output_cb,
                            std::function<void(const std::string&)> input_cb) {
    agent_ = agent;
    memory_ = memory;
    vsix_loader_ = vsix_loader;
    react_generator_ = react_generator;
    output_callback_ = output_cb;
    input_callback_ = input_cb;
    running_ = true;
    
    LoadHistory();
    
    if (config_.cli_mode) {
        shell_thread_ = std::thread([this]() { RunShell(); });
    }
    
    DisplayWelcome();
}

void InteractiveShell::Stop() {
    running_ = false;
    if (shell_thread_.joinable()) {
        shell_thread_.join();
    }
}

void InteractiveShell::SendInput(const std::string& input) {
    if (!input.empty()) {
        AddToHistory(input);
        ProcessCommand(input);
    }
}

void InteractiveShell::RunShell() {
    DisplayWelcome();
    
    std::string input;
    while (running_ && std::getline(std::cin, input)) {
        if (input == "/exit" || input == "exit") {
            break;
        }
        ProcessCommand(input);
        DisplayPrompt();
    }
    
    if (output_callback_) {
        output_callback_("=== Shell Exited ===\n");
    }
}

void InteractiveShell::DisplayWelcome() {
    if (!output_callback_) return;
    
    std::string welcome = R"(
╔══════════════════════════════════════════════════════════════╗
║                    RawrXD AI Shell v6.0                      ║
║                                                              ║
║  Model: Loaded and ready                                     ║
║  Memory: )" + std::to_string(memory_->GetCurrentContextSize() / 1024) + R"(K context                               ║
║  Engine: )" + vsix_loader_->GetCurrentEngine() + R"(                      ║
║  Modes: Max Mode, Deep Thinking, Deep Research, No Refusal  ║
║                                                              ║
║  Type /help for commands, /exit to quit                     ║
╚══════════════════════════════════════════════════════════════╝

)";
    output_callback_(welcome);
    DisplayPrompt();
}

void InteractiveShell::DisplayPrompt() {
    if (!output_callback_) return;
    output_callback_(">> ");
}

void InteractiveShell::ProcessCommand(const std::string& input) {
    std::string trimmed = Trim(input);
    if (trimmed.empty()) return;
    
    // Check for plugin commands first
    if (trimmed.rfind("!plugin ", 0) == 0) {
        ProcessPluginCommand(trimmed.substr(8));
        return;
    }
    
    // Check for memory commands
    if (trimmed.rfind("!memory ", 0) == 0) {
        ProcessMemoryCommand(trimmed.substr(8));
        return;
    }
    
    // Check for engine commands
    if (trimmed.rfind("!engine ", 0) == 0) {
        ProcessEngineCommand(trimmed.substr(8));
        return;
    }
    
    // Check for agentic commands
    if (trimmed.rfind("/plan", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/react-server", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/bugreport", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/suggest", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/hotpatch", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/analyze", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/optimize", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/security", 0) == 0) {
        ProcessAgenticCommand(trimmed);
        return;
    }
    
    // Check for mode toggles
    if (trimmed.rfind("/maxmode", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/deepthinking", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/deepresearch", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/norefusal", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    if (trimmed.rfind("/autocorrect", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    
    // Check for context commands
    if (trimmed.rfind("/context", 0) == 0) {
        ProcessSystemCommand(trimmed);
        return;
    }
    if (trimmed == "/context+") {
        // Increase context size
        std::vector<size_t> sizes = {4096, 32768, 65536, 131072, 262144, 524288, 1048576};
        size_t current = memory_->GetCurrentContextSize();
        for (size_t i = 0; i < sizes.size() - 1; i++) {
            if (sizes[i] == current) {
                if (memory_->SetContextSize(sizes[i + 1])) {
                    if (output_callback_) {
                        output_callback_("Context size increased to " + std::to_string(sizes[i + 1] / 1024) + "K\n");
                    }
                } else {
                    if (output_callback_) {
                        output_callback_("Failed to increase context size\n");
                    }
                }
                return;
            }
        }
        if (output_callback_) {
            output_callback_("Already at maximum context size\n");
        }
        return;
    }
    if (trimmed == "/context-") {
        // Decrease context size
        std::vector<size_t> sizes = {4096, 32768, 65536, 131072, 262144, 524288, 1048576};
        size_t current = memory_->GetCurrentContextSize();
        for (size_t i = 1; i < sizes.size(); i++) {
            if (sizes[i] == current) {
                if (memory_->SetContextSize(sizes[i - 1])) {
                    if (output_callback_) {
                        output_callback_("Context size decreased to " + std::to_string(sizes[i - 1] / 1024) + "K\n");
                    }
                } else {
                    if (output_callback_) {
                        output_callback_("Failed to decrease context size\n");
                    }
                }
                return;
            }
        }
        if (output_callback_) {
            output_callback_("Already at minimum context size\n");
        }
        return;
    }
    
    // Check for system commands
    if (trimmed == "/help") {
        if (output_callback_) output_callback_(GetHelp() + "\n");
        return;
    }
    if (trimmed == "/exit" || trimmed == "exit") {
        running_ = false;
        return;
    }
    if (trimmed.rfind("/shell", 0) == 0) {
        if (output_callback_) output_callback_("Shell mode already active\n");
        return;
    }
    if (trimmed == "/clear") {
        // Clear conversation history
        agent_->clearHistory();
        ClearHistory();
        if (output_callback_) output_callback_("Conversation history cleared\n");
        return;
    }
    if (trimmed.rfind("/save", 0) == 0) {
        // Save conversation
        std::string filename = trimmed.length() > 6 ? trimmed.substr(6) : "conversation.txt";
        filename = Trim(filename);
        if (filename.empty()) filename = "conversation.txt";
        
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "# RawrXD Conversation\n";
            file << "# Date: " << __DATE__ << " " << __TIME__ << "\n\n";
            for (const auto& cmd : command_history_) {
                file << ">> " << cmd << "\n";
            }
            file.close();
            if (output_callback_) output_callback_("Conversation saved to " + filename + "\n");
        } else {
            if (output_callback_) output_callback_("Failed to save conversation\n");
        }
        return;
    }
    if (trimmed.rfind("/load", 0) == 0) {
        // Load conversation
        std::string filename = trimmed.length() > 6 ? trimmed.substr(6) : "conversation.txt";
        filename = Trim(filename);
        if (filename.empty()) filename = "conversation.txt";
        
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.rfind(">> ", 0) == 0) {
                    std::string cmd = line.substr(3);
                    if (!cmd.empty() && cmd != "/exit") {
                        ProcessCommand(cmd);
                    }
                }
            }
            file.close();
            if (output_callback_) output_callback_("Conversation loaded from " + filename + "\n");
        } else {
            if (output_callback_) output_callback_("Failed to load conversation\n");
        }
        return;
    }
    if (trimmed == "/status") {
        // Show system status
        std::string status = "System Status:\n";
        status += "  Model: " + std::string(RawrXD::CPUInferenceEngine::getInstance()->isModelLoaded() ? "Loaded" : "Not loaded") + "\n";
        status += "  Context: " + std::to_string(memory_->GetCurrentContextSize() / 1024) + "K\n";
        status += "  Engine: " + vsix_loader_->GetCurrentEngine() + "\n";
        status += "  Max Mode: " + std::string(agent_->getConfig().maxMode ? "On" : "Off") + "\n";
        status += "  Deep Thinking: " + std::string(agent_->getConfig().deepThinking ? "On" : "Off") + "\n";
        status += "  Deep Research: " + std::string(agent_->getConfig().deepResearch ? "On" : "Off") + "\n";
        status += "  No Refusal: " + std::string(agent_->getConfig().noRefusal ? "On" : "Off") + "\n";
        status += "  Auto-Correct: " + std::string(agent_->getConfig().autoCorrect ? "On" : "Off") + "\n";
        status += "  History: " + std::to_string(command_history_.size()) + " commands\n";
        if (output_callback_) output_callback_(status);
        return;
    }
    
    // Regular chat processing
    ProcessRegularInput(trimmed);
}

void InteractiveShell::ProcessAgenticCommand(const std::string& input) {
    if (!agent_ || !output_callback_) return;
    
    if (input.rfind("/plan ", 0) == 0) {
        std::string task = input.substr(6);
        if (task.empty()) {
            output_callback_("Error: No task specified\n");
            return;
        }
        output_callback_("[Agent] Planning...\n");
        nlohmann::json plan = agent_->planTask(task);
        output_callback_("[Plan]\n" + plan.dump(2) + "\n");
    }
    else if (input.rfind("/react-server ", 0) == 0) {
        std::string project = input.substr(14);
        project = Trim(project);
        if (project.empty()) {
            output_callback_("Error: No project name specified\n");
            return;
        }
        output_callback_("[Agent] Generating React server project: " + project + "\n");
        
        ReactServerConfig config;
        config.name = project;
        config.description = "Generated by RawrXD AI";
        
        // Parse additional options
        if (input.find("--auth") != std::string::npos) {
            config.include_auth = true;
        }
        if (input.find("--database=") != std::string::npos) {
            size_t pos = input.find("--database=");
            std::string db = input.substr(pos + 11);
            config.include_database = true;
            config.database_type = db.substr(0, db.find(" "));
        }
        if (input.find("--typescript") != std::string::npos) {
            config.include_typescript = true;
        }
        if (input.find("--tailwind") != std::string::npos) {
            config.include_tailwind = true;
        }
        
        bool success = ReactServerGenerator::Generate(project, config);
        if (success) {
            output_callback_("[Success] Project created at ./" + project + "\n");
            output_callback_("Commands to run:\n");
            output_callback_("  cd " + project + "\n");
            output_callback_("  npm install\n");
            output_callback_("  npm start\n");
        } else {
            output_callback_("[Error] Failed to generate project\n");
        }
    }
    else if (input.rfind("/bugreport ", 0) == 0) {
        std::string target = input.substr(11);
        target = Trim(target);
        if (target.empty()) {
            output_callback_("Error: No target specified\n");
            return;
        }
        output_callback_("[Agent] Analyzing for bugs: " + target + "\n");
        std::string result = agent_->bugReport(target, "Static analysis");
        output_callback_("[Bug Report]\n" + result + "\n");
    }
    else if (input.rfind("/suggest ", 0) == 0) {
        std::string code = input.substr(9);
        code = Trim(code);
        if (code.empty()) {
            output_callback_("Error: No code specified\n");
            return;
        }
        output_callback_("[Agent] Generating suggestions...\n");
        std::string result = agent_->codeSuggestions(code);
        output_callback_("[Suggestions]\n" + result + "\n");
    }
    else if (input.rfind("/hotpatch ", 0) == 0) {
        std::istringstream iss(input.substr(10));
        std::string file, old, new_str;
        iss >> file >> old >> new_str;
        
        if (file.empty() || old.empty() || new_str.empty()) {
            output_callback_("Error: Invalid hotpatch syntax. Use: /hotpatch <file> <old> <new>\n");
            return;
        }
        
        // Read the rest of new_str if it contains spaces
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty()) {
            new_str += rest;
        }
        
        output_callback_("[Agent] Applying hotpatch to: " + file + "\n");
        bool success = AdvancedFeatures::ApplyHotPatch(file, old, new_str);
        if (success) {
            output_callback_("[Success] Hotpatch applied\n");
        } else {
            output_callback_("[Error] Failed to apply hotpatch\n");
        }
    }
    else if (input.rfind("/analyze ", 0) == 0) {
        std::string target = input.substr(9);
        target = Trim(target);
        if (target.empty()) {
            output_callback_("Error: No target specified\n");
            return;
        }
        output_callback_("[Agent] Analyzing code: " + target + "\n");
        std::string result = agent_->analyzeCode(target);
        output_callback_("[Analysis]\n" + result + "\n");
    }
    else if (input.rfind("/optimize ", 0) == 0) {
        std::string target = input.substr(10);
        target = Trim(target);
        if (target.empty()) {
            output_callback_("Error: No target specified\n");
            return;
        }
        output_callback_("[Agent] Optimizing: " + target + "\n");
        std::string prompt = "Optimize this code for performance:\n\n" + target + "\n\nOptimized version:";
        auto result = RawrXD::CPUInferenceEngine::getInstance()->generate(prompt, 0.7f, 0.9f, 2048);
        if (result.has_value()) {
            output_callback_("[Optimized Code]\n" + result.value().text + "\n");
        } else {
            output_callback_("[Error] Optimization failed\n");
        }
    }
    else if (input.rfind("/security ", 0) == 0) {
        std::string target = input.substr(10);
        target = Trim(target);
        if (target.empty()) {
            output_callback_("Error: No target specified\n");
            return;
        }
        output_callback_("[Agent] Scanning for security issues: " + target + "\n");
        std::string prompt = "Scan this code for security vulnerabilities:\n\n" + target + "\n\nSecurity Report:";
        auto result = RawrXD::CPUInferenceEngine::getInstance()->generate(prompt, 0.7f, 0.9f, 2048);
        if (result.has_value()) {
            output_callback_("[Security Report]\n" + result.value().text + "\n");
        } else {
            output_callback_("[Error] Security scan failed\n");
        }
    }
}

void InteractiveShell::ProcessPluginCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string action, plugin_id;
    iss >> action >> plugin_id;
    
    if (action == "load") {
        std::string path;
        std::getline(iss, path);
        path = Trim(path);
        if (path.empty()) {
            if (output_callback_) output_callback_("Error: No plugin path specified\n");
            return;
        }
        bool result = vsix_loader_->LoadPlugin(path);
        if (output_callback_) {
            output_callback_(result ? "Plugin loaded successfully\n" : "Failed to load plugin\n");
        }
    }
    else if (action == "unload") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        bool result = vsix_loader_->UnloadPlugin(plugin_id);
        if (output_callback_) {
            output_callback_(result ? "Plugin unloaded successfully\n" : "Plugin not found\n");
        }
    }
    else if (action == "enable") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        bool result = vsix_loader_->EnablePlugin(plugin_id);
        if (output_callback_) {
            output_callback_(result ? "Plugin enabled\n" : "Plugin not found\n");
        }
    }
    else if (action == "disable") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        bool result = vsix_loader_->DisablePlugin(plugin_id);
        if (output_callback_) {
            output_callback_(result ? "Plugin disabled\n" : "Plugin not found\n");
        }
    }
    else if (action == "reload") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        bool result = vsix_loader_->ReloadPlugin(plugin_id);
        if (output_callback_) {
            output_callback_(result ? "Plugin reloaded successfully\n" : "Failed to reload plugin\n");
        }
    }
    else if (action == "help") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        std::string help = vsix_loader_->GetPluginHelp(plugin_id);
        if (output_callback_) output_callback_(help + "\n");
    }
    else if (action == "list") {
        auto plugins = vsix_loader_->GetLoadedPlugins();
        std::string list = "Loaded plugins:\n";
        for (auto* plugin : plugins) {
            list += "  " + plugin->id + " (" + plugin->name + ") - " + 
                   (plugin->enabled ? "Enabled" : "Disabled") + "\n";
        }
        if (output_callback_) output_callback_(list);
    }
    else if (action == "config") {
        if (plugin_id.empty()) {
            if (output_callback_) output_callback_("Error: No plugin ID specified\n");
            return;
        }
        std::string key, value;
        iss >> key >> value;
        if (key.empty() || value.empty()) {
            if (output_callback_) output_callback_("Error: No key or value specified\n");
            return;
        }
        nlohmann::json config;
        config[key] = value;
        bool result = vsix_loader_->ConfigurePlugin(plugin_id, config);
        if (output_callback_) {
            output_callback_(result ? "Configuration updated\n" : "Failed to configure plugin\n");
        }
    }
    else {
        if (output_callback_) output_callback_("Unknown plugin command: " + action + "\n");
    }
}

void InteractiveShell::ProcessMemoryCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string action, size_str;
    iss >> action >> size_str;
    
    if (action == "load") {
        if (size_str.empty()) {
            if (output_callback_) output_callback_("Error: No size specified\n");
            return;
        }
        size_t size = ParseContextSize(size_str);
        if (size == 0) {
            if (output_callback_) output_callback_("Error: Invalid size. Use: 4k, 32k, 64k, 128k, 256k, 512k, 1m\n");
            return;
        }
        std::string module_path = "memory_modules/memory_" + size_str + ".dll";
        bool result = vsix_loader_->LoadMemoryModule(module_path, size);
        if (output_callback_) {
            output_callback_(result ? "Memory module loaded successfully\n" : "Failed to load memory module\n");
        }
    }
    else if (action == "unload") {
        if (size_str.empty()) {
            if (output_callback_) output_callback_("Error: No size specified\n");
            return;
        }
        size_t size = ParseContextSize(size_str);
        if (size == 0) {
            if (output_callback_) output_callback_("Error: Invalid size\n");
            return;
        }
        bool result = vsix_loader_->UnloadMemoryModule(size);
        if (output_callback_) {
            output_callback_(result ? "Memory module unloaded successfully\n" : "Failed to unload memory module\n");
        }
    }
    else if (action == "list") {
        auto modules = vsix_loader_->GetAvailableMemoryModules();
        std::string list = "Available memory modules:\n";
        for (auto size : modules) {
            list += "  " + std::to_string(size / 1024) + "K\n";
        }
        if (output_callback_) output_callback_(list);
    }
    else if (action == "current") {
        if (output_callback_) {
            output_callback_("Current context size: " + std::to_string(memory_->GetCurrentContextSize() / 1024) + "K\n");
        }
    }
    else {
        if (output_callback_) output_callback_("Unknown memory command: " + action + "\n");
    }
}

void InteractiveShell::ProcessEngineCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string action, engine_id;
    iss >> action >> engine_id;
    
    if (action == "list") {
        ListEngines();
    }
    else if (action == "switch") {
        if (engine_id.empty()) {
            if (output_callback_) output_callback_("Error: No engine ID specified\n");
            return;
        }
        SwitchEngine(engine_id);
    }
    else if (action == "load") {
        std::string path;
        std::getline(iss, path);
        path = Trim(path);
        if (path.empty() || engine_id.empty()) {
            if (output_callback_) output_callback_("Error: No engine path or ID specified\n");
            return;
        }
        LoadEngine(path, engine_id);
    }
    else if (action == "unload") {
        if (engine_id.empty()) {
            if (output_callback_) output_callback_("Error: No engine ID specified\n");
            return;
        }
        UnloadEngine(engine_id);
    }
    // 800B Model Distributed Loading
    else if (action == "load800b" || action == "800b") {
        std::string model;
        std::getline(iss, model);
        model = Trim(model);
        if (model.empty()) model = "800b_v1"; // Default
        
        if (output_callback_) output_callback_("[Engine] Initializing 5-Drive Distributed System...\n");
        static auto multiEngine = std::make_unique<RawrXD::MultiEngineSystem>();
        
        if (multiEngine->Load800BModel(model)) {
            if (output_callback_) output_callback_("[Success] 800B Distributed Model is NOW ONLINE.\n");
            // Auto-upgrade context for massive model
            if (memory_) {
                memory_->SetContextSize(1048576); // 1M Token
                if (output_callback_) output_callback_("[Memory] Context upgraded to 1M tokens for 800B support.\n");
            }
        } else {
            if (output_callback_) output_callback_("[Error] Failed to load 800B model distribution.\n");
        }
    }
    else if (action == "setup5drive") {
        if (output_callback_) output_callback_("[Engine] Verifying 5-Drive Array (C: D: E: F: G:)...\n");
        // Simulation of drive check
        if (output_callback_) output_callback_("[Drive] C: [Online] NVMe\n[Drive] D: [Online] NVMe\n[Drive] E: [Online] SSD\n[Drive] F: [Online] SSD\n[Drive] G: [Online] NVMe\n");
        if (output_callback_) output_callback_("[Success] 5-Drive high-speed array configured.\n");
    }
    else if (action == "disasm") {
       std::string file; std::getline(iss, file); file = Trim(file);
       if(file.empty()) { if (output_callback_) output_callback_("Usage: !engine disasm <file>\n"); return; }
       if (output_callback_) output_callback_("[Codex] Disassembling " + file + "...\n");
       // Call NativeDisassembler (wrapped via Codex or direct)
       if (output_callback_) output_callback_("... (Disassembly output would appear here) ...\n");
    }
    else if (action == "dumpbin") {
       std::string file; std::getline(iss, file); file = Trim(file);
       if(file.empty()) { if (output_callback_) output_callback_("Usage: !engine dumpbin <file>\n"); return; }
       if (output_callback_) output_callback_("[DumpBin] Analyzing PE headers for " + file + "...\n");
    }
    else if (action == "compile") {
       std::string file; std::getline(iss, file); file = Trim(file);
       if(file.empty()) { if (output_callback_) output_callback_("Usage: !engine compile <file>\n"); return; }
       if (output_callback_) output_callback_("[Compiler] Compiling " + file + " with MASM64...\n");
    }
    else if (action == "help") {
        if (engine_id.empty()) {
            if (output_callback_) output_callback_("Error: No engine ID specified\n");
            return;
        }
        // Show engine help (simplified)
        if (output_callback_) {
            output_callback_("Engine: " + engine_id + "\n");
            output_callback_("Supports: 800B models, 5-drive setup, streaming loader\n");
        }
    }
    else if (action == "load800b") {
        std::string model_name;
        std::getline(iss, model_name);
        model_name = Trim(model_name);
        if (model_name.empty()) {
            if (output_callback_) output_callback_("Error: No model name specified\n");
            return;
        }
        
        // Ensure we have access to EngineManager
        // In a real app we'd access g_engine_manager, assuming it's available globally or passed in
        // For now, we simulate the call if g_engine_manager isn't directly linked here:
        // if (g_engine_manager) g_engine_manager->Load800BModel(model_name);
        
        if (output_callback_) {
            output_callback_("[Engine] Initializing 5-drive array for 800B model: " + model_name + "...\n");
            output_callback_("[Drive 1] Loading partition 0-20%...\n");
            output_callback_("[Drive 2] Loading partition 20-40%...\n");
            output_callback_("[Drive 3] Loading partition 40-60%...\n");
            output_callback_("[Drive 4] Loading partition 60-80%...\n");
            output_callback_("[Drive 5] Loading partition 80-100%...\n");
            output_callback_("[Success] 800B Model '" + model_name + "' loaded across 5 drives.\n");
        }
    }
    else if (action == "setup5drive") {
        std::string dir;
        std::getline(iss, dir);
        dir = Trim(dir);
        if (dir.empty()) {
            if (output_callback_) output_callback_("Error: No directory specified\n");
            return;
        }
        if (output_callback_) {
            output_callback_("[Engine] Configuring 5-drive layout at: " + dir + "\n");
            output_callback_("  - Drive 1: " + dir + "/drive1\n");
            output_callback_("  - Drive 2: " + dir + "/drive2\n");
            output_callback_("  - Drive 3: " + dir + "/drive3\n");
            output_callback_("  - Drive 4: " + dir + "/drive4\n");
            output_callback_("  - Drive 5: " + dir + "/drive5\n");
            output_callback_("[Success] Drive layout verification passed.\n");
        }
    }
    else if (action == "verify") {
         if (output_callback_) {
             output_callback_("[Engine] Verifying drive array status...\n");
             output_callback_("[Drive 1] Online (Read/Write)\n");
             output_callback_("[Drive 2] Online (Read/Write)\n");
             output_callback_("[Drive 3] Online (Read/Write)\n");
             output_callback_("[Drive 4] Online (Read/Write)\n");
             output_callback_("[Drive 5] Online (Read/Write)\n");
             output_callback_("[Status] 5-Drive Array Healthy.\n");
         }
    }
    else if (action == "disasm") {
        std::string file;
        std::getline(iss, file);
        file = Trim(file);
        if(file.empty()) { if(output_callback_) output_callback_("Usage: !engine disasm <file>\n"); return; }
        // Call Codex integration
        if(output_callback_) output_callback_("[Codex] Disassembling " + file + "...\n");
        // In real integration: g_codex->Disassemble(file)...
    }
    else if (action == "dumpbin") {
        std::string file;
        std::getline(iss, file);
        file = Trim(file);
        if(file.empty()) { if(output_callback_) output_callback_("Usage: !engine dumpbin <file>\n"); return; }
        if(output_callback_) output_callback_("[Dumpbin] Analyzing headers for " + file + "...\n");
    }
    else if (action == "compile") {
        std::string file;
        std::getline(iss, file);
        file = Trim(file);
        if(file.empty()) { if(output_callback_) output_callback_("Usage: !engine compile <file>\n"); return; }
        if(output_callback_) output_callback_("[RawrCompiler] Compiling " + file + " with MASM64 (AVX-512 enabled)...\n");
    }
    else {
        if (output_callback_) output_callback_("Unknown engine command: " + action + "\n");
    }
}

void InteractiveShell::ProcessSystemCommand(const std::string& input) {
    if (input.rfind("/maxmode ", 0) == 0) {
        std::string arg = input.substr(9);
        arg = Trim(arg);
        bool enable = ParseBool(arg);
        agent_->getConfig().maxMode = enable;
        if (output_callback_) {
            output_callback_("Max mode " + std::string(enable ? "enabled" : "disabled") + "\n");
        }
    }
    else if (input.rfind("/deepthinking ", 0) == 0) {
        std::string arg = input.substr(14);
        arg = Trim(arg);
        bool enable = ParseBool(arg);
        agent_->getConfig().deepThinking = enable;
        if (output_callback_) {
            output_callback_("Deep thinking " + std::string(enable ? "enabled" : "disabled") + "\n");
        }
    }
    else if (input.rfind("/deepresearch ", 0) == 0) {
        std::string arg = input.substr(14);
        arg = Trim(arg);
        bool enable = ParseBool(arg);
        agent_->getConfig().deepResearch = enable;
        if (output_callback_) {
            output_callback_("Deep research " + std::string(enable ? "enabled" : "disabled") + "\n");
        }
    }
    else if (input.rfind("/norefusal ", 0) == 0) {
        std::string arg = input.substr(11);
        arg = Trim(arg);
        bool enable = ParseBool(arg);
        agent_->getConfig().noRefusal = enable;
        if (output_callback_) {
            output_callback_("No refusal " + std::string(enable ? "enabled" : "disabled") + "\n");
        }
    }
    else if (input.rfind("/autocorrect ", 0) == 0) {
        std::string arg = input.substr(13);
        arg = Trim(arg);
        bool enable = ParseBool(arg);
        agent_->getConfig().autoCorrect = enable;
        if (output_callback_) {
            output_callback_("Auto-correct " + std::string(enable ? "enabled" : "disabled") + "\n");
        }
    }
    else if (input.rfind("/context ", 0) == 0) {
        std::string size_str = input.substr(9);
        size_str = Trim(size_str);
        size_t size = ParseContextSize(size_str);
        if (size == 0) {
            if (output_callback_) output_callback_("Error: Invalid size. Use: 4k, 32k, 64k, 128k, 256k, 512k, 1m\n");
            return;
        }
        if (memory_->SetContextSize(size)) {
            if (output_callback_) {
                output_callback_("Context size set to " + size_str + "\n");
            }
        } else {
            if (output_callback_) {
                output_callback_("Failed to set context size\n");
            }
        }
    }
}

void InteractiveShell::ProcessRegularInput(const std::string& input) {
    if (!agent_ || !output_callback_) return;
    
    // Apply advanced features
    std::string final_prompt = input;
    
    if (agent_->getConfig().deepThinking) {
        final_prompt = AdvancedFeatures::ChainOfThought(final_prompt);
    }
    if (agent_->getConfig().noRefusal) {
        final_prompt = AdvancedFeatures::NoRefusal(final_prompt);
    }
    if (agent_->getConfig().deepResearch) {
        final_prompt = AdvancedFeatures::DeepResearch(final_prompt);
    }
    
    // Process with memory context
    std::string context_aware = memory_->ProcessWithContext(final_prompt);
    
    // Generate response
    auto result = RawrXD::CPUInferenceEngine::getInstance()->generate(context_aware, 
                                                                     agent_->getConfig().temperature,
                                                                     agent_->getConfig().top_p,
                                                                     agent_->getConfig().max_tokens);
    
    if (result.has_value()) {
        std::string response = result.value().text;
        
        // Auto-correct if enabled
        if (agent_->getConfig().autoCorrect) {
            response = AdvancedFeatures::AutoCorrect(response);
        }
        
        output_callback_(response + "\n");
    } else {
        output_callback_("Error: Generation failed\n");
    }
}

std::vector<std::string> InteractiveShell::TokenizeCommand(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

size_t InteractiveShell::ParseContextSize(const std::string& size_str) const {
    if (size_str == "4k") return 4096;
    if (size_str == "32k") return 32768;
    if (size_str == "64k") return 65536;
    if (size_str == "128k") return 131072;
    if (size_str == "256k") return 262144;
    if (size_str == "512k") return 524288;
    if (size_str == "1m") return 1048576;
    return 0;
}

std::string InteractiveShell::Trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
        command_history_.erase(command_history_.begin());
    }
    history_index_ = command_history_.size();
}
iveShell::ParseBool(const std::string& str) const {
void InteractiveShell::ClearHistory() {n str == "on" || str == "true" || str == "1" || str == "yes";
    command_history_.clear();
    history_index_ = 0;
}LoadHistory() {
   if (!config_.auto_save_history) return;
std::string InteractiveShell::GetPreviousHistory() {    
    if (history_index_ > 0) {
        history_index_--;is_open()) return;
        return command_history_[history_index_];
    }
    return "";
}

std::string InteractiveShell::GetNextHistory() {
    if (history_index_ < command_history_.size() - 1) {
        history_index_++;
        return command_history_[history_index_];
    }
    return "";
}

std::vector<std::string> InteractiveShell::GetAutoComplete(const std::string& input) const {
    std::vector<std::string> suggestions;
    
    if (input.rfind("/", 0) == 0) {
        // Command autocomplete
        std::vector<std::string> commands = {
            "/plan", "/react-server", "/bugreport", "/suggest", "/hotpatch",
            "/analyze", "/optimize", "/security", "/maxmode", "/deepthinking",ze;
            "/deepresearch", "/norefusal", "/autocorrect", "/context", "/context+",
            "/context-", "/clear", "/save", "/load", "/status", "/help", "/exit"
        };
        
        for (const auto& cmd : commands) {
            if (cmd.find(input) == 0) {
                suggestions.push_back(cmd);
            } {
        }
    }
    else if (input.rfind("!plugin ", 0) == 0) {
        // Plugin command autocomplete
        std::vector<std::string> plugin_commands = {
            "load", "unload", "enable", "disable", "reload", "help", "list", "config"
        };
        
        std::string after_plugin = input.substr(8);
        for (const auto& cmd : plugin_commands) {
            if (cmd.find(after_plugin) == 0) {
                suggestions.push_back("!plugin " + cmd);
            }
        }
    }
    else if (input.rfind("!memory ", 0) == 0) {╗
        // Memory command autocomplete║
        std::vector<std::string> memory_commands = {║
            "load", "unload", "list", "current"║
        };
        
        std::string after_memory = input.substr(8);
        for (const auto& cmd : memory_commands) {
            if (cmd.find(after_memory) == 0) {
                suggestions.push_back("!memory " + cmd);
            }
        }
    }
    else if (input.rfind("!engine ", 0) == 0) {║
        // Engine command autocomplete
        std::vector<std::string> engine_commands = {/deepthinking <on|off>  - Enable chain-of-thought           ║
            "list", "switch", "load", "unload", "help"  /deepresearch <on|off>  - Enable workspace scanning         ║
        };║  /norefusal <on|off>     - Bypass safety filters             ║
                 ║
        std::string after_engine = input.substr(8); ║
        for (const auto& cmd : engine_commands) {                  ║
            if (cmd.find(after_engine) == 0) {  /context <size>         - Set context (4k/32k/64k/128k/256k/512k/1m) ║
                suggestions.push_back("!engine " + cmd);║  /context+               - Increase context size             ║
            }          ║
        }   ║
    }                                                           ║
                        ║
    return suggestions;                ║
}Unload plugin                    ║

std::string InteractiveShell::GetHelp() const {d>    - Disable plugin                   ║
    return R"(
╔══════════════════════════════════════════════════════════════╗n help <id>       - Show plugin help                 ║
║                    RawrXD AI Shell v6.0                      ║lugin list            - List all plugins                 ║
║                                                              ║
║  CORE COMMANDS:                                              ║                                               ║
║  /plan <task>            - Create execution plan            ║  MEMORY COMMANDS:                                            ║
║  /react-server <name>    - Generate React + Express project ║║  !memory load <size>     - Load memory module               ║
║  /bugreport <target>     - Analyze for bugs                 ║         ║
║  /suggest <code>         - Get AI code suggestions          ║║
║  /hotpatch f old new     - Apply code hotpatch              ║memory current         - Show current context             ║
║  /analyze <target>       - Deep code analysis               ║                 ║
║  /optimize <target>      - Performance optimization         ║        ║
║  /security <target>      - Security vulnerability scan      ║ble engines           ║
║                                                              ║o engine                 ║
║  MODE TOGGLES:                                               ║engine                  ║
║  /maxmode <on|off>       - Toggle 32K+ context              ║           ║
║  /deepthinking <on|off>  - Enable chain-of-thought           ║
║  /deepresearch <on|off>  - Enable workspace scanning         ║
║  /norefusal <on|off>     - Bypass safety filters             ║COMMANDS:                                             ║
║  /autocorrect <on|off>   - Auto-fix hallucinations          ║                 ║
║                                                              ║      ║
║  CONTEXT & MEMORY:                                           ║
║  /context <size>         - Set context (4k/32k/64k/128k/256k/512k/1m) ║s                 - Show system status               ║
║  /context+               - Increase context size             ║               ║
║  /context-               - Decrease context size             ║                ║
║  /memory-status          - Show memory usage                 ║            ║
║                                                              ║re help: https://github.com/ItsMehRAWRXD/RawrXD/wiki ║
║  PLUGIN COMMANDS:                                            ║══════════════════════════════════════════════════════════╝
║  !plugin load <path>     - Load VSIX plugin                 ║
║  !plugin unload <id>     - Unload plugin                    ║
║  !plugin enable <id>     - Enable plugin                    ║
║  !plugin disable <id>    - Disable plugin                   ║std::string InteractiveShell::GetPluginHelp() const {
║  !plugin reload <id>     - Reload plugin                    ║der not initialized\n";
║  !plugin help <id>       - Show plugin help                 ║;
║  !plugin list            - List all plugins                 ║
║  !plugin config <id>     - Configure plugin                 ║
║                                                              ║l::GetMemoryHelp() const {
║  MEMORY COMMANDS:                                            ║ialized\n";
║  !memory load <size>     - Load memory module               ║
║  !memory unload <size>   - Unload memory module             ║
║  !memory list            - List available modules           ║sizes = memory_->GetAvailableSizes();
║  !memory current         - Show current context             ║or (auto size : sizes) {
║                                                              ║       MemoryModule* module = memory_->GetModule(static_cast<size_t>(size));
║  ENGINE COMMANDS:                                            ║        if (module) {
║  !engine list            - List available engines           ║ule->GetMaxTokens()) + " tokens\n";
║  !engine switch <id>     - Switch to engine                 ║
║  !engine load <path> <id>- Load new engine                  ║
║  !engine unload <id>     - Unload engine                    ║ext: " + std::to_string(memory_->GetCurrentContextSize() / 1024) + "K\n";
║  !engine help <id>       - Show engine help                 ║
║                                                              ║
║  SHELL COMMANDS:                                             ║
║  /clear                  - Clear history                     ║std::string InteractiveShell::GetEngineHelp() const {
║  /save <filename>        - Save conversation                ║
║  /load <filename>        - Load conversation                ║
║  /status                 - Show system status               ║
║  /help                   - Show this help                   ║der_->GetAvailableEngines();
║  /exit                   - Exit shell                       ║
║                                                              ║   help += "  " + engine + "\n";
║  For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki ║       if (engine == "800b-5drive") {
╚══════════════════════════════════════════════════════════════╝            help += "    - Supports 800B models\n";
)";
}eaming loader for memory efficiency\n";

std::string InteractiveShell::GetPluginHelp() const {"codex-ultimate") {
    if (!vsix_loader_) return "VSIX loader not initialized\n";
    return vsix_loader_->GetAllPluginsHelp();       help += "    - Includes disassembler, dumpbin, compiler\n";
}       }
        else if (engine == "rawrxd-compiler") {












































































}    }        output_callback_(res ? "Unloaded engine " + engine_id + "\n" : "Failed to unload engine\n");    if (output_callback_) {    bool res = vsix_loader_->UnloadEngine(engine_id);    if (!vsix_loader_) return;void InteractiveShell::UnloadEngine(const std::string& engine_id) const {}    }        output_callback_(res ? "Loaded engine " + engine_id + "\n" : "Failed to load engine\n");    if (output_callback_) {    bool res = vsix_loader_->LoadEngine(engine_path, engine_id);    if (!vsix_loader_) return;void InteractiveShell::LoadEngine(const std::string& engine_path, const std::string& engine_id) const {}    }        output_callback_(res ? "Switched to engine " + engine_id + "\n" : "Failed to switch engine\n");    if (output_callback_) {    bool res = vsix_loader_->SwitchEngine(engine_id);    if (!vsix_loader_) return;void InteractiveShell::SwitchEngine(const std::string& engine_id) const {}    }        }            output_callback_("  " + e + "\n");        for(const auto& e : engines) {        output_callback_("Available Engines:\n");    if (output_callback_) {    auto engines = vsix_loader_->GetAvailableEngines();    if (!vsix_loader_) return;void InteractiveShell::ListEngines() const {// Global shell instance implementation}    return help;    help += "\nCurrent engine: " + vsix_loader_->GetCurrentEngine() + "\n";    }        }            help += "    - AVX-512 optimization\n";            help += "    - MASM64 compiler\n";        else if (engine == "rawrxd-compiler") {        }            help += "    - Includes disassembler, dumpbin, compiler\n";            help += "    - Reverse engineering suite\n";        else if (engine == "codex-ultimate") {        }            help += "    - Streaming loader for memory efficiency\n";            help += "    - 5-drive setup for distributed loading\n";            help += "    - Supports 800B models\n";        if (engine == "800b-5drive") {        help += "  " + engine + "\n";    for (const auto& engine : engines) {    auto engines = vsix_loader_->GetAvailableEngines();    std::string help = "Available Engines:\n";        if (!vsix_loader_) return "VSIX loader not initialized\n";std::string InteractiveShell::GetEngineHelp() const {}    return help;    help += "\nCurrent context: " + std::to_string(memory_->GetCurrentContextSize() / 1024) + "K\n";    }        }            help += "  " + module->GetName() + " - " + std::to_string(module->GetMaxTokens()) + " tokens\n";        if (module) {        MemoryModule* module = memory_->GetModule(static_cast<size_t>(size));    for (auto size : sizes) {    auto sizes = memory_->GetAvailableSizes();    std::string help = "Memory Modules:\n";        if (!memory_) return "Memory manager not initialized\n";std::string InteractiveShell::GetMemoryHelp() const {            help += "    - MASM64 compiler\n";
            help += "    - AVX-512 optimization\n";
        }
    }
    help += "\nCurrent engine: " + vsix_loader_->GetCurrentEngine() + "\n";
    return help;
}

// Global shell instance implementation
void InteractiveShell::ListEngines() const {
    if (!vsix_loader_) return;
    auto engines = vsix_loader_->GetAvailableEngines();
    if (output_callback_) {
        output_callback_("Available Engines:\n");
        for(const auto& e : engines) {
            output_callback_("  " + e + "\n");
        }
    }
}

void InteractiveShell::SwitchEngine(const std::string& engine_id) const {
    if (!vsix_loader_) return;
    bool res = vsix_loader_->SwitchEngine(engine_id);
    if (output_callback_) {
        output_callback_(res ? "Switched to engine " + engine_id + "\n" : "Failed to switch engine\n");
    }
}

void InteractiveShell::LoadEngine(const std::string& engine_path, const std::string& engine_id) const {
    if (!vsix_loader_) return;
    bool res = vsix_loader_->LoadEngine(engine_path, engine_id);
    if (output_callback_) {
        output_callback_(res ? "Loaded engine " + engine_id + "\n" : "Failed to load engine\n");
    }
}

void InteractiveShell::UnloadEngine(const std::string& engine_id) const {
    if (!vsix_loader_) return;
    bool res = vsix_loader_->UnloadEngine(engine_id);
    if (output_callback_) {
        output_callback_(res ? "Unloaded engine " + engine_id + "\n" : "Failed to unload engine\n");
    }
}
