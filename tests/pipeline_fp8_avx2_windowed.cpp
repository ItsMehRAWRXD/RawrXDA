// Pipeline FP8 AVX2 with Windowed Credit System
// Fixes over-constrained per-token crediting with batch-window amortization

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// =============================================================================
// Windowed Credit System - Amortized Streaming Model
// Credits issued in batches (4096 tokens) rather than per-token
// =============================================================================
class WindowedCreditSystem {
    static constexpr size_t WINDOW_SIZE = 4096;  // Credit window size
    
    alignas(64) std::atomic<int64_t> credits_{0};  // Available credits (can go negative temporarily)
    alignas(64) std::atomic<uint64_t> windows_issued_{0};
    alignas(64) std::atomic<uint64_t> windows_returned_{0};
    alignas(64) std::atomic<uint64_t> wait_count_{0};

public:
    // Producer acquires a window of credits (non-blocking best-effort)
    bool acquire_window(size_t& window_start, size_t window_size = WINDOW_SIZE) {
        int64_t current = credits_.load(std::memory_order_relaxed);
        
        // Fast path: check if window available
        if (current < static_cast<int64_t>(window_size)) {
            wait_count_++;
            return false;  // Window not available
        }
        
        // Try to acquire window atomically
        if (credits_.compare_exchange_weak(
            current, 
            current - static_cast<int64_t>(window_size),
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            window_start = windows_issued_.fetch_add(1) * WINDOW_SIZE;
            return true;
        }
        
        wait_count_++;
        return false;
    }
    
    // Producer acquires window with spin (for guaranteed progress)
    void acquire_window_blocking(size_t& window_start, size_t window_size = WINDOW_SIZE) {
        while (!acquire_window(window_start, window_size)) {
            _mm_pause();
        }
    }
    
    // Consumer returns a window of credits
    void return_window(size_t window_size = WINDOW_SIZE) {
        credits_.fetch_add(static_cast<int64_t>(window_size), std::memory_order_release);
        windows_returned_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Initialize with total capacity (in windows)
    void initialize(size_t total_windows) {
        credits_.store(static_cast<int64_t>(total_windows * WINDOW_SIZE), std::memory_order_relaxed);
    }
    
    // Metrics
    uint64_t get_waits() const { return wait_count_.load(); }
    double get_wait_rate(uint64_t total_ops) const {
        return total_ops > 0 ? static_cast<double>(wait_count_.load()) / total_ops * 100.0 : 0.0;
    }
};

// Simple SPSC queue (unchanged from stable version)
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
class WindowedEgress {
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
    std::cout << "=== FP8 AVX2 Windowed Credit Pipeline ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    std::cout << "[OK] AVX2 available" << std::endl;
    
    // Configuration
    constexpr size_t CAPACITY = 65536;
    constexpr size_t TOTAL = 1000000;
    constexpr size_t SPEC_INTERVAL = 4;
    constexpr size_t NUM_WINDOWS = 256;  // Total credit windows
    
    SimpleSPSC<CAPACITY> q1;
    SimpleSPSC<CAPACITY> q2;
    WindowedEgress egress;
    
    // Credit systems for each stage transition
    WindowedCreditSystem producer_credits;   // Producer -> Decoder
    WindowedCreditSystem decoder_credits;    // Decoder -> Consumer
    
    // Initialize credits (capacity in windows)
    producer_credits.initialize(NUM_WINDOWS);
    decoder_credits.initialize(NUM_WINDOWS);
    
    std::atomic<bool> producer_done{false};
    std::atomic<bool> decoder_done{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> decoded{0};
    std::atomic<size_t> consumed{0};
    
    auto t0 = high_resolution_clock::now();
    
    // Stage 1: Producer with windowed credits
    std::thread t1([&]() {
        size_t local_produced = 0;
        size_t window_start = 0;
        
        while (local_produced < TOTAL) {
            // Acquire window of credits (non-blocking)
            if (!producer_credits.acquire_window(window_start)) {
                _mm_pause();
                continue;
            }
            
            // Produce entire window without synchronization
            size_t to_produce = std::min(WindowedCreditSystem::WINDOW_SIZE, TOTAL - local_produced);
            for (size_t i = 0; i < to_produce; ++i) {
                while (!q1.try_push(static_cast<float>(local_produced + i))) {
                    _mm_pause();
                }
            }
            
            local_produced += to_produce;
            produced.store(local_produced);
        }
        producer_done.store(true);
    });
    
    // Stage 2: Decoder with windowed credits + speculation
    std::thread t2([&]() {
        size_t local_decoded = 0;
        size_t speculation_counter = 0;
        size_t window_start = 0;
        
        while (!producer_done.load() || !q1.empty()) {
            float tok;
            if (!q1.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Acquire decoder credits in windows
            if (local_decoded % WindowedCreditSystem::WINDOW_SIZE == 0) {
                decoder_credits.acquire_window_blocking(window_start);
            }
            
            // Base token
            while (!q2.try_push(tok)) _mm_pause();
            local_decoded++;
            
            // Speculative amplification (within credit window)
            speculation_counter++;
            if (speculation_counter >= SPEC_INTERVAL) {
                speculation_counter = 0;
                while (!q2.try_push(tok * 0.5f)) _mm_pause();
                local_decoded++;
            }
            
            // Return producer credits in windows
            if (local_decoded % WindowedCreditSystem::WINDOW_SIZE == 0) {
                producer_credits.return_window();
            }
            
            decoded.store(local_decoded);
        }
        
        // Return remaining credits
        producer_credits.return_window();
        decoder_done.store(true);
    });
    
    // Stage 3: Consumer with AVX2 FP8
    std::thread t3([&]() {
        size_t local_consumed = 0;
        size_t window_start = 0;
        
        while (!decoder_done.load() || !q2.empty()) {
            float tok;
            if (!q2.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Return decoder credits in windows
            if (local_consumed > 0 && local_consumed % WindowedCreditSystem::WINDOW_SIZE == 0) {
                decoder_credits.return_window();
            }
            
            egress.push(tok);
            local_consumed++;
            consumed.store(local_consumed);
        }
        
        // Return final credits
        decoder_credits.return_window();
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
    std::cout << "Producer waits: " << producer_credits.get_waits() << std::endl;
    std::cout << "Decoder waits: " << decoder_credits.get_waits() << std::endl;
    std::cout << "Producer wait rate: " << producer_credits.get_wait_rate(produced.load()) << "%" << std::endl;
    std::cout << "Decoder wait rate: " << decoder_credits.get_wait_rate(decoded.load()) << "%" << std::endl;
    
    double in_tps = produced.load() * 1e6 / us;
    double out_tps = consumed.load() * 1e6 / us;
    double amp = static_cast<double>(decoded.load()) / produced.load();
    
    std::cout << "\n=== Performance ===" << std::endl;
    std::cout << "Input TPS: " << static_cast<uint64_t>(in_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(out_tps) << std::endl;
    std::cout << "Amplification: " << amp << "x" << std::endl;
    
    double ratio = static_cast<double>(consumed.load()) / produced.load();
    std::cout << "\nToken ratio: " << ratio << std::endl;
    
    if (ratio >= 1.0 && ratio <= 1.5 && in_tps > 5e6) {
        std::cout << "[PASS] Windowed credit system with restored throughput" << std::endl;
        return 0;
    }
    std::cout << "[FAIL] Throughput collapsed" << std::endl;
    return 1;
}
