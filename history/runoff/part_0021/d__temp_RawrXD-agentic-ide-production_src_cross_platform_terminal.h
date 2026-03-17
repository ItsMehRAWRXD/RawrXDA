#ifndef CROSS_PLATFORM_TERMINAL_H
#define CROSS_PLATFORM_TERMINAL_H

#include <string>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <spawn.h>
#endif

class CrossPlatformTerminal {
public:
    enum ShellType {
        PWSH = 0,    // Windows PowerShell
        CMD = 1,     // Windows Command Prompt
        BASH = 2,    // Linux/macOS Bash
        ZSH = 3,     // macOS Zsh
        FISH = 4     // Linux/macOS Fish
    };

    CrossPlatformTerminal();
    ~CrossPlatformTerminal();

    bool startShell(ShellType shellType);
    bool sendCommand(const std::string& command);
    std::string readOutput();
    std::string readError();
    bool isRunning() const;
    void stop();
    
    static std::string getDefaultShell();
    static std::vector<ShellType> getAvailableShells();
    
private:
#ifdef _WIN32
    HANDLE m_hChildStd_IN_Rd;
    HANDLE m_hChildStd_IN_Wr;
    HANDLE m_hChildStd_OUT_Rd;
    HANDLE m_hChildStd_OUT_Wr;
    HANDLE m_hChildStd_ERR_Rd;
    HANDLE m_hChildStd_ERR_Wr;
    PROCESS_INFORMATION m_pi;
#else
    int m_stdin_pipe[2];
    int m_stdout_pipe[2];
    int m_stderr_pipe[2];
    pid_t m_pid;
#endif
    
    bool m_running;
    ShellType m_currentShell;
    
    bool createPipes();
    bool spawnProcess(const std::string& command);
    std::string readFromPipe(void* handle);
};

#endif // CROSS_PLATFORM_TERMINAL_H