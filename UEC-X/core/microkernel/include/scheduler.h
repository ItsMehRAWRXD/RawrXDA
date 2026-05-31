// UEC-X Microkernel - Scheduler
// Manages extension lifecycle and task scheduling

#pragma once

#include "uec_core.h"
#include <queue>
#include <unordered_map>
#include <future>

namespace uec {

// =============================================================================
// Task Types
// =============================================================================

enum class TaskPriority : uint32_t {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Background = 4
};

enum class TaskState : uint32_t {
    Pending = 0,
    Running = 1,
    Completed = 2,
    Failed = 3,
    Cancelled = 4
};

struct Task {
    uint64_t id;
    TaskPriority priority;
    ExtensionId owner;
    std::function<void()> work;
    std::chrono::milliseconds timeout;
    Timestamp scheduledAt;
    Timestamp startedAt;
    std::atomic<TaskState> state{TaskState::Pending};
    std::promise<void> completion;
};

// =============================================================================
// Scheduler
// =============================================================================

class UEC_API Scheduler {
public:
    Scheduler();
    ~Scheduler();

    // Non-copyable, non-movable
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    // Lifecycle
    Result<void> Initialize(uint32_t workerThreads);
    Result<void> Shutdown();
    bool IsInitialized() const;
    bool IsRunning() const;

    // Task submission
    Result<uint64_t> Schedule(
        std::function<void()> work,
        TaskPriority priority = TaskPriority::Normal,
        ExtensionId owner = 0,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000)
    );
    
    Result<void> ScheduleDelayed(
        std::function<void()> work,
        std::chrono::milliseconds delay,
        TaskPriority priority = TaskPriority::Normal,
        ExtensionId owner = 0
    );
    
    Result<void> ScheduleRepeating(
        std::function<void()> work,
        std::chrono::milliseconds interval,
        TaskPriority priority = TaskPriority::Normal,
        ExtensionId owner = 0
    );

    // Task control
    Result<void> CancelTask(uint64_t taskId);
    Result<void> CancelAllTasks(ExtensionId owner);
    Result<void> WaitForTask(uint64_t taskId, std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    Result<void> WaitForAllTasks(ExtensionId owner, std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    // Extension lifecycle
    Result<void> RegisterExtension(ExtensionId id);
    Result<void> UnregisterExtension(ExtensionId id);
    bool IsExtensionRegistered(ExtensionId id) const;

    // Query
    size_t GetPendingTaskCount() const;
    size_t GetRunningTaskCount() const;
    size_t GetCompletedTaskCount() const;
    TaskState GetTaskState(uint64_t taskId) const;

    // Statistics
    struct Stats {
        uint64_t tasksSubmitted = 0;
        uint64_t tasksCompleted = 0;
        uint64_t tasksFailed = 0;
        uint64_t tasksCancelled = 0;
        uint64_t tasksTimedOut = 0;
        double averageExecutionTimeMs = 0.0;
    };
    Stats GetStats() const;

private:
    struct TaskComparator {
        bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) const {
            return static_cast<uint32_t>(a->priority) > static_cast<uint32_t>(b->priority);
        }
    };

    void WorkerThread();
    void ProcessDelayedTasks();
    void ExecuteTask(std::shared_ptr<Task> task);

    std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, TaskComparator> m_taskQueue;
    std::unordered_map<uint64_t, std::shared_ptr<Task>> m_activeTasks;
    std::unordered_map<ExtensionId, std::vector<uint64_t>> m_extensionTasks;
    std::vector<std::thread> m_workers;
    std::thread m_delayedTaskThread;
    
    mutable std::mutex m_mutex;
    std::condition_variable m_taskCondition;
    std::condition_variable m_completionCondition;
    
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shutdown{false};
    std::atomic<uint64_t> m_nextTaskId{1};
    
    Stats m_stats;
    mutable std::mutex m_statsMutex;
};

} // namespace uec
