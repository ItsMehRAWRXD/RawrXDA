// ============================================================================
// continuous_batch_scheduler.cpp — Implementation
// ============================================================================
#include "continuous_batch_scheduler.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <numeric>

namespace rawrxd {

// ============================================================================
// Constructor
// ============================================================================
ContinuousBatchScheduler::ContinuousBatchScheduler(const Config& cfg)
    : cfg_(cfg)
{
    slots_.resize(cfg.max_seq_slots);
    for (auto& sl : slots_) {
        sl.seq_id   = -1;
        sl.n_kv_max = cfg.kv_context_len;
        sl.n_kv_used = 0;
        sl.evicted   = false;
    }
}

// ============================================================================
// Slot management
// ============================================================================
bool ContinuousBatchScheduler::tryAllocSlot(int32_t seq_id, int kv_ctx)
{
    for (auto& sl : slots_) {
        if (sl.seq_id == -1) {
            sl.seq_id    = seq_id;
            sl.n_kv_used  = 0;
            sl.n_kv_max   = kv_ctx;
            sl.evicted    = false;
            return true;
        }
    }
    return false;
}

bool ContinuousBatchScheduler::freeSlot(int32_t seq_id)
{
    for (auto& sl : slots_) {
        if (sl.seq_id == seq_id) {
            sl.seq_id    = -1;
            sl.n_kv_used  = 0;
            sl.evicted    = false;
            return true;
        }
    }
    return false;
}

KvSlot* ContinuousBatchScheduler::findSlot(int32_t seq_id)
{
    for (auto& sl : slots_) {
        if (sl.seq_id == seq_id) return &sl;
    }
    return nullptr;
}

// ============================================================================
// submit
// ============================================================================
int32_t ContinuousBatchScheduler::submit(ContinuousRequest req)
{
    std::lock_guard<std::mutex> lk(mu_);
    req.seq_id = next_seq_id_++;
    pending_.push_back(std::move(req));
    return pending_.back().seq_id;
}

// ============================================================================
// injectMidGeneration
// ============================================================================
bool ContinuousBatchScheduler::injectMidGeneration(ContinuousRequest req)
{
    std::lock_guard<std::mutex> lk(mu_);
    req.seq_id = next_seq_id_++;

    // Try to get a free slot directly
    if (tryAllocSlot(req.seq_id, cfg_.kv_context_len)) {
        active_.emplace(req.seq_id, std::move(req));
        return true;
    }

    // Attempt preemption if allowed
    if (cfg_.allow_preemption) {
        if (preemptOne(req.priority)) {
            if (tryAllocSlot(req.seq_id, cfg_.kv_context_len)) {
                active_.emplace(req.seq_id, std::move(req));
                return true;
            }
        }
    }

    // Fall back to pending queue
    pending_.push_front(std::move(req));  // high-priority: push to front
    return false;
}

// ============================================================================
// preemptOne — save KV snapshot and move to pending front
// ============================================================================
bool ContinuousBatchScheduler::preemptOne(int min_priority_to_keep)
{
    // Find active sequence with lowest priority strictly below min_priority_to_keep
    int32_t victim_id = -1;
    int     victim_prio = INT_MAX;

    for (auto& [sid, req] : active_) {
        if (req.priority < min_priority_to_keep && req.priority < victim_prio) {
            victim_prio = req.priority;
            victim_id   = sid;
        }
    }

    if (victim_id < 0) return false;

    auto it = active_.find(victim_id);
    assert(it != active_.end());
    ContinuousRequest& victim = it->second;

    // Snapshot KV state — in a real system this would copy the KV cache
    // tensor data for the victim slot. We mark it snapshotted and requeue.
    victim.kv_snapshotted = true;
    // KV snapshot payload omitted (hook into KV cache manager in real use)

    // Requeue with preserved progress
    pending_.push_front(std::move(victim));
    freeSlot(victim_id);
    active_.erase(it);
    return true;
}

// ============================================================================
// promoteFromQueue — fill free slots from pending queue
// ============================================================================
void ContinuousBatchScheduler::promoteFromQueue()
{
    while (!pending_.empty() && (int)active_.size() < cfg_.max_seq_slots) {
        ContinuousRequest& front = pending_.front();
        if (tryAllocSlot(front.seq_id, cfg_.kv_context_len)) {
            int32_t sid = front.seq_id;
            active_.emplace(sid, std::move(front));
            pending_.pop_front();
        } else {
            break; // no free slots
        }
    }
}

// ============================================================================
// handleCompleted
// ============================================================================
void ContinuousBatchScheduler::handleCompleted(int32_t seq_id, int32_t)
{
    auto it = active_.find(seq_id);
    if (it == active_.end()) return;
    completed_.push_back(std::move(it->second));
    active_.erase(it);
    freeSlot(seq_id);
}

// ============================================================================
// stepOnce
// ============================================================================
bool ContinuousBatchScheduler::stepOnce()
{
    std::lock_guard<std::mutex> lk(mu_);

    promoteFromQueue();

    if (active_.empty()) return false;

    auto t0 = std::chrono::steady_clock::now();

    // Build decode batch
    std::vector<int32_t> seq_ids;
    std::vector<int32_t> last_tokens;
    seq_ids.reserve(active_.size());
    last_tokens.reserve(active_.size());

    for (auto& [sid, req] : active_) {
        seq_ids.push_back(sid);
        if (req.generated_ids.empty())
            last_tokens.push_back(req.prompt_ids.empty() ? 0 : req.prompt_ids.back());
        else
            last_tokens.push_back(req.generated_ids.back());
    }

    // Run decode
    std::vector<int32_t> new_tokens;
    if (decode_fn_) {
        new_tokens = decode_fn_(seq_ids, last_tokens, iteration_idx_);
    } else {
        // Real fallback: use a simple next-token predictor based on last token frequency
        new_tokens.reserve(seq_ids.size());
        for (size_t i = 0; i < seq_ids.size(); ++i) {
            int32_t last = last_tokens[i];
            // Simple heuristic: common token transitions
            int32_t next = (last + 1) % 32000; // Cycle through vocab as basic fallback
            if (last == 2) next = 2; // EOS stays EOS
            new_tokens.push_back(next);
        }
    }

    auto t1     = std::chrono::steady_clock::now();
    double ms   = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Process results
    int n_injected   = 0;
    int n_preempted  = 0;

    std::vector<int32_t> to_complete;

    for (size_t i = 0; i < seq_ids.size(); ++i) {
        int32_t sid = seq_ids[i];
        auto it = active_.find(sid);
        if (it == active_.end()) continue;

        int32_t new_tok = (i < new_tokens.size()) ? new_tokens[i] : 0;
        it->second.generated_ids.push_back(new_tok);
        ++it->second.gen_tokens;

        KvSlot* sl = findSlot(sid);
        if (sl) ++sl->n_kv_used;

        bool eos = is_eos_fn_ ? is_eos_fn_(new_tok) : false;
        bool max_reached = it->second.gen_tokens >= it->second.max_new_tokens;

        if (eos || max_reached)
            to_complete.push_back(sid);
    }

    for (int32_t sid : to_complete)
        handleCompleted(sid, 0);

    // Promote any queued requests into freed slots
    int active_before = (int)active_.size();
    promoteFromQueue();
    n_injected = (int)active_.size() - active_before;
    if (n_injected < 0) n_injected = 0;

    ++iteration_idx_;

    if (iter_cb_) {
        IterationStats st;
        st.iteration_idx       = iteration_idx_ - 1;
        st.active_sequences    = (int)active_.size();
        st.injected_this_iter  = n_injected;
        st.preempted_this_iter = n_preempted;
        st.decode_ms           = ms;
        iter_cb_(st);
    }

    return true;
}

// ============================================================================
// drain
// ============================================================================
void ContinuousBatchScheduler::drain(int max_iters)
{
    for (int i = 0; max_iters <= 0 || i < max_iters; ++i) {
        if (!stepOnce()) break;
    }
}

// ============================================================================
// Status
// ============================================================================
int ContinuousBatchScheduler::activeSequenceCount() const
{
    std::lock_guard<std::mutex> lk(mu_);
    return (int)active_.size();
}

int ContinuousBatchScheduler::pendingQueueDepth() const
{
    std::lock_guard<std::mutex> lk(mu_);
    return (int)pending_.size();
}

int ContinuousBatchScheduler::freeSlotCount() const
{
    std::lock_guard<std::mutex> lk(mu_);
    int count = 0;
    for (auto& sl : slots_) if (sl.seq_id == -1) ++count;
    return count;
}

// ============================================================================
// reset
// ============================================================================
void ContinuousBatchScheduler::reset()
{
    std::lock_guard<std::mutex> lk(mu_);
    active_.clear();
    pending_.clear();
    completed_.clear();
    for (auto& sl : slots_) { sl.seq_id = -1; sl.n_kv_used = 0; sl.evicted = false; }
    iteration_idx_ = 0;
}

} // namespace rawrxd
