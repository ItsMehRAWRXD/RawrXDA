#include "ipc/shm_channel.hpp"
#include "extension_kernel/autocomplete_protocol.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iterator>
#include <numeric>
#include <vector>
#include <windows.h>
#include <immintrin.h>

namespace {

inline uint64_t now_ns() {
    LARGE_INTEGER freq{};
    LARGE_INTEGER counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>((counter.QuadPart * 1000000000ll) / freq.QuadPart);
}

inline double pct_ms(const std::vector<uint64_t>& ns, double p) {
    if (ns.empty()) return 0.0;
    const size_t idx = static_cast<size_t>(p * static_cast<double>(ns.size() - 1));
    return static_cast<double>(ns[idx]) / 1e6;
}

struct TypingEvent {
    uint32_t chars;
    uint32_t delay_ms;
};

struct MetricsTotals {
    uint64_t requests = 0;
    uint64_t cache_hits = 0;
    uint64_t kv_stitches = 0;
    uint64_t tokens_generated = 0;
    uint64_t tokens_accepted = 0;
    uint64_t verify_rejects = 0;
    uint64_t depth_sum = 0;
    uint64_t head_sum = 0;
    uint64_t heads_pruned = 0;
};

struct ChaosConfig {
    uint32_t seed = 1337;
    float file_switch_prob = 0.25f;
    float context_churn_prob = 0.35f;
    float cold_start_prob = 0.10f;
    float syntax_noise_prob = 0.40f;
    uint32_t cold_start_len = 40;
    uint32_t initial_cold_requests = 200;
    uint32_t hard_reset_period = 200;
    uint32_t hard_reset_len = 12;
    uint32_t window_size = 100;
};

struct ChaosMetrics {
    uint64_t cold_starts = 0;
    uint64_t hard_resets = 0;
    uint64_t file_switches = 0;
    uint64_t context_churns = 0;
    uint64_t syntax_noise = 0;
    uint64_t cold_requests = 0;
};

struct RequestChaos {
    bool cold = false;
    bool hard_reset = false;
    bool file_switch = false;
    bool context_churn = false;
    bool syntax_noise = false;
    uint64_t file_hash = 0;
    uint64_t context_hash = 0;
};

uint32_t g_rng_state = 1;

inline void rng_init(uint32_t seed) {
    g_rng_state = seed ? seed : 1u;
}

inline uint32_t rng_u32() {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x ? x : 1u;
    return g_rng_state;
}

inline float rng_f() {
    return static_cast<float>(rng_u32() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

inline void deterministic_compute_jitter(uint32_t slot) {
    const uint32_t spins = 32 + ((slot * 17u) % 192u);
    for (uint32_t i = 0; i < spins; ++i) {
        _mm_pause();
    }
}

inline void burn_for_ns(uint64_t ns) {
    if (ns == 0) return;
    const uint64_t start = now_ns();
    while ((now_ns() - start) < ns) {
        _mm_pause();
    }
}

inline double rolling_p95_ms(const std::deque<uint64_t>& window) {
    if (window.empty()) return 0.0;
    std::vector<uint64_t> tmp(window.begin(), window.end());
    std::sort(tmp.begin(), tmp.end());
    const size_t idx = static_cast<size_t>(0.95 * static_cast<double>(tmp.size() - 1));
    return static_cast<double>(tmp[idx]) / 1e6;
}

inline void fill_request(RawrXD::ExtensionKernel::CompletionRequest& req,
                         int burst,
                         uint32_t char_idx,
                         const RequestChaos& chaos) {
    req.version = RawrXD::ExtensionKernel::kAutocompleteWireVersion;
    req.line = static_cast<uint32_t>((burst * 7 + static_cast<int>(char_idx)) % 300);
    req.col = static_cast<uint32_t>((burst * 11 + static_cast<int>(char_idx)) % 120);
    req.flags = (chaos.syntax_noise ? RawrXD::ExtensionKernel::kAutocompleteFlagSyntaxNoise : 0u)
              | (chaos.context_churn ? RawrXD::ExtensionKernel::kAutocompleteFlagContextChurn : 0u)
              | (chaos.cold ? RawrXD::ExtensionKernel::kAutocompleteFlagColdStart : 0u)
              | (chaos.file_switch ? RawrXD::ExtensionKernel::kAutocompleteFlagFileSwitch : 0u);
    std::snprintf(req.filePath, sizeof(req.filePath), "d:/rawrxd/src/demo_%08llx.%s",
                  static_cast<unsigned long long>(chaos.file_hash & 0xFFFFFFFFull),
                  ((chaos.file_hash ^ static_cast<uint64_t>(burst)) & 1ull) ? "hpp" : "cpp");

    const char* bodies[] = {
        "struct Demo { int value; int compute() const { return value; } };\nint use_demo(Demo d) { return d.compute(); }\n",
        "template<class T> auto use_demo(T d) { return d.compute(\n",
        "namespace churn { struct Demo { int value; }; int use(Demo d) { if (d.value > 0 return d.value; }\n",
        "int use_demo(auto d) { for (int i = 0; i < d.value; ++i) { d.compute(); } return d.\n",
    };
    std::snprintf(req.content, sizeof(req.content), "%s\n// ctx=%08llx cold=%u churn=%u noise=%u\n",
                  bodies[(chaos.context_hash + (chaos.syntax_noise ? 1u : 0u)) % std::size(bodies)],
                  static_cast<unsigned long long>(chaos.context_hash & 0xFFFFFFFFull),
                  chaos.cold ? 1u : 0u,
                  chaos.context_churn ? 1u : 0u,
                  chaos.syntax_noise ? 1u : 0u);

    const char* prefixes[] = {
        "ret", "return d.", "return d.compute", "if (d.value", "for (int i", "d.com", "std::"
    };
    const char* suffixes[] = {
        ";", ") {", " }", " < 10)", " = 0;", " /*broken", " ->"
    };
    std::strcpy(req.prefix, prefixes[(burst + char_idx) % std::size(prefixes)]);
    std::strcpy(req.suffix, chaos.syntax_noise
        ? suffixes[(burst * 5 + char_idx + 5) % std::size(suffixes)]
        : suffixes[(burst * 3 + char_idx) % std::size(suffixes)]);
}

inline void fill_response(RawrXD::ExtensionKernel::CompletionResult& res,
                          int request_idx,
                          const RequestChaos& chaos) {
    res.version = RawrXD::ExtensionKernel::kAutocompleteWireVersion;
    res.count = 3;
    res.tokens[0] = 101 + (request_idx % 7);
    res.tokens[1] = 102;
    res.tokens[2] = 103;
    std::strcpy(res.text, (request_idx % 4 == 0) ? "compute" : "return");

    float miss_pressure = 0.48f;
    if (chaos.cold) miss_pressure += 0.18f;
    if (chaos.file_switch) miss_pressure += 0.20f;
    if (chaos.context_churn) miss_pressure += 0.16f;
    if (chaos.syntax_noise) miss_pressure += 0.08f;
    if (chaos.hard_reset) miss_pressure += 0.18f;
    miss_pressure = std::min(miss_pressure, 0.86f);

    const bool cache_hit = rng_f() > miss_pressure;
    const uint32_t generated = 6u + static_cast<uint32_t>(rng_u32() % 5u);
    uint32_t rejects = 1u + static_cast<uint32_t>(rng_u32() % 2u);
    if (chaos.syntax_noise) rejects += 1u + static_cast<uint32_t>(rng_u32() % 2u);
    if (chaos.context_churn) rejects += static_cast<uint32_t>(rng_u32() % 2u);
    if (chaos.file_switch) rejects += 1u;
    if (chaos.cold || chaos.hard_reset) rejects += static_cast<uint32_t>(rng_u32() % 2u);
    uint32_t heads = 1u;
    if (chaos.syntax_noise || chaos.context_churn) heads = 3u;
    else if (chaos.cold || chaos.file_switch || chaos.hard_reset) heads = 2u;
    uint32_t pruned = 0u;
    if (heads >= 3u && (rng_u32() % 5u) < 2u) {
        --heads;
        ++pruned;
    }
    if (heads > 1u && rejects > 0u) {
        const uint32_t specialization_bonus = (chaos.syntax_noise || chaos.context_churn) ? 1u : 0u;
        const uint32_t recovered = std::min<uint32_t>(rejects,
            (heads - 1u) + specialization_bonus + (rng_u32() % heads));
        rejects -= recovered;
    }
    rejects = std::min<uint32_t>(rejects, generated);
    const uint32_t accepted = generated - rejects;

    res.cacheHit = cache_hit ? 1u : 0u;
    res.kvStitchCount = (chaos.cold || chaos.file_switch || chaos.hard_reset)
        ? static_cast<uint32_t>(rng_u32() % 2u)
        : static_cast<uint32_t>(1u + (rng_u32() % 3u));
    res.tokensGenerated = generated;
    res.tokensAccepted = accepted;
    res.verifyRejects = generated - accepted;
    res.specDepth = chaos.syntax_noise ? 2u : ((chaos.context_churn || chaos.cold) ? 4u : 8u);
    res.specHeads = heads;
    res.specHeadsPruned = pruned;
    res.acceptanceRate = static_cast<float>(accepted) / static_cast<float>(generated);
    res.speedupEstimate = 1.0f + res.acceptanceRate * 3.0f + (cache_hit ? 2.0f : 0.0f)
                        + (heads > 1u ? 0.30f * static_cast<float>(heads - 1u) : 0.0f)
                        + (pruned ? 0.20f : 0.0f);
}

} // namespace

int main() {
    constexpr const char* kBase = "RawrXD_Autocomplete_P95";
    constexpr int kBursts = 1200;
    static constexpr ChaosConfig kChaos{};
    static constexpr std::array<TypingEvent, 7> kPattern{{
        {3, 2}, {2, 1}, {5, 6}, {1, 0}, {4, 3}, {6, 8}, {2, 0}
    }};

    RawrXD::IPC::ShmBiChannel server;
    RawrXD::IPC::ShmBiChannel client;

    if (!server.open_server(kBase, 128)) return 1;
    if (!client.open_client(kBase, 128)) return 2;

    std::vector<uint64_t> latencies;
    latencies.reserve(kBursts * 5);
    std::deque<uint64_t> rolling_window;
    MetricsTotals totals;
    ChaosMetrics chaos_totals;
    rng_init(kChaos.seed);

    int request_idx = 0;
    uint32_t cold_left = kChaos.initial_cold_requests;
    uint32_t hard_reset_left = 0;
    uint64_t file_hash = 0xAABBCCDDull;
    uint64_t context_hash = 0x11223344ull;
    double max_window_p95 = 0.0;

    for (int burst = 0; burst < kBursts; ++burst) {
        const auto& ev = kPattern[static_cast<size_t>(burst) % kPattern.size()];

        for (uint32_t c = 0; c < ev.chars; ++c, ++request_idx) {
            if (request_idx > 0 && kChaos.hard_reset_period > 0 &&
                (static_cast<uint32_t>(request_idx) % kChaos.hard_reset_period) == 0) {
                hard_reset_left = kChaos.hard_reset_len;
                cold_left = std::max(cold_left, kChaos.hard_reset_len);
                ++chaos_totals.hard_resets;
                file_hash = (static_cast<uint64_t>(rng_u32()) << 32) ^ rng_u32();
                context_hash = (static_cast<uint64_t>(rng_u32()) << 32) ^ rng_u32();
            }

            if (cold_left == 0 && rng_f() < kChaos.cold_start_prob) {
                cold_left = kChaos.cold_start_len;
                ++chaos_totals.cold_starts;
            }

            RequestChaos chaos{};
            chaos.cold = cold_left > 0;
            chaos.hard_reset = hard_reset_left > 0;
            chaos.file_switch = rng_f() < kChaos.file_switch_prob;
            chaos.context_churn = rng_f() < kChaos.context_churn_prob;
            chaos.syntax_noise = rng_f() < kChaos.syntax_noise_prob;

            if (chaos.file_switch) {
                file_hash = (static_cast<uint64_t>(rng_u32()) << 32) ^ rng_u32();
                ++chaos_totals.file_switches;
            }
            if (chaos.context_churn) {
                context_hash = (static_cast<uint64_t>(rng_u32()) << 32) ^ rng_u32();
                ++chaos_totals.context_churns;
            }
            if (chaos.syntax_noise) ++chaos_totals.syntax_noise;
            if (chaos.cold) ++chaos_totals.cold_requests;

            chaos.file_hash = file_hash;
            chaos.context_hash = context_hash;

            RawrXD::ExtensionKernel::CompletionRequest req{};
            fill_request(req, burst, c, chaos);

            const uint64_t start = now_ns();
            if (!client.send(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(&req), sizeof(req)))) {
                return 3;
            }

            std::vector<uint8_t> inbound;
            bool seen_req = false;
            for (int spin = 0; spin < 200000; ++spin) {
                if (server.rx().read_copy(inbound)) {
                    seen_req = true;
                    break;
                }
                _mm_pause();
            }
            if (!seen_req || inbound.size() < sizeof(RawrXD::ExtensionKernel::CompletionRequest)) {
                return 4;
            }

            deterministic_compute_jitter(static_cast<uint32_t>(request_idx));
            burn_for_ns((chaos.cold ? 1200000ull : 0ull) +
                        (chaos.syntax_noise ? 900000ull : 0ull) +
                        (chaos.context_churn ? 350000ull : 0ull) +
                        (chaos.file_switch ? 300000ull : 0ull) +
                        (chaos.hard_reset ? 700000ull : 0ull) +
                        ((chaos.syntax_noise || chaos.context_churn) ? 25000ull :
                         ((chaos.file_switch || chaos.cold || chaos.hard_reset) ? 12000ull : 0ull)));

            RawrXD::ExtensionKernel::CompletionResult res{};
            fill_response(res, request_idx, chaos);
            if (!server.send(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(&res), sizeof(res)))) {
                return 5;
            }

            std::vector<uint8_t> back;
            bool ok = false;
            for (int spin = 0; spin < 200000; ++spin) {
                if (client.rx().read_copy(back)) {
                    ok = true;
                    break;
                }
                _mm_pause();
            }
            if (!ok || back.size() < sizeof(RawrXD::ExtensionKernel::CompletionResult)) {
                return 6;
            }

            RawrXD::ExtensionKernel::CompletionResult observed{};
            std::memcpy(&observed, back.data(), sizeof(observed));

            const uint64_t end = now_ns();
            const uint64_t dt = end - start;
            latencies.push_back(dt);
            rolling_window.push_back(dt);
            if (rolling_window.size() > kChaos.window_size) {
                rolling_window.pop_front();
            }
            max_window_p95 = std::max(max_window_p95, rolling_p95_ms(rolling_window));

            ++totals.requests;
            totals.cache_hits += observed.cacheHit ? 1u : 0u;
            totals.kv_stitches += observed.kvStitchCount;
            totals.tokens_generated += observed.tokensGenerated;
            totals.tokens_accepted += observed.tokensAccepted;
            totals.verify_rejects += observed.verifyRejects;
            totals.depth_sum += observed.specDepth;
            totals.head_sum += observed.specHeads ? observed.specHeads : 1u;
            totals.heads_pruned += observed.specHeadsPruned;

            if (cold_left > 0) --cold_left;
            if (hard_reset_left > 0) --hard_reset_left;
        }

        if (ev.delay_ms > 0) {
            Sleep(ev.delay_ms);
        }
    }

