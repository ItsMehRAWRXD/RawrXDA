#include "kernels/query_fusion_kernel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <immintrin.h>
#include <intrin.h>
#endif

namespace {

using Clock = std::chrono::high_resolution_clock;

struct BenchStats {
    double elapsed_ms = 0.0;
    double qps = 0.0;
    double mcandidates_per_sec = 0.0;
};

struct Config {
    uint32_t candidate_count = 4096;
    uint32_t iterations = 20000;
    uint32_t warmup = 200;
    float penalty_per_missing = 0.15f;
    float clamp_max = 0.50f;
    uint32_t max_missing_terms = 3;
};

bool has_avx512f() {
#if defined(_MSC_VER) && defined(_M_X64)
    int cpu_info[4] = {};
    __cpuidex(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }

    const unsigned long long xcr0 = _xgetbv(0);
    const bool os_supports_zmm = (xcr0 & 0xE6) == 0xE6;
    if (!os_supports_zmm) {
        return false;
    }

    __cpuidex(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 16)) != 0;
#else
    return false;
#endif
}

uint32_t parse_u32_arg(const char* value, uint32_t fallback) {
    if (!value || !*value) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (!end || *end != '\0') {
        return fallback;
    }
    return static_cast<uint32_t>(parsed);
}

void parse_args(int argc, char** argv, Config& config) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--candidates" || arg == "-c") && i + 1 < argc) {
            config.candidate_count = parse_u32_arg(argv[++i], config.candidate_count);
        } else if ((arg == "--iterations" || arg == "-n") && i + 1 < argc) {
            config.iterations = parse_u32_arg(argv[++i], config.iterations);
        } else if (arg == "--warmup" && i + 1 < argc) {
            config.warmup = parse_u32_arg(argv[++i], config.warmup);
        }
    }
}

void run_scalar_reference(
    const float* row0,
    const float* row1,
    std::span<const uint32_t> missing_counts,
    std::span<const uint32_t> phrase_gate_failures,
    float penalty_per_missing,
    float clamp_max,
    std::span<float> out_scores) {
    const size_t n = out_scores.size();
    for (size_t i = 0; i < n; ++i) {
        float score = (row0[i] * 0.75f) + (row1[i] * 0.25f);
        score = std::max(0.0f, score - (static_cast<float>(missing_counts[i]) * penalty_per_missing));
        if (phrase_gate_failures[i] != 0) {
            score = std::min(score, clamp_max);
        }
        out_scores[i] = score;
    }
}

template <typename Fn>
BenchStats measure(uint32_t iterations, uint32_t candidate_count, Fn&& fn) {
    const auto start = Clock::now();
    for (uint32_t i = 0; i < iterations; ++i) {
        fn();
    }
    const auto end = Clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double elapsed_s = elapsed_ms / 1000.0;
    BenchStats stats;
    stats.elapsed_ms = elapsed_ms;
    stats.qps = elapsed_s > 0.0 ? (static_cast<double>(iterations) / elapsed_s) : 0.0;
    stats.mcandidates_per_sec = elapsed_s > 0.0
        ? ((static_cast<double>(iterations) * static_cast<double>(candidate_count)) / 1000000.0) / elapsed_s
        : 0.0;
    return stats;
}

double max_abs_diff(std::span<const float> a, std::span<const float> b) {
    const size_t n = std::min(a.size(), b.size());
    double diff = 0.0;
    for (size_t i = 0; i < n; ++i) {
        diff = std::max(diff, static_cast<double>(std::fabs(a[i] - b[i])));
    }
    return diff;
}

