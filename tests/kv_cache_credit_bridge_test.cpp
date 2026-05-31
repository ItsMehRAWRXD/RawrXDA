// kv_cache_credit_bridge_test.cpp
// Stress test: simulate 70B model prefill colliding with 10K-line lib.rs parse.
// Validates that the bridge throttles credits before OOM.

#include "kv_cache_credit_bridge.hpp"
#include "flow_control/credit_based_flow_control.hpp"
#include <cstdio>
#include <cstdint>
#include <thread>
#include <chrono>
#include <cmath>

using namespace Rawrxd::KVCache;
using namespace RawrXD::FlowControl;

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_passed; printf("  [PASS] %s\n", msg); } \
    else { ++g_failed; printf("  [FAIL] %s\n", msg); } \
} while(0)

// Simulate KV-cache growth during 70B model prefill
static void simulate_70b_prefill(KVCacheCreditBridge& bridge, CreditCounter& credits) {
    printf("\n[Test] 70B model prefill simulation\n");

    // 70B model KV-cache: ~2GB per 1K tokens at FP8
    // Prefill of 4K context = ~8GB
    const uint64_t maxKvCache = 8ULL * 1024 * 1024 * 1024; // 8GB
    const uint64_t step = 256ULL * 1024 * 1024; // 256MB steps

    for (uint64_t kv = 0; kv <= maxKvCache; kv += step) {
        bridge.ReportKVCacheUsage(kv, kv + 512ULL * 1024 * 1024);

        // Simulate token generation attempting to acquire credits
        auto result = credits.TryAcquire(32);

        auto telemetry = bridge.GetTelemetry();
        printf("  KV=%.2fGB reduction=%.2f credits=%s\n",
               kv / (1024.0 * 1024 * 1024),
               telemetry.reductionFactor,
               result == CreditResult::Success ? "OK" : "BLOCKED");

        // At critical pressure (>6GB), credits should be throttled
        if (kv > 6ULL * 1024 * 1024 * 1024) {
            CHECK(telemetry.reductionFactor <= 0.5f, "critical pressure throttles credits");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Simulate 10K-line lib.rs parse under memory pressure
static void simulate_rust_parse_under_pressure(KVCacheCreditBridge& bridge, CreditCounter& credits) {
    printf("\n[Test] 10K-line lib.rs parse under pressure\n");

    // Set high KV-cache pressure (6GB allocated)
    bridge.ReportKVCacheUsage(6ULL * 1024 * 1024 * 1024, 7ULL * 1024 * 1024 * 1024);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto telemetry = bridge.GetTelemetry();
    float reduction = telemetry.reductionFactor;

    CHECK(reduction < 1.0f, "pressure reduces credits");

    // Attempt to parse (acquire credits for AST operations)
    auto result = credits.TryAcquire(16);
    CHECK(result == CreditResult::Success || result == CreditResult::Blocked,
          "parse either succeeds or is blocked gracefully");

    // Release KV-cache pressure
    bridge.ReportKVCacheUsage(1ULL * 1024 * 1024 * 1024, 2ULL * 1024 * 1024 * 1024);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    telemetry = bridge.GetTelemetry();
    CHECK(telemetry.reductionFactor > reduction, "pressure release restores credits");
}

// Test PID adaptive tuning
static void test_pid_adaptive_tuning(KVCacheCreditBridge& bridge, CreditCounter& credits) {
    printf("\n[Test] PID adaptive tuning\n");

    // PID is already enabled from main() initialization
    // Just verify it works under pressure

    // Ramp up pressure gradually
    for (int i = 0; i < 10; ++i) {
        uint64_t kv = static_cast<uint64_t>(i) * 512ULL * 1024 * 1024;
        bridge.ReportKVCacheUsage(kv, kv + 256ULL * 1024 * 1024);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto telemetry = bridge.GetTelemetry();
    CHECK(telemetry.reductionFactor < 1.0f, "PID reduces credits under pressure");
    CHECK(telemetry.creditThrottlesApplied > 0, "PID applied throttles");
}

// Test manual override
static void test_manual_override(KVCacheCreditBridge& bridge, CreditCounter& credits) {
    printf("\n[Test] Manual override\n");

    bridge.ReportKVCacheUsage(7ULL * 1024 * 1024 * 1024, 8ULL * 1024 * 1024 * 1024);

    auto telemetry = bridge.GetTelemetry();
    float autoReduction = telemetry.reductionFactor;

    // Force 50% reduction
    bridge.SetManualReduction(0.5f);
    // Trigger re-evaluation
    bridge.ReportKVCacheUsage(7ULL * 1024 * 1024 * 1024, 8ULL * 1024 * 1024 * 1024);

    telemetry = bridge.GetTelemetry();
    CHECK(std::abs(telemetry.reductionFactor - 0.5f) < 0.01f, "manual override applied");

    // Disable override
    bridge.SetManualReduction(-1.0f);
    bridge.ReportKVCacheUsage(7ULL * 1024 * 1024 * 1024, 8ULL * 1024 * 1024 * 1024);

    telemetry = bridge.GetTelemetry();
    CHECK(std::abs(telemetry.reductionFactor - autoReduction) < 0.1f,
          "manual override released, auto resumes");
}

int main() {
    printf("========================================\n");
    printf("KV-Cache Credit Bridge Stress Test\n");
    printf("70B prefill + 10K-line parse collision\n");
    printf("========================================\n");

    CreditCounter credits;
    CreditConfig creditConfig;
    creditConfig.initialCredits = 1024;
    creditConfig.maxCredits = 4096;
    creditConfig.minCredits = 64;
    credits.Initialize(creditConfig);

    KVCacheCreditBridge& bridge = KVCacheCreditBridge::Instance();
    bridge.Initialize(&credits);

    simulate_70b_prefill(bridge, credits);
    simulate_rust_parse_under_pressure(bridge, credits);
    test_pid_adaptive_tuning(bridge, credits);
    test_manual_override(bridge, credits);

    bridge.Shutdown();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    return g_failed > 0 ? 1 : 0;
}
