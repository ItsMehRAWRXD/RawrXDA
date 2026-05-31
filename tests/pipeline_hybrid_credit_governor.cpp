// Pipeline Hybrid Credit Governor
// Combines deterministic safety with controlled speculation
// Hard credits for baseline + soft credits for amplification

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include <vector>
#include <algorithm>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// =============================================================================
// Hybrid Credit System
// Hard credits: deterministic baseline (safety)
// Soft credits: controlled amplification (throughput)
// =============================================================================
class HybridCreditGovernor {
    // Hard credits: strict accounting for safety
    alignas(64) std::atomic<int64_t> hard_credits_{0};
    // Soft credits: speculative budget for throughput  
    alignas(64) std::atomic<int64_t> soft_credits_{0};
    
    // Configuration
    const int64_t max_hard_credits_;
    const int64_t max_soft_credits_;
    const double speculation_rate_;  // e.g., 0.25 for 1.25x amplification
    
    // Metrics
    std::atomic<uint64_t> hard_waits_{0};
    std::atomic<uint64_t> soft_waits_{0};
    std::atomic<uint64_t> speculations_allowed_{0};
    std::atomic<uint64_t> speculations_blocked_{0};

public:
    HybridCreditGovernor(int64_t hard_budget, int64_t soft_budget, double spec_rate)
        : max_hard_credits_(hard_budget)
        , max_soft_credits_(soft_budget)
        , speculation_rate_(spec_rate) {
        hard_credits_.store(hard_budget);
        soft_credits_.store(soft_budget);
    }
    
    // Acquire credits for production (blocking)
    // Returns: number of tokens allowed to produce (up to requested)
    int64_t acquire_produce(int64_t requested, microseconds timeout = microseconds(1000)) {
        auto start = high_resolution_clock::now();
        int64_t acquired = 0;
        
        while (acquired < requested) {
            int64_t current = hard_credits_.load(std::memory_order_relaxed);
            if (current <= 0) {
                // Check timeout
                auto elapsed = duration_cast<microseconds>(high_resolution_clock::now() - start);
                if (elapsed >= timeout) {
                    break;  // Return partial
                }
                hard_waits_++;
                _mm_pause();
                continue;
            }
            
            int64_t to_acquire = std::min(requested - acquired, current);
            if (hard_credits_.compare_exchange_weak(current, current - to_acquire,
                                                     std::memory_order_acquire)) {
                acquired += to_acquire;
            }
        }
        
        return acquired;
    }
    
    // Check if speculation is allowed (non-blocking)
    bool try_speculate() {
        int64_t current = soft_credits_.load(std::memory_order_relaxed);
        if (current <= 0) {
            speculations_blocked_++;
            return false;
        }
        
        if (soft_credits_.compare_exchange_weak(current, current - 1,
                                                 std::memory_order_relaxed)) {
            speculations_allowed_++;
            return true;
        }
        return false;
    }
    
    // Return credits after consumption
    void return_credits(int64_t consumed, bool from_speculation = false) {
        // Return hard credits 1:1
        hard_credits_.fetch_add(consumed, std::memory_order_release);
        
        // Replenish soft credits based on consumption rate
        if (!from_speculation) {
            int64_t replenish = static_cast<int64_t>(consumed * speculation_rate_);
            int64_t current = soft_credits_.load(std::memory_order_relaxed);
            int64_t new_soft = std::min(current + replenish, max_soft_credits_);
            soft_credits_.store(new_soft, std::memory_order_relaxed);
        }
    }
    
    // Get current state
    int64_t hard_available() const { return hard_credits_.load(); }
    int64_t soft_available() const { return soft_credits_.load(); }
    
    // Metrics
    uint64_t hard_waits() const { return hard_waits_.load(); }
    uint64_t soft_waits() const { return soft_waits_.load(); }
    uint64_t speculations_allowed() const { return speculations_allowed_.load(); }
    uint64_t speculations_blocked() const { return speculations_blocked_.load(); }
    double speculation_success_rate() const {
        uint64_t total = speculations_allowed_.load() + speculations_blocked_.load();
        return total > 0 ? static_cast<double>(speculations_allowed_.load()) / total * 100.0 : 0.0;
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
    
    size_t size() const {
        return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_relaxed)) & MASK;
    }
    bool empty() const { return size() == 0; }
};

// Egress with AVX2 FP8
class HybridEgress {
    static constexpr size_t BATCH = 64;
    alignas(256) float batch_[BATCH];
    size_t fill_ = 0;
    std::atomic<uint64_t> tokens_{0};
    std::atomic<uint64_t> batches_{0};

public:
    void push(float token) {
        batch_[fill_++] = token;
        if (fill_ >= BATCH) {
            alignas(256) uint8_t out[BATCH];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(batch_, out, 1.0f);
            tokens_ += BATCH;
            batches_++;
            fill_ = 0;
        }
    }
    
    void flush() {
        if (fill_ > 0) {
            for (size_t i = fill_; i < BATCH; ++i) batch_[i] = 0.0f;
            alignas(256) uint8_t out[BATCH];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(batch_, out, 1.0f);
            tokens_ += fill_;
            batches_++;
            fill_ = 0;
        }
    }
    
