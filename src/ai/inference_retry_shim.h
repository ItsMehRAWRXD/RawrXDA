#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

namespace rxd::ai {

enum class InferenceStatus : uint8_t {
    OK = 0,
    Retryable,      // 502/503/504, timeout, transient
    NonRetryable,   // 400/401/403/404 — fail fast
    CircuitOpen     // breaker tripped
};

struct RetryPolicy {
    uint32_t max_retries = 5;
    uint32_t base_ms = 50;          // exponential backoff base
    uint32_t max_backoff_ms = 5000;
    uint32_t circuit_threshold = 3; // failures before open
    uint32_t circuit_reset_ms = 30000;
    double   jitter_frac = 0.25;    // ±25% jitter
};

class InferenceRetryShim {
public:
    explicit InferenceRetryShim(RetryPolicy p = {});

    // Main entry: wraps your existing SubmitInference lambda.
    // Returns OK on success, or last failure code.
    InferenceStatus Execute(
        std::function<InferenceStatus()> submit_fn,
        const std::string& endpoint_tag = "default"
    );

    // Circuit breaker state query
    bool IsCircuitOpen(const std::string& tag) const;

    // Force-close a tripped circuit (manual recovery)
    void ResetCircuit(const std::string& tag);

    // Metrics
    struct Metrics {
        uint64_t total_calls = 0;
        uint64_t retries = 0;
        uint64_t circuit_opens = 0;
        uint64_t successes = 0;
    };
    Metrics GetMetrics() const;

private:
    struct CircuitState {
        std::atomic<uint32_t> failures{0};
        std::atomic<uint64_t> last_failure_ms{0};
        std::atomic<bool>     open{false};
    };

    RetryPolicy policy_;
    mutable std::mutex circuit_mtx_;
    // unique_ptr because CircuitState has atomics (non-copyable / non-movable)
    std::unordered_map<std::string, std::unique_ptr<CircuitState>> circuits_;

    std::atomic<uint64_t> total_calls_{0};
    std::atomic<uint64_t> retries_{0};
    std::atomic<uint64_t> circuit_opens_{0};
    std::atomic<uint64_t> successes_{0};

    uint32_t JitteredBackoff(uint32_t attempt) const;
    CircuitState& GetCircuit(const std::string& tag);
    bool ShouldAllow(CircuitState& cs) const;
    void RecordFailure(CircuitState& cs);
    void RecordSuccess(CircuitState& cs);
};

} // namespace rxd::ai
