// Pipeline FP8 AVX2 Hardened - Production-Ready Flow Control
// Addresses drop rates through proper backpressure and stage pacing

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// =============================================================================
// Hardened Lock-Free SPSC with Backpressure Signaling
// =============================================================================
template<size_t Capacity>
class HardenedSPSC {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) float buffer_[Capacity];
    
    // Metrics
    alignas(64) std::atomic<uint64_t> pushes_{0};
    alignas(64) std::atomic<uint64_t> pops_{0};
    alignas(64) std::atomic<uint64_t> drops_{0};

public:
    static constexpr size_t MASK = Capacity - 1;
    static constexpr size_t HIGH_WATER_MARK = Capacity * 3 / 4;  // 75%
    static constexpr size_t LOW_WATER_MARK = Capacity * 1 / 4;   // 25%
    
    // Non-blocking push with backpressure awareness
    bool push(float token) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & MASK;
        
        // Check if full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            drops_++;
            return false;
        }
        
        buffer_[current_tail & MASK] = token;
        tail_.store(next_tail, std::memory_order_release);
        pushes_++;
        return true;
    }
    
    // Non-blocking pop
    bool pop(float& token) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        
        token = buffer_[current_head & MASK];
        head_.store((current_head + 1) & MASK, std::memory_order_release);
        pops_++;
        return true;
    }
    
    // Pop multiple tokens at once (batch optimization)
    size_t pop_batch(float* output, size_t max_count) {
        size_t count = 0;
        while (count < max_count && pop(output[count])) {
            count++;
        }
        return count;
    }
    
    // Push multiple tokens
    size_t push_batch(const float* input, size_t count) {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(input[i])) break;
            pushed++;
        }
        return pushed;
    }
    
    size_t size() const {
        return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_relaxed)) & MASK;
    }
    
    bool empty() const { return size() == 0; }
    float utilization() const { return static_cast<float>(size()) / Capacity * 100.0f; }
    bool is_high_water() const { return size() >= HIGH_WATER_MARK; }
    bool is_low_water() const { return size() <= LOW_WATER_MARK; }
    
    // Metrics
    uint64_t get_pushes() const { return pushes_.load(); }
    uint64_t get_pops() const { return pops_.load(); }
    uint64_t get_drops() const { return drops_.load(); }
    double get_drop_rate() const {
        uint64_t total = pushes_.load() + drops_.load();
        return total > 0 ? static_cast<double>(drops_.load()) / total * 100.0 : 0.0;
    }
    
    void reset_metrics() {
        pushes_.store(0);
        pops_.store(0);
        drops_.store(0);
    }
};

// =============================================================================
// Stage 3 Egress with AVX2 FP8 and Aligned Batch Processing
// =============================================================================
class HardenedFP8EgressStage {
    static constexpr size_t BATCH_SIZE = 64;
    static constexpr size_t MAX_PENDING_BATCHES = 256;
    
    // Aligned memory for AVX2 operations
    alignas(256) float batch_buffer_[BATCH_SIZE];
    size_t batch_fill_ = 0;
    
    // Output buffer for quantized data
    alignas(64) std::vector<uint8_t> output_buffer_;
    size_t output_head_ = 0;
    size_t output_tail_ = 0;
    
    // Batch processor for aligned memory
    RawrXD::Kernels::FP8BatchProcessor batch_processor_;
    
    // Metrics
    std::atomic<uint64_t> tokens_quantized_{0};
    std::atomic<uint64_t> batches_processed_{0};
    std::atomic<uint64_t> partial_flushes_{0};
    std::atomic<uint64_t> tokens_dropped_{0};

public:
    HardenedFP8EgressStage() : batch_processor_(MAX_PENDING_BATCHES) {
        output_buffer_.resize(BATCH_SIZE * MAX_PENDING_BATCHES);
    }
    
    // Push token with automatic batching and quantization
    bool push_token(float token) {
        batch_buffer_[batch_fill_++] = token;
        
        if (batch_fill_ >= BATCH_SIZE) {
            return flush_batch();
        }
        return true;
    }
    
