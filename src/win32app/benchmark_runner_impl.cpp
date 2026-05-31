// Build-compat shim for legacy benchmark runner stub references.
// The production implementation lives in src/benchmark_runner.cpp.

#include "../../include/benchmark_runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {
std::atomic<uint64_t> g_benchmarkRunnerStubHits{0};
std::atomic<uint64_t> g_benchmarkRunnerObservedResults{0};
std::atomic<uint64_t> g_benchmarkRunnerObservedPasses{0};
std::atomic<uint64_t> g_benchmarkRunnerObservedRequests{0};
std::atomic<uint64_t> g_benchmarkRunnerRunCalls{0};
std::atomic<uint64_t> g_benchmarkRunnerStopCalls{0};
std::atomic<uint64_t> g_benchmarkRunnerLastDurationMs{0};
std::atomic<uint64_t> g_benchmarkRunnerTotalDurationMs{0};
std::atomic<uint64_t> g_benchmarkRunnerLastIterationCount{0};
std::atomic<uint64_t> g_benchmarkRunnerInvalidIterationCount{0};
std::atomic<uint64_t> g_benchmarkRunnerClampedIterationCount{0};
std::atomic<uint64_t> g_benchmarkRunnerLastModeMask{0};

bool isTruthy(const char* value)
{
    if (!value) {
        return false;
    }
    return std::string(value) == "1" || std::string(value) == "true" || std::string(value) == "TRUE" ||
           std::string(value) == "on" || std::string(value) == "ON";
}

uint64_t configuredIterations()
{
    if (const char* value = std::getenv("RAWRXD_BENCHMARK_STUB_ITERATIONS")) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(value, &end, 10);
        if (end != value && parsed > 0) {
            if (parsed > 8) {
                g_benchmarkRunnerClampedIterationCount.fetch_add(1, std::memory_order_relaxed);
                return 8;
            }
            return static_cast<uint64_t>(parsed);
        }
        g_benchmarkRunnerInvalidIterationCount.fetch_add(1, std::memory_order_relaxed);
    }
    return 1;
}
}

extern "C" void RawrXD_BenchmarkRunnerStubAnchor() {
    g_benchmarkRunnerStubHits.fetch_add(1, std::memory_order_relaxed);

    // Deterministic fallback: touch live benchmark subsystem and snapshot aggregate stats.
    BenchmarkRunner runner;
    const bool shouldRun = isTruthy(std::getenv("RAWRXD_BENCHMARK_STUB_RUN"));
    const bool gpuEnabled = isTruthy(std::getenv("RAWRXD_BENCHMARK_STUB_GPU"));
    const uint64_t iterations = configuredIterations();
    g_benchmarkRunnerLastIterationCount.store(iterations, std::memory_order_relaxed);

    const auto t0 = std::chrono::steady_clock::now();
    if (shouldRun) {
        for (uint64_t i = 0; i < iterations; ++i) {
            g_benchmarkRunnerRunCalls.fetch_add(1, std::memory_order_relaxed);
            runner.runBenchmarks({"warm-cache"}, "", gpuEnabled, false);
        }
    }

    runner.stop();
    g_benchmarkRunnerStopCalls.fetch_add(1, std::memory_order_relaxed);

    const auto t1 = std::chrono::steady_clock::now();
    const uint64_t durationMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    g_benchmarkRunnerLastDurationMs.store(durationMs, std::memory_order_relaxed);
    g_benchmarkRunnerTotalDurationMs.fetch_add(durationMs, std::memory_order_relaxed);

    const auto& results = runner.getResults();

    uint64_t passCount = 0;
    uint64_t requestCount = 0;
    for (const auto& r : results) {
        if (r.passed) {
            ++passCount;
        }
        requestCount += static_cast<uint64_t>(r.totalRequests);
    }

    g_benchmarkRunnerObservedResults.store(static_cast<uint64_t>(results.size()), std::memory_order_relaxed);
    g_benchmarkRunnerObservedPasses.store(passCount, std::memory_order_relaxed);
    g_benchmarkRunnerObservedRequests.store(requestCount, std::memory_order_relaxed);

    uint64_t modeMask = 0;
    if (shouldRun) modeMask |= 0x1ULL;
    if (gpuEnabled) modeMask |= 0x2ULL;
    if (iterations > 1) modeMask |= 0x4ULL;
    if (!results.empty()) modeMask |= 0x8ULL;
    g_benchmarkRunnerLastModeMask.store(modeMask, std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubHitCount() {
    return g_benchmarkRunnerStubHits.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubResultCount() {
    return g_benchmarkRunnerObservedResults.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubPassCount() {
    return g_benchmarkRunnerObservedPasses.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubTotalRequests() {
    return g_benchmarkRunnerObservedRequests.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubTotalDurationMs() {
    return g_benchmarkRunnerTotalDurationMs.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubStats() {
    // [63:56] stop_calls, [55:48] run_calls, [47:40] result_count,
    // [39:32] pass_count, [31:24] hit_count, [23:16] mode_mask,
    // [15:0] last_duration_ms(low16).
    const uint64_t stopCalls = g_benchmarkRunnerStopCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t runCalls = g_benchmarkRunnerRunCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t results = g_benchmarkRunnerObservedResults.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t passes = g_benchmarkRunnerObservedPasses.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_benchmarkRunnerStubHits.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_benchmarkRunnerLastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t lastDuration = g_benchmarkRunnerLastDurationMs.load(std::memory_order_relaxed) & 0xFFFFu;
    return (stopCalls << 56) | (runCalls << 48) | (results << 40) | (passes << 32) |
           (hits << 24) | (mode << 16) | lastDuration;
}

extern "C" uint64_t RawrXD_BenchmarkRunnerStubExtendedStats() {
    // [63:56] clamped_iterations, [55:48] invalid_iterations, [47:40] last_iterations,
    // [39:32] total_requests, [31:16] total_duration_ms(low16), [15:8] hits, [7:0] mode_mask.
    const uint64_t clamped = g_benchmarkRunnerClampedIterationCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t invalid = g_benchmarkRunnerInvalidIterationCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t iterations = g_benchmarkRunnerLastIterationCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t requests = g_benchmarkRunnerObservedRequests.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t totalDuration = g_benchmarkRunnerTotalDurationMs.load(std::memory_order_relaxed) & 0xFFFFu;
    const uint64_t hits = g_benchmarkRunnerStubHits.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_benchmarkRunnerLastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    return (clamped << 56) | (invalid << 48) | (iterations << 40) | (requests << 32) |
           (totalDuration << 16) | (hits << 8) | mode;
}

extern "C" void RawrXD_BenchmarkRunnerStubResetStats() {
    g_benchmarkRunnerStubHits.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerObservedResults.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerObservedPasses.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerObservedRequests.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerRunCalls.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerStopCalls.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerLastDurationMs.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerTotalDurationMs.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerLastIterationCount.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerInvalidIterationCount.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerClampedIterationCount.store(0, std::memory_order_relaxed);
    g_benchmarkRunnerLastModeMask.store(0, std::memory_order_relaxed);
}
