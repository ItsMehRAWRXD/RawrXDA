// distributed_pipeline_orchestrator.hpp — Phase 13: Distributed Pipeline Orchestrator
// DAG-based task scheduling with work-stealing thread pool,
// pipeline parallelism, deadline-aware priority queues, and
// dynamic load balancing across heterogeneous compute nodes.
//
// Architecture: Lock-free ring buffers for inter-stage communication,
//               topological sort for dependency resolution,
//               exponential backoff for retry logic.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#pragma once

#ifndef RAWRXD_DISTRIBUTED_PIPELINE_ORCHESTRATOR_HPP
#define RAWRXD_DISTRIBUTED_PIPELINE_ORCHESTRATOR_HPP

#include "model_memory_hotpatch.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <functional>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cstring>

// ============================================================================
// Forward Declarations
// ============================================================================
struct PipelineNode;
struct PipelineEdge;
struct PipelineDAG;
struct WorkItem;

// ============================================================================
// Pipeline Task Priority
// ============================================================================
enum class TaskPriority : uint8_t {
    Critical    = 0,   // Deadline-sensitive, preemptable
    High        = 1,
    Normal      = 2,
    Low         = 3,
    Background  = 4
};

// ============================================================================
// Pipeline Stage State
// ============================================================================
enum class StageState : uint8_t {
    Pending     = 0,
    Queued      = 1,
    Running     = 2,
    Completed   = 3,
    Failed      = 4,
    Cancelled   = 5,
    TimedOut    = 6,
    Retrying    = 7
};

// ============================================================================
// Node Affinity — Hardware / location preferences
// ============================================================================
enum class NodeAffinity : uint8_t {
    Any         = 0,
    CPUOnly     = 1,
    GPUPreferred = 2,
    GPURequired = 3,
    LocalOnly   = 4,   // Must run on submitting node
    Remote      = 5    // Offload to remote compute
};

// ============================================================================
// Pipeline Task Result — Structured result for individual tasks
// ============================================================================
struct TaskResult {
    bool        success;
    const char* detail;
    int         errorCode;
    int64_t     executionTimeUs;     // Microseconds
    uint64_t    bytesProcessed;
    uint64_t    taskId;

    static TaskResult ok(uint64_t id, int64_t timeUs = 0, uint64_t bytes = 0) {
        TaskResult r{};
        r.success         = true;
        r.detail          = "Task completed successfully";
        r.errorCode       = 0;
        r.executionTimeUs = timeUs;
        r.bytesProcessed  = bytes;
        r.taskId          = id;
        return r;
    }

    static TaskResult error(uint64_t id, const char* msg, int code = -1) {
        TaskResult r{};
        r.success         = false;
        r.detail          = msg;
        r.errorCode       = code;
        r.executionTimeUs = 0;
        r.bytesProcessed  = 0;
        r.taskId          = id;
        return r;
    }
};

// ============================================================================
// Pipeline Task — Represents a unit of work in the DAG
// ============================================================================
struct PipelineTask {
    uint64_t        id;
    std::string     name;
    TaskPriority    priority;
    NodeAffinity    affinity;
    StageState      state;

    // Execution
    void*           taskData;               // Opaque user data
    bool          (*execute)(void* data, void* output, size_t* outSize);
    void          (*cleanup)(void* data);   // Optional cleanup

    // Timing constraints
    int64_t         deadlineMs;             // 0 = no deadline
    int64_t         timeoutMs;              // Per-execution timeout
    int64_t         estimatedDurationMs;    // For scheduling heuristics

    // Retry policy
    int             maxRetries;
    int             retryCount;
    int64_t         retryBackoffBaseMs;     // Exponential backoff base

    // Dependencies (task IDs that must complete first)
    std::vector<uint64_t> dependencies;

    // Output routing
    std::vector<uint64_t> dependents;       // Tasks waiting on this

    // Metrics
    std::chrono::steady_clock::time_point submitTime;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    // Resource requirements
    uint64_t        requiredMemoryBytes;
    uint32_t        requiredCores;

    PipelineTask()
        : id(0), priority(TaskPriority::Normal), affinity(NodeAffinity::Any)
        , state(StageState::Pending), taskData(nullptr)
        , execute(nullptr), cleanup(nullptr)
        , deadlineMs(0), timeoutMs(30000), estimatedDurationMs(0)
        , maxRetries(3), retryCount(0), retryBackoffBaseMs(100)
        , requiredMemoryBytes(0), requiredCores(1) {}
};

// ============================================================================
// Compute Node — Represents a worker in the cluster
// ============================================================================
struct ComputeNode {
    uint32_t        nodeId;
    std::string     hostname;
    uint32_t        totalCores;
    uint32_t        availableCores;
    uint64_t        totalMemory;
    uint64_t        availableMemory;
    bool            hasGPU;
    uint32_t        gpuCount;
    double          loadAverage;           // 0.0 - 1.0
    bool            alive;
    std::chrono::steady_clock::time_point lastHeartbeat;