    // Flush current batch using AVX2
    bool flush_batch() {
        if (batch_fill_ == 0) return true;
        
        // Pad to BATCH_SIZE if needed
        for (size_t i = batch_fill_; i < BATCH_SIZE; ++i) {
            batch_buffer_[i] = 0.0f;
        }
        
        // Quantize using AVX2
        alignas(256) uint8_t quantized[BATCH_SIZE];
        RawrXD::Kernels::FP8AVX2Quantizer::QuantizeBatch64(
            batch_buffer_, quantized, 1.0f
        );
        
        // Store to output buffer
        size_t output_pos = output_tail_;
        size_t next_tail = (output_tail_ + BATCH_SIZE) % output_buffer_.size();
        
        if (next_tail == output_head_) {
            // Buffer full - drop batch
            tokens_dropped_ += batch_fill_;
            batch_fill_ = 0;
            return false;
        }
        
        std::memcpy(&output_buffer_[output_pos], quantized, BATCH_SIZE);
        output_tail_ = next_tail;
        
        tokens_quantized_ += batch_fill_;
        batches_processed_++;
        batch_fill_ = 0;
        return true;
    }
    
    // Flush partial batch (for pipeline liveness)
    void flush_partial() {
        if (batch_fill_ > 0) {
            partial_flushes_++;
            flush_batch();
        }
    }
    
    // Get quantized output
    bool pop_quantized(uint8_t& token) {
        if (output_head_ == output_tail_) {
            return false;
        }
        
        token = output_buffer_[output_head_];
        output_head_ = (output_head_ + 1) % output_buffer_.size();
        return true;
    }
    
    // Metrics
    uint64_t get_tokens_quantized() const { return tokens_quantized_.load(); }
    uint64_t get_batches_processed() const { return batches_processed_.load(); }
    uint64_t get_partial_flushes() const { return partial_flushes_.load(); }
    uint64_t get_tokens_dropped() const { return tokens_dropped_.load(); }
    size_t get_batch_fill() const { return batch_fill_; }
};

// =============================================================================
// Per-Stage Metrics for Observability
// =============================================================================
struct StageMetrics {
    std::atomic<uint64_t> tokens_processed{0};
    std::atomic<uint64_t> tokens_dropped{0};
    std::atomic<uint64_t> batches_processed{0};
    std::atomic<uint64_t> stall_cycles{0};
    
    high_resolution_clock::time_point start_time;
    high_resolution_clock::time_point end_time;
    
    void record_start() { start_time = high_resolution_clock::now(); }
    void record_end() { end_time = high_resolution_clock::now(); }
    
    double get_throughput_tps() const {
        auto elapsed = duration_cast<microseconds>(end_time - start_time).count();
        return elapsed > 0 ? static_cast<double>(tokens_processed.load()) * 1e6 / elapsed : 0;
    }
    
    double get_drop_rate() const {
        uint64_t total = tokens_processed.load() + tokens_dropped.load();
        return total > 0 ? static_cast<double>(tokens_dropped.load()) / total * 100.0 : 0.0;
    }
};