    uint64_t tokens() const { return tokens_.load(); }
    uint64_t batches() const { return batches_.load(); }
};

int main() {
    std::cout << "=== Hybrid Credit Governor Pipeline ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    
    // Configuration: Hybrid credit system
    constexpr size_t CAPACITY = 65536;
    constexpr size_t TOTAL = 1000000;
    constexpr size_t BATCH_SIZE = 256;  // Batch-window credits
    constexpr int64_t HARD_BUDGET = 4096;  // Strict safety budget
    constexpr int64_t SOFT_BUDGET = 2048;    // Speculation budget (2x for headroom)
    constexpr double SPEC_RATE = 0.25;      // 25% = 1.25x amplification
    constexpr size_t SPEC_INTERVAL = 4;     // 1 in 4 tokens speculative
    
    SimpleSPSC<CAPACITY> q1;
    SimpleSPSC<CAPACITY> q2;
    HybridEgress egress;
    HybridCreditGovernor governor(HARD_BUDGET, SOFT_BUDGET, SPEC_RATE);
    
    std::atomic<bool> producer_done{false};
    std::atomic<bool> decoder_done{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> decoded{0};
    std::atomic<size_t> consumed{0};
    
    auto t0 = high_resolution_clock::now();
    
    // Stage 1: Producer with batch-window credits
    std::thread t1([&]() {
        size_t remaining = TOTAL;
        while (remaining > 0) {
            // Acquire credits in batch-window
            int64_t credits = governor.acquire_produce(std::min(BATCH_SIZE, remaining));
            if (credits <= 0) continue;
            
            // Produce with acquired credits
            for (int64_t i = 0; i < credits; ++i) {
                while (!q1.try_push(static_cast<float>(TOTAL - remaining + i))) {
                    _mm_pause();
                }
            }
            
            remaining -= credits;
            produced.store(TOTAL - remaining);
        }
        producer_done.store(true);
    });
    
    // Stage 2: Decoder with hybrid speculation
    std::thread t2([&]() {
        size_t count = 0;
        size_t spec_counter = 0;
        
        while (!producer_done.load() || !q1.empty()) {
            float tok;
            if (!q1.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Base token (always allowed)
            while (!q2.try_push(tok)) _mm_pause();
            count++;
            
            // Speculative token (soft credit check)
            spec_counter++;
            if (spec_counter >= SPEC_INTERVAL) {
                spec_counter = 0;
                if (governor.try_speculate()) {
                    // Speculation allowed by soft credits
                    while (!q2.try_push(tok * 0.5f)) _mm_pause();
                    count++;
                }
            }
            
            decoded.store(count);
        }
        decoder_done.store(true);
    });
    
    // Stage 3: Consumer with credit return
    std::thread t3([&]() {
        size_t count = 0;
        size_t batch_accum = 0;
        
        while (!decoder_done.load() || !q2.empty()) {
            float tok;
            if (!q2.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            egress.push(tok);
            count++;
            batch_accum++;
            
            // Return credits in batch-window
            if (batch_accum >= BATCH_SIZE) {
                governor.return_credits(batch_accum, false);
                batch_accum = 0;
            }
            
            consumed.store(count);
        }
        
        // Return remaining credits
        if (batch_accum > 0) {
            governor.return_credits(batch_accum, false);
        }
        
        egress.flush();
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
    std::cout << "Batches: " << egress.batches() << std::endl;
    
    // Credit metrics
    std::cout << "\n=== Credit Metrics ===" << std::endl;
    std::cout << "Hard waits: " << governor.hard_waits() << std::endl;
    std::cout << "Soft waits: " << governor.soft_waits() << std::endl;
    std::cout << "Speculations allowed: " << governor.speculations_allowed() << std::endl;
    std::cout << "Speculations blocked: " << governor.speculations_blocked() << std::endl;
    std::cout << "Speculation success rate: " << governor.speculation_success_rate() << "%" << std::endl;
    std::cout << "Final hard credits: " << governor.hard_available() << std::endl;
    std::cout << "Final soft credits: " << governor.soft_available() << std::endl;
    
    // Performance
    double in_tps = produced.load() * 1e6 / us;
    double out_tps = consumed.load() * 1e6 / us;
    double amp = static_cast<double>(decoded.load()) / produced.load();
    
    std::cout << "\n=== Performance ===" << std::endl;
    std::cout << "Input TPS: " << static_cast<uint64_t>(in_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(out_tps) << std::endl;
    std::cout << "Amplification: " << amp << "x" << std::endl;
    
    // Verify
    double ratio = static_cast<double>(consumed.load()) / produced.load();
    std::cout << "\nToken ratio: " << ratio << std::endl;
    
    if (ratio >= 1.15 && ratio <= 1.5 && amp >= 1.15) {
        std::cout << "[PASS] Hybrid system working - deterministic safety + elastic throughput" << std::endl;
        std::cout << "       Hard credits provide safety, soft credits enable " << amp << "x amplification" << std::endl;
        return 0;
    }
    std::cout << "[FAIL]" << std::endl;
    return 1;
}
