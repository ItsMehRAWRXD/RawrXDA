// Minimal TPS Measurement Harness for RawrXD Token Pipeline
// Produces first real throughput number

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "inference/token_pipeline.h"

using namespace RawrXD::Inference;
using namespace std::chrono;

int main() {
    printf("========================================\n");
    printf("RawrXD Token Pipeline TPS Measurement\n");
    printf("========================================\n\n");

    // Configuration
    const size_t TOKEN_COUNT = 1'000'000;
    const size_t BATCH_SIZE = 64;
    
    // Generate synthetic tokens (avoids tokenizer complexity)
    printf("Generating %zu synthetic tokens...\n", TOKEN_COUNT);
    std::vector<uint32_t> tokens(TOKEN_COUNT);
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        tokens[i] = static_cast<uint32_t>(i % 32000);
    }
    printf("Done.\n\n");

    // Initialize pipeline
    printf("Initializing TokenPipeline...\n");
    TokenPipeline pipeline;
    if (!pipeline.Initialize(4096, 1)) {
        printf("FAILED: Pipeline initialization failed\n");
        return 1;
    }
    printf("Pipeline initialized.\n\n");

    // Measure TPS
    printf("Running TPS measurement...\n");
    printf("Pushing %zu tokens in batches of %zu...\n\n", TOKEN_COUNT, BATCH_SIZE);

    auto start = high_resolution_clock::now();

    // Producer thread: Push tokens
    std::atomic<size_t> pushed{0};
    std::atomic<bool> producer_done{false};
    
    std::thread producer([&]() {
        size_t local_pushed = 0;
        while (local_pushed < TOKEN_COUNT) {
            size_t batch = (local_pushed + BATCH_SIZE <= TOKEN_COUNT) ? BATCH_SIZE : (TOKEN_COUNT - local_pushed);
            
            // Create batch
            std::vector<uint32_t> batch_tokens(batch);
            std::vector<float> batch_logits(batch);
            for (size_t i = 0; i < batch; ++i) {
                batch_tokens[i] = tokens[local_pushed + i];
                batch_logits[i] = 0.0f;
            }
            
            // Submit to pipeline
            if (!pipeline.SubmitTokens(batch_tokens.data(), batch_logits.data(), batch, 1)) {
                // Queue full, retry
                std::this_thread::yield();
                continue;
            }
            
            local_pushed += batch;
            pushed.store(local_pushed);
        }
        producer_done.store(true);
    });

    // Consumer: Drain the pipeline
    std::vector<uint32_t> drained_tokens(TOKEN_COUNT);
    std::vector<float> drained_logits(TOKEN_COUNT);
    uint64_t seq_id;
    size_t retrieved = 0;
    
    while (retrieved < TOKEN_COUNT || !producer_done.load()) {
        size_t to_retrieve = (retrieved + BATCH_SIZE <= TOKEN_COUNT) ? BATCH_SIZE : (TOKEN_COUNT - retrieved);
        size_t got = pipeline.RetrieveTokens(&drained_tokens[retrieved], &drained_logits[retrieved], to_retrieve, seq_id);
        if (got == 0) {
            // Empty, check if producer is done
            if (producer_done.load() && pipeline.GetStats().pending_tokens == 0) {
                break;
            }
            std::this_thread::yield();
        }
        retrieved += got;
    }

    producer.join();

    auto end = high_resolution_clock::now();

    // Calculate metrics
    double seconds = duration<double>(end - start).count();
    double tps = TOKEN_COUNT / seconds;
    double latency_us = (seconds * 1'000'000.0) / TOKEN_COUNT;

    printf("\n========================================\n");
    printf("RESULTS\n");
    printf("========================================\n");
    printf("Tokens processed: %zu\n", TOKEN_COUNT);
    printf("Time elapsed:     %.3f seconds\n", seconds);
    printf("Throughput:       %.0f tokens/sec\n", tps);
    printf("Latency:          %.3f microseconds/token\n", latency_us);
    printf("========================================\n");

    // Validate
    bool valid = true;
    for (size_t i = 0; i < TOKEN_COUNT && i < 1000; ++i) {
        if (drained_tokens[i] != tokens[i]) {
            valid = false;
            break;
        }
    }
    
    if (valid) {
        printf("\n✅ Token integrity verified (first 1000 tokens match)\n");
    } else {
        printf("\n⚠️  Token mismatch detected\n");
    }

    // Pipeline stats
    printf("\nPipeline Statistics:\n");
    auto stats = pipeline.GetStats();
    printf("  Total submitted:  %zu\n", pushed.load());
    printf("  Total retrieved:  %zu\n", retrieved);
    printf("  Dropped tokens:   %zu\n", stats.dropped_tokens);

    printf("\n🚀 RawrXD Token Pipeline: MEASURABLE\n");
    
    return 0;
}
