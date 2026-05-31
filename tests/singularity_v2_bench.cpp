#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <random>
#include "../src/ai/singularity_spec_decoder_v2.h"

int main() {
    rxd::ai::SingularityV2Config cfg;
    cfg.vocab_size = 32000;
    cfg.max_draft_width = 128;
    cfg.mlp_hidden = 512;
    cfg.mlp_embed = 512;
    cfg.slim_vocab = 2048;
    cfg.octo_gram_bits = 16;      // 64k buckets x 128B = 8MB per table
    cfg.sedecim_gram_bits = 14;  // 16k buckets x 128B = 2MB per table pair
    cfg.minhash_slots = 1u << 15; // 32k slots
    cfg.kv_cache_rows = 4096;
    rxd::ai::SingularitySpecDecoderV2 dec(cfg);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, cfg.vocab_size - 1);
    uint32_t history[256];
    for (int i = 0; i < 256; ++i) history[i] = dist(rng);

    // Train on deeply repetitive code-like pattern
    uint32_t pattern[64] = {
        100,200,300,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500,1600,
        100,200,300,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500,1600,
        100,200,300,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500,1700,
        100,200,300,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500,1600
    };
    for (int epoch = 0; epoch < 2000; ++epoch) dec.FeedAccepted(pattern, 64);

    const uint32_t kSteps = 500000;
    uint32_t draft[128], target[128];
    uint64_t total_acc = 0, total_draft = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (uint32_t step = 0; step < kSteps; ++step) {
        uint32_t n = dec.Draft(history, 256, draft, 64);
        for (uint32_t i = 0; i < n; ++i) {
            bool match = (rng() % 100) < (80u >> (i >> 1));
            target[i] = match ? draft[i] : (draft[i] + 1 + (rng() % 6)) % cfg.vocab_size;
        }
        uint32_t acc = dec.ValidateArgmax(draft, n, target);
        total_acc += acc;
        total_draft += n;
        std::memmove(history, history + 1, 255 * sizeof(uint32_t));
        history[255] = (acc > 0) ? draft[0] : target[0];
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(t1 - t0).count();
    double sps = kSteps / secs;
    double tps = total_draft / secs;
    double ar = (total_draft > 0) ? total_acc / (double)total_draft : 0.0;
    auto st = dec.GetStats();

    printf("=== SINGULARITY V2 SPEC DECODER ===\n");
    printf("Steps/sec:  %.1f\n", sps);
    printf("Tokens/sec: %.1f\n", tps);
    printf("Accepted:   %llu / %llu (%.2f%%)\n",
           (unsigned long long)total_acc, (unsigned long long)total_draft, ar * 100.0);
    printf("EWMA:       %.3f | Width: %u\n", st.acceptance_ewma, st.current_width);
    printf("Cascade: %llu | Octo: %llu | Sedecim: %llu | MinHash: %llu | KV: %llu | Restless: %llu\n",
           (unsigned long long)st.cascade_recoveries,
           (unsigned long long)st.octo_gram_hits,
           (unsigned long long)st.sedecim_gram_hits,
           (unsigned long long)st.minhash_hits,
           (unsigned long long)st.kv_cache_hits,
           (unsigned long long)st.restless_prefetch_hits);
    printf("Head weights:");
    for (int i = 0; i < 12; ++i) printf("  h[%d]=%.3f", i, st.head_bandit_weights[i]);
    printf("\n");
    return (sps > 1e6) ? 0 : 1;
}
