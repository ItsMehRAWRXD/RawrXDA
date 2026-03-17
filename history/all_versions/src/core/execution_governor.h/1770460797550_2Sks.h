// ============================================================================
// execution_governor.h — Phase 10A: Execution Governor + Terminal Watchdog
// ============================================================================
//
// The spine of autonomous execution safety. Every external action (terminal
// command, tool invocation, agent action, background job) routes through
// this governor or it does not execute.
//
// Guarantees:
//   - No blocking calls on UI or agent threads
//   - Every task has: timeout, cancellation, hard kill path
//   - Terminal commands cannot hang the system
//   - Agents remain live even if tasks fail
//   - Partial output captured even on timeout
//
// Architecture:
//   - GovernoredTask: wraps any external action with timeout + state
//   - ExecutionGovernor: singleton scheduler with watchdog thread
//   - TerminalWatchdog: non-blocking CreateProcess + PeekNamedPipe wrapper
//
// Build:  Compiled as part of RawrXD-Win32IDE target
// Rule:   NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once
#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <cstdint>

// ============================================================================
// ENUMS
// ============================================================================

enum class GovernorTaskState {
    Pending     = 0,
    Running     = 1,
    Completed   = 2,
    Cancelled   = 3,
    TimedOut    = 4,
    Killed      = 5,
    Failed      = 6,
    Count       = 7
};

enum class GovernorTaskType {
    TerminalCommand   = 0,
    ToolInvocation    = 1,
    AgentAction       = 2,
    BackgroundJob     = 3,
    FileOperation     = 4,
    NetworkRequest    = 5,
    BuildTask         = 6,
    Count             = 7
};

enum class GovernorRiskTier {
    None     = 0,   // Read-only, no side effects
    Low      = 1,   // Local reads, temp files
    Medium   = 2,   // File writes, git ops
    High     = 3,   // Network, process exec
    Critical = 4,   // System modification, admin ops
    Count    = 5
};

// ============================================================================
// STRUCTS
// ============================================================================

// Unique task identifier (monotonic, never reused)
using GovernorTaskId = uint64_t;

// Result of a governed command execution
struct GovernorCommandResult {
    std::string output;           // Captured stdout+stderr (partial on timeout)
    int         exitCode;         // Process exit code (-1 if killed/timeout)
    bool        timedOut;         // True if watchdog killed the process
    bool        cancelled;        // True if cancelled by user/agent
    double      durationMs;       // Wall-clock execution time
    uint64_t    bytesRead;        // Total bytes captured from pipe
    std::string statusDetail;     // Human-readable status string
};

// A single governed task in the scheduler
struct GovernoredTask {
    GovernorTaskId        id;
    GovernorTaskType      type;
    GovernorRiskTier      risk;
    std::string           description;       // Human-readable task name
    std::string           command;            // Shell command (for terminal tasks)
    uint64_t              timeoutMs;          // Hard timeout in milliseconds
    std::atomic<GovernorTaskState> state;
    HANDLE                hProcess;           // Win32 process handle (if applicable)
    HANDLE                hReadPipe;          // Stdout/stderr read pipe
    HANDLE                hThread;            // Win32 thread handle
    std::string           outputBuffer;       // Accumulated output
    uint64_t              bytesRead;          // Total bytes captured
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    std::function<void()> cancelFn;           // Custom cancel callback
    std::function<void(const GovernorCommandResult&)> onComplete;  // Completion callback
    bool                  rollbackEnabled;    // If true, rollback on failure
    std::function<void()> rollbackFn;         // Rollback callback

    GovernoredTask()
        : id(0), type(GovernorTaskType::TerminalCommand),
          risk(GovernorRiskTier::Low), timeoutMs(4000),
          state(GovernorTaskState::Pending),
          hProcess(nullptr), hReadPipe(nullptr), hThread(nullptr),
          bytesRead(0), rollbackEnabled(false) {}
};

// Stats for the governor
struct GovernorStats {
    uint64_t totalSubmitted   = 0;
    uint64_t totalCompleted   = 0;
    uint64_t totalTimedOut    = 0;
    uint64_t totalKilled      = 0;
    uint64_t totalCancelled   = 0;
    uint64_t totalFailed      = 0;
    uint64_t totalRollbacks   = 0;
    double   totalElapsedMs   = 0.0;
    double   avgTaskDurationMs= 0.0;
    uint64_t longestTaskMs    = 0;
    uint64_t activeTaskCount  = 0;
    uint64_t peakConcurrent   = 0;
};

// ============================================================================
// TERMINAL WATCHDOG — Non-blocking CreateProcess + PeekNamedPipe
// ============================================================================
//
// Executes a shell command via CreateProcess with redirected pipes.
// PeekNamedPipe ensures read operations never block.
// Hard kill via TerminateProcess if timeout exceeded.
//
class TerminalWatchdog {
public:
    // Execute command with hard timeout.
    // Returns partial output even if timed out.
    // This is a BLOCKING call that internally polls non-blockingly.
    static GovernorCommandResult ExecuteSafe(
        const std::string& command,
        uint64_t maxDurationMs,
        std::atomic<GovernorTaskState>* statePtr = nullptr);

