// Pipeline FP8 AVX2 Stable - Working Version with Proper Flow Control
// Uses non-blocking with yield and proper completion signaling

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

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
class StableEgress {
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
    std::cout << "=== FP8 AVX2 Stable Pipeline ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    std::cout << "[OK] AVX2 available" << std::endl;
    
    constexpr size_t CAPACITY = 65536;  // Increased for 1M tokens
    constexpr size_t TOTAL = 1000000;    // 1M tokens
    constexpr size_t SPEC_INTERVAL = 4;
    
    SimpleSPSC<CAPACITY> q1;  // Producer -> Decoder
    SimpleSPSC<CAPACITY> q2;  // Decoder -> Consumer
    StableEgress egress;
    
    std::atomic<bool> done1{false};
    std::atomic<bool> done2{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> decoded{0};
    std::atomic<size_t> consumed{0};
    
    auto t0 = high_resolution_clock::now();
    
    // Producer
    std::thread t1([&]() {
        for (size_t i = 0; i < TOTAL; ++i) {
            while (!q1.try_push(static_cast<float>(i))) {
                _mm_pause();
            }
            produced.store(i + 1);
        }
        done1.store(true);
    });
    
    // Decoder with amplification
    std::thread t2([&]() {
        size_t count = 0;
        size_t spec = 0;
        while (!done1.load() || !q1.empty()) {
            float tok;
            if (!q1.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            
            // Base token
            while (!q2.try_push(tok)) _mm_pause();
            count++;
            
            // Speculative
            spec++;
            if (spec >= SPEC_INTERVAL) {
                spec = 0;
                while (!q2.try_push(tok * 0.5f)) _mm_pause();
                count++;
            }
            decoded.store(count);
        }
        done2.store(true);
    });
    
    // Consumer with FP8
    std::thread t3([&]() {
        size_t count = 0;
        while (!done2.load() || !q2.empty()) {
            float tok;
            if (!q2.try_pop(tok)) {
                _mm_pause();
                continue;
            }
            egress.push(tok);
            count++;
            consumed.store(count);
        }
        egress.flush();
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    auto t1_end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t1_end - t0).count();
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Time: " << us << " us" << std::endl;
    std::cout << "Produced: " << produced.load() << std::endl;
    std::cout << "Decoded: " << decoded.load() << std::endl;
    std::cout << "Consumed: " << consumed.load() << std::endl;
    std::cout << "Batches: " << egress.batches() << std::endl;
    
    double in_tps = produced.load() * 1e6 / us;
    double out_tps = consumed.load() * 1e6 / us;
    double amp = static_cast<double>(decoded.load()) / produced.load();
    
    std::cout << "\nInput TPS: " << static_cast<uint64_t>(in_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(out_tps) << std::endl;
    std::cout << "Amplification: " << amp << "x" << std::endl;
    
    double ratio = static_cast<double>(consumed.load()) / produced.load();
    std::cout << "\nRatio: " << ratio << std::endl;
    
    if (ratio >= 1.0 && ratio <= 1.5) {
        std::cout << "[PASS]" << std::endl;
        return 0;
    }
    std::cout << "[FAIL]" << std::endl;
    return 1;
}
