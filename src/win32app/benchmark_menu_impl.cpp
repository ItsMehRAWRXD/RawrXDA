// Build-compat shim for legacy benchmark menu stub references.
// The production implementation lives in src/benchmark_menu_widget.cpp.

#include "../../include/benchmark_menu_widget.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {
std::atomic<uint64_t> g_benchmarkMenuStubHits{0};
std::atomic<uint64_t> g_benchmarkMenuNullMainWindowBinds{0};
std::atomic<uint64_t> g_benchmarkMenuInitCalls{0};
std::atomic<uint64_t> g_benchmarkMenuOpenCalls{0};
std::atomic<uint64_t> g_benchmarkMenuShowCalls{0};
std::atomic<uint64_t> g_benchmarkMenuRunCalls{0};
std::atomic<uint64_t> g_benchmarkMenuViewCalls{0};
std::atomic<uint64_t> g_benchmarkMenuStopCalls{0};
std::atomic<uint64_t> g_benchmarkMenuNotifyCalls{0};
std::atomic<uint64_t> g_benchmarkMenuLastCycleCount{0};
std::atomic<uint64_t> g_benchmarkMenuTotalCycleCount{0};
std::atomic<uint64_t> g_benchmarkMenuLastStepCount{0};
std::atomic<uint64_t> g_benchmarkMenuLastActionMask{0};

bool isTruthy(const char* value)
{
    if (!value) {
        return false;
    }
    const std::string v(value);
    return v == "1" || v == "true" || v == "TRUE" || v == "on" || v == "ON";
}

uint64_t configuredCycles()
{
    if (const char* value = std::getenv("RAWRXD_BENCHMARK_MENU_STUB_CYCLES")) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(value, &end, 10);
        if (end != value && parsed > 0) {
            return parsed > 4 ? 4 : static_cast<uint64_t>(parsed);
        }
    }
    return 1;
}
}

extern "C" void RawrXD_BenchmarkMenuStubAnchor() {
    g_benchmarkMenuStubHits.fetch_add(1, std::memory_order_relaxed);

    // Deterministic fallback: replay a bounded subset of the production menu flow.
    const bool shouldOpen = isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_OPEN"));
    const bool shouldShow = isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_SHOW"));
    const bool shouldRun = isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_RUN"));
    const bool shouldView = isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_VIEW"));
    const bool shouldStop = shouldRun || isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_STOP"));
    const bool shouldNotify = isTruthy(std::getenv("RAWRXD_BENCHMARK_MENU_STUB_NOTIFY"));
    const uint64_t cycles = configuredCycles();

    uint64_t actionMask = 0;
    uint64_t stepCount = 0;
    for (uint64_t cycle = 0; cycle < cycles; ++cycle) {
        BenchmarkMenu menu(nullptr);
        menu.setMainWindow(nullptr);
        g_benchmarkMenuNullMainWindowBinds.fetch_add(1, std::memory_order_relaxed);

        menu.initialize();
        g_benchmarkMenuInitCalls.fetch_add(1, std::memory_order_relaxed);
        actionMask |= 0x1ULL;
        ++stepCount;

        if (shouldOpen) {
            menu.openBenchmarkDialog();
            g_benchmarkMenuOpenCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x2ULL;
            ++stepCount;
        }

        if (shouldShow) {
            menu.show();
            g_benchmarkMenuShowCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x4ULL;
            ++stepCount;
        }

        if (shouldRun) {
            menu.runSelectedBenchmarks();
            g_benchmarkMenuRunCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x8ULL;
            ++stepCount;
        }

        if (shouldView) {
            menu.viewBenchmarkResults();
            g_benchmarkMenuViewCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x10ULL;
            ++stepCount;
        }

        if (shouldStop) {
            menu.stopBenchmarks();
            g_benchmarkMenuStopCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x20ULL;
            ++stepCount;
        }

        if (shouldNotify) {
            menu.notifyFinished();
            g_benchmarkMenuNotifyCalls.fetch_add(1, std::memory_order_relaxed);
            actionMask |= 0x40ULL;
            ++stepCount;
        }
    }

    if (cycles > 1) {
        actionMask |= 0x80ULL;
    }

    g_benchmarkMenuLastCycleCount.store(cycles, std::memory_order_relaxed);
    g_benchmarkMenuTotalCycleCount.fetch_add(cycles, std::memory_order_relaxed);
    g_benchmarkMenuLastStepCount.store(stepCount, std::memory_order_relaxed);
    g_benchmarkMenuLastActionMask.store(actionMask, std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkMenuStubHitCount() {
    return g_benchmarkMenuStubHits.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkMenuStubNullWindowBindCount() {
    return g_benchmarkMenuNullMainWindowBinds.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkMenuStubLastCycleCount() {
    return g_benchmarkMenuLastCycleCount.load(std::memory_order_relaxed);
}

extern "C" uint64_t RawrXD_BenchmarkMenuStubStats() {
    // [63:56] stop, [55:48] view, [47:40] run, [39:32] show,
    // [31:24] init, [23:16] null_binds, [15:8] hits, [7:0] action_mask_low.
    const uint64_t stop = g_benchmarkMenuStopCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t view = g_benchmarkMenuViewCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t run = g_benchmarkMenuRunCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t show = g_benchmarkMenuShowCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t init = g_benchmarkMenuInitCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t binds = g_benchmarkMenuNullMainWindowBinds.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_benchmarkMenuStubHits.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mask = g_benchmarkMenuLastActionMask.load(std::memory_order_relaxed) & 0xFFu;
    return (stop << 56) | (view << 48) | (run << 40) | (show << 32) | (init << 24) | (binds << 16) |
           (hits << 8) | mask;
}

extern "C" uint64_t RawrXD_BenchmarkMenuStubExtendedStats() {
    // [63:56] notify, [55:48] open, [47:40] last_cycles, [39:32] last_steps,
    // [31:24] total_cycles, [23:16] stop, [15:8] hits, [7:0] action_mask_low.
    const uint64_t notify = g_benchmarkMenuNotifyCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t open = g_benchmarkMenuOpenCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t lastCycles = g_benchmarkMenuLastCycleCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t lastSteps = g_benchmarkMenuLastStepCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t totalCycles = g_benchmarkMenuTotalCycleCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t stop = g_benchmarkMenuStopCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_benchmarkMenuStubHits.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mask = g_benchmarkMenuLastActionMask.load(std::memory_order_relaxed) & 0xFFu;
    return (notify << 56) | (open << 48) | (lastCycles << 40) | (lastSteps << 32) |
           (totalCycles << 24) | (stop << 16) | (hits << 8) | mask;
}

extern "C" void RawrXD_BenchmarkMenuStubResetStats() {
    g_benchmarkMenuStubHits.store(0, std::memory_order_relaxed);
    g_benchmarkMenuNullMainWindowBinds.store(0, std::memory_order_relaxed);
    g_benchmarkMenuInitCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuOpenCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuShowCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuRunCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuViewCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuStopCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuNotifyCalls.store(0, std::memory_order_relaxed);
    g_benchmarkMenuLastCycleCount.store(0, std::memory_order_relaxed);
    g_benchmarkMenuTotalCycleCount.store(0, std::memory_order_relaxed);
    g_benchmarkMenuLastStepCount.store(0, std::memory_order_relaxed);
    g_benchmarkMenuLastActionMask.store(0, std::memory_order_relaxed);
}