    // Spawn a process and return handles for external management.
    // Caller is responsible for polling and cleanup.
    static bool SpawnProcess(
        const std::string& command,
        HANDLE& outProcess,
        HANDLE& outReadPipe,
        HANDLE& outThread);

    // Non-blocking poll: read available data from pipe, check timeout.
    // Returns false when process is done and pipe is drained.
    static bool PollOnce(
        HANDLE hProcess,
        HANDLE hReadPipe,
        std::string& outputBuffer,
        uint64_t& bytesRead,
        uint64_t startTickMs,
        uint64_t maxDurationMs,
        bool& timedOut);

    // Hard kill a process
    static void KillProcess(HANDLE hProcess, HANDLE hThread, HANDLE hReadPipe);

    // Default timeout for agent commands (milliseconds)
    static constexpr uint64_t DEFAULT_AGENT_TIMEOUT_MS = 4000;

    // Default timeout for build commands (longer)
    static constexpr uint64_t DEFAULT_BUILD_TIMEOUT_MS = 120000;

    // Maximum concurrent processes
    static constexpr int MAX_CONCURRENT_PROCESSES = 16;
};

// ============================================================================
// EXECUTION GOVERNOR — Singleton scheduler with watchdog thread
// ============================================================================
//
// All external actions go through this. It:
//   1. Assigns a unique task ID
//   2. Enforces timeout via background watchdog
//   3. Manages concurrent task limits
//   4. Provides kill/cancel for any running task
//   5. Captures partial output even on timeout
//   6. Fires completion callbacks
//   7. Maintains statistics
//
class ExecutionGovernor {
public:
    // Singleton access
    static ExecutionGovernor& instance();

    // Initialize the governor (starts watchdog thread)
    bool init();

    // Shutdown (kills all running tasks, stops watchdog)
    void shutdown();

    // Submit a terminal command for governed execution.
    // Returns task ID immediately (non-blocking).
    // The command runs asynchronously; use getTaskResult() to check.
    GovernorTaskId submitCommand(
        const std::string& command,
        uint64_t timeoutMs = TerminalWatchdog::DEFAULT_AGENT_TIMEOUT_MS,
        GovernorRiskTier risk = GovernorRiskTier::Medium,
        const std::string& description = "",
        std::function<void(const GovernorCommandResult&)> onComplete = nullptr);

    // Submit a generic task (tool invocation, agent action, etc.)
    GovernorTaskId submitTask(GovernoredTask&& task);

    // Cancel a running task (cooperative first, then hard kill)
    bool cancelTask(GovernorTaskId id);

    // Hard kill a task immediately
    bool killTask(GovernorTaskId id);

    // Get result of a completed task (returns false if still running)
    bool getTaskResult(GovernorTaskId id, GovernorCommandResult& outResult);

    // Check if a task is still active
    bool isTaskActive(GovernorTaskId id) const;

    // Get current state of a task
    GovernorTaskState getTaskState(GovernorTaskId id) const;

    // Block until task completes (with own timeout — for synchronous callers)
    GovernorCommandResult waitForTask(GovernorTaskId id, uint64_t waitMs = 30000);

    // Get stats
    GovernorStats getStats() const;

    // Get status string for display
    std::string getStatusString() const;

    // Get list of active task descriptions
    std::vector<std::string> getActiveTaskDescriptions() const;

    // Kill all running tasks
    void killAll();

    // Set maximum concurrent tasks
    void setMaxConcurrent(int max);

    // Check if governor is initialized
    bool isInitialized() const { return m_initialized.load(); }

private:
    ExecutionGovernor();
    ~ExecutionGovernor();
    ExecutionGovernor(const ExecutionGovernor&) = delete;
    ExecutionGovernor& operator=(const ExecutionGovernor&) = delete;

    // Watchdog thread: polls all running tasks, enforces timeouts
    void watchdogLoop();

    // Worker thread: executes a single terminal command
    void executeCommandWorker(GovernorTaskId taskId);

    // Finalize a task (set state, fire callback, update stats)
    void finalizeTask(GovernorTaskId taskId, GovernorTaskState finalState);

    // Cleanup completed/old tasks from the map
    void pruneCompletedTasks();

    std::atomic<bool>                            m_initialized;
    std::atomic<bool>                            m_running;
    std::thread                                  m_watchdogThread;
    mutable std::mutex                           m_mutex;
    std::map<GovernorTaskId, std::unique_ptr<GovernoredTask>>     m_tasks;
    GovernorTaskId                               m_nextId;
    GovernorStats                                m_stats;
    int                                          m_maxConcurrent;
    std::atomic<int>                             m_activeTasks;

    // Prune threshold: remove tasks older than this
    static constexpr int PRUNE_AFTER_SECONDS = 300;
    static constexpr int MAX_RETAINED_TASKS  = 1000;
};

