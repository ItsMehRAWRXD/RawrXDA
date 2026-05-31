#include "inference_retry_shim.h"
#include <algorithm>
#include <thread>

namespace rxd::ai {

static uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

InferenceRetryShim::InferenceRetryShim(RetryPolicy p) : policy_(p) {}

InferenceStatus InferenceRetryShim::Execute(
    std::function<InferenceStatus()> submit_fn,
    const std::string& endpoint_tag)
{
    total_calls_.fetch_add(1, std::memory_order_relaxed);
    auto& circ = GetCircuit(endpoint_tag);

    if (!ShouldAllow(circ)) {
        circuit_opens_.fetch_add(1, std::memory_order_relaxed);
        return InferenceStatus::CircuitOpen;
    }

    InferenceStatus last = InferenceStatus::Retryable;

    for (uint32_t attempt = 0; attempt <= policy_.max_retries; ++attempt) {
        if (attempt > 0) {
            retries_.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(JitteredBackoff(attempt)));
        }

        last = submit_fn();

        if (last == InferenceStatus::OK) {
            RecordSuccess(circ);
            successes_.fetch_add(1, std::memory_order_relaxed);
            return InferenceStatus::OK;
        }

        if (last == InferenceStatus::NonRetryable) {
            break; // fail fast, no retry
        }

        // Retryable: record failure, loop again
        RecordFailure(circ);
    }

    return last; // exhausted retries or non-retryable
}

bool InferenceRetryShim::IsCircuitOpen(const std::string& tag) const {
    std::lock_guard<std::mutex> lk(circuit_mtx_);
    auto it = circuits_.find(tag);
    return it != circuits_.end() && it->second->open.load(std::memory_order_acquire);
}

void InferenceRetryShim::ResetCircuit(const std::string& tag) {
    std::lock_guard<std::mutex> lk(circuit_mtx_);
    auto it = circuits_.find(tag);
    if (it != circuits_.end()) {
        it->second->open.store(false, std::memory_order_release);
        it->second->failures.store(0, std::memory_order_release);
    }
}

InferenceRetryShim::Metrics InferenceRetryShim::GetMetrics() const {
    return {
        total_calls_.load(std::memory_order_relaxed),
        retries_.load(std::memory_order_relaxed),
        circuit_opens_.load(std::memory_order_relaxed),
        successes_.load(std::memory_order_relaxed)
    };
}

uint32_t InferenceRetryShim::JitteredBackoff(uint32_t attempt) const {
    uint32_t shift = std::min(attempt, 10u);
    uint64_t base = static_cast<uint64_t>(policy_.base_ms) << shift;
    if (base > policy_.max_backoff_ms) base = policy_.max_backoff_ms;

    // jitter: base * (1 ± jitter_frac)
    static thread_local std::mt19937 rng(static_cast<uint32_t>(NowMs()));
    std::uniform_real_distribution<double> dist(-policy_.jitter_frac, policy_.jitter_frac);
    double j = std::clamp(1.0 + dist(rng), 0.5, 2.0);
    return static_cast<uint32_t>(static_cast<double>(base) * j);
}

InferenceRetryShim::CircuitState& InferenceRetryShim::GetCircuit(const std::string& tag) {
    std::lock_guard<std::mutex> lk(circuit_mtx_);
    auto it = circuits_.find(tag);
    if (it == circuits_.end()) {
        it = circuits_.emplace(tag, std::make_unique<CircuitState>()).first;
    }
    return *it->second;
}

bool InferenceRetryShim::ShouldAllow(CircuitState& cs) const {
    if (!cs.open.load(std::memory_order_acquire)) return true;

    uint64_t last = cs.last_failure_ms.load(std::memory_order_relaxed);
    if ((NowMs() - last) > policy_.circuit_reset_ms) {
        cs.open.store(false, std::memory_order_release);
        cs.failures.store(0, std::memory_order_release);
        return true;
    }
    return false;
}

void InferenceRetryShim::RecordFailure(CircuitState& cs) {
    uint32_t f = cs.failures.fetch_add(1, std::memory_order_relaxed) + 1;
    cs.last_failure_ms.store(NowMs(), std::memory_order_relaxed);
    if (f >= policy_.circuit_threshold) {
        cs.open.store(true, std::memory_order_release);
    }
}

void InferenceRetryShim::RecordSuccess(CircuitState& cs) {
    cs.failures.store(0, std::memory_order_relaxed);
    cs.open.store(false, std::memory_order_release);
}

} // namespace rxd::ai
