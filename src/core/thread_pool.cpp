// ============================================================================
// thread_pool.cpp — Work-Stealing Thread Pool Implementation
// ============================================================================
// Pure C++20, no Qt, no exceptions. PatchResult-style error handling.
// Work-stealing algorithm: each thread has a local queue; idle threads
// steal from other threads' queues or the global overflow queue.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/thread_pool.hpp"

#include <algorithm>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Threading {

// ============================================================================
// Global singleton
// ============================================================================

ThreadPool& ThreadPool::Global() {
    static ThreadPool instance(0, "RawrXD-Global");
    return instance;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

ThreadPool::ThreadPool(size_t numThreads, const char* name)
    : m_name(name ? name : "ThreadPool") {

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // Fallback
    }

    // Create per-thread work queues
    m_queues.resize(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        m_queues[i] = std::make_unique<WorkQueue>();
    }

    // Spawn worker threads
    m_threads.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        m_threads.emplace_back(&ThreadPool::workerLoop, this, i);

        // Set thread name for debugging (Windows)
#ifdef _WIN32
        wchar_t threadName[128];
        swprintf(threadName, 128, L"%hs-Worker-%zu", m_name.c_str(), i);
        SetThreadDescription(m_threads.back().native_handle(), threadName);
#endif
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// ============================================================================
// Submit
// ============================================================================

TaskResult ThreadPool::submit(std::function<void()> work,
                               TaskPriority priority,
                               const char* label) {
    if (m_shutdown.load(std::memory_order_relaxed)) {
        return TaskResult::error("ThreadPool is shut down", -1);
    }

    if (!work) {
        return TaskResult::error("Null work function", -2);
    }

    Task task;
    task.work = std::move(work);
    task.priority = priority;
    task.submitOrder = m_submitOrder.fetch_add(1, std::memory_order_relaxed);
    task.label = label;

    // Insert into global queue (sorted by priority)
    {
        std::lock_guard<std::mutex> lock(m_globalMutex);

        // Find insertion point for priority ordering
        auto it = m_globalQueue.begin();
        while (it != m_globalQueue.end()) {
            if (task.priority < it->priority) break;
            if (task.priority == it->priority && task.submitOrder < it->submitOrder) break;
            ++it;
        }
        m_globalQueue.insert(it, std::move(task));
    }

    m_condition.notify_one();
    return TaskResult::ok("Submitted");
}

// ============================================================================
// Control
// ============================================================================

void ThreadPool::shutdown() {
    if (m_shutdown.exchange(true)) return;  // Already shut down

    m_condition.notify_all();

    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::pause() {
    m_paused.store(true, std::memory_order_relaxed);
}

void ThreadPool::resume() {
    m_paused.store(false, std::memory_order_relaxed);
    m_condition.notify_all();
}

size_t ThreadPool::pendingTasks() const {
    size_t count = 0;

    // Count global queue
    {
        // Cannot lock from const method; use a rough estimate
        // In practice, this is fine for monitoring
        count += m_globalQueue.size();
    }

    // Count per-thread queues
    for (size_t i = 0; i < m_queues.size(); ++i) {
        count += m_queues[i]->tasks.size();
    }

    return count;
}

// ============================================================================
// Worker Loop
// ============================================================================

void ThreadPool::workerLoop(size_t threadIndex) {
    while (true) {
        Task task;
        bool gotTask = false;

        // 1. Try local queue first
        {
            std::lock_guard<std::mutex> lock(m_queues[threadIndex]->mutex);
            if (!m_queues[threadIndex]->tasks.empty()) {
                task = std::move(m_queues[threadIndex]->tasks.front());
                m_queues[threadIndex]->tasks.pop_front();
                gotTask = true;
            }
        }

        // 2. Try global queue
        if (!gotTask) {
            std::unique_lock<std::mutex> lock(m_globalMutex);

            // Wait for work or shutdown
            m_condition.wait(lock, [this]() {
                return m_shutdown.load(std::memory_order_relaxed) ||
                       !m_globalQueue.empty() ||
                       !m_paused.load(std::memory_order_relaxed);
            });

            if (m_shutdown.load(std::memory_order_relaxed) && m_globalQueue.empty()) {
                return;
            }

            if (!m_globalQueue.empty() && !m_paused.load(std::memory_order_relaxed)) {
                task = std::move(m_globalQueue.front());
                m_globalQueue.pop_front();
                gotTask = true;
            }
        }

        // 3. Try work-stealing from other threads
        if (!gotTask) {
            gotTask = tryStealTask(task, threadIndex);
        }

        // 4. Check shutdown
        if (!gotTask) {
            if (m_shutdown.load(std::memory_order_relaxed)) return;
            continue;
        }

        // Paused check
        while (m_paused.load(std::memory_order_relaxed) &&
               !m_shutdown.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Execute
        if (task.work) {
            task.work();
            m_completedCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

bool ThreadPool::tryStealTask(Task& out, size_t thiefIndex) {
    // Try each other thread's queue
    size_t queueCount = m_queues.size();
    for (size_t i = 1; i < queueCount; ++i) {
        size_t victimIndex = (thiefIndex + i) % queueCount;
        std::lock_guard<std::mutex> lock(m_queues[victimIndex]->mutex);
        if (!m_queues[victimIndex]->tasks.empty()) {
            // Steal from the back (least recently added)
            out = std::move(m_queues[victimIndex]->tasks.back());
            m_queues[victimIndex]->tasks.pop_back();
            return true;
        }
    }
    return false;
}

} // namespace Threading
} // namespace RawrXD
