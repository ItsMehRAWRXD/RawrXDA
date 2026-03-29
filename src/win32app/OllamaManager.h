#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <windows.h>

namespace RawrXD {

class OllamaManager {
public:
    enum class State {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error
    };

    using OutputCallback = std::function<void(const std::string&)>;
    using StateChangeCallback = std::function<void(State)>;

    OllamaManager();
    ~OllamaManager();

    // Lifecycle management
    bool Start();
    bool Stop();
    bool IsRunning() const { return m_state == State::Running; }
    State GetState() const { return m_state; }
    std::string GetStateString() const;

    // Configuration
    void SetExecutablePath(const std::string& path) { m_ollamaExePath = path; }
    void SetPort(uint16_t port) { m_port = port; }
    void SetOutputCallback(OutputCallback cb) { m_outputCallback = cb; }
    void SetStateChangeCallback(StateChangeCallback cb) { m_stateChangeCallback = cb; }

    // Query health
    bool IsHealthy() const;
    std::string GetEndpointURL() const;

    // Process info
    DWORD GetProcessID() const { return m_processId; }
    std::string GetVersion() const { return m_version; }

    // Auto-download capability
    static bool EnsureOllamaAvailable(std::string& outPath);
    static std::string GetDefaultOllamaPath();

private:
    uint16_t m_port = 11434;
    std::string m_ollamaExePath;
    std::atomic<State> m_state = State::Stopped;
    DWORD m_processId = 0;
    HANDLE m_processHandle = nullptr;
    std::string m_version;

    std::thread m_monitorThread;
    std::atomic<bool> m_shouldMonitor = false;
    std::mutex m_mutex;

    OutputCallback m_outputCallback;
    StateChangeCallback m_stateChangeCallback;

    // Internal helpers
    void MonitorProcess();
    void SetState(State newState);
    bool LaunchOllamaProcess();
    bool TerminateOllamaProcess();
    std::string QueryOllamaVersion();
    bool WaitForHealthcheck(int maxAttempts = 30);

    // Output handling
    void LogOutput(const std::string& message);
};

}  // namespace RawrXD
