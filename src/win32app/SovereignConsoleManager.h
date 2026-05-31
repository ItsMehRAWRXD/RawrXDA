#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace RawrXD {

class SovereignConsoleManager {
public:
    SovereignConsoleManager();
    ~SovereignConsoleManager();

    bool Start(const char* modelPath = nullptr, const char* workingDir = nullptr);
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    void WriteInput(const std::string& data);
    
    std::function<void(const std::string&)> OnOutput;
    std::function<void(const std::string&)> OnError;
    std::function<void()> OnStarted;
    std::function<void(int)> OnFinished;

private:
    bool StartWithPipes(const std::string& cmd, const char* workingDir);
    void ReadOutputThread();
    void ReadErrorThread();
    void MonitorProcessThread();

    HANDLE m_hProcess{nullptr};
    HANDLE m_hThread{nullptr};
    HANDLE m_hStdInWrite{nullptr};
    HANDLE m_hStdOutRead{nullptr};
    HANDLE m_hStdErrRead{nullptr};
    
    std::thread m_outputThread;
    std::thread m_errorThread;
    std::thread m_monitorThread;
    
    std::atomic<bool> m_running{false};
    std::mutex m_stdinMutex;
};

} // namespace RawrXD