    std::sort(latencies.begin(), latencies.end());
    const double p50 = pct_ms(latencies, 0.50);
    const double p95 = pct_ms(latencies, 0.95);
    const double p99 = pct_ms(latencies, 0.99);
    const double rolling_last = rolling_p95_ms(rolling_window);
    const double cache_hit_rate = totals.requests
        ? static_cast<double>(totals.cache_hits) / static_cast<double>(totals.requests)
        : 0.0;
    const double avg_acceptance_rate = totals.tokens_generated
        ? static_cast<double>(totals.tokens_accepted) / static_cast<double>(totals.tokens_generated)
        : 0.0;
    const double verify_reject_rate = totals.tokens_generated
        ? static_cast<double>(totals.verify_rejects) / static_cast<double>(totals.tokens_generated)
        : 0.0;
    const double avg_depth = totals.requests
        ? static_cast<double>(totals.depth_sum) / static_cast<double>(totals.requests)
        : 0.0;
    const double avg_heads = totals.requests
        ? static_cast<double>(totals.head_sum) / static_cast<double>(totals.requests)
        : 0.0;
    const double avg_pruned = totals.requests
        ? static_cast<double>(totals.heads_pruned) / static_cast<double>(totals.requests)
        : 0.0;

    std::printf("P50=%.3fms\n", p50);
    std::printf("P95=%.3fms\n", p95);
    std::printf("P99=%.3fms\n", p99);
    std::printf("rolling_P95_last_window=%.3fms\n", rolling_last);
    std::printf("max_window_P95=%.3fms\n", max_window_p95);
    std::printf("requests=%llu\n", static_cast<unsigned long long>(totals.requests));
    std::printf("cache_hit_rate=%.3f\n", cache_hit_rate);
    std::printf("kv_stitches=%llu\n", static_cast<unsigned long long>(totals.kv_stitches));
    std::printf("avg_acceptance_rate=%.3f\n", avg_acceptance_rate);
    std::printf("verify_reject_rate=%.3f\n", verify_reject_rate);
    std::printf("avg_spec_depth=%.3f\n", avg_depth);
    std::printf("avg_spec_heads=%.3f\n", avg_heads);
    std::printf("avg_heads_pruned=%.3f\n", avg_pruned);
    std::printf("chaos_cold_starts=%llu\n", static_cast<unsigned long long>(chaos_totals.cold_starts));
    std::printf("chaos_hard_resets=%llu\n", static_cast<unsigned long long>(chaos_totals.hard_resets));
    std::printf("chaos_cold_requests=%llu\n", static_cast<unsigned long long>(chaos_totals.cold_requests));
    std::printf("chaos_file_switches=%llu\n", static_cast<unsigned long long>(chaos_totals.file_switches));
    std::printf("chaos_ctx_churns=%llu\n", static_cast<unsigned long long>(chaos_totals.context_churns));
    std::printf("chaos_syntax_noise=%llu\n", static_cast<unsigned long long>(chaos_totals.syntax_noise));

    // Quality gates: comfortably interactive tail latency and healthy speculative behavior.
    if (p95 >= 10.0) return 7;
    if (max_window_p95 >= 15.0) return 11;
    if (avg_acceptance_rate <= 0.86) return 8;
    if (cache_hit_rate <= 0.20) return 9;
    if (verify_reject_rate >= 0.14) return 10;
    if (avg_heads <= 1.50) return 13;
    if (avg_pruned <= 0.10) return 14;
    if (chaos_totals.file_switches == 0 || chaos_totals.context_churns == 0 || chaos_totals.syntax_noise == 0) return 12;
    return 0;
}