    double utilizationScore() const {
        if (!alive) return 999.0;
        return loadAverage + (1.0 - static_cast<double>(availableCores) /
               static_cast<double>(totalCores > 0 ? totalCores : 1));
    }
};

// ============================================================================
// Pipeline Statistics
// ============================================================================
struct PipelineStats {
    std::atomic<uint64_t> tasksSubmitted{0};
    std::atomic<uint64_t> tasksCompleted{0};
    std::atomic<uint64_t> tasksFailed{0};
    std::atomic<uint64_t> tasksCancelled{0};
    std::atomic<uint64_t> tasksTimedOut{0};
    std::atomic<uint64_t> tasksRetried{0};
    std::atomic<uint64_t> tasksStolen{0};        // Work-stealing count
    std::atomic<uint64_t> totalExecutionTimeUs{0};
    std::atomic<uint64_t> totalBytesProcessed{0};
    std::atomic<uint64_t> deadlineMisses{0};
    std::atomic<uint64_t> peakQueueDepth{0};

    // Extended stats — work-stealing and pipeline tracking
    std::atomic<uint64_t> stealsAttempted{0};
    std::atomic<uint64_t> stealsSucceeded{0};
    std::atomic<uint64_t> activePipelines{0};

    double avgExecutionTimeMs() const {
        auto completed = tasksCompleted.load();
        if (completed == 0) return 0.0;
        return static_cast<double>(totalExecutionTimeUs.load()) /
               (static_cast<double>(completed) * 1000.0);
    }

    double successRate() const {
        auto total = tasksSubmitted.load();
        if (total == 0) return 1.0;
        return static_cast<double>(tasksCompleted.load()) /
               static_cast<double>(total);
    }

    double throughputPerSec(int64_t windowMs) const {
        if (windowMs <= 0) return 0.0;
        return static_cast<double>(tasksCompleted.load()) /
               (static_cast<double>(windowMs) / 1000.0);
    }
};

// ============================================================================
// Work-Stealing Deque — Lock-based for correctness (Chase-Lev variant)
// ============================================================================
class WorkStealingDeque {
public:
    explicit WorkStealingDeque(size_t capacity = 4096)
        : m_capacity(capacity), m_buffer(capacity) {}

    // Push to bottom (owner thread)
    bool push(uint64_t taskId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size >= m_capacity) return false;
        m_buffer[(m_bottom) % m_capacity] = taskId;
        m_bottom++;
        m_size++;
        return true;
    }

    // Pop from bottom (owner thread) — LIFO
    bool pop(uint64_t& taskId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) return false;
        m_bottom--;
        m_size--;
        taskId = m_buffer[m_bottom % m_capacity];
        return true;
    }

    // Steal from top (other threads) — FIFO
    bool steal(uint64_t& taskId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) return false;
        taskId = m_buffer[m_top % m_capacity];
        m_top++;
        m_size--;
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size == 0;
    }

private:
    size_t                  m_capacity;
    std::vector<uint64_t>   m_buffer;
    size_t                  m_top    = 0;
    size_t                  m_bottom = 0;
    size_t                  m_size   = 0;
    mutable std::mutex      m_mutex;
};

// ============================================================================
// Callbacks
// ============================================================================
using TaskCompletionCallback = void(*)(const TaskResult& result, void* userData);
using PipelineEventCallback  = void(*)(uint64_t taskId, StageState newState, void* userData);
using NodeFailureCallback    = void(*)(uint32_t nodeId, const char* reason, void* userData);

// ============================================================================
// DistributedPipelineOrchestrator — Main class
// ============================================================================
class DistributedPipelineOrchestrator {
public:
    static DistributedPipelineOrchestrator& instance();

    // ── Lifecycle ──
    PatchResult initialize(uint32_t workerThreads = 0);  // 0 = auto-detect
    PatchResult shutdown();
    bool isRunning() const { return m_running.load(); }

    // ── Task Submission ──
    TaskResult submitTask(PipelineTask& task);
    TaskResult submitBatch(std::vector<PipelineTask>& tasks);
    PatchResult cancelTask(uint64_t taskId);
    PatchResult cancelAll();

    // ── DAG Construction ──
    PatchResult addDependency(uint64_t fromTask, uint64_t toTask);
    PatchResult removeDependency(uint64_t fromTask, uint64_t toTask);
    bool hasCycle() const;
    std::vector<uint64_t> topologicalSort() const;

    // ── Compute Nodes ──
    PatchResult registerNode(const ComputeNode& node);
    PatchResult deregisterNode(uint32_t nodeId);
    PatchResult heartbeat(uint32_t nodeId, double loadAvg, uint32_t freeCores, uint64_t freeMem);
    std::vector<ComputeNode> getNodeStatus() const;
    uint32_t aliveNodeCount() const;

