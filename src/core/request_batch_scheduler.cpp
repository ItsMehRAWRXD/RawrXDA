// ============================================================================
// request_batch_scheduler.cpp — Adaptive Request Coalescer & Dynamic Batcher
// ============================================================================
#include "request_batch_scheduler.h"
#include "../config/IDEConfig.h"
#include <algorithm>
#include <cmath>

namespace rawrxd {

// ============================================================================
// Construction / destruction
// ============================================================================

RequestBatchScheduler::RequestBatchScheduler(const BatchSchedulerConfig& cfg)
    : m_cfg(cfg)
    , m_currentWindowUs(cfg.coalesceWindowUs)
{
    METRICS.gauge("runtime.scheduler.window_us", static_cast<double>(cfg.coalesceWindowUs));
    METRICS.gauge("runtime.scheduler.pending_requests", 0.0);
    m_thread = std::thread(&RequestBatchScheduler::coalescerLoop, this);
}

RequestBatchScheduler::~RequestBatchScheduler()
{
    shutdown();
}

void RequestBatchScheduler::shutdown()
{
    bool expected = false;
    if (!m_stop.compare_exchange_strong(expected, true))
        return; // already stopped
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

// ============================================================================
// submit — caller-thread entry point
// ============================================================================

uint64_t RequestBatchScheduler::submit(
    std::vector<int32_t> prompt,
    size_t maxTokens,
    std::function<void(const std::string&)> tokenCb,
    std::function<void(bool)> doneCb)
{
    InferSlot slot;
    slot.requestId          = m_nextReqId.fetch_add(1, std::memory_order_relaxed);
    slot.prompt             = std::move(prompt);
    slot.maxTokens          = maxTokens;
    slot.tokenCallback      = std::move(tokenCb);
    slot.completionCallback = std::move(doneCb);

    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_pending.push_back(std::move(slot));
        METRICS.gauge("runtime.scheduler.pending_requests", static_cast<double>(m_pending.size()));
    }
    m_cv.notify_one();
    m_statRequests.fetch_add(1, std::memory_order_relaxed);
    METRICS.increment("runtime.scheduler.requests_total");
    return slot.requestId;
}

// ============================================================================
// setBatchExecutor
// ============================================================================

void RequestBatchScheduler::setBatchExecutor(BatchExecutorFn fn)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_executor = std::move(fn);
}

// ============================================================================
// getStats — relaxed snapshot
// ============================================================================

RequestBatchScheduler::Stats RequestBatchScheduler::getStats() const
{
    Stats s;
    s.totalRequests = m_statRequests.load(std::memory_order_relaxed);
    s.totalBatches  = m_statBatches.load(std::memory_order_relaxed);
    s.avgBatchSize  = (s.totalBatches > 0)
        ? static_cast<double>(s.totalRequests) / static_cast<double>(s.totalBatches)
        : 0.0;
    uint64_t cSum = m_statCoalesceUsSum.load(std::memory_order_relaxed);
    s.avgCoalesceUs = (s.totalBatches > 0)
        ? static_cast<double>(cSum) / static_cast<double>(s.totalBatches)
        : 0.0;
    METRICS.gauge("runtime.scheduler.total_requests", static_cast<double>(s.totalRequests));
    METRICS.gauge("runtime.scheduler.total_batches", static_cast<double>(s.totalBatches));
    METRICS.gauge("runtime.scheduler.avg_batch_size", s.avgBatchSize);
    METRICS.gauge("runtime.scheduler.avg_coalesce_us", s.avgCoalesceUs);
    return s;
}

// ============================================================================
// coalescerLoop — background thread
// ============================================================================

