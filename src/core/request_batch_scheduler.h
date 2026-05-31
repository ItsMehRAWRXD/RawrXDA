// ============================================================================
// request_batch_scheduler.h — Adaptive Request Coalescer & Dynamic Batcher
// ============================================================================
// Collects concurrent inference requests and coalesces them into a single
// batch dispatch.  Tracks per-request KV offsets so outputs can be
// de-interleaved after the batched forward pass.
//
// Architecture:
//   InferSlot        — one pending request with its prompt, callback, etc.
//   BatchWindow      — a fused group of InferSlots (contiguous KV offsets)
//   RequestBatchScheduler — the scheduler that forms windows, dispatches,
//                           and routes outputs back to individual callers.
//
// Throughput model:
//   Single-request latency             L
//   N-request coalesced batch latency  ~1.2 L  (GPU occupancy amortised)
//   Effective throughput multiplier     ~N / 1.2
//
// Threading: one background coalescer thread + caller threads submit via CV.
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// InferSlot — a single queued inference request
// ---------------------------------------------------------------------------
struct InferSlot {
    uint64_t                             requestId;
    std::vector<int32_t>                 prompt;
    size_t                               maxTokens;
    std::function<void(const std::string&)> tokenCallback;      // per-token
    std::function<void(bool)>            completionCallback;     // done(ok?)

    // Assigned by the scheduler when the slot enters a batch
    uint32_t batchIndex   = 0;   // position within the batch
    uint32_t kvOffset     = 0;   // token offset in the fused KV cache
};

// ---------------------------------------------------------------------------
// BatchWindow — a group of slots dispatched together
// ---------------------------------------------------------------------------
struct BatchWindow {
    uint64_t                   batchId;
    std::vector<InferSlot>     slots;
    uint32_t                   totalPromptTokens = 0;
    std::chrono::steady_clock::time_point formed;
};

// ---------------------------------------------------------------------------
// BatchSchedulerConfig
// ---------------------------------------------------------------------------
struct BatchSchedulerConfig {
    uint32_t maxBatchSize         = 8;       // max requests per batch
    uint32_t maxBatchTokens       = 4096;    // max total prompt tokens per batch
    uint32_t coalesceWindowUs     = 2000;    // microseconds to wait for more requests
    uint32_t minBatchSize         = 1;       // fire even if only 1 request waiting
    bool     adaptiveWindow       = true;    // grow/shrink coalesce window with load
};

// ---------------------------------------------------------------------------
// Batch execution callback — the scheduler calls this to actually run inference.
// Receives the full fused prompt and per-slot metadata.  Must call each
// slot's tokenCallback/completionCallback.
// ---------------------------------------------------------------------------
using BatchExecutorFn = std::function<void(BatchWindow& batch)>;

// ---------------------------------------------------------------------------
// RequestBatchScheduler
// ---------------------------------------------------------------------------
class RequestBatchScheduler {
public:
    explicit RequestBatchScheduler(const BatchSchedulerConfig& cfg = {});
    ~RequestBatchScheduler();

    // Submit a request.  Returns a request ID.
    uint64_t submit(std::vector<int32_t> prompt,
                    size_t maxTokens,
                    std::function<void(const std::string&)> tokenCb,
                    std::function<void(bool)> doneCb);

    // Set the function that actually runs a batch.
    void setBatchExecutor(BatchExecutorFn fn);

    // Runtime stats
    struct Stats {
        uint64_t totalRequests;
        uint64_t totalBatches;
        double   avgBatchSize;
        double   avgCoalesceUs;
    };
    Stats getStats() const;

    // Drain all pending, then stop the coalescer thread.
    void shutdown();

private:
    void coalescerLoop();
    BatchWindow formBatch();

    BatchSchedulerConfig          m_cfg;
    BatchExecutorFn               m_executor;

    mutable std::mutex            m_mu;
    std::condition_variable       m_cv;
    std::deque<InferSlot>         m_pending;
    std::atomic<uint64_t>         m_nextReqId{1};
    std::atomic<uint64_t>         m_nextBatchId{1};

    // Stats (relaxed atomics — no ordering needed for telemetry)
    std::atomic<uint64_t>         m_statRequests{0};
    std::atomic<uint64_t>         m_statBatches{0};
    std::atomic<uint64_t>         m_statCoalesceUsSum{0};

    // Adaptive coalesce window (microseconds)
    std::atomic<uint32_t>         m_currentWindowUs;

    std::thread                   m_thread;
    std::atomic<bool>             m_stop{false};
};

} // namespace rawrxd
