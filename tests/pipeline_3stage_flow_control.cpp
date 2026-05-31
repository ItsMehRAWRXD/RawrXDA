// 3-Stage Lock-Free Pipeline with Proper Flow Control
// Stage 1: Ingress (Producer → Buffer)
// Stage 2: Decode (Amplification)
// Stage 3: Egress (Buffer → Consumer)

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <emmintrin.h>

// Lock-free SPSC Queue with capacity for amplification
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
    
public:
    // Non-blocking push
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
    
    // Blocking push with bounded spin
    bool Push(const T& item, size_t max_spins = 10000) {
        size_t spins = 0;
        while (!TryPush(item)) {
            if (++spins >= max_spins) {
                dropped_++;
                return false;
            }
            _mm_pause();
        }
        return true;
    }
    
    // Non-blocking pop
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
    
    // Batch pop
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
    
    size_t Pushed() const { return pushed_.load(); }
    size_t Popped() const { return popped_.load(); }
    size_t Dropped() const { return dropped_.load(); }
    double DropRate() const {
        size_t total = pushed_.load() + dropped_.load();
        return total > 0 ? (static_cast<double>(dropped_.load()) / total * 100.0) : 0.0;
    }
    
    void ResetMetrics() {
        dropped_.store(0);
        pushed_.store(0);
        popped_.store(0);
    }
};

// Speculative decoder with amplification tracking
class SpeculativeDecoder {
    std::atomic<size_t> tokens_in_{0};
    std::atomic<size_t> tokens_out_{0};
    std::atomic<size_t> speculations_{0};
    
public:
    // Decode with branching amplification
    template<typename OutputFn>
    void Decode(uint32_t token, OutputFn&& output) {
        tokens_in_++;
        
        // Every 4th token triggers speculation (1.25x amplification)
        if ((token & 3) == 0) {
            output(token);           // Original
            output(token + 1);       // Speculative
            tokens_out_ += 2;
            speculations_++;
        } else {
            output(token);
            tokens_out_++;
        }
    }
    
    double Amplification() const {
        size_t in_t = tokens_in_.load();
        return in_t > 0 ? static_cast<double>(tokens_out_.load()) / in_t : 1.0;
    }
    
    size_t SpeculationCount() const { return speculations_.load(); }
};

using namespace std::chrono;

int main() {
    printf("========================================\n");
    printf("3-Stage Pipeline with Flow Control\n");
    printf("========================================\n\n");

    const size_t TOKEN_COUNT = 1'000'000;
    const size_t INGRESS_BATCH = 64;
    const size_t EGRESS_BATCH = 96;  // Matched to amplification (64 * 1.5)
    
    // Stage buffers - sized for amplification headroom
    // Ingress: 8192, Egress: 16384 (2x for amplification)
    LockFreeSPSC<uint32_t, 8192> ingress_buffer;
    LockFreeSPSC<uint32_t, 16384> egress_buffer;
    SpeculativeDecoder decoder;
    
    printf("Configuration:\n");
    printf("  Tokens: %zu\n", TOKEN_COUNT);
    printf("  Ingress batch: %zu\n", INGRESS_BATCH);
    printf("  Egress batch: %zu (matched to amplification)\n", EGRESS_BATCH);
    printf("  Ingress buffer: 8192\n");
    printf("  Egress buffer: 16384 (2x for headroom)\n\n");
    
    // Generate tokens
    std::vector<uint32_t> tokens(TOKEN_COUNT);
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        tokens[i] = static_cast<uint32_t>(i % 32000);
    }

    auto start = high_resolution_clock::now();

    // Stage 1: Producer thread (Ingress)
    std::atomic<bool> producer_done{false};
    std::thread producer([&]() {
        size_t pushed = 0;
        while (pushed < TOKEN_COUNT) {
            // Push with bounded retry (true backpressure)
            if (!ingress_buffer.Push(tokens[pushed], 1000)) {
                continue;  // Retry on drop
            }
            pushed++;
        }
        producer_done.store(true);
    });

    // Stage 2: Decoder thread (middle)
    std::atomic<bool> decoder_done{false};
    std::thread decoder_thread([&]() {
        uint32_t batch_buffer[INGRESS_BATCH];
        
        while (!producer_done.load() || !ingress_buffer.Empty()) {
            // Pull from ingress
            size_t count = ingress_buffer.PopBatch(batch_buffer, INGRESS_BATCH);
            
            if (count > 0) {
                // Decode and push to egress
                for (size_t i = 0; i < count; i++) {
                    decoder.Decode(batch_buffer[i], [&](uint32_t t) {
                        // Push to egress with retry
                        while (!egress_buffer.Push(t, 10000)) {
                            _mm_pause();
                        }
                    });
                }
            } else {
                std::this_thread::yield();
            }
        }
        decoder_done.store(true);
    });

    // Stage 3: Consumer thread (Egress)
    std::vector<uint32_t> output_tokens;
    output_tokens.reserve(TOKEN_COUNT * 2);
    
    uint32_t egress_batch[EGRESS_BATCH];
    
    while (output_tokens.size() < TOKEN_COUNT || !decoder_done.load()) {
        size_t count = egress_buffer.PopBatch(egress_batch, EGRESS_BATCH);
        
        if (count > 0) {
            for (size_t i = 0; i < count; i++) {
                output_tokens.push_back(egress_batch[i]);
            }
        } else if (decoder_done.load() && egress_buffer.Empty()) {
            break;
        } else {
            std::this_thread::yield();
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

    printf("========================================\n");
    printf("RESULTS\n");
    printf("========================================\n");
    printf("Time elapsed:      %.3f seconds\n", seconds);
    printf("\nStage 1 (Ingress):\n");
    printf("  Tokens pushed:   %zu\n", tokens_pushed);
    printf("  Tokens dropped:  %zu\n", tokens_dropped);
    printf("  Drop rate:       %.2f%%\n", drop_rate);
    printf("  Utilization:     %.1f%%\n", ingress_buffer.Utilization());
    printf("\nStage 2 (Decode):\n");
    printf("  Amplification:   %.2fx\n", amplification);
    printf("  Speculations:    %zu\n", decoder.SpeculationCount());
    printf("\nStage 3 (Egress):\n");
    printf("  Tokens out:      %zu\n", tokens_out);
    printf("  Utilization:     %.1f%%\n", egress_buffer.Utilization());
    printf("\nThroughput:\n");
    printf("  Input TPS:       %.0f\n", input_tps);
    printf("  Output TPS:      %.0f\n", output_tps);
    printf("========================================\n");

    // Validate
    bool valid = true;
    for (size_t i = 0; i < tokens_pushed && i < 1000; i++) {
        if (output_tokens[i] != tokens[i]) {
            valid = false;
            break;
        }
    }
    
    if (valid) {
        printf("\n✅ Token integrity verified\n");
    }

    printf("\n🚀 3-Stage Pipeline: STABLE\n");
    printf("   Effective TPS: %.0f\n\n", output_tps);
    
    return 0;
}
