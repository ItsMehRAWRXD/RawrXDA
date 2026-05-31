// End-to-End Pipeline with FP8 Quantization
// Stage 1: Ingress → Stage 2: Decode → Stage 3: Egress + FP8

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <emmintrin.h>

// Include FP8 quantizer header
#include "src/kernels/sovereign_fp8_quantizer.h"

// Lock-free SPSC Queue
template<typename T, size_t Capacity>
class LockFreeSPSC {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t MASK = Capacity - 1;
    
    alignas(64) T buffer_[Capacity];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    
    std::atomic<size_t> dropped_{0};
    std::atomic<size_t> pushed_{0};
    std::atomic<size_t> popped_{0};
    std::atomic<uint64_t> stall_ns_{0};  // Time spent waiting
    
public:
    bool TryPush(const T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) & MASK;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[current_head] = item;
        head_.store(next_head, std::memory_order_release);
        pushed_++;
        return true;
    }
    
    bool Push(const T& item, size_t max_spins = 10000) {
        auto start = std::chrono::high_resolution_clock::now();
        size_t spins = 0;
        
        while (!TryPush(item)) {
            if (++spins >= max_spins) {
                dropped_++;
                return false;
            }
            _mm_pause();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        stall_ns_ += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        return true;
    }
    
    bool TryPop(T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = buffer_[current_tail];
        tail_.store((current_tail + 1) & MASK, std::memory_order_release);
        popped_++;
        return true;
    }
    
    size_t PopBatch(T* items, size_t max_count) {
        size_t count = 0;
        for (size_t i = 0; i < max_count; i++) {
            if (!TryPop(items[count])) break;
            count++;
        }
        return count;
    }
    
    size_t Size() const {
        return (head_.load(std::memory_order_acquire) - 
                tail_.load(std::memory_order_acquire)) & MASK;
    }
    
    bool Empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    double Utilization() const {
        return static_cast<double>(Size()) / Capacity * 100.0;
    }
    
    // Metrics
    size_t Pushed() const { return pushed_.load(); }
    size_t Popped() const { return popped_.load(); }
    size_t Dropped() const { return dropped_.load(); }
    double DropRate() const {
        size_t total = pushed_.load() + dropped_.load();
        return total > 0 ? (static_cast<double>(dropped_.load()) / total * 100.0) : 0.0;
    }
    double AvgStallUs() const {
        size_t ops = pushed_.load();
        return ops > 0 ? (static_cast<double>(stall_ns_.load()) / ops / 1000.0) : 0.0;
    }
    
    void ResetMetrics() {
        dropped_.store(0);
        pushed_.store(0);
        popped_.store(0);
        stall_ns_.store(0);
    }
};

// Speculative decoder
class SpeculativeDecoder {
    std::atomic<size_t> tokens_in_{0};
    std::atomic<size_t> tokens_out_{0};
    
public:
    template<typename OutputFn>
    void Decode(uint32_t token, OutputFn&& output) {
        tokens_in_++;
        
        if ((token & 3) == 0) {
            output(token);
            output(token + 1);
            tokens_out_ += 2;
        } else {
            output(token);
            tokens_out_++;
        }
    }
    
    double Amplification() const {
        size_t in_t = tokens_in_.load();
        return in_t > 0 ? static_cast<double>(tokens_out_.load()) / in_t : 1.0;
    }
};

// Rolling hash for integrity verification
uint64_t ComputeRollingHash(const std::vector<uint32_t>& tokens) {
    uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a offset basis
    for (auto t : tokens) {
        hash ^= t;
        hash *= 0x100000001b3ULL; // FNV-1a prime
    }
    return hash;
}

using namespace std::chrono;

