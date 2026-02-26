// ============================================================================
// agentic_task_graph.hpp — DAG-Based Persistent Task Orchestrator
// ============================================================================
// Provides Cursor "Agent" / Copilot "Workspace" equivalent:
//   - Persistent multi-step task execution with DAG scheduling
//   - Checkpoint/resume with on-disk persistence
//   - Auto-rollback on failure (git-based + in-memory snapshots)
//   - Unified orchestration of all 7 AI subsystems
//   - Topological execution with parallelism where deps allow
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
/* Qt removed */
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <thread>
#include <queue>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Result type — PatchResult-compatible
// ============================================================================
struct TaskResult {
    bool success;
    const char* detail;
    int errorCode;

    static TaskResult ok(const char* msg = "OK") {
        TaskResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static TaskResult error(const char* msg, int code = -1) {
        TaskResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Task Priority Levels
// ============================================================================
enum class TaskPriority : uint8_t {
    CRITICAL   = 0,   // System-critical, never preempted
    HIGH       = 1,   // User-initiated "refactor this repo"
    NORMAL     = 2,   // Standard AI operations
    LOW        = 3,   // Background indexing, precompute
    IDLE       = 4    // Only when system is otherwise idle
};

// ============================================================================
// Task State Machine
// ============================================================================
enum class TaskState : uint8_t {
    PENDING     = 0,  // Waiting for dependencies
    READY       = 1,  // All deps satisfied, waiting for executor
    RUNNING     = 2,  // Currently executing
    PAUSED      = 3,  // Checkpointed, waiting for resume
    SUCCEEDED   = 4,  // Completed successfully
    FAILED      = 5,  // Failed, awaiting rollback
    ROLLED_BACK = 6,  // Rolled back after failure
    CANCELLED   = 7,  // User-cancelled
    SKIPPED     = 8   // Skipped (dep failed and not retryable)
};

// ============================================================================
// Rollback Strategy
// ============================================================================
enum class RollbackStrategy : uint8_t {
    NONE           = 0,  // No rollback
    GIT_REVERT     = 1,  // Revert git commits
    SNAPSHOT        = 2,  // Restore in-memory snapshot
    UNDO_LOG       = 3,  // Replay inverse operations
    FULL_RESTORE   = 4   // Snapshot + git revert + cache invalidation
};

// ============================================================================
// TaskNode — Individual unit of work in the DAG
// ============================================================================
struct TaskNode {
    uint64_t id;
    std::string name;            // Human-readable: "Parse symbols in src/"
    std::string description;     // Detailed description for LLM context
    TaskPriority priority;
    TaskState state;
    RollbackStrategy rollback;

    // DAG edges
    std::vector<uint64_t> dependsOn;       // Upstream task IDs
    std::vector<uint64_t> dependedOnBy;    // Downstream task IDs

    // Execution metadata
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point completedAt;
    uint32_t progressPercentage;
    uint32_t retryCount;
    uint32_t maxRetries;
    uint32_t timeoutSeconds;

    // Subsystem routing — which AI system handles this
    enum class Subsystem : uint8_t {
        COMPLETION_ENGINE = 0,     // Real-time code completion
        REFACTOR_ENGINE   = 1,     // Safe refactoring pipeline
        AGENTIC_LOOP      = 2,     // Bounded agent loop (tool-calling)
        SWARM_ENGINE      = 3,     // Distributed compilation/analysis
        INFERENCE_CORE    = 4,     // Raw model inference
        CONTEXT_ANALYZER  = 5,     // Symbol indexing / embeddings
        HOTPATCH_ENGINE   = 6,     // Live binary patching
        CUSTOM            = 7      // User-defined handler
    };
    Subsystem subsystem;

    // Execution handler (set by orchestrator)
    using ExecutorFn = TaskResult(*)(TaskNode* self, void* context);
    ExecutorFn executor;
    void* executorContext;

    // Checkpoint blob (serialized state for resume)
    std::vector<uint8_t> checkpointData;
    uint64_t checkpointVersion;

    // Rollback data (inverse operations or snapshot ref)
    std::vector<uint8_t> rollbackData;
    std::string rollbackGitRef;    // Git commit hash to revert to

    // Output data (result passed to downstream tasks)
    std::vector<uint8_t> outputData;
    std::string outputSummary;     // Human-readable summary

    TaskNode()
        : id(0), priority(TaskPriority::NORMAL), state(TaskState::PENDING),
          rollback(RollbackStrategy::SNAPSHOT), progressPercentage(0),
          retryCount(0), maxRetries(3), timeoutSeconds(300),
          subsystem(Subsystem::AGENTIC_LOOP), executor(nullptr),
          executorContext(nullptr), checkpointVersion(0) {}
};

// ============================================================================
// TaskGraph — The DAG container
// ============================================================================
struct TaskGraphConfig {
    uint32_t maxParallel;          // Max concurrent task execution (default: 4)
    uint32_t checkpointIntervalMs; // Auto-checkpoint interval (default: 5000)
    bool enableAutoRollback;       // Auto-rollback failed tasks (default: true)
    bool enableAutoRetry;          // Auto-retry transient failures (default: true)
    bool persistToDisk;            // Save state to disk (default: true)
    std::string persistPath;       // Path for persistence (default: ".rawrxd/task_graphs/")
    uint32_t maxGraphHistory;      // Max completed graphs to keep (default: 50)
    bool enableTelemetry;          // /* emit */ telemetry events (default: true)

    TaskGraphConfig()
        : maxParallel(4), checkpointIntervalMs(5000),
          enableAutoRollback(true), enableAutoRetry(true),
          persistToDisk(true), persistPath(".rawrxd/task_graphs/"),
          maxGraphHistory(50), enableTelemetry(true) {}
};

// ============================================================================
// TaskGraphEvent — Observable events for UI/telemetry
// ============================================================================
struct TaskGraphEvent {
    enum Type : uint8_t {
        GRAPH_CREATED       = 0,
        GRAPH_STARTED       = 1,
        GRAPH_COMPLETED     = 2,
        GRAPH_FAILED        = 3,
        TASK_STARTED        = 4,
        TASK_COMPLETED      = 5,
        TASK_FAILED         = 6,
        TASK_RETRYING       = 7,
        TASK_ROLLED_BACK    = 8,
        TASK_CHECKPOINTED   = 9,
        TASK_RESUMED        = 10,
        TASK_CANCELLED      = 11,
        DEPENDENCY_RESOLVED = 12,
        PARALLELISM_CHANGED = 13
    };

    Type type;
    uint64_t graphId;
    uint64_t taskId;
    uint32_t progressPercentage;
    const char* detail;
    std::chrono::steady_clock::time_point timestamp;
};

// Event callback — function pointer per project rules (not std::function)
using TaskGraphEventCallback = void(*)(const TaskGraphEvent& evt, void* userData);

// ============================================================================
// AgenticTaskGraph — The main orchestrator
// ============================================================================
class AgenticTaskGraph {
public:
    static AgenticTaskGraph& instance();

    // -----------------------------------------------------------------------
    // Graph lifecycle
    // -----------------------------------------------------------------------

    // Create a new empty graph for a high-level goal
    // e.g., "Refactor authentication module across 47 files"
    uint64_t createGraph(const std::string& goalDescription,
                         const TaskGraphConfig& config = TaskGraphConfig());

    // Add a task to the graph. Returns task ID.
    uint64_t addTask(uint64_t graphId, const TaskNode& task);

    // Add dependency: taskId depends on dependsOnId completing first
    TaskResult addDependency(uint64_t graphId,
                             uint64_t taskId,
                             uint64_t dependsOnId);

    // Validate graph: check for cycles, unreachable nodes, missing executors
    TaskResult validateGraph(uint64_t graphId);

    // Execute the entire graph (topological order, parallel where possible)
    TaskResult executeGraph(uint64_t graphId);

    // Cancel a running graph (completes in-flight, cancels pending)
    TaskResult cancelGraph(uint64_t graphId);

    // Pause a running graph (checkpoints in-flight tasks)
    TaskResult pauseGraph(uint64_t graphId);

    // Resume a paused graph from last checkpoint
    TaskResult resumeGraph(uint64_t graphId);

    // -----------------------------------------------------------------------
    // Task-level operations
    // -----------------------------------------------------------------------

    // Get task state
    TaskState getTaskState(uint64_t graphId, uint64_t taskId) const;

    // Update task progress (called by executor)
    void updateTaskProgress(uint64_t graphId, uint64_t taskId,
                            uint32_t progressPercentage);

    // Force-retry a failed task
    TaskResult retryTask(uint64_t graphId, uint64_t taskId);

    // Skip a task (marks as SKIPPED, unblocks dependents)
    TaskResult skipTask(uint64_t graphId, uint64_t taskId);

    // Manually rollback a specific task
    TaskResult rollbackTask(uint64_t graphId, uint64_t taskId);

    // -----------------------------------------------------------------------
    // Graph-level rollback
    // -----------------------------------------------------------------------

    // Rollback entire graph: reverse-topological order
    TaskResult rollbackGraph(uint64_t graphId);

    // Rollback to a specific checkpoint
    TaskResult rollbackToCheckpoint(uint64_t graphId, uint64_t checkpointId);

    // -----------------------------------------------------------------------
    // Persistence
    // -----------------------------------------------------------------------

    // Save graph state to disk (JSON)
    TaskResult saveGraph(uint64_t graphId, const char* filepath = nullptr);

    // Load graph from disk
    uint64_t loadGraph(const char* filepath);

    // List saved graphs
    std::vector<std::string> listSavedGraphs() const;

    // -----------------------------------------------------------------------
    // Natural language interface (LLM-guided planning)
    // -----------------------------------------------------------------------

    // Decompose a high-level goal into a task graph using LLM
    // e.g., "Refactor this repo to use dependency injection"
    // → generates 15-50 TaskNodes with proper dependencies
    uint64_t planFromGoal(const std::string& goal,
                          const TaskGraphConfig& config = TaskGraphConfig());

    // Replan: when a task fails, ask LLM for alternative approach
    TaskResult replanFromFailure(uint64_t graphId, uint64_t failedTaskId);

    // -----------------------------------------------------------------------
    // Monitoring / Statistics
    // -----------------------------------------------------------------------

    struct GraphStats {
        uint64_t graphId;
        uint32_t totalTasks;
        uint32_t pendingTasks;
        uint32_t runningTasks;
        uint32_t completedTasks;
        uint32_t failedTasks;
        uint32_t rolledBackTasks;
        uint32_t skippedTasks;
        uint32_t overallProgressPercentage;
        std::chrono::milliseconds elapsedTime;
        std::chrono::milliseconds estimatedRemaining;
        uint32_t currentParallelism;
    };

    GraphStats getGraphStats(uint64_t graphId) const;

    // Event subscription (function pointer + userData)
    void addEventListener(TaskGraphEventCallback callback, void* userData);
    void removeEventListener(TaskGraphEventCallback callback);

    // -----------------------------------------------------------------------
    // Built-in task executors for common operations
    // -----------------------------------------------------------------------

    static TaskResult executor_codeRefactor(TaskNode* self, void* context);
    static TaskResult executor_symbolIndex(TaskNode* self, void* context);
    static TaskResult executor_runTests(TaskNode* self, void* context);
    static TaskResult executor_buildProject(TaskNode* self, void* context);
    static TaskResult executor_agenticLoop(TaskNode* self, void* context);
    static TaskResult executor_hotpatch(TaskNode* self, void* context);
    static TaskResult executor_swarmCompile(TaskNode* self, void* context);

    // Shutdown: cancel all, persist state
    void shutdown();

private:
    AgenticTaskGraph();
    ~AgenticTaskGraph();
    AgenticTaskGraph(const AgenticTaskGraph&) = delete;
    AgenticTaskGraph& operator=(const AgenticTaskGraph&) = delete;

    // -----------------------------------------------------------------------
    // Internal graph representation
    // -----------------------------------------------------------------------
    struct Graph {
        uint64_t id;
        std::string goal;
        TaskGraphConfig config;
        std::unordered_map<uint64_t, TaskNode> tasks;
        std::chrono::steady_clock::time_point createdAt;
        std::chrono::steady_clock::time_point startedAt;
        bool isRunning;
        bool isPaused;
        bool isCompleted;
        std::atomic<uint32_t> activeTasks;

        Graph() : id(0), isRunning(false), isPaused(false),
                  isCompleted(false), activeTasks(0) {}
        Graph(Graph&& o) noexcept
            : id(o.id), goal(std::move(o.goal)), config(std::move(o.config)),
              tasks(std::move(o.tasks)), createdAt(o.createdAt),
              startedAt(o.startedAt), isRunning(o.isRunning),
              isPaused(o.isPaused), isCompleted(o.isCompleted),
              activeTasks(o.activeTasks.load(std::memory_order_relaxed)) {}
        Graph& operator=(Graph&& o) noexcept {
            id = o.id; goal = std::move(o.goal); config = std::move(o.config);
            tasks = std::move(o.tasks); createdAt = o.createdAt;
            startedAt = o.startedAt; isRunning = o.isRunning;
            isPaused = o.isPaused; isCompleted = o.isCompleted;
            activeTasks.store(o.activeTasks.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }
    };

    // Topological sort with cycle detection
    TaskResult topologicalSort(const Graph& g,
                               std::vector<uint64_t>& sorted) const;

    // Find tasks ready to execute (all deps satisfied)
    std::vector<uint64_t> findReadyTasks(const Graph& g) const;

    // Execute a single task (with timeout, checkpoint, rollback)
    TaskResult executeTask(Graph& g, uint64_t taskId);

    // Checkpoint a running task
    TaskResult checkpointTask(Graph& g, uint64_t taskId);

    // Perform rollback for a task
    TaskResult performRollback(Graph& g, uint64_t taskId);

    // /* emit */ event to all listeners
    void emitEvent(const TaskGraphEvent& evt);

    // Persistence serializer
    TaskResult serializeGraph(const Graph& g, std::string& jsonOut) const;
    TaskResult deserializeGraph(const std::string& json, Graph& g);

    // Worker thread pool
    void workerThread();

    // Auto-checkpoint thread
    void checkpointThread();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex graphMutex_;
    std::unordered_map<uint64_t, Graph> graphs_;
    uint64_t nextGraphId_;
    uint64_t nextTaskId_;

    // Worker pool
    std::vector<std::thread> workers_;
    std::queue<std::pair<uint64_t, uint64_t>> workQueue_;  // (graphId, taskId)
    std::mutex workMutex_;
    std::condition_variable workCV_;
    std::atomic<bool> shutdownRequested_;

    // Event ring buffer
    static constexpr size_t MAX_EVENT_LISTENERS = 16;
    struct EventListener {
        TaskGraphEventCallback callback;
        void* userData;
    };
    EventListener eventListeners_[MAX_EVENT_LISTENERS];
    std::atomic<uint32_t> eventListenerCount_;

    // Auto-checkpoint
    std::thread checkpointThread_;
    std::atomic<bool> checkpointRunning_;
};

} // namespace Agentic
} // namespace RawrXD
