#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

class Win32TerminalManager
{
  public:
    enum ShellType
    {
        PowerShell,
        CommandPrompt
    };

    Win32TerminalManager();
    ~Win32TerminalManager();

    /// Optional UTF-8 working directory for the child shell (CreateProcess lpCurrentDirectory).
    /// Pass nullptr to inherit the IDE process current directory.
    bool start(ShellType shell, const char* workingDirectoryUtf8 = nullptr);
    void stop();
    DWORD pid() const;
    bool isRunning() const;
    void writeInput(const std::string& data);

    // Callbacks
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    std::function<void()> onStarted;
    std::function<void(int)> onFinished;

  private:
    bool startWithConPTY(const std::string& cmd, const char* workingDirectoryUtf8);
    bool startWithPipes(const std::string& cmd, const char* workingDirectoryUtf8);
    void readOutputThread();
    void readErrorThread();
    void monitorProcessThread();
    void appendToOutputRing(const char* data, size_t size);
    /// After natural exit, threads are done but handles may still be open — release before a new start().
    void resetStaleStateFromExitedProcess();

    HANDLE m_hProcess;
    HANDLE m_hThread;
    DWORD m_processId;

    HANDLE m_hStdInRead;
    HANDLE m_hStdInWrite;
    HANDLE m_hStdOutRead;
    HANDLE m_hStdOutWrite;
    HANDLE m_hStdErrRead;
    HANDLE m_hStdErrWrite;
    HANDLE m_hPseudoConsole;
    HANDLE m_hPtyInputWrite;
    HANDLE m_hPtyOutputRead;

    std::thread m_outputThread;
    std::thread m_errorThread;
    std::thread m_monitorThread;

    std::atomic<bool> m_running;
    std::atomic<bool> m_destroying;
    ShellType m_shellType;
    std::mutex m_stdinWriteMutex;

    std::mutex m_outputRingMutex;
    std::deque<std::string> m_outputRing;
    size_t m_outputRingBytes;
    static constexpr size_t kMaxOutputRingBytes = 512ull * 1024ull * 1024ull;
};
