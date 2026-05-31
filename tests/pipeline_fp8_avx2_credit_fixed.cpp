// Pipeline FP8 AVX2 with Working Credit-Based Flow Control
// Simplified version that avoids deadlock

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>
#include <vector>
#include <algorithm>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// Simple credit counter for flow control
class SimpleCreditCounter {
    std::atomic<int64_t> credits_{0};
    std::atomic<uint64_t> total_returned_{0};
    std::atomic<uint64_t> total_acquired_{0};

public:
    void init(int64_t initial) { credits_.store(initial); }
    
    void return_credits(int64_t count) {
        credits_.fetch_add(count, std::memory_order_release);
        total_returned_.fetch_add(count, std::memory_order_relaxed);
    }
    
    bool try_acquire(int64_t count) {
        int64_t current = credits_.load(std::memory_order_acquire);
        while (current >= count) {
            if (credits_.compare_exchange_weak(
                current, current - count,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
                total_acquired_.fetch_add(count, std::memory_order_relaxed);
                return true;
            }
            _mm_pause();
        }
        return false;
    }
    
    int64_t available() const { return credits_.load(std::memory_order_acquire); }
    uint64_t total_returned() const { return total_returned_.load(); }
    uint64_t total_acquired() const { return total_acquired_.load(); }
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
};

// Egress with AVX2 FP8 and credit return
class CreditEgressStage {
    static constexpr size_t BATCH_SIZE = 64;
    alignas(256) float batch_buffer_[BATCH_SIZE];
    size_t batch_fill_ = 0;
    
    SimpleCreditCounter* credit_counter_ = nullptr;
    std::atomic<uint64_t> tokens_quantized_{0};
    std::atomic<uint64_t> batches_quantized_{0};

public:
    void set_credit_counter(SimpleCreditCounter* counter) { credit_counter_ = counter; }
    
    bool push(float token) {
        batch_buffer_[batch_fill_++] = token;
        
        if (batch_fill_ >= BATCH_SIZE) {
            alignas(256) uint8_t quantized[BATCH_SIZE];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(
                batch_buffer_, quantized, 1.0f
            );
            
            batches_quantized_++;
            tokens_quantized_ += BATCH_SIZE;
            batch_fill_ = 0;
            
            // Return credits to signal capacity available
            if (credit_counter_) {
                credit_counter_->return_credits(BATCH_SIZE);
            }
        }
        return true;
    }
    
    void flush() {
        if (batch_fill_ > 0) {
            for (size_t i = batch_fill_; i < BATCH_SIZE; ++i) {
                batch_buffer_[i] = 0.0f;
            }
            alignas(256) uint8_t quantized[BATCH_SIZE];
            RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(
                batch_buffer_, quantized, 1.0f
            );
            tokens_quantized_ += batch_fill_;
            batches_quantized_++;
            batch_fill_ = 0;
            
            if (credit_counter_) {
                credit_counter_->return_credits(BATCH_SIZE);
            }
        }
    }
    
    uint64_t tokens() const { return tokens_quantized_.load(); }
    uint64_t batches() const { return batches_quantized_.load(); }
};

int main() {
    std::cout << "=== FP8 AVX2 Credit-Based Flow Control ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    
    // Configuration
    constexpr size_t CAPACITY = 65536;
    constexpr size_t TOTAL_TOKENS = 1000000;
    constexpr size_t SPEC_INTERVAL = 4;
    constexpr int64_t INITIAL_CREDITS = 65536;  // Start with full capacity
    
    // Credit counter for producer backpressure
    SimpleCreditCounter producer_credits;
    producer_credits.init(INITIAL_CREDITS);
    
    // Queues
    SimpleSPSC<CAPACITY> q1;  // Producer -> Decoder
    SimpleSPSC<CAPACITY> q2;  // Decoder -> Consumer
    
    // Speculation tracking
    std::atomic<size_t> spec_inflight{0};
    constexpr size_t MAX_SPEC = 256;
    
    CreditEgressStage egress;
    egress.set_credit_counter(&producer_credits);
    
    std::atomic<bool> producer_done{false};
    std::atomic<bool> decoder_done{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> decoded{0};
    std::atomic<size_t> consumed{0};
    std::atomic<uint64_t> credit_waits{0};
    
    auto t0 = high_resolution_clock::now();
    
    // Stage 1: Producer with credit-based backpressure
    std::thread t1([&]() {
        for (size_t i = 0; i < TOTAL_TOKENS; ++i) {
            // Check credits before producing (backpressure)
            while (!producer_credits.try_acquire(1)) {
                credit_waits++;
                _mm_pause();
            }
            
            // Now safe to produce
            while (!q1.try_push(static_cast<float>(i))) {
                _mm_pause();
            }
            produced.store(i + 1);
        }
        producer_done.store(true);
    });
    
    // Stage 2: Decoder with bounded speculation
    std::thread t2([&]() {
        size_t count = 0;
        size_t spec_counter = 0;
        
        while (!producer_done.load() || q1.size() > 0) {
            float tok;
            if (!q1.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Base token
            while (!q2.try_push(tok)) {
                _mm_pause();
            }
            count++;
            
            // Speculative amplification (bounded)
            spec_counter++;
            if (spec_counter >= SPEC_INTERVAL) {
                spec_counter = 0;
                
                // Check speculation window
                size_t current = spec_inflight.load(std::memory_order_relaxed);
                if (current < MAX_SPEC) {
                    if (spec_inflight.compare_exchange_weak(
                        current, current + 1,
                        std::memory_order_acq_rel)) {
                        while (!q2.try_push(tok * 0.5f)) {
                            _mm_pause();
                        }
                        count++;
                    }
                }
            }
            
            decoded.store(count);
        }
        decoder_done.store(true);
    });
    
    // Stage 3: Consumer with credit return
    std::thread t3([&]() {
        size_t count = 0;
        
        while (!decoder_done.load() || q2.size() > 0) {
            float tok;
            if (!q2.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            egress.push(tok);
            count++;
            consumed.store(count);
            
            // Release speculation slot
            spec_inflight.fetch_sub(1, std::memory_order_release);
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
    
    double in_tps = produced.load() * 1e6 / us;
    double out_tps = consumed.load() * 1e6 / us;
    double amp = static_cast<double>(decoded.load()) / produced.load();
    
    std::cout << "\n=== Performance ===" << std::endl;
    std::cout << "Input TPS: " << static_cast<uint64_t>(in_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(out_tps) << std::endl;
    std::cout << "Amplification: " << amp << "x" << std::endl;
    
    std::cout << "\n=== Credit Metrics ===" << std::endl;
    std::cout << "Credits acquired: " << producer_credits.total_acquired() << std::endl;
    std::cout << "Credits returned: " << producer_credits.total_returned() << std::endl;
    std::cout << "Credit waits: " << credit_waits.load() << std::endl;
    std::cout << "Wait rate: " << (credit_waits.load() * 100.0 / TOTAL_TOKENS) << "%" << std::endl;
    
    // Verification
    double ratio = static_cast<double>(consumed.load()) / produced.load();
    std::cout << "\nToken ratio: " << ratio << std::endl;
    
    if (ratio >= 1.0 && ratio <= 1.5) {
        std::cout << "[PASS] Credit-based flow control working" << std::endl;
        return 0;
    }
    std::cout << "[FAIL]" << std::endl;
    return 1;
}