void print_stats(const char* label, const BenchStats& stats) {
    std::cout << std::left << std::setw(16) << label
              << " elapsed_ms=" << std::setw(10) << std::fixed << std::setprecision(3) << stats.elapsed_ms
              << " qps=" << std::setw(12) << std::fixed << std::setprecision(2) << stats.qps
              << " Mcand/s=" << std::setw(12) << std::fixed << std::setprecision(2) << stats.mcandidates_per_sec
              << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    Config config;
    parse_args(argc, argv, config);

    std::cout << "============================================================\n";
    std::cout << "  RawrXD Query Fusion Microbenchmark\n";
    std::cout << "  Path: ew75_p15 weighted fusion + numeric phrase-gate clamp\n";
    std::cout << "============================================================\n\n";

    std::cout << "Candidates      : " << config.candidate_count << "\n";
    std::cout << "Iterations      : " << config.iterations << "\n";
    std::cout << "Warmup          : " << config.warmup << "\n";
    std::cout << "Penalty         : " << config.penalty_per_missing << "\n";
    std::cout << "Clamp Max       : " << config.clamp_max << "\n";
    std::cout << "AVX-512F        : " << (has_avx512f() ? "yes" : "no") << "\n\n";

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> score_dist(0.70f, 1.00f);
    std::uniform_int_distribution<uint32_t> missing_dist(0, config.max_missing_terms);
    std::bernoulli_distribution gate_dist(0.15);

    std::vector<float> score_matrix(static_cast<size_t>(config.candidate_count) * 2u, 0.0f);
    std::vector<float> weights{0.75f, 0.25f};
    std::vector<uint32_t> missing_counts(config.candidate_count, 0);
    std::vector<uint32_t> phrase_gate_failures(config.candidate_count, 0);
    std::vector<float> scalar_out(config.candidate_count, 0.0f);
    std::vector<float> native_out(config.candidate_count, 0.0f);

    float* row0 = score_matrix.data();
    float* row1 = score_matrix.data() + config.candidate_count;
    for (uint32_t i = 0; i < config.candidate_count; ++i) {
        row0[i] = score_dist(rng);
        row1[i] = score_dist(rng);
        missing_counts[i] = missing_dist(rng);
        phrase_gate_failures[i] = gate_dist(rng) ? 1u : 0u;
    }

    for (uint32_t i = 0; i < config.warmup; ++i) {
        run_scalar_reference(
            row0,
            row1,
            std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
            std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
            config.penalty_per_missing,
            config.clamp_max,
            std::span<float>(scalar_out.data(), scalar_out.size()));
    }

    const BenchStats scalar_stats = measure(config.iterations, config.candidate_count, [&]() {
        run_scalar_reference(
            row0,
            row1,
            std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
            std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
            config.penalty_per_missing,
            config.clamp_max,
            std::span<float>(scalar_out.data(), scalar_out.size()));
    });

    print_stats("scalar", scalar_stats);

    if (!has_avx512f()) {
        std::cout << "\nNative AVX-512 path skipped on this CPU.\n";
        return 0;
    }

    for (uint32_t i = 0; i < config.warmup; ++i) {
        std::vector<float> fused;
        if (!RawrXD::Fusion::WeightedFusion(
                score_matrix.data(),
                weights.data(),
                2,
                config.candidate_count,
                fused)) {
            std::cerr << "Warmup WeightedFusion failed\n";
            return 1;
        }
        if (!RawrXD::Fusion::ApplyPenaltyAndPhraseGate(
                std::span<float>(fused.data(), fused.size()),
                std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
                std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
                config.penalty_per_missing,
                config.clamp_max)) {
            std::cerr << "Warmup ApplyPenaltyAndPhraseGate failed\n";
            return 1;
        }
    }

    const BenchStats native_stats = measure(config.iterations, config.candidate_count, [&]() {
        if (!RawrXD::Fusion::WeightedFusion(
                score_matrix.data(),
                weights.data(),
                2,
                config.candidate_count,
                native_out)) {
            std::cerr << "WeightedFusion failed\n";
            std::abort();
        }
        if (!RawrXD::Fusion::ApplyPenaltyAndPhraseGate(
                std::span<float>(native_out.data(), native_out.size()),
                std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
                std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
                config.penalty_per_missing,
                config.clamp_max)) {
            std::cerr << "ApplyPenaltyAndPhraseGate failed\n";
            std::abort();
        }
    });

    run_scalar_reference(
        row0,
        row1,
        std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
        std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
        config.penalty_per_missing,
        config.clamp_max,
        std::span<float>(scalar_out.data(), scalar_out.size()));
    if (!RawrXD::Fusion::WeightedFusion(score_matrix.data(), weights.data(), 2, config.candidate_count, native_out) ||
        !RawrXD::Fusion::ApplyPenaltyAndPhraseGate(
            std::span<float>(native_out.data(), native_out.size()),
            std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
            std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
            config.penalty_per_missing,
            config.clamp_max)) {
        std::cerr << "Final verification run failed\n";
        return 1;
    }

    const double diff = max_abs_diff(
        std::span<const float>(scalar_out.data(), scalar_out.size()),
        std::span<const float>(native_out.data(), native_out.size()));

    print_stats("native_avx512", native_stats);
    std::cout << "\nMax abs diff    : " << std::fixed << std::setprecision(8) << diff << "\n";

    if (diff > 1.0e-5) {
        std::cerr << "Verification failed: native path diverges from scalar reference\n";
        return 2;
    }

    const double speedup = native_stats.elapsed_ms > 0.0 ? (scalar_stats.elapsed_ms / native_stats.elapsed_ms) : 0.0;
    std::cout << "Speedup         : " << std::fixed << std::setprecision(2) << speedup << "x\n";
    std::cout << "\nStatus          : PASS\n";
    return 0;
}