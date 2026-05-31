// Minimal TPS Measurement Harness for RawrXD Token Pipeline
// Simplified version that measures raw throughput without sequence ID complexity

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>

// Minimal SPSC queue for TPS measurement
template<typename T, size_t Capacity>
class SimpleSPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    T buffer_[Capacity];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    
public:
    bool Push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Capacity - 1);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool Pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    size_t Size() const {
        return (tail_.load(std::memory_order_acquire) - 
                head_.load(std::memory_order_acquire)) & (Capacity - 1);
    }
};

using namespace std::chrono;

int main() {
    printf("========================================\n");
    printf("RawrXD Token Pipeline TPS Measurement\n");
    printf("========================================\n\n");

    // Configuration
    const size_t TOKEN_COUNT = 1'000'000;
    const size_t QUEUE_SIZE = 8192;
    
    // Simple SPSC queue
    SimpleSPSCQueue<uint32_t, QUEUE_SIZE> queue;
    
    // Generate synthetic tokens
    printf("Generating %zu synthetic tokens...\n", TOKEN_COUNT);
    std::vector<uint32_t> tokens(TOKEN_COUNT);
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        tokens[i] = static_cast<uint32_t>(i % 32000);
    }
    printf("Done.\n\n");

    // Measure TPS
    printf("Running TPS measurement...\n");
    printf("Queue capacity: %zu tokens\n\n", QUEUE_SIZE);

    auto start = high_resolution_clock::now();

    // Producer thread
    std::atomic<size_t> pushed{0};
    std::atomic<bool> producer_done{false};
    
    std::thread producer([&]() {
        size_t local_pushed = 0;
        while (local_pushed < TOKEN_COUNT) {
            if (!queue.Push(tokens[local_pushed])) {
                std::this_thread::yield();
                continue;
            }
            local_pushed++;
            if (local_pushed % 10000 == 0) {
                pushed.store(local_pushed);
            }
        }
        pushed.store(local_pushed);
        producer_done.store(true);
    });

    // Consumer
    std::vector<uint32_t> drained_tokens;
    drained_tokens.reserve(TOKEN_COUNT);
    uint32_t token;
    
    while (drained_tokens.size() < TOKEN_COUNT) {
        if (queue.Pop(token)) {
            drained_tokens.push_back(token);
        } else if (producer_done.load() && queue.Size() == 0) {
            break;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();

    auto end = high_resolution_clock::now();

    // Calculate metrics
    double seconds = duration<double>(end - start).count();
    double tps = drained_tokens.size() / seconds;
    double latency_ns = (seconds * 1'000'000'000.0) / drained_tokens.size();

    printf("\n========================================\n");
    printf("RESULTS\n");
    printf("========================================\n");
    printf("Tokens processed: %zu\n", drained_tokens.size());
    printf("Time elapsed:     %.3f seconds\n", seconds);
    printf("Throughput:       %.0f tokens/sec\n", tps);
    printf("Latency:          %.1f nanoseconds/token\n", latency_ns);
    printf("========================================\n");

    // Validate
    bool valid = true;
    for (size_t i = 0; i < drained_tokens.size() && i < 1000; ++i) {
        if (drained_tokens[i] != tokens[i]) {
            valid = false;
            printf("Mismatch at %zu: expected %u, got %u\n", i, tokens[i], drained_tokens[i]);
            break;
        }
    }
    
    if (valid) {
        printf("\n✅ Token integrity verified (first 1000 tokens match)\n");
    } else {
        printf("\n⚠️  Token mismatch detected\n");
    }

    printf("\n🚀 RawrXD Token Pipeline: MEASURABLE\n");
    printf("   Throughput: %.0f TPS\n", tps);
    
    return 0;
}
