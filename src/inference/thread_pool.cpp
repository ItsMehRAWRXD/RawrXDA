/**
 * @file thread_pool.cpp
 * @brief Dedicated inference thread management
 * 
 * Provides:
 * - Worker thread pool for inference tasks
 * - Task queue with priority
 * - Thread affinity for NUMA optimization
 * - Dynamic thread scaling
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "thread_pool.h"
#include <windows.h>
#include <algorithm>

namespace RawrXD::Inference {

// ============================================================================
// ThreadPool Implementation
// ============================================================================

ThreadPool::ThreadPool(const Config& config)
    : m_config(config)
    , m_running(false)
    , m_activeTasks(0)
{
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool ThreadPool::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_running) {
        return true;
    }
    
    m_running = true;
    
    // Create worker threads
    int numThreads = m_config.numThreads > 0 ? m_config.numThreads : getDefaultThreadCount();
    
    for (int i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&ThreadPool::workerLoop, this, i);
    }
    
    return true;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    
    m_condition.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    m_workers.clear();
}

// ============================================================================
// Task Submission
// ============================================================================

std::future<void> ThreadPool::submit(TaskFunc task, int priority) {
    std::packaged_task<void()> packaged(std::move(task));
    std::future<void> future = packaged.get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        Task wrapper;
        wrapper.task = std::move(packaged);
        wrapper.priority = priority;
        wrapper.submitTime = std::chrono::steady_clock::now();
        
        m_taskQueue.push(std::move(wrapper));
    }
    
    m_condition.notify_one();
    
    return future;
}

std::future<void> ThreadPool::submitToThread(int threadId, TaskFunc task) {
    std::packaged_task<void()> packaged(std::move(task));
    std::future<void> future = packaged.get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (threadId >= 0 && threadId < static_cast<int>(m_workers.size())) {
            // Add to thread-specific queue
            m_threadQueues[threadId].push(std::move(packaged));
        } else {
            // Fallback to general queue
            Task wrapper;
            wrapper.task = std::move(packaged);
            wrapper.priority = 0;
            wrapper.submitTime = std::chrono::steady_clock::now();
            m_taskQueue.push(std::move(wrapper));
        }
    }
    
    m_condition.notify_one();
    
    return future;
}

// ============================================================================
// Worker Loop
// ============================================================================

void ThreadPool::workerLoop(int threadId) {
    // Set thread affinity if configured
    if (m_config.useThreadAffinity) {
        setThreadAffinity(threadId);
    }
    
    // Set thread name for debugging
    std::string threadName = "InferenceWorker_" + std::to_string(threadId);
    #ifdef _WIN32
    // Windows thread naming
    #endif
    
    while (m_running) {
        Task task;
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            m_condition.wait(lock, [this] {
                return !m_running || !m_taskQueue.empty() ||
                       std::any_of(m_threadQueues.begin(), m_threadQueues.end(),
                                  [](const auto& q) { return !q.empty(); });
            });
            
            if (!m_running) break;
            
            // Check thread-specific queue first
            if (threadId >= 0 && threadId < static_cast<int>(m_threadQueues.size()) &&
                !m_threadQueues[threadId].empty()) {
                task.task = std::move(m_threadQueues[threadId].front());
                m_threadQueues[threadId].pop();
            } else if (!m_taskQueue.empty()) {
                task = std::move(const_cast<Task>&(m_taskQueue.top()));
                m_taskQueue.pop();
            } else {
                continue;
            }
            
            m_activeTasks++;
        }
        
        // Execute task
        try {
            task.task();
        } catch (...) {
            // Task exception handling
        }
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeTasks--;
        }
        
        m_completionCondition.notify_all();
    }
}

// ============================================================================
// Thread Affinity
// ============================================================================

void ThreadPool::setThreadAffinity(int threadId) {
    #ifdef _WIN32
    DWORD_PTR mask = 1ULL << threadId;
    SetThreadAffinityMask(GetCurrentThread(), mask);
    #else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadId, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #endif
}

// ============================================================================
// Dynamic Scaling
// ============================================================================

void ThreadPool::scaleThreads(int numThreads) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int currentThreads = static_cast<int>(m_workers.size());
    
    if (numThreads > currentThreads) {
        // Add threads
        for (int i = currentThreads; i < numThreads; ++i) {
            m_workers.emplace_back(&ThreadPool::workerLoop, this, i);
        }
    } else if (numThreads < currentThreads) {
        // Remove threads (mark for shutdown)
        // In production, this would gracefully shutdown excess threads
    }
}

// ============================================================================
// Wait and Status
// ============================================================================

void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    m_completionCondition.wait(lock, [this] {
        return m_taskQueue.empty() && m_activeTasks == 0;
    });
}

bool ThreadPool::waitForAllWithTimeout(int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    return m_completionCondition.wait_for(lock, 
                                          std::chrono::milliseconds(timeoutMs),
                                          [this] {
        return m_taskQueue.empty() && m_activeTasks == 0;
    });
}

ThreadPoolStats ThreadPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ThreadPoolStats stats;
    stats.numThreads = static_cast<int>(m_workers.size());
    stats.activeTasks = m_activeTasks;
    stats.pendingTasks = static_cast<int>(m_taskQueue.size());
    
    for (const auto& queue : m_threadQueues) {
        stats.pendingTasks += static_cast<int>(queue.size());
    }
    
    return stats;
}

// ============================================================================
// Utility
// ============================================================================

int ThreadPool::getDefaultThreadCount() {
    #ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<int>(sysInfo.dwNumberOfProcessors);
    #else
    return static_cast<int>(std::thread::hardware_concurrency());
    #endif
}

} // namespace RawrXD::Inference
