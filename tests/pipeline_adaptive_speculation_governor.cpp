// Adaptive Speculation Governor - Self-Tuning Amplification Control
// Dynamically adjusts soft credit budget to target 85% speculation success
// Maximizes amplification × stability²

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include <algorithm>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// =============================================================================
// Adaptive Credit Governor with Self-Tuning Speculation Control
// =============================================================================
class AdaptiveCreditGovernor {
    // Hard credits: strict safety (batch-window based)
    alignas(64) std::atomic<int64_t> hard_credits_{0};
    
    // Soft credits: speculation budget (adaptive)
    alignas(64) std::atomic<int64_t> soft_credits_{0};
    
    // Adaptive control parameters
    static constexpr int64_t HARD_WINDOW_SIZE = 4096;  // Batch window for hard credits
    static constexpr int64_t INITIAL_SOFT_BUDGET = 512;  // Moderate initial budget
    static constexpr int64_t MAX_SOFT_BUDGET = 4096;   // Higher upper bound for headroom
    static constexpr int64_t MIN_SOFT_BUDGET = 128;    // Lower bound
    
    // PID-like controller for soft credit adjustment
    float target_success_rate_ = 0.85f;  // Target 85% speculation success
    float current_success_rate_ = 0.0f;
    int64_t last_adjustment_ = 0;
    
    // Metrics for adaptation
    std::atomic<uint64_t> speculation_attempts_{0};
    std::atomic<uint64_t> speculation_successes_{0};
    std::atomic<uint64_t> hard_waits_{0};
    std::atomic<uint64_t> soft_waits_{0};
    
    // Rate limiting for soft credit replenishment
    std::atomic<uint64_t> tokens_consumed_since_refill_{0};
    static constexpr uint64_t REFILL_INTERVAL = 256;  // Replenish every 256 tokens

public:
    AdaptiveCreditGovernor() {
        hard_credits_.store(HARD_WINDOW_SIZE);
        soft_credits_.store(INITIAL_SOFT_BUDGET);
    }
    
    // Producer acquires hard credits in batch windows
    bool acquire_hard_credits(int64_t count) {
        while (true) {
            int64_t current = hard_credits_.load(std::memory_order_relaxed);
            if (current < count) {
                hard_waits_++;
                return false;  // Backpressure
            }
            if (hard_credits_.compare_exchange_weak(
                current, current - count,
                std::memory_order_relaxed)) {
                return true;
            }
            _mm_pause();
        }
    }
    
    // Check soft credits (non-blocking speculation gate)
    bool try_consume_soft_credit() {
        // Try to atomically decrement soft credits
        int64_t current = soft_credits_.load(std::memory_order_relaxed);
        while (current > 0) {
            if (soft_credits_.compare_exchange_weak(
                current, current - 1,
                std::memory_order_relaxed)) {
                speculation_attempts_++;
                return true;
            }
            // CAS failed, reload current value
            current = soft_credits_.load(std::memory_order_relaxed);
        }
        // No soft credits available
        soft_waits_++;
        return false;
    }
    
    // Record speculation success/failure for adaptation
    void record_speculation(bool success) {
        if (success) {
            speculation_successes_++;
        }
    }
    
    // Consumer returns credits and triggers adaptation
    void return_credits(int64_t consumed_tokens) {
        // Replenish hard credits 1:1
        hard_credits_.fetch_add(consumed_tokens, std::memory_order_relaxed);
        
        // Conservative soft credit replenishment
        // Only replenish a fraction of consumed tokens to create scarcity
        tokens_consumed_since_refill_ += consumed_tokens;
        
        if (tokens_consumed_since_refill_.load() >= REFILL_INTERVAL) {
            // Balanced soft credit replenishment
            // Target: 85% speculation success
            // Replenish ~20% of consumed tokens for moderate pressure
            int64_t soft_replenish = static_cast<int64_t>(REFILL_INTERVAL * 0.20f);
            
            // Adjust based on current success rate vs target (85%)
            float success = get_speculation_success_rate();
            if (success > 0.90f) {
                // Too much success = too many soft credits, reduce replenishment
                soft_replenish = static_cast<int64_t>(REFILL_INTERVAL * 0.15f);  // 15%
            } else if (success < 0.80f) {
                // Too little success = not enough soft credits, increase replenishment
                soft_replenish = static_cast<int64_t>(REFILL_INTERVAL * 0.25f);  // 25%
            }
            
            // Cap soft credits to prevent accumulation
            int64_t current_soft = soft_credits_.load(std::memory_order_relaxed);
            if (current_soft + soft_replenish > MAX_SOFT_BUDGET) {
                soft_replenish = MAX_SOFT_BUDGET - current_soft;
                if (soft_replenish < 0) soft_replenish = 0;
            }
            
            soft_credits_.fetch_add(soft_replenish, std::memory_order_relaxed);
            tokens_consumed_since_refill_ -= REFILL_INTERVAL;
        }
        
        // Periodic adaptation of soft budget limits
        maybe_adapt();
    }
    
