/**
 * @file thread_pool.h
 * @brief Dedicated inference thread management
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <functional>
#include <chrono>

namespace RawrXD::Inference {

// ============================================================================
// Task Function Type
// ============================================================================

using TaskFunc = std::function<void()>;

// ============================================================================
// Thread Pool Configuration
// ============================================================================

struct ThreadPoolConfig {
    int numThreads = 0; // 0 = auto-detect
    bool useThreadAffinity = false;
    int maxQueueDepth = 1000;
};

// ============================================================================
// Task Wrapper
// ============================================================================

struct Task {
    std::packaged_task<void()> task;
    int priority = 0;
    std::chrono::steady_clock::time_point submitTime;
    
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

// ============================================================================
// Thread Pool Statistics
// ============================================================================

struct ThreadPoolStats {
    int numThreads = 0;
    int activeTasks = 0;
    int pendingTasks = 0;
};

// ============================================================================
// Thread Pool
// ============================================================================

class ThreadPool {
public:
    explicit ThreadPool(const Config& config);
    ~ThreadPool();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const { return m_running; }
    
    // Task submission
    std::future<void> submit(TaskFunc task, int priority = 0);
    std::future<void> submitToThread(int threadId, TaskFunc task);
    
    // Dynamic scaling
    void scaleThreads(int numThreads);
    
    // Wait
    void waitForAll();
    bool waitForAllWithTimeout(int timeoutMs);
    
    // Statistics
    ThreadPoolStats getStats() const;
    
private:
    void workerLoop(int threadId);
    void setThreadAffinity(int threadId);
    int getDefaultThreadCount();
    
    Config m_config;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_completionCondition;
    
    bool m_running;
    int m_activeTasks;
    std::vector<std::thread> m_workers;
    std::priority_queue<Task> m_taskQueue;
    std::vector<std::queue<std::packaged_task<void()>>> m_threadQueues;
};

} // namespace RawrXD::Inference
