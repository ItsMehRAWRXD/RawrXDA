/**
 * @file terminal_pool.hpp
 * @brief Terminal pool management for concurrent terminal sessions
 */

#ifndef TERMINAL_POOL_HPP_INCLUDED
#define TERMINAL_POOL_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <functional>
#include <atomic>
#include <unordered_map>

/**
 * @struct TerminalSession
 * @brief Represents a single terminal session
 */
struct TerminalSession {
    int id = 0;
    std::string name;
    std::string workingDirectory;
    bool active = false;
    bool busy = false;
    
    // Platform-specific handle
#ifdef _WIN32
    void* processHandle = nullptr;
    void* inputPipe = nullptr;
    void* outputPipe = nullptr;
#else
    int pid = -1;
    int inputFd = -1;
    int outputFd = -1;
#endif
    
    std::string outputBuffer;
    std::string errorBuffer;
};

/**
 * @struct CommandResult
 * @brief Result of executing a command
 */
struct CommandResult {
    bool success = false;
    std::string output;
    std::string error;
    int exitCode = 0;
    double executionTimeMs = 0.0;
    int terminalId = -1;
};

/**
 * @class TerminalPool
 * @brief Manages a pool of terminal sessions for concurrent command execution
 */
class TerminalPool {
public:
    explicit TerminalPool(int poolSize = 4);
    ~TerminalPool();

    // Pool management
    void initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }
    int getPoolSize() const { return static_cast<int>(m_sessions.size()); }
    int getActiveCount() const;
    int getAvailableCount() const;

    // Terminal session management
    int createSession(const std::string& name = "");
    bool closeSession(int sessionId);
    TerminalSession* getSession(int sessionId);
    std::vector<int> getActiveSessionIds() const;

    // Command execution
    CommandResult executeCommand(const std::string& command, int sessionId = -1);
    CommandResult executeCommandAsync(const std::string& command, int sessionId = -1);
    bool sendInput(int sessionId, const std::string& input);
    std::string readOutput(int sessionId, int timeoutMs = 1000);
    std::string readError(int sessionId, int timeoutMs = 1000);

    // Working directory
    void setDefaultWorkingDirectory(const std::string& path);
    std::string getDefaultWorkingDirectory() const;
    bool setSessionWorkingDirectory(int sessionId, const std::string& path);

    // Shell configuration
    void setShell(const std::string& shell);
    std::string getShell() const;
    void setEnvironmentVariable(const std::string& name, const std::string& value);
    void clearEnvironmentVariable(const std::string& name);

    // Callbacks
    using OutputCallback = std::function<void(int sessionId, const std::string& output)>;
    using ErrorCallback = std::function<void(int sessionId, const std::string& error)>;
    using SessionCallback = std::function<void(int sessionId)>;

    void setOutputCallback(OutputCallback cb) { m_onOutput = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { m_onError = std::move(cb); }
    void setSessionCreatedCallback(SessionCallback cb) { m_onSessionCreated = std::move(cb); }
    void setSessionClosedCallback(SessionCallback cb) { m_onSessionClosed = std::move(cb); }

    // Statistics
    struct Statistics {
        int totalCommandsExecuted = 0;
        int successfulCommands = 0;
        int failedCommands = 0;
        double totalExecutionTimeMs = 0.0;
        double averageExecutionTimeMs = 0.0;
    };
    Statistics getStatistics() const;
    void resetStatistics();

private:
    // Internal helpers
    int acquireSession();
    void releaseSession(int sessionId);
    bool spawnProcess(TerminalSession& session);
    void terminateProcess(TerminalSession& session);
    std::string readFromPipe(TerminalSession& session, bool isStderr, int timeoutMs);
    bool writeToPipe(TerminalSession& session, const std::string& data);
    void outputReaderThread(int sessionId);

    // Session pool
    std::vector<std::unique_ptr<TerminalSession>> m_sessions;
    mutable std::mutex m_sessionsMutex;
    std::condition_variable m_sessionAvailable;
    int m_nextSessionId = 1;

    // Configuration
    std::string m_shell;
    std::string m_defaultWorkingDirectory;
    std::unordered_map<std::string, std::string> m_environment;
    int m_poolSize;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};

    // Statistics
    mutable std::mutex m_statsMutex;
    Statistics m_stats;

    // Callbacks
    OutputCallback m_onOutput;
    ErrorCallback m_onError;
    SessionCallback m_onSessionCreated;
    SessionCallback m_onSessionClosed;

    // Reader threads
    std::vector<std::thread> m_readerThreads;
};

#endif // TERMINAL_POOL_HPP_INCLUDED
