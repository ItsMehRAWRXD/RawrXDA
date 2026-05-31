#pragma once
/**
 * @file shell_integration.h
 * @brief Shell integration for command execution
 * Batch 5 - Item 65: Shell integration
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <future>

namespace RawrXD::Terminal {

enum class ShellType {
    Cmd,
    PowerShell,
    PowerShellCore,
    Bash,
    Zsh,
    Fish,
    Wsl,
    GitBash
};

struct EnvironmentVariable {
    std::string name;
    std::string value;
};

struct ShellProcess {
    void* handle{nullptr};
    void* stdinWrite{nullptr};
    void* stdoutRead{nullptr};
    void* stderrRead{nullptr};
    bool running{false};
    int exitCode{0};
};

struct CommandResult {
    int exitCode;
    std::string stdout;
    std::string stderr;
    std::chrono::milliseconds duration;
};

class ShellIntegration {
public:
    ShellIntegration();
    ~ShellIntegration();

    // Initialization
    bool initialize();
    void shutdown();

    // Shell detection
    ShellType detectShell();
    std::vector<ShellType> getAvailableShells();
    std::string getShellName(ShellType type) const;
    std::string getShellPath(ShellType type) const;

    // Process management
    bool startShell(ShellType type = ShellType::Cmd);
    bool startShell(const std::string& command);
    void stopShell();
    bool isRunning() const;

    // Command execution
    bool executeCommand(const std::string& command);
    std::future<CommandResult> executeCommandAsync(const std::string& command);
    CommandResult executeCommandSync(const std::string& command, int timeoutMs = 30000);

    // Interactive mode
    void sendInput(const std::string& input);
    std::string readOutput();
    std::string readError();
    bool hasOutput() const;
    bool hasError() const;

    // Environment
    void setEnvironmentVariable(const std::string& name, const std::string& value);
    void unsetEnvironmentVariable(const std::string& name);
    std::string getEnvironmentVariable(const std::string& name);
    std::vector<EnvironmentVariable> getEnvironmentVariables();

    // Working directory
    void setWorkingDirectory(const std::string& path);
    std::string getWorkingDirectory() const;

    // Configuration
    void setShellArgs(const std::vector<std::string>& args);
    void setStartupCommands(const std::vector<std::string>& commands);

    // Events
    using OutputCallback = std::function<void(const std::string& data)>;
    using ErrorCallback = std::function<void(const std::string& data)>;
    using ExitCallback = std::function<void(int exitCode)>;
    void onOutput(OutputCallback callback);
    void onError(ErrorCallback callback);
    void onExit(ExitCallback callback);

private:
    ShellProcess m_process;
    ShellType m_shellType{ShellType::Cmd};
    std::string m_workingDirectory;
    std::vector<std::string> m_shellArgs;
    std::vector<std::string> m_startupCommands;

    OutputCallback m_outputCallback;
    ErrorCallback m_errorCallback;
    ExitCallback m_exitCallback;

    std::thread m_outputThread;
    std::thread m_errorThread;
    std::atomic<bool> m_running{false};

    void readOutputLoop();
    void readErrorLoop();
    bool createShellProcess();
    void closeShellProcess();
};

// Global instance
ShellIntegration& getShellIntegration();

} // namespace RawrXD::Terminal
