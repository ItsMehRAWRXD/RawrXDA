#pragma once

/**
 * @file AsyncTerminalManager.h
 * @brief Thread-safe async terminal manager with QThreadPool integration
 * 
 * Provides non-blocking terminal operations using QtConcurrent::run()
 * for PowerShell/CMD execution with persistent session support.
 * 
 * Features:
 * - Async command execution without blocking UI
 * - Persistent terminal sessions with state preservation
 * - Thread-safe signal-slot communication
 * - Support for PowerShell, CMD, and other shells
 * - Background process management
 * - Terminal session serialization/restoration
 */

#include <QObject>
#include <QProcess>
#include <QString>
#include <QThreadPool>
#include <QMutex>
#include <QQueue>
#include <QMap>

namespace RawrXD {

/**
 * @brief Terminal session state for persistence
 */
struct TerminalSession {
    QString sessionId;           ///< Unique session identifier
    QString shellType;           ///< "PowerShell", "CMD", "Bash", etc.
    QString workingDirectory;    ///< Current working directory
    QStringList commandHistory;  ///< Command history
    QMap<QString, QString> environment;  ///< Environment variables
    QDateTime created;           ///< Session creation time
    QDateTime lastUsed;          ///< Last command execution time
    qint64 pid;                  ///< Process ID (0 if not running)
    
    TerminalSession() : pid(0) {}
    
    /**
     * @brief Serialize session to JSON
     */
    QByteArray toJson() const;
    
    /**
     * @brief Restore session from JSON
     */
    static TerminalSession fromJson(const QByteArray& json);
};

/**
 * @brief Async task for terminal command execution
 */
class TerminalTask {
public:
    QString sessionId;
    QString command;
    bool waitForCompletion;
    int timeoutMs;
    
    TerminalTask(const QString& sid, const QString& cmd, bool wait = true, int timeout = 30000)
        : sessionId(sid), command(cmd), waitForCompletion(wait), timeoutMs(timeout) {}
};

/**
 * @brief Result of terminal command execution
 */
struct TerminalResult {
    bool success;
    int exitCode;
    QString stdout;
    QString stderr;
    qint64 executionTimeMs;
    
    TerminalResult() : success(false), exitCode(-1), executionTimeMs(0) {}
};

/**
 * @brief Thread-safe async terminal manager
 * 
 * Manages multiple terminal sessions with async execution using QThreadPool.
 * Supports persistent sessions that can be saved/restored across IDE restarts.
 */
class AsyncTerminalManager : public QObject
{
    Q_OBJECT
    
public:
    enum ShellType {
        PowerShell,
        CommandPrompt,
        Bash,
        Python,
        NodeJS
    };
    
    explicit AsyncTerminalManager(QObject* parent = nullptr);
    ~AsyncTerminalManager() override;
    
    /**
     * @brief Create a new terminal session
     * @param shellType Type of shell to create
     * @param workingDir Initial working directory
     * @return Session ID for the new session
     */
    QString createSession(ShellType shellType, const QString& workingDir = QString());
    
    /**
     * @brief Execute a command asynchronously
     * @param sessionId Session to execute command in
     * @param command Command to execute
     * @param waitForCompletion If true, wait for command to finish
     * @return Task ID for tracking execution
     */
    QString executeCommandAsync(const QString& sessionId, const QString& command, bool waitForCompletion = false);
    
    /**
     * @brief Execute a command synchronously (blocking)
     * @param sessionId Session to execute command in
     * @param command Command to execute
     * @param timeoutMs Maximum time to wait
     * @return Execution result
     */
    TerminalResult executeCommandSync(const QString& sessionId, const QString& command, int timeoutMs = 30000);
    
    /**
     * @brief Close a terminal session
     * @param sessionId Session to close
     * @param saveState If true, save session state for restoration
     */
    void closeSession(const QString& sessionId, bool saveState = true);
    
    /**
     * @brief Get active sessions
     * @return Map of session ID -> session info
     */
    QMap<QString, TerminalSession> getActiveSessions() const;
    
    /**
     * @brief Check if a session is running
     */
    bool isSessionRunning(const QString& sessionId) const;
    
    /**
     * @brief Get process ID for a session
     */
    qint64 getSessionPid(const QString& sessionId) const;
    
    /**
     * @brief Save all sessions to disk
     * @param path Directory to save session files
     */
    bool saveSessions(const QString& path);
    
    /**
     * @brief Restore sessions from disk
     * @param path Directory containing session files
     */
    bool restoreSessions(const QString& path);
    
    /**
     * @brief Set maximum concurrent terminal processes
     */
    void setMaxConcurrentProcesses(int max);
    
    /**
     * @brief Get thread pool statistics
     */
    QString getThreadPoolStats() const;

signals:
    /**
     * @brief Emitted when a session is created
     */
    void sessionCreated(const QString& sessionId, const QString& shellType);
    
    /**
     * @brief Emitted when a session is closed
     */
    void sessionClosed(const QString& sessionId);
    
    /**
     * @brief Emitted when command execution starts
     */
    void commandStarted(const QString& sessionId, const QString& taskId, const QString& command);
    
    /**
     * @brief Emitted when command execution completes
     */
    void commandCompleted(const QString& sessionId, const QString& taskId, const TerminalResult& result);
    
    /**
     * @brief Emitted when stdout data is available
     */
    void stdoutReady(const QString& sessionId, const QByteArray& data);
    
    /**
     * @brief Emitted when stderr data is available
     */
    void stderrReady(const QString& sessionId, const QByteArray& data);
    
    /**
     * @brief Emitted when a session changes working directory
     */
    void workingDirectoryChanged(const QString& sessionId, const QString& newDir);

private:
    /**
     * @brief Execute command in background thread
     */
    TerminalResult executeInBackground(const TerminalTask& task);
    
    /**
     * @brief Get or create process for session
     */
    QProcess* getOrCreateProcess(const QString& sessionId);
    
    /**
     * @brief Generate unique task ID
     */
    QString generateTaskId();
    
    /**
     * @brief Get shell program and arguments
     */
    QPair<QString, QStringList> getShellCommand(ShellType type);
    
    /**
     * @brief Save session state to file
     */
    bool saveSessionToFile(const TerminalSession& session, const QString& path);
    
    /**
     * @brief Load session state from file
     */
    TerminalSession loadSessionFromFile(const QString& path);

    struct SessionData {
        TerminalSession session;
        QProcess* process;
        QMutex mutex;
        
        SessionData() : process(nullptr) {}
    };
    
    mutable QMutex m_sessionsMutex;                  ///< Protects m_sessions
    QMap<QString, SessionData*> m_sessions;          ///< Active sessions
    QThreadPool* m_threadPool;                       ///< Thread pool for async execution
    QMap<QString, TerminalTask> m_pendingTasks;      ///< Tasks waiting for execution
    int m_taskCounter;                               ///< For generating unique task IDs
};

} // namespace RawrXD
