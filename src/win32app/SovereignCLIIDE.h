// ============================================================================
// SovereignCLIIDE.h — CLI IDE that works standalone and as GUI tab
// ============================================================================
//
// PURPOSE:
//   Provides a command-line interface IDE that can run as:
//   1. Standalone executable (sovereign-cli-ide.exe)
//   2. Integrated tab in Win32IDE (like Terminal but for CLI commands)
//
// FEATURES:
//   - Multi-session CLI command execution
//   - Real-time output streaming
//   - Command history and completion
//   - Integrated with agentic system
//   - Tabbed interface in GUI mode
//
// PATTERN: No exceptions. Returns PatchResult-style structured results.
// ============================================================================

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <deque>
#include <atomic>

// Forward declarations
class Win32TerminalManager;

class SovereignCLIIDE
{
public:
    enum class RunMode {
        Standalone,     // Run as standalone executable
        IntegratedTab   // Run as tab in Win32IDE
    };

    // Command execution result
    struct CommandResult {
        bool success;
        int exitCode;
        std::string output;
        std::string error;
        double executionTime;
    };

    // Callbacks
    using OutputCallback = std::function<void(const std::string& output)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using CompletionCallback = std::function<void(const CommandResult& result)>;

    SovereignCLIIDE(RunMode mode = RunMode::Standalone);
    ~SovereignCLIIDE();

    // Initialization
    bool initialize();
    void shutdown();

    // Command execution
    CommandResult executeCommand(const std::string& command);
    void executeCommandAsync(const std::string& command, CompletionCallback callback = nullptr);
    
    // Agentic execution helpers
    CommandResult executeInference(const std::string& prompt);
    CommandResult executeAgenticCommand(const std::string& command);
    
    // Session management
    void startSession();
    void endSession();
    void clearHistory();
    
    // Integrated tab interface
    HWND createTabContent(HWND parent);
    void destroyTabContent();
    void focusTab();
    
    // History and completion
    std::vector<std::string> getCommandHistory() const;
    std::vector<std::string> getCommandSuggestions(const std::string& prefix) const;
    
    // Configuration
    void setWorkingDirectory(const std::string& path);
    void setEnvironmentVariables(const std::vector<std::pair<std::string, std::string>>& envVars);
    
    // Callback registration
    void setOutputCallback(OutputCallback callback);
    void setErrorCallback(ErrorCallback callback);

private:
    // Internal implementation
    bool initializeTerminal();
    void cleanupTerminal();
    void readOutputThread();
    void readErrorThread();
    void processOutput(const std::string& data);
    void processError(const std::string& data);
    
    // Tab UI creation
    HWND createOutputWindow(HWND parent);
    HWND createInputWindow(HWND parent);
    HWND createTabBar(HWND parent);
    
    // Message handlers
    static LRESULT CALLBACK OutputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    RunMode m_mode;
    std::unique_ptr<Win32TerminalManager> m_terminal;
    std::thread m_outputThread;
    std::thread m_errorThread;
    std::atomic<bool> m_running;
    
    // UI components (for integrated tab mode)
    HWND m_hwndOutput;
    HWND m_hwndInput;
    HWND m_hwndTabBar;
    HWND m_hwndParent;
    
    // State management
    std::string m_workingDirectory;
    std::vector<std::pair<std::string, std::string>> m_environmentVars;
    std::deque<std::string> m_commandHistory;
    std::vector<CommandResult> m_executionResults;
    bool m_agenticMode;  // Agentic/Autopilot mode enabled
    
    // Callbacks
    OutputCallback m_outputCallback;
    ErrorCallback m_errorCallback;
    
    mutable std::mutex m_mutex;
};

// Global instance for standalone mode
extern SovereignCLIIDE* g_sovereignCLIIDE;