    // Get current soft credit budget (for monitoring)
    int64_t get_soft_budget() const {
        return soft_credits_.load(std::memory_order_relaxed);
    }
    
    // Get metrics
    float get_speculation_success_rate() const {
        uint64_t attempts = speculation_attempts_.load();
        uint64_t successes = speculation_successes_.load();
        return attempts > 0 ? static_cast<float>(successes) / attempts : 0.0f;
    }
    
    uint64_t get_hard_waits() const { return hard_waits_.load(); }
    uint64_t get_soft_waits() const { return soft_waits_.load(); }

private:
    void maybe_adapt() {
        // Adapt soft budget limits based on long-term success rate
        uint64_t total = speculation_attempts_.load();
        if (total - last_adjustment_ < 50000) return;  // Adapt every 50k attempts
        
        last_adjustment_ = total;
        
        float success_rate = get_speculation_success_rate();
        int64_t current_budget = soft_credits_.load();
        
        // Simple proportional adjustment
        float error = target_success_rate_ - success_rate;
        int64_t adjustment = static_cast<int64_t>(error * 500);  // Scale factor
        
        int64_t new_budget = std::clamp(
            current_budget + adjustment,
            MIN_SOFT_BUDGET,
            MAX_SOFT_BUDGET
        );
        
        // Only adjust if significant change
        if (std::abs(new_budget - current_budget) > 50) {
            soft_credits_.store(new_budget, std::memory_order_relaxed);
        }
    }
};

// Simple SPSC queue
template<size_t Capacity>
class SimpleSPSC {
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) float buffer_[Capacity];

