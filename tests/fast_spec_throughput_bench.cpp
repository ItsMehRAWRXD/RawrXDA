// d:/rawrxd/tests/fast_spec_throughput_bench.cpp
//
// Standalone microbench for FastSpec. Validates the two design rules:
//   1. The hot loop sustains 10K+ TPS on a single core with no model
//      pressure (target: 10K–50K+ depending on hardware).
//   2. Validation is off-path: the producer never blocks on training,
//      and dropped pushes degrade gracefully.
//
// Build (standalone, no other RawrXD deps required):
//   cl /std:c++17 /O2 /EHsc /I src tests\fast_spec_throughput_bench.cpp ^
//      src\ai\fast_spec.cpp /Fe:fast_spec_bench.exe
//
// The bench is deterministic — the seeded LCG drives anchor selection and
// "actual" tokens so wall-time is the only variable.

#include "ai/fast_spec.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

constexpr uint32_t kVocab = 32000;
constexpr uint32_t kTopK  = 4;

// Tiny xorshift64 — enough entropy for an anchor/actual stream and avoids
// pulling in <random> overhead inside the hot loop.
struct Xs64 {
    uint64_t s;
    explicit Xs64(uint64_t seed) : s(seed ? seed : 0xDEADBEEFCAFEBABEULL) {}
    uint64_t next() noexcept {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    uint32_t next_token(uint32_t vocab) noexcept {
        return static_cast<uint32_t>(next() % vocab);
    }
};

double NowSeconds() noexcept {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

int Run(uint64_t iterations, uint32_t draft_width, bool quiet) {
    RawrXD::FastSpec::Config cfg;
    cfg.vocab_size  = kVocab;
    cfg.top_k       = kTopK;
    cfg.ring_log2   = 16;          // 65536-slot ring
    cfg.start_drain = true;
    RawrXD::FastSpec spec(cfg);

    if (draft_width == 0) draft_width = kTopK;
    if (draft_width > kTopK) draft_width = kTopK;

    // Pre-seed the bigram table by enqueuing some validations for the
    // first slice, so the hot loop has live entries to read. This is
    // realistic: production callers run validation continuously alongside
    // speculation.
    {
        Xs64 seed(0xC0FFEE);
        const uint64_t warm = 200000;
        for (uint64_t i = 0; i < warm; ++i) {
            uint32_t anchor    = seed.next_token(kVocab);
            uint32_t actual    = seed.next_token(kVocab);
            uint32_t predicted = (seed.next() & 1) ? actual : seed.next_token(kVocab);
            // EnqueueValidation may drop under heavy contention; that's OK.
            spec.EnqueueValidation(anchor, predicted, actual);
        }
        // Force training to apply before timing starts.
        for (int i = 0; i < 64; ++i) {
            if (spec.DrainNow() == 0) std::this_thread::yield();
        }
    }

    uint32_t draft[16];  // > kTopK headroom
    Xs64 rng(0xA5A5A5A5ULL);

    const double t0 = NowSeconds();
    uint64_t emitted = 0;
    for (uint64_t i = 0; i < iterations; ++i) {
        const uint32_t anchor = rng.next_token(kVocab);
        emitted += spec.SpeculateFast(anchor, draft, draft_width);

        // Off-path validation push — deliberately happens AFTER the hot
        // candidate read so the timed cost reflects what a real GPU
        // verifier would do on its own thread. We still call it on the
        // same thread to ensure the SPSC contract holds and to measure the
        // realistic combined cost.
        const uint32_t actual    = rng.next_token(kVocab);
        const uint32_t predicted = (draft[0] != RawrXD::FastSpec::kNoToken)
            ? draft[0] : actual;
        spec.EnqueueValidation(anchor, predicted, actual);
    }
    const double t1 = NowSeconds();
    const double elapsed = t1 - t0;
    const double tps = (elapsed > 0.0) ? (iterations / elapsed) : 0.0;

    // Let the drain thread catch up so accuracy stats settle.
    for (int i = 0; i < 64; ++i) {
        if (spec.DrainNow() == 0) std::this_thread::yield();
    }
    const auto stats = spec.GetStats();

    std::printf("{");
    std::printf("\"iterations\":%llu,",          (unsigned long long)iterations);
    std::printf("\"draft_width\":%u,",           draft_width);
    std::printf("\"elapsed_s\":%.6f,",           elapsed);
    std::printf("\"speculative_tps\":%.1f,",     tps);
    std::printf("\"hot_candidates_emit\":%llu,", (unsigned long long)stats.hot_candidates_emit);
    std::printf("\"validations_pushed\":%llu,",  (unsigned long long)stats.validations_pushed);
    std::printf("\"validations_dropped\":%llu,", (unsigned long long)stats.validations_dropped);
    std::printf("\"validations_drained\":%llu,", (unsigned long long)stats.validations_drained);
    std::printf("\"correct_predictions\":%llu,", (unsigned long long)stats.correct_predictions);
    std::printf("\"accuracy_rate\":%.4f",        stats.accuracy_rate);
    std::printf("}\n");

    if (!quiet) {
        std::printf("[FastSpec] %.0f spec-TPS (%.2fM emit/s) over %.2fs; "
                    "drained=%llu pushed=%llu dropped=%llu acc=%.3f\n",
                    tps,
                    static_cast<double>(stats.hot_candidates_emit) / 1.0e6 / elapsed,
                    elapsed,
                    (unsigned long long)stats.validations_drained,
                    (unsigned long long)stats.validations_pushed,
                    (unsigned long long)stats.validations_dropped,
                    stats.accuracy_rate);
    }

    // Gate: we expect at least 10K spec-TPS on any reasonable host.
    if (tps < 10000.0) {
        std::fprintf(stderr,
                     "[FastSpec] FAIL: speculative TPS %.1f below 10K floor\n",
                     tps);
        return 2;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    uint64_t iterations  = 1'000'000;
    uint32_t draft_width = kTopK;
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--iter" && i + 1 < argc) {
            iterations = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--width" && i + 1 < argc) {
            draft_width = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--help" || a == "-h") {
            std::printf("Usage: %s [--iter N] [--width K] [--quiet]\n", argv[0]);
            return 0;
        }
    }
    return Run(iterations, draft_width, quiet);
}
