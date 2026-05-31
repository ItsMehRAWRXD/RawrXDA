// d:/rawrxd/tests/fast_spec_bridge_smoke.cpp
//
// Smoke + microbench for FastSpecInferenceBridge.
//
// Validates:
//   1. The bridge sustains > 1M steps/sec when given a precomputed argmax
//      (matching the GPU sampler's output).
//   2. PrefillContext seeds the bigram table such that subsequent matching
//      sequences are accepted.
//   3. Greedy validation returns the target argmax even when no draft matches.
//
// Build (standalone):
//   cl /std:c++17 /O2 /EHsc /MD /I src ^
//      tests\fast_spec_bridge_smoke.cpp ^
//      src\ai\fast_spec.cpp src\ai\fast_spec_inference_bridge.cpp ^
//      /Fe:build\fastspec\fast_spec_bridge_smoke.exe

#include "ai/fast_spec_inference_bridge.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

double Now() noexcept {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

int TestPrefillRecallsBigram() {
    RawrXD::FastSpecInferenceBridge::Config cfg;
    cfg.vocab_size  = 4096;
    cfg.draft_width = 4;
    RawrXD::FastSpecInferenceBridge bridge(cfg);

    // Seed a known sequence A -> B -> C -> D and then drive the bridge with
    // (anchor=A, target=B); FastSpec should have learned the pair.
    std::vector<uint32_t> prompt = {7, 13, 21, 34};
    bridge.PrefillContext(prompt);

    auto step = bridge.GenerateTokenWithArgmax(7, 13);
    if (step.accepted_token != 13) {
        std::fprintf(stderr, "prefill: expected token 13, got %u\n", step.accepted_token);
        return 1;
    }
    if (step.accepted_count != 1) {
        std::fprintf(stderr, "prefill: expected 1 accepted draft, got %u (drafts=%u)\n",
                     step.accepted_count, step.draft_count);
        return 1;
    }
    return 0;
}

int TestStepThroughput(uint64_t iters) {
    RawrXD::FastSpecInferenceBridge::Config cfg;
    cfg.vocab_size  = 32000;
    cfg.draft_width = 4;
    RawrXD::FastSpecInferenceBridge bridge(cfg);

    // Warm with a synthetic Markov chain so drafts have something to hit.
    std::vector<uint32_t> warm;
    warm.reserve(50000);
    uint64_t s = 0xDEADBEEFCAFEBABEULL;
    for (size_t i = 0; i < 50000; ++i) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        warm.push_back(static_cast<uint32_t>(s % cfg.vocab_size));
    }
    bridge.PrefillContext(warm);

    uint32_t last = warm.back();
    const double t0 = Now();
    for (uint64_t i = 0; i < iters; ++i) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        const uint32_t target = static_cast<uint32_t>(s % cfg.vocab_size);
        auto step = bridge.GenerateTokenWithArgmax(last, target);
        last = step.accepted_token;
    }
    const double dt = Now() - t0;
    const double tps = iters / dt;

    auto stats = bridge.GetStats();
    std::printf("{\"iters\":%llu,\"elapsed_s\":%.6f,\"step_tps\":%.1f,"
                "\"draft_emit\":%llu,\"draft_accepted\":%llu,"
                "\"acceptance_rate\":%.4f,\"effective_speedup\":%.4f}\n",
                (unsigned long long)iters, dt, tps,
                (unsigned long long)stats.draft_emit,
                (unsigned long long)stats.draft_accepted,
                stats.acceptance_rate,
                stats.effective_speedup);

    if (tps < 1'000'000.0) {
        std::fprintf(stderr, "throughput FAIL: step_tps %.1f below 1M floor\n", tps);
        return 2;
    }
    return 0;
}

// Validates probabilistic rejection sampling: with a sharply peaked target
// distribution (one logit dominant), the bridge must commit the dominant
// token at near-100% rate even when the draft proposes other things.
int TestSampledRejectionRespectsTarget() {
    RawrXD::FastSpecInferenceBridge::Config cfg;
    cfg.vocab_size  = 256;
    cfg.draft_width = 4;
    RawrXD::FastSpecInferenceBridge bridge(cfg);

    std::vector<float> logits(cfg.vocab_size, -10.0f);
    const uint32_t kHotToken = 42;
    logits[kHotToken] = 20.0f;  // softmax ≈ 1.0 on token 42

    uint64_t rng = 0xA5A5A5A5DEADBEEFULL;
    uint64_t hits = 0;
    const uint64_t N = 20000;
    for (uint64_t i = 0; i < N; ++i) {
        auto step = bridge.GenerateTokenSampled(7, logits, &rng);
        if (step.accepted_token == kHotToken) ++hits;
    }
    const double rate = static_cast<double>(hits) / static_cast<double>(N);
    std::printf("{\"sampled_hot_token_rate\":%.4f,\"N\":%llu}\n",
                rate, (unsigned long long)N);
    if (rate < 0.98) {
        std::fprintf(stderr, "sampled FAIL: hot-token rate %.4f < 0.98\n", rate);
        return 3;
    }
    return 0;
}

// Validates that a non-degenerate distribution actually produces variety:
// uniform logits should yield many distinct committed tokens.
int TestSampledUniformProducesVariety() {
    RawrXD::FastSpecInferenceBridge::Config cfg;
    cfg.vocab_size  = 256;
    cfg.draft_width = 4;
    RawrXD::FastSpecInferenceBridge bridge(cfg);

    std::vector<float> logits(cfg.vocab_size, 0.0f);  // uniform softmax
    uint64_t rng = 0x123456789ABCDEFULL;

    std::vector<uint8_t> seen(cfg.vocab_size, 0);
    const uint64_t N = 5000;
    for (uint64_t i = 0; i < N; ++i) {
        auto step = bridge.GenerateTokenSampled(13, logits, &rng);
        if (step.accepted_token < cfg.vocab_size) seen[step.accepted_token] = 1;
    }
    uint32_t distinct = 0;
    for (uint8_t b : seen) distinct += b;
    std::printf("{\"sampled_uniform_distinct_tokens\":%u,\"vocab\":%u}\n",
                distinct, cfg.vocab_size);
    if (distinct < cfg.vocab_size / 2) {
        std::fprintf(stderr, "sampled FAIL: only %u/%u distinct tokens on uniform dist\n",
                     distinct, cfg.vocab_size);
        return 4;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    uint64_t iters = 1'000'000;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--iter" && i + 1 < argc) {
            iters = std::strtoull(argv[++i], nullptr, 10);
        }
    }

    int rc = TestPrefillRecallsBigram();
    if (rc) return rc;

    rc = TestStepThroughput(iters);
    if (rc) return rc;

    rc = TestSampledRejectionRespectsTarget();
    if (rc) return rc;

    rc = TestSampledUniformProducesVariety();
    if (rc) return rc;

    std::printf("[FastSpecBridge] OK\n");
    return 0;
}
