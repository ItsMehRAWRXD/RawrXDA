#pragma once
// ============================================================================
// continuous_batch_scheduler.h — Iteration-level continuous batching
//
// Extends the batch-boundary RequestBatchScheduler with:
//   - KV slot pool tracking (per-sequence free/occupied slots)
//   - Mid-generation injection of new requests between decode steps
//   - Priority-based preemption with KV snapshot save/restore
//   - Iteration callback hook for per-step monitoring
// ============================================================================
#include <cstdint>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace rawrxd {

// ============================================================================
// KV Slot — represents one sequence's KV cache position in the pool
// ============================================================================
struct KvSlot {
    int32_t seq_id      = -1;   // owning sequence ID (-1 = free)
    int32_t layer_start = 0;    // first layer index this slot covers
    int32_t n_kv_used   = 0;    // number of filled KV positions
    int32_t n_kv_max    = 0;    // capacity (context window per sequence)
    bool    evicted     = false; // true if preempted and snapshotted
};

// ============================================================================
// ContinuousRequest — a single inflight generation request
// ============================================================================
struct ContinuousRequest {
    int32_t  seq_id      = -1;
    int       priority    = 0;     // higher = more urgent
    int32_t  prompt_len  = 0;
    int32_t  gen_tokens  = 0;     // tokens generated so far
    int32_t  max_new_tokens = 128;
    std::vector<int32_t> prompt_ids;
    std::vector<int32_t> generated_ids;

    // Snapshot of KV state for preemption/resume
    std::vector<uint8_t> kv_snapshot;
    bool kv_snapshotted = false;
};

// ============================================================================
// IterationStats — passed to each iteration callback
// ============================================================================
struct IterationStats {
    int64_t  iteration_idx      = 0;
    int       active_sequences   = 0;
    int       injected_this_iter = 0;
    int       preempted_this_iter = 0;
    double    decode_ms          = 0.0;
};

// ============================================================================
// ContinuousBatchScheduler
//
// Manages a fixed KV slot pool and runs a continuous batching loop.
//
// Key methods:
//   submit()           — enqueue a new generation request (any time)
//   injectMidGeneration() — force-insert a high-priority request between steps
//   stepOnce()         — advance all active sequences by one decode iteration
//   onIteration()      — register callback called after each step
//   drain()            — run until all requests finish or timeout
// ============================================================================
class ContinuousBatchScheduler {
public:
    struct Config {
        int   max_seq_slots    = 8;     // max concurrent sequences
        int   kv_context_len   = 2048;  // KV positions per slot
        int   max_batch_tokens = 512;   // max tokens across all seqs per step
        bool  allow_preemption = true;
        int   preempt_low_prio_threshold = 0; // preempt seqs with priority <= this
    };

    // Decode executor signature: called per iteration with a batch
    // Returns new token IDs for each active sequence (same order as input seq_ids)
    using DecodeFunc = std::function<std::vector<int32_t>(
        const std::vector<int32_t>& seq_ids,
        const std::vector<int32_t>& last_tokens,
        int                          iteration_idx)>;

    // Token is EOS predicate
    using IsEosFunc = std::function<bool(int32_t token_id)>;

    explicit ContinuousBatchScheduler(const Config& cfg);

    // Configuration accessor
    const Config& config() const noexcept { return cfg_; }

    // -------------------------------------------------------------------------
    // Submission API
    // -------------------------------------------------------------------------

    // Enqueue a request. Thread-safe.
    // Returns the assigned seq_id.
    int32_t submit(ContinuousRequest req);

    // Inject a high-priority request immediately into the active batch,
    // potentially preempting a lower-priority sequence.
    // Thread-safe; returns false if no slot could be freed.
    bool injectMidGeneration(ContinuousRequest req);

    // -------------------------------------------------------------------------
    // Execution
    // -------------------------------------------------------------------------

    // Wire in the decode function (must be set before stepOnce/drain)
    void setDecodeFunc(DecodeFunc fn) { decode_fn_ = std::move(fn); }
    void setIsEosFunc(IsEosFunc fn)   { is_eos_fn_ = std::move(fn); }

    // Register a per-iteration callback
    void onIteration(std::function<void(const IterationStats&)> cb) {
        iter_cb_ = std::move(cb); }

    // Advance all active sequences by exactly one decode step.
    // Returns false if there is nothing to do.
    bool stepOnce();

    // Run until queue is empty and all active sequences complete,
    // or until max_iters is reached (0 = unlimited).
    void drain(int max_iters = 0);

    // -------------------------------------------------------------------------
    // Status
    // -------------------------------------------------------------------------
    int  activeSequenceCount() const;
    int  pendingQueueDepth()   const;
    int  freeSlotCount()       const;
    bool isIdle()              const { return activeSequenceCount() == 0 && pendingQueueDepth() == 0; }

    // Reset — clears all state
    void reset();

private:
    bool tryAllocSlot(int32_t seq_id, int kv_context_len);
    bool freeSlot(int32_t seq_id);
    KvSlot* findSlot(int32_t seq_id);

    // Preempt the lowest-priority active sequence to free a slot.
    // Takes a snapshot of its KV state so it can resume later.
    // Returns false if nothing can be preempted.
    bool preemptOne(int min_priority_to_keep);

    void promoteFromQueue();
    void handleCompleted(int32_t seq_id, int32_t eos_token);

    Config cfg_;
    std::vector<KvSlot> slots_;

    mutable std::mutex mu_;

    // Active sequences (in the current decode batch)
    std::unordered_map<int32_t, ContinuousRequest> active_;

    // Waiting queue (submitted but not yet scheduled)
    std::deque<ContinuousRequest> pending_;

    // Completed sequences (for caller to collect results)
    std::vector<ContinuousRequest> completed_;

    int32_t next_seq_id_  = 1;
    int64_t iteration_idx_ = 0;

    DecodeFunc decode_fn_;
    IsEosFunc  is_eos_fn_;
    std::function<void(const IterationStats&)> iter_cb_;
};

} // namespace rawrxd