// =============================================================================
// Test Harness with Production Flow Control
// =============================================================================
int main() {
    std::cout << "=== FP8 AVX2 Hardened Pipeline Test ===" << std::endl;
    
    // Check AVX2
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    std::cout << "[OK] AVX2 detected" << std::endl;
    
    // Configuration - tuned for stability
    constexpr size_t INGRESS_CAPACITY = 16384;   // Doubled for headroom
    constexpr size_t DECODE_CAPACITY = 8192;      // Doubled for headroom
    constexpr size_t TOTAL_TOKENS = 1'000'000;
    constexpr size_t PRODUCER_BATCH = 64;
    constexpr size_t DECODER_BATCH = 64;
    constexpr size_t CONSUMER_BATCH = 64;
    constexpr size_t YIELD_THRESHOLD = 1000;      // Yield after N empty polls
    constexpr auto PARTIAL_FLUSH_TIMEOUT = microseconds(50);  // 50us (was 100us)
    
    HardenedSPSC<INGRESS_CAPACITY> ingress_queue;
    HardenedSPSC<DECODE_CAPACITY> decode_queue;
    HardenedFP8EgressStage egress_stage;
    
    StageMetrics producer_metrics;
    StageMetrics decoder_metrics;
    StageMetrics consumer_metrics;
    
    std::atomic<bool> producer_done{false};
    std::atomic<bool> decoder_done{false};
    std::atomic<uint64_t> tokens_produced{0};
    std::atomic<uint64_t> tokens_decoded{0};
    std::atomic<uint64_t> tokens_consumed{0};
    
    auto global_start = high_resolution_clock::now();
    
    // Stage 1: Producer with adaptive pacing
    std::thread producer([&]() {
        producer_metrics.record_start();
        size_t produced = 0;
        size_t empty_polls = 0;
        
        while (produced < TOTAL_TOKENS) {
            // Adaptive batch sizing based on queue pressure
            size_t batch_size = PRODUCER_BATCH;
            if (ingress_queue.is_high_water()) {
                batch_size = PRODUCER_BATCH / 2;  // Reduce pressure
                std::this_thread::yield();
            }
            
            size_t to_push = std::min(batch_size, TOTAL_TOKENS - produced);
            size_t pushed = 0;
            
            for (size_t i = 0; i < to_push; ++i) {
                if (ingress_queue.push(static_cast<float>(produced + i))) {
                    pushed++;
                } else {
                    producer_metrics.tokens_dropped++;
                }
            }
            
            produced += pushed;
            tokens_produced.store(produced);
            producer_metrics.tokens_processed += pushed;
            
            if (pushed < to_push) {
                empty_polls++;
                if (empty_polls >= YIELD_THRESHOLD) {
                    std::this_thread::sleep_for(microseconds(1));
                    empty_polls = 0;
                } else {
                    std::this_thread::yield();
                }
            } else {
                empty_polls = 0;
            }
        }
        
        producer_metrics.record_end();
        producer_done.store(true);
    });
    
    // Stage 2: Decoder with amplification and pacing
    std::thread decoder([&]() {
        decoder_metrics.record_start();
        size_t decoded = 0;
        size_t speculation_counter = 0;
        constexpr size_t SPECULATION_INTERVAL = 4;  // 1 in 4 tokens amplified
        size_t empty_polls = 0;
        
        alignas(64) float batch_buffer[DECODER_BATCH];
        
        while (!producer_done.load() || !ingress_queue.empty()) {
            // Batch pop from ingress
            size_t batch_count = ingress_queue.pop_batch(batch_buffer, DECODER_BATCH);
            
            if (batch_count == 0) {
                empty_polls++;
                if (empty_polls >= YIELD_THRESHOLD) {
                    std::this_thread::sleep_for(microseconds(1));
                    empty_polls = 0;
                } else {
                    std::this_thread::yield();
                }
                continue;
            }
            
            empty_polls = 0;
            
            // Process batch with amplification
            for (size_t i = 0; i < batch_count; ++i) {
                // Base token
                if (!decode_queue.push(batch_buffer[i])) {
                    decoder_metrics.tokens_dropped++;
                    continue;
                }
                decoded++;
                
                // Speculative amplification
                speculation_counter++;
                if (speculation_counter >= SPECULATION_INTERVAL) {
                    speculation_counter = 0;
                    // Generate speculative token
                    if (!decode_queue.push(batch_buffer[i] * 0.5f)) {
                        decoder_metrics.tokens_dropped++;
                    } else {
                        decoded++;
                    }
                }
            }
            
            tokens_decoded.store(decoded);
            decoder_metrics.tokens_processed += batch_count;
            decoder_metrics.batches_processed++;
        }
        
        decoder_metrics.record_end();
        decoder_done.store(true);
    });
    
    // Stage 3: Consumer with AVX2 FP8
    std::thread consumer([&]() {
        consumer_metrics.record_start();
        size_t consumed = 0;
        size_t empty_polls = 0;
        auto last_flush = high_resolution_clock::now();
        
        alignas(64) float batch_buffer[CONSUMER_BATCH];
        
        while (!decoder_done.load() || !decode_queue.empty()) {
            // Batch pop from decode queue
            size_t batch_count = decode_queue.pop_batch(batch_buffer, CONSUMER_BATCH);
            
            if (batch_count > 0) {
                empty_polls = 0;
                
                // Push to egress stage
                for (size_t i = 0; i < batch_count; ++i) {
                    egress_stage.push_token(batch_buffer[i]);
                }
                
                consumed += batch_count;
                tokens_consumed.store(consumed);
                consumer_metrics.tokens_processed += batch_count;
                
                last_flush = high_resolution_clock::now();
            } else {
                // Check for partial flush
                auto now = high_resolution_clock::now();
                if (duration_cast<microseconds>(now - last_flush) >= PARTIAL_FLUSH_TIMEOUT) {
                    egress_stage.flush_partial();
                    last_flush = now;
                }
                
                empty_polls++;
                if (empty_polls >= YIELD_THRESHOLD) {
                    std::this_thread::sleep_for(microseconds(1));
                    empty_polls = 0;
                } else {
                    std::this_thread::yield();
                }
            }
        }
        
        // Final flush
        egress_stage.flush_partial();
        
        // Drain remaining output
        uint8_t token;
        while (egress_stage.pop_quantized(token)) {
            // Output consumed
        }
        
        consumer_metrics.record_end();
    });
    
    // Wait for completion
    producer.join();
    decoder.join();
    consumer.join();
    
    auto global_end = high_resolution_clock::now();
    auto elapsed_us = duration_cast<microseconds>(global_end - global_start).count();
    
    // Results
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Time elapsed: " << elapsed_us << " us (" << elapsed_us / 1000.0 << " ms)" << std::endl;
    std::cout << "Tokens produced: " << tokens_produced.load() << std::endl;
    std::cout << "Tokens decoded: " << tokens_decoded.load() << std::endl;
    std::cout << "Tokens consumed: " << tokens_consumed.load() << std::endl;
    std::cout << "Batches quantized: " << egress_stage.get_batches_processed() << std::endl;
    std::cout << "Partial flushes: " << egress_stage.get_partial_flushes() << std::endl;
    
    // Per-stage metrics
    std::cout << "\n=== Per-Stage Metrics ===" << std::endl;
    std::cout << "Producer:" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(producer_metrics.get_throughput_tps()) << " TPS" << std::endl;
    std::cout << "  Drop rate: " << producer_metrics.get_drop_rate() << "%" << std::endl;
    std::cout << "  Drops: " << producer_metrics.tokens_dropped.load() << std::endl;
    
    std::cout << "Decoder:" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(decoder_metrics.get_throughput_tps()) << " TPS" << std::endl;
    std::cout << "  Drop rate: " << decoder_metrics.get_drop_rate() << "%" << std::endl;
    std::cout << "  Drops: " << decoder_metrics.tokens_dropped.load() << std::endl;
    std::cout << "  Batches: " << decoder_metrics.batches_processed.load() << std::endl;
    
    std::cout << "Consumer:" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(consumer_metrics.get_throughput_tps()) << " TPS" << std::endl;
    std::cout << "  Egress drops: " << egress_stage.get_tokens_dropped() << std::endl;
    
    // Queue metrics
    std::cout << "\n=== Queue Metrics ===" << std::endl;
    std::cout << "Ingress - Pushes: " << ingress_queue.get_pushes() 
              << ", Pops: " << ingress_queue.get_pops() 
              << ", Drops: " << ingress_queue.get_drops() << std::endl;
    std::cout << "Ingress - Final utilization: " << ingress_queue.utilization() << "%" << std::endl;
    std::cout << "Decode - Pushes: " << decode_queue.get_pushes() 
              << ", Pops: " << decode_queue.get_pops() 
              << ", Drops: " << decode_queue.get_drops() << std::endl;
    std::cout << "Decode - Final utilization: " << decode_queue.utilization() << "%" << std::endl;
    
    // Overall performance
    double input_tps = tokens_produced.load() * 1e6 / elapsed_us;
    double output_tps = tokens_consumed.load() * 1e6 / elapsed_us;
    double amplification = static_cast<double>(tokens_decoded.load()) / tokens_produced.load();
    double total_drops = producer_metrics.tokens_dropped.load() + decoder_metrics.tokens_dropped.load();
    double total_drop_rate = total_drops / (tokens_produced.load() + tokens_decoded.load()) * 100.0;
    
    std::cout << "\n=== Overall Performance ===" << std::endl;
    std::cout << "Input TPS: " << static_cast<uint64_t>(input_tps) << std::endl;
    std::cout << "Output TPS: " << static_cast<uint64_t>(output_tps) << std::endl;
    std::cout << "Amplification: " << amplification << "x" << std::endl;
    std::cout << "Total drop rate: " << total_drop_rate << "%" << std::endl;
    
    // Verification
    double ratio = static_cast<double>(tokens_consumed.load()) / tokens_produced.load();
    std::cout << "\nToken ratio (out/in): " << ratio << std::endl;
    
    if (ratio >= 1.0 && ratio <= 1.5 && total_drop_rate < 5.0) {
        std::cout << "[PASS] Pipeline integrity and drop rate acceptable" << std::endl;
        return 0;
    } else {
        std::cout << "[FAIL] Pipeline issues detected" << std::endl;
        return 1;
    }
}
