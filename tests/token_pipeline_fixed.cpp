// Fixed Token Pipeline - Lock-free SPSC with proper forward progress
// Removes sequence ID deadlock by using simple head/tail indices

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>

// Fixed Token Pipeline
class FixedTokenPipeline {
    static constexpr size_t CAPACITY = 8192;
    static constexpr size_t MASK = CAPACITY - 1;
    
    // Ring buffer storage
    alignas(64) uint32_t tokens_[CAPACITY];
    alignas(64) float logits_[CAPACITY];
    
    // Indices
    alignas(64) std::atomic<size_t> head_{0};  // Producer writes here
    alignas(64) std::atomic<size_t> tail_{0};  // Consumer reads from here
    
    std::atomic<size_t> total_pushed_{0};
    std::atomic<size_t> total_popped_{0};
    std::atomic<size_t> dropped_{0};
    
public:
    // Push single token
    bool Push(uint32_t token, float logit) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) & MASK;
        
        // Check if full (leave 1 slot gap)
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        
        tokens_[current_head] = token;
        logits_[current_head] = logit;
        head_.store(next_head, std::memory_order_release);
        total_pushed_++;
        return true;
    }
    
    // Pop single token
    bool Pop(uint32_t& token, float& logit) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        // Check if empty
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        token = tokens_[current_tail];
        logit = logits_[current_tail];
        tail_.store((current_tail + 1) & MASK, std::memory_order_release);
        total_popped_++;
        return true;
    }
    
    // Batch push
    size_t PushBatch(const uint32_t* tokens, const float* logits, size_t count) {
        size_t pushed = 0;
        for (size_t i = 0; i < count; i++) {
            if (!Push(tokens[i], logits ? logits[i] : 0.0f)) {
                dropped_ += (count - i);
                break;
            }
            pushed++;
        }
        return pushed;
    }
    
    // Batch pop
    size_t PopBatch(uint32_t* tokens, float* logits, size_t max_count) {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; i++) {
            if (!Pop(tokens[i], logits[i])) {
                break;
            }
            popped++;
        }
        return popped;
    }
    
    size_t Size() const {
        return (head_.load(std::memory_order_acquire) - 
                tail_.load(std::memory_order_acquire)) & MASK;
    }
    
    bool Empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    size_t TotalPushed() const { return total_pushed_.load(); }
    size_t TotalPopped() const { return total_popped_.load(); }
    size_t Dropped() const { return dropped_.load(); }
};

using namespace std::chrono;

int main() {
    printf("========================================\n");
    printf("Fixed Token Pipeline TPS Test\n");
    printf("========================================\n\n");

    const size_t TOKEN_COUNT = 1'000'000;
    
    FixedTokenPipeline pipeline;
    
    // Generate tokens
    printf("Generating %zu synthetic tokens...\n", TOKEN_COUNT);
    std::vector<uint32_t> tokens(TOKEN_COUNT);
    std::vector<float> logits(TOKEN_COUNT);
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        tokens[i] = static_cast<uint32_t>(i % 32000);
        logits[i] = static_cast<float>(i) / 1000.0f;
    }
    printf("Done.\n\n");

    printf("Running TPS measurement...\n");
    printf("Pipeline capacity: 8192 tokens\n\n");

    auto start = high_resolution_clock::now();

    // Producer thread
    std::atomic<bool> producer_done{false};
    std::thread producer([&]() {
        size_t pushed = 0;
        while (pushed < TOKEN_COUNT) {
            size_t batch = std::min(size_t(64), TOKEN_COUNT - pushed);
            size_t sent = pipeline.PushBatch(&tokens[pushed], &logits[pushed], batch);
            if (sent == 0) {
                std::this_thread::yield();
                continue;
            }
            pushed += sent;
        }
        producer_done.store(true);
    });

    // Consumer
    std::vector<uint32_t> drained_tokens;
    std::vector<float> drained_logits;
    drained_tokens.reserve(TOKEN_COUNT);
    drained_logits.reserve(TOKEN_COUNT);
    
    uint32_t token;
    float logit;
    
    while (drained_tokens.size() < TOKEN_COUNT) {
        if (pipeline.Pop(token, logit)) {
            drained_tokens.push_back(token);
            drained_logits.push_back(logit);
        } else if (producer_done.load() && pipeline.Empty()) {
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
        if (drained_tokens[i] != tokens[i] || 
            std::abs(drained_logits[i] - logits[i]) > 0.001f) {
            valid = false;
            printf("Mismatch at %zu\n", i);
            break;
        }
    }
    
    if (valid) {
        printf("\n✅ Token integrity verified (first 1000 tokens)\n");
    }

    printf("\nPipeline stats:\n");
    printf("  Pushed:  %zu\n", pipeline.TotalPushed());
    printf("  Popped:  %zu\n", pipeline.TotalPopped());
    printf("  Dropped: %zu\n", pipeline.Dropped());

    printf("\n🚀 Fixed Token Pipeline: COMPLETE\n");
    printf("   Throughput: %.0f TPS\n\n", tps);
    
    return 0;
}
