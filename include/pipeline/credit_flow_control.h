// Pipeline FP8 AVX2 with Credit-Based Flow Control
// Replaces timeout-based backpressure with explicit credit counters
// Author: RawrXD Core Team

#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>

namespace RawrXD {
namespace Pipeline {

// Credit-based flow control token
// Passed upstream to signal consumption capacity
struct CreditToken {
    static constexpr uint32_t MAX_CREDITS = 1024;
    
    std::atomic<uint32_t> credits{MAX_CREDITS};
    std::atomic<uint64_t> total_consumed{0};
    
    // Consumer calls this to signal capacity available
    void return_credits(uint32_t count) {
        credits.fetch_add(count, std::memory_order_release);
        total_consumed.fetch_add(count, std::memory_order_relaxed);
    }
    
    // Producer calls this to check available capacity
    bool acquire_credits(uint32_t count) {
        uint32_t current = credits.load(std::memory_order_acquire);
        while (current >= count) {
            if (credits.compare_exchange_weak(
                current, current - count,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
                return true;
            }
            _mm_pause();
        }
        return false;
    }
    
    // Non-blocking check
    uint32_t available() const {
        return credits.load(std::memory_order_acquire);
    }
};

// Bounded speculative window
// Prevents unbounded amplification from overwhelming egress
class SpeculativeWindow {
    static constexpr size_t MAX_INFLIGHT = 256;  // Max speculative tokens in flight
    
    std::atomic<size_t> inflight_{0};
    std::atomic<uint64_t> total_speculations_{0};
    std::atomic<uint64_t> blocked_attempts_{0};

public:
    // Try to acquire slot for speculative token
    bool try_acquire() {
        size_t current = inflight_.load(std::memory_order_relaxed);
        while (current < MAX_INFLIGHT) {
            if (inflight_.compare_exchange_weak(
                current, current + 1,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
                total_speculations_++;
                return true;
            }
            _mm_pause();
        }
        blocked_attempts_++;
        return false;
    }
    
    // Release slot when speculation completes
    void release() {
        inflight_.fetch_sub(1, std::memory_order_release);
    }
    
    size_t inflight() const { return inflight_.load(std::memory_order_acquire); }
    uint64_t total_speculations() const { return total_speculations_.load(); }
    uint64_t blocked_attempts() const { return blocked_attempts_.load(); }
    
    double block_rate() const {
        uint64_t total = total_speculations_.load() + blocked_attempts_.load();
        return total > 0 ? static_cast<double>(blocked_attempts_.load()) / total * 100.0 : 0.0;
    }
};

// Per-stage metrics for credit-based system
struct CreditMetrics {
    std::atomic<uint64_t> credits_acquired{0};
    std::atomic<uint64_t> credits_returned{0};
    std::atomic<uint64_t> credit_waits{0};      // Times blocked waiting for credits
    std::atomic<uint64_t> tokens_processed{0};
    
    double get_credit_pressure() const {
        uint64_t acquired = credits_acquired.load();
        uint64_t returned = credits_returned.load();
        return acquired > 0 ? static_cast<double>(acquired - returned) / acquired * 100.0 : 0.0;
    }
    
    double get_wait_rate() const {
        uint64_t processed = tokens_processed.load();
        uint64_t waits = credit_waits.load();
        return processed > 0 ? static_cast<double>(waits) / processed * 100.0 : 0.0;
    }
};

// Credit-aware SPSC queue
template<size_t Capacity>
class CreditSPSC {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) float buffer_[Capacity];
    
    CreditToken* credit_token_ = nullptr;
    CreditMetrics* metrics_ = nullptr;

public:
    static constexpr size_t MASK = Capacity - 1;
    
    void set_credit_token(CreditToken* token) { credit_token_ = token; }
    void set_metrics(CreditMetrics* metrics) { metrics_ = metrics; }
    
    // Credit-aware push
    bool push(float token, uint32_t credits_required = 1) {
        // Check credits first (if credit system attached)
        if (credit_token_ && !credit_token_->acquire_credits(credits_required)) {
            if (metrics_) metrics_->credit_waits++;
            return false;
        }
        
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t next = (t + 1) & MASK;
        
        if (next == head_.load(std::memory_order_acquire)) {
            // Queue full - return credits
            if (credit_token_) credit_token_->return_credits(credits_required);
            return false;
        }
        
        buffer_[t] = token;
        tail_.store(next, std::memory_order_release);
        if (metrics_) {
            metrics_->credits_acquired += credits_required;
            metrics_->tokens_processed++;
        }
        return true;
    }
    
    // Pop with credit return
    bool pop(float& token, uint32_t credits_to_return = 1) {
        size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;
        
        token = buffer_[h];
        head_.store((h + 1) & MASK, std::memory_order_release);
        
        // Return credits to signal capacity
        if (credit_token_) credit_token_->return_credits(credits_to_return);
        if (metrics_) metrics_->credits_returned += credits_to_return;
        
        return true;
    }
    
    size_t size() const {
        return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_relaxed)) & MASK;
    }
};

} // namespace Pipeline
} // namespace RawrXD
