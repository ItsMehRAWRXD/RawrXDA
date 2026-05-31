#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace RawrXD {

// Thread pool with per-thread work queues and work-stealing.
// Minimises synchronisation overhead vs a single shared queue.
class WorkStealingPool {
public:
    explicit WorkStealingPool(
        size_t n_threads = std::thread::hardware_concurrency()
    )
        : m_stop(false)
        , m_activeTasks(0)
        , m_queueMutex(n_threads)
    {
        m_queues.resize(n_threads);
        m_threads.reserve(n_threads);
        for (size_t i = 0; i < n_threads; ++i) {
            m_threads.emplace_back([this, i] { workerLoop(i); });
        }
    }

    ~WorkStealingPool() {
        {
            std::unique_lock<std::mutex> lock(m_globalMutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& t : m_threads) {
            if (t.joinable()) t.join();
        }
    }

    // Submit a task to the caller's thread-local queue (or queue 0 if external)
    template<typename F>
    void submit(F&& task) {
        const size_t tid = threadIndex() % m_queues.size();
        {
            std::lock_guard<std::mutex> lk(m_queueMutex[tid]);
            m_queues[tid].push_back(std::forward<F>(task));
        }
        m_cv.notify_one();
    }

    // Parallel-for: distributes [start, end) in chunks across all workers.
    // Blocks until all iterations complete.
    template<typename F>
    void parallelFor(size_t start, size_t end, F&& func) {
        if (start >= end) return;

        const size_t n          = end - start;
        const size_t n_workers  = m_threads.size();
        const size_t chunk_size = std::max<size_t>(1, n / (n_workers * 4));

        std::atomic<size_t> next{start};
        std::atomic<size_t> pending{0};

        // Count tasks first so we can wait precisely
        size_t n_chunks = (n + chunk_size - 1) / chunk_size;
        pending.store(n_chunks, std::memory_order_relaxed);

        std::mutex              doneMtx;
        std::condition_variable doneCv;

        for (size_t c = 0; c < n_chunks; ++c) {
            const size_t cs = start + c * chunk_size;
            const size_t ce = std::min(cs + chunk_size, end);

            submit([cs, ce, &func, &pending, &doneMtx, &doneCv] {
                for (size_t i = cs; i < ce; ++i) func(i);
                if (pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lk(doneMtx);
                    doneCv.notify_all();
                }
            });
        }

        std::unique_lock<std::mutex> lk(doneMtx);
        doneCv.wait(lk, [&pending] { return pending.load(std::memory_order_acquire) == 0; });
    }

    size_t size() const { return m_threads.size(); }

private:
    void workerLoop(size_t id) {
        setThreadIndex(id);

        while (true) {
            std::function<void()> task;

            // 1. Pop from own queue
            {
                std::lock_guard<std::mutex> lk(m_queueMutex[id]);
                if (!m_queues[id].empty()) {
                    task = std::move(m_queues[id].front());
                    m_queues[id].pop_front();
                }
            }

            // 2. Steal from another queue (round-robin)
            if (!task) {
                for (size_t j = 1; j < m_queues.size(); ++j) {
                    size_t victim = (id + j) % m_queues.size();
                    std::lock_guard<std::mutex> lk(m_queueMutex[victim]);
                    if (!m_queues[victim].empty()) {
                        task = std::move(m_queues[victim].back());
                        m_queues[victim].pop_back();
                        break;
                    }
                }
            }

            if (task) {
                task();
                continue;
            }

            // 3. Nothing to do — wait
            std::unique_lock<std::mutex> lk(m_globalMutex);
            m_cv.wait(lk, [this] { return m_stop || hasAnyTask(); });
            if (m_stop && !hasAnyTask()) return;
        }
    }

    bool hasAnyTask() const {
        for (const auto& q : m_queues) {
            if (!q.empty()) return true;
        }
        return false;
    }

    // Per-thread index stored in thread_local storage
    static size_t threadIndex() {
        static thread_local size_t idx = 0;
        return idx;
    }
    static void setThreadIndex(size_t id) {
        static thread_local size_t idx = 0;
        idx = id;
    }

    std::vector<std::thread>                         m_threads;
    std::vector<std::deque<std::function<void()>>>   m_queues;
    std::vector<std::mutex>                          m_queueMutex;
    std::mutex                                       m_globalMutex;
    std::condition_variable                          m_cv;
    std::atomic<bool>                                m_stop;
    std::atomic<int>                                 m_activeTasks;
};

} // namespace RawrXD
