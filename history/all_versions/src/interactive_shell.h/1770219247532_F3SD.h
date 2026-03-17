#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

class AgenticEngine;
class MemoryManager;
class VSIXLoader;
namespace RawrXD { class ReactServerGenerator; }
using RawrXD::ReactServerGenerator;

struct ShellConfig {
    bool cli_mode = true;
    bool show_welcome = true;
    bool auto_save_history = true;
    bool enable_colors = true;
    std::string history_file = "shell_history.txt";
    size_t max_history_size = 1000;
    bool enable_suggestions = true;
    bool enable_autocomplete = true;
};

class InteractiveShell {
private:
    std::atomic<bool> running_;
    std::thread shell_thread_;
    mutable std::mutex execution_mutex_;
    AgenticEngine* agent_;
    MemoryManager* memory_;
    VSIXLoader* vsix_loader_;
    ReactServerGenerator* react_generator_;
    std::function<void(const std::string&)> output_callback_;
    std::function<void(const std::string&)> input_callback_;
    ShellConfig config_;
    std::vector<std::string> command_history_;
    size_t history_index_;
    std::string current_input_;
    
public:
    InteractiveShell(const ShellConfig& config = {});
    ~InteractiveShell();
    
    void Start(AgenticEngine* agent, MemoryManager* memory, VSIXLoader* vsix_loader,
               ReactServerGenerator* react_generator,
               std::function<void(const std::string&)> output_cb,
               std::function<void(const std::string&)> input_cb = nullptr);
    void Stop();
    
    void SendInput(const std::string& input);
    bool IsRunning() const { return running_; }
    
    // Command processing
    void ProcessCommand(const std::string& input);
    std::string ExecuteCommand(const std::string& input);
    std::string GetPrompt() const;
    
    // Help system
    std::string GetHelp() const;
    std::string GetPluginHelp() const;
    std::string GetMemoryHelp() const;
    std::string GetEngineHelp() const;
    
    // History management
    void LoadHistory();
    void SaveHistory();
    void AddToHistory(const std::string& command);
    std::string GetPreviousHistory();
    std::string GetNextHistory();
    void ClearHistory();
    
    // Auto-completion
    std::vector<std::string> GetAutoComplete(const std::string& input) const;
    
private:
    void RunShell();
    void ProcessRegularInput(const std::string& input);
    void DisplayWelcome();
    void DisplayPrompt();
    
    // Command processors
    void ProcessAgenticCommand(const std::string& input);
    void ProcessPluginCommand(const std::string& cmd);
    void ProcessMemoryCommand(const std::string& cmd);
    void ProcessEngineCommand(const std::string& cmd);
    void ProcessSystemCommand(const std::string& input);
    
    // Utility
    std::vector<std::string> TokenizeCommand(const std::string& input) const;
    size_t ParseContextSize(const std::string& size_str) const;
    std::string Trim(const std::string& str) const;
    bool ParseBool(const std::string& str) const;
    
    // Engine management
    void ListEngines() const;
    void SwitchEngine(const std::string& engine_id) const;
    void LoadEngine(const std::string& engine_path, const std::string& engine_id) const;
    void UnloadEngine(const std::string& engine_id) const;
};

// Global shell instance
extern std::unique_ptr<InteractiveShell> g_shell;
