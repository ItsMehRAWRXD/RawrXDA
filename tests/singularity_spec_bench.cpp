#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <random>
#include "../src/ai/singularity_spec_decoder.h"

int main() {
    rxd::ai::SingularityConfig cfg;
    cfg.vocab_size      = 32000;
    cfg.max_draft_width = 64;
    rxd::ai::SingularitySpecDecoder dec(cfg);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, cfg.vocab_size - 1);
    uint32_t history[256];
    for (int i = 0; i < 256; ++i) history[i] = dist(rng);

    // Repetitive structured pattern to seed caches
    uint32_t pattern[32] = {
        100,200,300,400,500,600,700,800,
        100,200,300,400,500,600,700,800,
        100,200,300,400,500,600,700,900,
        100,200,300,400,500,600,700,800
    };
    for (int e = 0; e < 1000; ++e) dec.FeedAccepted(pattern, 32);

    const uint32_t kSteps = 500000;
    uint32_t draft[64], target[64];
    uint64_t total_acc = 0, total_drift = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (uint32_t step = 0; step < kSteps; ++step) {
        uint32_t n = dec.Draft(history, 256, draft, 32);
        if (!n) { n = 1; draft[0] = dist(rng); }   // safety: always validate >=1

        for (uint32_t i = 0; i < n; ++i) {
            bool match = (rng() % 100) < (75u >> i);
            target[i] = match ? draft[i] : (draft[i] + 1 + (rng() % 8)) % cfg.vocab_size;
        }
        uint32_t acc = dec.ValidateArgmax(draft, n, target);
        total_acc   += acc;
        total_drift += n;
        std::memmove(history, history + 1, 255 * sizeof(uint32_t));
        history[255] = (acc > 0) ? draft[0] : target[0];
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(t1 - t0).count();
    double sps  = kSteps  / secs;
    double tps  = total_drift / secs;
    double ar   = total_acc / (double)std::max(1ULL, total_drift);
    auto   st   = dec.GetStats();

    printf("=== SINGULARITY SPEC DECODER ===\n");
    printf("Steps/sec:    %.1f\n", sps);
    printf("Tokens/sec:   %.1f  (draft tokens generated)\n", tps);
    printf("Accepted:     %llu / %llu  (%.2f%%)\n",
           (unsigned long long)total_acc,
           (unsigned long long)total_drift,
           ar * 100.0);
    printf("EWMA acc:     %.3f  |  Width: %u\n", st.acceptance_ewma, st.current_width);
    printf("Cascade rec:  %llu\n", (unsigned long long)st.cascade_recoveries);
    printf("Octo hits:    %llu\n", (unsigned long long)st.octo_gram_hits);
    printf("MinHash hits: %llu\n", (unsigned long long)st.minhash_hits);
    printf("Head weights: ");
    for (int i = 0; i < 8; ++i) printf("[%d]=%.3f ", i, st.head_bandit_weights[i]);
    printf("\n");
    return (sps > 1e6) ? 0 : 1;
}