public:
    static constexpr size_t MASK = Capacity - 1;
    
    bool try_push(float token) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t next = (t + 1) & MASK;
        if (next == head_.load(std::memory_order_acquire)) return false;
        buffer_[t] = token;
        tail_.store(next, std::memory_order_release);
        return true;
    }
    
    bool try_pop(float& token) {
        size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;
        token = buffer_[h];
        head_.store((h + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    bool empty() const {
        return head_.load(std::memory_order_relaxed) == 
               tail_.load(std::memory_order_acquire);
    }
};

// Egress with AVX2 FP8
class AdaptiveEgress {
    static constexpr size_t BATCH = 64;
    alignas(256) float batch_[BATCH];
    size_t fill_ = 0;
    std::atomic<uint64_t> tokens_{0};

public:
    void push(float token) {
        batch_[fill_++] = token;
        if (fill_ >= BATCH) {
            alignas(256) uint8_t out[BATCH];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(batch_, out, 1.0f);
            tokens_ += BATCH;
            fill_ = 0;
        }
    }
    
    void flush() {
        if (fill_ > 0) {
            for (size_t i = fill_; i < BATCH; ++i) batch_[i] = 0.0f;
            alignas(256) uint8_t out[BATCH];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(batch_, out, 1.0f);
            tokens_ += fill_;
            fill_ = 0;
        }
    }
    
    uint64_t tokens() const { return tokens_.load(); }
};

int main() {
    std::cout << "=== Adaptive Speculation Governor ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    
    constexpr size_t CAPACITY = 65536;
    constexpr size_t TOTAL = 1000000;
    constexpr size_t BATCH = 64;
    
    SimpleSPSC<CAPACITY> q1;
    SimpleSPSC<CAPACITY> q2;
    AdaptiveEgress egress;
    AdaptiveCreditGovernor governor;
    
    std::atomic<bool> done1{false};
    std::atomic<bool> done2{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> decoded{0};
    std::atomic<size_t> consumed{0};
    
    auto t0 = high_resolution_clock::now();
    
    // Producer with adaptive hard credit acquisition
    std::thread t1([&]() {
        size_t count = 0;
        while (count < TOTAL) {
            // Acquire hard credits in batch windows
            if (!governor.acquire_hard_credits(BATCH)) {
                _mm_pause();
                continue;
            }
            
            // Produce batch
            for (size_t i = 0; i < BATCH && count < TOTAL; ++i) {
                while (!q1.try_push(static_cast<float>(count))) _mm_pause();
                count++;
            }
            produced.store(count);
        }
        done1.store(true);
    });
    
    // Decoder with adaptive speculation
    std::thread t2([&]() {
        size_t count = 0;
        size_t spec_counter = 0;
        constexpr size_t SPEC_INTERVAL = 4;
        
        while (!done1.load() || !q1.empty()) {
            float tok;
            if (!q1.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Base token (always)
            while (!q2.try_push(tok)) _mm_pause();
            count++;
            
            // Adaptive speculation
            spec_counter++;
            if (spec_counter >= SPEC_INTERVAL) {
                spec_counter = 0;
                
                // Try soft credit (non-blocking)
                if (governor.try_consume_soft_credit()) {
                    // Speculation allowed
                    while (!q2.try_push(tok * 0.5f)) _mm_pause();
                    count++;
                    governor.record_speculation(true);
                } else {
                    governor.record_speculation(false);
                }
            }
            
            decoded.store(count);
        }
        done2.store(true);
    });
    
    // Consumer with credit return
    std::thread t3([&]() {
        size_t count = 0;
        size_t last_return = 0;
        
        while (!done2.load() || !q2.empty()) {
            float tok;
            if (!q2.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            egress.push(tok);
            count++;
            
            // Return credits periodically (batch)
            if (count - last_return >= BATCH) {
                governor.return_credits(BATCH);
                last_return = count;
            }
            
            consumed.store(count);
        }
        
        egress.flush();
        // Return remaining credits
        governor.return_credits(count - last_return);
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    auto t1_end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t1_end - t0).count();
    
    // Results
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Time: " << us << " us" << std::endl;
    std::cout << "Produced: " << produced.load() << std::endl;
    std::cout << "Decoded: " << decoded.load() << std::endl;
    std::cout << "Consumed: " << consumed.load() << std::endl;
    std::cout << "Egress tokens: " << egress.tokens() << std::endl;
    
    double in_tps = produced.load() * 1e6 / us;
    double out_tps = consumed.load() * 1e6 / us;
    double amp = static_cast<double>(decoded.load()) / produced.load();
    
    std::cout << "\n=== Performance ===" << std::endl;
    std::cout << "Input TPS: " << static_cast<uint64_t>(in_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(out_tps) << std::endl;
    std::cout << "Amplification: " << amp << "x" << std::endl;
    
    // Governor metrics
    std::cout << "\n=== Governor Metrics ===" << std::endl;
    std::cout << "Speculation success rate: " << (governor.get_speculation_success_rate() * 100.0f) << "%" << std::endl;
    std::cout << "Hard waits: " << governor.get_hard_waits() << std::endl;
    std::cout << "Soft waits: " << governor.get_soft_waits() << std::endl;
    std::cout << "Final soft budget: " << governor.get_soft_budget() << std::endl;
    
    double ratio = static_cast<double>(consumed.load()) / produced.load();
    std::cout << "\nToken ratio: " << ratio << std::endl;
    
    // Success criteria
    float success_rate = governor.get_speculation_success_rate();
    bool pass = (ratio >= 1.0 && ratio <= 1.5 && 
                 success_rate >= 0.80 && success_rate <= 0.90 &&
                 in_tps >= 10e6);
    
    if (pass) {
        std::cout << "[PASS] Adaptive governor in sweet spot (80-90% success, 10M+ TPS)" << std::endl;
        return 0;
    }
    std::cout << "[FAIL] Outside target range" << std::endl;
    return 1;
}