void RequestBatchScheduler::coalescerLoop()
{
    while (!m_stop.load(std::memory_order_acquire)) {
        // Wait for at least one request
        {
            std::unique_lock<std::mutex> lk(m_mu);
            m_cv.wait(lk, [&] {
                return !m_pending.empty() || m_stop.load(std::memory_order_acquire);
            });
            if (m_stop.load(std::memory_order_acquire) && m_pending.empty())
                return;
        }

        // Coalesce window — wait a bit for more requests to arrive
        auto windowStart = std::chrono::steady_clock::now();
        uint32_t windowUs = m_currentWindowUs.load(std::memory_order_relaxed);
        {
            std::unique_lock<std::mutex> lk(m_mu);
            m_cv.wait_for(lk, std::chrono::microseconds(windowUs), [&] {
                return m_pending.size() >= m_cfg.maxBatchSize
                    || m_stop.load(std::memory_order_acquire);
            });
        }

        auto windowEnd = std::chrono::steady_clock::now();
        uint64_t actualUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(windowEnd - windowStart).count());
        METRICS.recordDuration("runtime.scheduler.coalesce_window_ms", actualUs / 1000.0);

        // Form and dispatch a batch
        BatchWindow batch = formBatch();
        if (batch.slots.empty())
            continue;

        m_statBatches.fetch_add(1, std::memory_order_relaxed);
        m_statCoalesceUsSum.fetch_add(actualUs, std::memory_order_relaxed);

        // Adaptive window: grow when batches are full, shrink when mostly empty
        if (m_cfg.adaptiveWindow) {
            double fillRatio = static_cast<double>(batch.slots.size())
                             / static_cast<double>(m_cfg.maxBatchSize);
            uint32_t cur = m_currentWindowUs.load(std::memory_order_relaxed);
            if (fillRatio > 0.8 && cur < m_cfg.coalesceWindowUs * 4) {
                // High load — grow window to capture more requests
                m_currentWindowUs.store(cur + cur / 4, std::memory_order_relaxed);
            } else if (fillRatio < 0.3 && cur > 500) {
                // Low load — shrink window to reduce latency
                m_currentWindowUs.store(cur - cur / 4, std::memory_order_relaxed);
            }
        }

        METRICS.gauge("runtime.scheduler.window_us",
                  static_cast<double>(m_currentWindowUs.load(std::memory_order_relaxed)));
        METRICS.gauge("runtime.scheduler.last_batch_size", static_cast<double>(batch.slots.size()));
        METRICS.gauge("runtime.scheduler.last_batch_tokens", static_cast<double>(batch.totalPromptTokens));

        // Execute the batch
        BatchExecutorFn exec;
        {
            std::lock_guard<std::mutex> lk(m_mu);
            exec = m_executor;
        }
        if (exec) {
            exec(batch);
        } else {
            // No executor — fail all slots
            for (auto& s : batch.slots) {
                if (s.completionCallback)
                    s.completionCallback(false);
            }
        }
    }
}

// ============================================================================
// formBatch — pop up to maxBatchSize/maxBatchTokens from the pending queue
// ============================================================================

BatchWindow RequestBatchScheduler::formBatch()
{
    BatchWindow batch;
    batch.batchId = m_nextBatchId.fetch_add(1, std::memory_order_relaxed);
    batch.formed  = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lk(m_mu);

    uint32_t tokenBudget = m_cfg.maxBatchTokens;
    uint32_t slotBudget  = m_cfg.maxBatchSize;
    uint32_t kvOff       = 0;
    uint32_t idx         = 0;

    while (!m_pending.empty() && idx < slotBudget) {
        auto& front = m_pending.front();
        uint32_t promptLen = static_cast<uint32_t>(front.prompt.size());

        if (batch.totalPromptTokens + promptLen > tokenBudget && !batch.slots.empty())
            break; // would exceed token budget and we already have ≥1 slot

        front.batchIndex = idx;
        front.kvOffset   = kvOff;
        kvOff += promptLen;
        batch.totalPromptTokens += promptLen;

        batch.slots.push_back(std::move(front));
        m_pending.pop_front();
        ++idx;
    }

    METRICS.gauge("runtime.scheduler.pending_requests", static_cast<double>(m_pending.size()));

    return batch;
}

} // namespace rawrxd
