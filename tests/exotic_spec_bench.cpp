// tests/exotic_spec_bench.cpp
// Build:
//   cl.exe /std:c++17 /O2 /arch:AVX2 /EHsc /MD /nologo /I src
//          tests\exotic_spec_bench.cpp src\ai\exotic_spec_decoder.cpp
//          /Fo:build\exotic\ /Fe:build\exotic\hydra_bench.exe
// Gate: steps/sec > 1 000 000
#include "../src/ai/exotic_spec_decoder.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>

int main() {
    rxd::ai::ExoticSpecConfig cfg;
    cfg.vocab_size      = 32000;
    cfg.max_draft_width = 32;
    cfg.mlp_hidden      = 64;
    cfg.mlp_embed       = 128;
    cfg.slim_vocab      = 512;
    rxd::ai::ExoticSpecDecoder dec(cfg);

    // ---------- warm up n-gram & oracle with a repetitive code pattern ----------
    uint32_t pat[20] = {100,200,300,400,500, 100,200,300,400,500,
                        100,200,300,400,600, 100,200,300,400,500};
    for (int ep = 0; ep < 1000; ++ep) dec.FeedAccepted(pat, 20);

    // ---------- rolling history ----------
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, cfg.vocab_size - 1);
    uint32_t history[256];
    for (auto& h : history) h = dist(rng);

    // ---------- bench loop ----------
    const uint32_t kSteps = 500'000;
    uint32_t draft[64], target[64];
    uint64_t total_acc = 0, total_draft = 0;

    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint32_t step = 0; step < kSteps; ++step) {
        uint32_t n = dec.Draft(history, 256, draft, 32);

        // synthetic target: position 0 matches 70% of the time, decays
        for (uint32_t i = 0; i < n; ++i) {
            bool match = (rng() % 100) < (70u >> i);
            target[i] = match ? draft[i] : (draft[i] + 1 + rng() % 10) % cfg.vocab_size;
        }

        uint32_t acc = dec.ValidateArgmax(draft, n, target);
        total_acc   += acc;
        total_draft += n;

        // slide history
        std::memmove(history, history + 1, 255 * sizeof(uint32_t));
        history[255] = (acc > 0) ? draft[0] : target[0];

        // online train on accepted prefix
        if (acc > 1) dec.FeedAccepted(draft, acc);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double secs   = std::chrono::duration<double>(t1 - t0).count();
    double sps    = kSteps / secs;
    double tps    = total_draft / secs;
    double ar     = double(total_acc) / double(total_draft > 0 ? total_draft : 1);

    auto st = dec.GetStats();

    std::printf("=== Hydra Speculative Decoder ===\n");
    std::printf("Steps:        %u\n",     kSteps);
    std::printf("Time:         %.3f s\n", secs);
    std::printf("Steps/sec:    %.1f\n",   sps);
    std::printf("Tokens/sec:   %.1f\n",   tps);
    std::printf("Accept rate:  %.2f%%\n", ar * 100.0);
    std::printf("EWMA accept:  %.3f\n",   st.acceptance_ewma);
    std::printf("Width:        %u\n",     st.current_width);
    std::printf("4-gram hits:  %llu\n",   (unsigned long long)st.ngram4_hits);
    std::printf("3-gram hits:  %llu\n",   (unsigned long long)st.ngram3_hits);
    std::printf("2-gram hits:  %llu\n",   (unsigned long long)st.ngram2_hits);
    std::printf("Oracle hits:  %llu\n",   (unsigned long long)st.oracle_hits);
    std::printf("Copy hits:    %llu\n",   (unsigned long long)st.copy_head_hits);
    std::printf("Cascade recs: %llu\n",   (unsigned long long)st.cascade_recoveries);
    std::printf("Oracle train: %u\n",     st.oracle_train_steps);

    return (sps >= 1'000'000.0) ? 0 : 1;
}
