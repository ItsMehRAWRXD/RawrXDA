// ============================================================================
// thread_pool.hpp — Work-Stealing Thread Pool
// ============================================================================
// Task-prioritized thread pool with work-stealing queues.
// Supports UI > Agent > Indexing priority ordering.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <future>
#include <memory>

namespace RawrXD {
namespace Threading {

// ============================================================================
// Task Priority
// ============================================================================

enum class TaskPriority : uint8_t {
    CRITICAL = 0,   // UI responsiveness, must not block
    HIGH     = 1,   // Agent operations (user-initiated)
    NORMAL   = 2,   // Background processing
    LOW      = 3,   // Indexing, embedding computation
    IDLE     = 4    // Deferred work, only when idle
};

// ============================================================================
// Task Result
// ============================================================================

struct TaskResult {
    bool success;
    const char* detail;
    int errorCode;

    static TaskResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static TaskResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// Task — a prioritized unit of work
// ============================================================================

struct Task {
    std::function<void()> work;
    TaskPriority priority;
    uint64_t submitOrder;   // For FIFO ordering within same priority
    const char* label;      // Debug label (optional)

    bool operator>(const Task& other) const {
        if (priority != other.priority)
            return static_cast<uint8_t>(priority) > static_cast<uint8_t>(other.priority);
        return submitOrder > other.submitOrder;
    }
};

// ============================================================================
// ThreadPool
// ============================================================================

class ThreadPool {
public:
    // ---- Construction ----
    // numThreads = 0 means hardware_concurrency()
    explicit ThreadPool(size_t numThreads = 0, const char* name = "RawrXD-Pool");
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ---- Submit Work ----
    
    // Submit a void task
    TaskResult submit(std::function<void()> work,
                      TaskPriority priority = TaskPriority::NORMAL,
                      const char* label = nullptr);

    // Submit a task with a future return
    template<typename F, typename R = std::invoke_result_t<F>>
    std::future<R> submitWithResult(F&& func,
                                     TaskPriority priority = TaskPriority::NORMAL,
                                     const char* label = nullptr) {
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();

        auto wrappedWork = [p = std::move(promise), f = std::forward<F>(func)]() mutable {
            if constexpr (std::is_void_v<R>) {
                f();
                p->set_value();
            } else {
                p->set_value(f());
            }
        };

        submit(std::move(wrappedWork), priority, label);
        return future;
    }

    // ---- Control ----
    void shutdown();
    void pause();
    void resume();

    // ---- Stats ----
    size_t threadCount() const { return m_threads.size(); }
    size_t pendingTasks() const;
    size_t completedTasks() const { return m_completedCount.load(std::memory_order_relaxed); }
    bool isShutdown() const { return m_shutdown.load(std::memory_order_relaxed); }

    // ---- Singleton convenience ----
    static ThreadPool& Global();

private:
    void workerLoop(size_t threadIndex);
    bool tryStealTask(Task& out, size_t thiefIndex);

    // ---- State ----
    std::vector<std::thread> m_threads;
    std::string m_name;

    // Per-thread work queues (for work-stealing)
    struct WorkQueue {
        std::deque<Task> tasks;
        std::mutex mutex;
    };
    std::vector<std::unique_ptr<WorkQueue>> m_queues;

    // Global overflow queue (for tasks submitted from outside pool)
    std::deque<Task> m_globalQueue;
    std::mutex m_globalMutex;
    std::condition_variable m_condition;

    std::atomic<bool> m_shutdown{false};
    std::atomic<bool> m_paused{false};
    std::atomic<uint64_t> m_submitOrder{0};
    std::atomic<uint64_t> m_completedCount{0};
    size_t m_nextQueue{0};  // Round-robin for task assignment
};

// ============================================================================
// Parallel helpers
// ============================================================================

// Parallel for-each with task priority
template<typename Iter, typename Func>
void parallelForEach(ThreadPool& pool, Iter begin, Iter end, Func func,
                     TaskPriority priority = TaskPriority::NORMAL) {
    std::vector<std::future<void>> futures;
    for (auto it = begin; it != end; ++it) {
        futures.push_back(pool.submitWithResult(
            [&func, &item = *it]() { func(item); },
            priority
        ));
    }
    for (auto& f : futures) {
        f.wait();
    }
}

} // namespace Threading
} // namespace RawrXD