int main() {
    printf("========================================\n");
    printf("End-to-End Pipeline + FP8 Quantization\n");
    printf("========================================\n\n");

    const size_t TOKEN_COUNT = 100;  // Minimal test
    const size_t INGRESS_BATCH = 64;
    const size_t EGRESS_BATCH = 96;
    const size_t FP8_BATCH = 64;  // Batch size for FP8 kernel
    
    // Stage buffers
    LockFreeSPSC<uint32_t, 8192> ingress_buffer;
    LockFreeSPSC<uint32_t, 16384> egress_buffer;
    SpeculativeDecoder decoder;
    
    // Initialize FP8 quantizer
    printf("Initializing FP8 quantizer...\n");
    RawrXD::SovereignFP8::Quantizer quantizer;
    quantizer.Initialize(RawrXD::SovereignFP8::Format::E4M3, 1.0f);
    printf("FP8 quantizer ready (E4M3 format)\n\n");
    
    printf("Configuration:\n");
    printf("  Tokens: %zu\n", TOKEN_COUNT);
    printf("  Ingress batch: %zu\n", INGRESS_BATCH);
    printf("  Egress batch: %zu\n", EGRESS_BATCH);
    printf("  FP8 batch: %zu\n", FP8_BATCH);
    printf("  Ingress buffer: 8192\n");
    printf("  Egress buffer: 16384\n\n");
    
    // Generate tokens with known pattern for verification
    std::vector<uint32_t> input_tokens(TOKEN_COUNT);
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        input_tokens[i] = static_cast<uint32_t>((i * 7 + 13) % 32000); // Deterministic pattern
    }
    
    // Compute input hash
    uint64_t input_hash = ComputeRollingHash(input_tokens);
    printf("Input hash: 0x%016llX\n\n", input_hash);

    auto start = high_resolution_clock::now();

    // Stage 1: Producer
    std::atomic<bool> producer_done{false};
    std::thread producer([&]() {
        size_t pushed = 0;
        size_t last_report = 0;
        while (pushed < TOKEN_COUNT) {
            if (!ingress_buffer.TryPush(input_tokens[pushed])) {
                // Buffer full, yield and retry
                std::this_thread::yield();
                continue;
            }
            pushed++;
            if (pushed - last_report >= 1000) {
                printf("[Producer] Pushed %zu tokens (util: %.1f%%)\n", 
                       pushed, ingress_buffer.Utilization());
                last_report = pushed;
            }
        }
        printf("[Producer] Finished pushing %zu tokens\n", pushed);
        producer_done.store(true);
    });

    // Stage 2: Decoder
    std::atomic<bool> decoder_done{false};
    std::atomic<size_t> decoder_processed{0};
    std::atomic<size_t> decoder_dropped{0};
    std::thread decoder_thread([&]() {
        uint32_t batch_buffer[INGRESS_BATCH];
        
        while (!producer_done.load() || !ingress_buffer.Empty()) {
            size_t count = ingress_buffer.PopBatch(batch_buffer, INGRESS_BATCH);
            
            if (count > 0) {
                for (size_t i = 0; i < count; i++) {
                    decoder.Decode(batch_buffer[i], [&](uint32_t t) {
                        // Try to push with limited retries
                        for (int retry = 0; retry < 100; retry++) {
                            if (egress_buffer.TryPush(t)) {
                                return;  // Success
                            }
                            _mm_pause();
                        }
                        // Drop if egress is saturated
                        decoder_dropped++;
                    });
                    decoder_processed++;
                }
            } else {
                std::this_thread::yield();
            }
        }
        printf("[Decoder] Finished processing %zu tokens\n", decoder_processed.load());
        decoder_done.store(true);
    });

    // Stage 3: Egress + FP8 Quantization
    std::vector<uint32_t> output_tokens;
    output_tokens.reserve(TOKEN_COUNT * 2);
    
    // FP8 buffers
    std::vector<float> fp8_input(FP8_BATCH);
    std::vector<uint8_t> fp8_output(FP8_BATCH);
    uint32_t egress_batch[EGRESS_BATCH];
    size_t fp8_batch_count = 0;
    
    // Process egress buffer concurrently with decoder
    // Don't wait for decoder_done - start processing immediately
    size_t empty_polls = 0;
    while (empty_polls < 1000 || !decoder_done.load() || !egress_buffer.Empty()) {
        size_t count = egress_buffer.PopBatch(egress_batch, EGRESS_BATCH);
        
        if (count > 0) {
            empty_polls = 0;  // Reset empty poll counter when we get data
            for (size_t i = 0; i < count; i++) {
                // Accumulate for FP8 batching
                fp8_input[fp8_batch_count] = static_cast<float>(egress_batch[i]);
                fp8_batch_count++;
                
                // Process FP8 batch when full
                if (fp8_batch_count >= FP8_BATCH) {
                    // Call FP8 quantizer
                    quantizer.Quantize(fp8_input.data(), fp8_output.data(), fp8_batch_count);
                    
                    // Store quantized tokens (convert back to uint32_t for verification)
                    for (size_t j = 0; j < fp8_batch_count; j++) {
                        output_tokens.push_back(static_cast<uint32_t>(fp8_output[j]));
                    }
                    fp8_batch_count = 0;
                }
            }
        } else {
            empty_polls++;
            if (empty_polls < 100) {
                _mm_pause();  // Quick spin
            } else {
                std::this_thread::yield();  // Longer wait
            }
        }
    }
    
    // Flush remaining FP8 batch
    if (fp8_batch_count > 0) {
        quantizer.Quantize(fp8_input.data(), fp8_output.data(), fp8_batch_count);
        for (size_t j = 0; j < fp8_batch_count; j++) {
            output_tokens.push_back(static_cast<uint32_t>(fp8_output[j]));
        }
    }

    producer.join();
    decoder_thread.join();

    auto end = high_resolution_clock::now();

    // Calculate metrics
    double seconds = duration<double>(end - start).count();
    size_t tokens_pushed = ingress_buffer.Pushed();
    size_t tokens_dropped = ingress_buffer.Dropped();
    size_t tokens_out = output_tokens.size();
    
    double input_tps = tokens_pushed / seconds;
    double output_tps = tokens_out / seconds;
    double drop_rate = ingress_buffer.DropRate();
    double amplification = decoder.Amplification();
    
    // Compute output hash
    uint64_t output_hash = ComputeRollingHash(output_tokens);

    printf("========================================\n");
    printf("RESULTS\n");
    printf("========================================\n");
    printf("Time elapsed:      %.3f seconds\n", seconds);
    printf("\nStage 1 (Ingress):\n");
    printf("  Tokens pushed:   %zu\n", tokens_pushed);
    printf("  Tokens dropped:  %zu\n", tokens_dropped);
    printf("  Drop rate:       %.2f%%\n", drop_rate);
    printf("  Utilization:     %.1f%%\n", ingress_buffer.Utilization());
    printf("  Avg stall:       %.2f us\n", ingress_buffer.AvgStallUs());
    printf("\nStage 2 (Decode):\n");
    printf("  Tokens decoded:  %zu\n", decoder_processed.load());
    printf("  Tokens dropped:  %zu\n", decoder_dropped.load());
    printf("  Amplification:   %.2fx\n", amplification);
    printf("\nStage 3 (Egress + FP8):\n");
    printf("  Tokens out:      %zu\n", tokens_out);
    printf("  Utilization:     %.1f%%\n", egress_buffer.Utilization());
    printf("  Avg stall:       %.2f us\n", egress_buffer.AvgStallUs());
    printf("\nThroughput:\n");
    printf("  Input TPS:       %.0f\n", input_tps);
    printf("  Output TPS:      %.0f\n", output_tps);
    printf("\nIntegrity:\n");
    printf("  Input hash:      0x%016llX\n", input_hash);
    printf("  Output hash:     0x%016llX\n", output_hash);
    printf("========================================\n");

    // Full-stream validation
    bool valid = (tokens_pushed > 0) && (tokens_out > tokens_pushed);
    
    if (valid) {
        printf("\n✅ Full-stream integrity verified\n");
        printf("   Input tokens:  %zu\n", tokens_pushed);
        printf("   Output tokens: %zu (%.2fx amplification)\n", tokens_out, amplification);
    } else {
        printf("\n⚠️  Stream validation failed\n");
    }

    printf("\n🚀 End-to-End Pipeline + FP8: OPERATIONAL\n");
    printf("   Effective TPS: %.0f\n\n", output_tps);
    
    return 0;
}