    // ── Pipeline Stages ──
    PatchResult definePipeline(const std::string& name, const std::vector<uint64_t>& stageOrder);
    PatchResult executePipeline(const std::string& name);
    PatchResult pausePipeline(const std::string& name);
    PatchResult resumePipeline(const std::string& name);

    // ── Monitoring ──
    const PipelineStats& getStats() const { return m_stats; }
    void resetStats();
    StageState getTaskState(uint64_t taskId) const;
    TaskResult getTaskResult(uint64_t taskId) const;
    std::vector<uint64_t> getPendingTasks() const;
    std::vector<uint64_t> getRunningTasks() const;
    size_t totalQueueDepth() const;

    // ── Callbacks ──
    void setCompletionCallback(TaskCompletionCallback cb, void* userData);
    void setEventCallback(PipelineEventCallback cb, void* userData);
    void setNodeFailureCallback(NodeFailureCallback cb, void* userData);

    // ── Configuration ──
    void setMaxQueueDepth(size_t depth);
    void setStealingEnabled(bool enabled);
    void setHeartbeatTimeoutMs(int64_t ms);
    void setDefaultTimeoutMs(int64_t ms);

private:
    DistributedPipelineOrchestrator();
    ~DistributedPipelineOrchestrator();
    DistributedPipelineOrchestrator(const DistributedPipelineOrchestrator&) = delete;
    DistributedPipelineOrchestrator& operator=(const DistributedPipelineOrchestrator&) = delete;

    // ── Worker Thread ──
    void workerLoop(uint32_t workerId);
    bool tryExecuteTask(uint32_t workerId);
    bool tryStealWork(uint32_t workerId, uint64_t& taskId);
    void executeTask(uint64_t taskId, uint32_t workerId);
    void handleTaskCompletion(uint64_t taskId, const TaskResult& result);
    void handleTaskFailure(uint64_t taskId, const char* reason, int code);
    void scheduleRetry(uint64_t taskId);

    // ── Scheduling ──
    uint32_t selectBestNode(const PipelineTask& task) const;
    void enqueueReadyTasks();
    bool areDependenciesMet(uint64_t taskId) const;
    int64_t computeBackoff(int retryCount, int64_t baseMs) const;

    // ── Cycle Detection (Kahn's algorithm) ──
    bool detectCycle() const;

    // ── Heartbeat Monitor ──
    void heartbeatMonitorLoop();

    // ── State ──
    mutable std::mutex                              m_mutex;
    std::atomic<bool>                               m_running{false};
    std::atomic<uint64_t>                           m_nextTaskId{1};

    // Task registry
    std::unordered_map<uint64_t, PipelineTask>      m_tasks;
    std::unordered_map<uint64_t, TaskResult>        m_results;

    // Priority queue (min-heap: lower priority value = higher priority)
    struct TaskComparator {
        bool operator()(const std::pair<TaskPriority, uint64_t>& a,
                        const std::pair<TaskPriority, uint64_t>& b) const {
            return static_cast<uint8_t>(a.first) > static_cast<uint8_t>(b.first);
        }
    };
    std::priority_queue<std::pair<TaskPriority, uint64_t>,
                        std::vector<std::pair<TaskPriority, uint64_t>>,
                        TaskComparator>             m_readyQueue;

    // Work-stealing deques (one per worker)
    std::vector<std::unique_ptr<WorkStealingDeque>> m_workerDeques;

    // Workers
    std::vector<std::thread>                        m_workers;
    std::condition_variable                         m_workAvailable;
    uint32_t                                        m_numWorkers = 0;

    // Compute nodes
    std::unordered_map<uint32_t, ComputeNode>       m_nodes;
    std::thread                                     m_heartbeatThread;

    // Named pipelines
    struct PipelineDefinition {
        std::string name;
        std::vector<uint64_t> stageOrder;
        bool paused;
    };
    std::unordered_map<std::string, PipelineDefinition> m_pipelines;

    // Callbacks
    TaskCompletionCallback  m_completionCb    = nullptr;
    void*                   m_completionData  = nullptr;
    PipelineEventCallback   m_eventCb         = nullptr;
    void*                   m_eventData       = nullptr;
    NodeFailureCallback     m_nodeFailCb      = nullptr;
    void*                   m_nodeFailData    = nullptr;

    // Configuration
    size_t                  m_maxQueueDepth    = 100000;
    bool                    m_stealingEnabled  = true;
    int64_t                 m_heartbeatTimeout = 30000;
    int64_t                 m_defaultTimeout   = 30000;

    // Statistics
    PipelineStats           m_stats;
};

#endif // RAWRXD_DISTRIBUTED_PIPELINE_ORCHESTRATOR_HPP
