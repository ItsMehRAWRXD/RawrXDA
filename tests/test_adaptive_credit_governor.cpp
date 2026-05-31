// ============================================================================
// test_adaptive_credit_governor.cpp — Stress test for Weight-Aware tuning
// ============================================================================
// Simulates multi-modal workloads (Parsing → Hotpatching → Mixed) and verifies
// that the AdaptiveCreditGovernor adjusts target throughput correctly.
//
// Build: cmake --build build-ninja --target test_adaptive_credit_governor
// Run:   .\build-ninja\tests\test_adaptive_credit_governor.exe

#include <iostream>
#include <cassert>
#include <cmath>
#include <thread>
#include <chrono>
#include "flow_control/adaptive_credit_governor.hpp"

using namespace RawrXD::FlowControl;
using namespace std::chrono;

int main() {
    std::cout << "=== Adaptive Credit Governor Stress Test ===\n\n";

    // 1. Initialize with baseline config
    CreditConfig baseConfig;
    baseConfig.initialCredits = 200;
    baseConfig.maxCredits = 500;
    baseConfig.minCredits = 50;
    baseConfig.silent = true;

    AdaptiveGovernorConfig adaptiveConfig;
    adaptiveConfig.base = GovernorConfig{};
    adaptiveConfig.base.targetThroughput = 10.0e9;  // 10B elem/s baseline
    adaptiveConfig.base.silent = true;
    adaptiveConfig.weightInference = 1.00f;
    adaptiveConfig.weightParsing = 0.85f;
    adaptiveConfig.weightHotpatching = 0.60f;
    adaptiveConfig.weightMixed = 0.45f;
    adaptiveConfig.renderReserveCredits = 50;
    adaptiveConfig.renderDropThreshold = 3;
    adaptiveConfig.toolSpikeBoostCredits = 30;
    adaptiveConfig.toolSpikeDurationMs = 500;
    adaptiveConfig.workloadHysteresisMs = 100;  // Short for test speed
    adaptiveConfig.silent = true;

    AdaptiveCreditGovernor governor;
    bool initOk = governor.Initialize(baseConfig, adaptiveConfig);
    assert(initOk);
    std::cout << "[1] Governor initialized\n";
    std::cout << "    ✓ Init success\n\n";

    // 2. Baseline: Idle workload → target should be ~10B
    WorkloadTelemetry idleTelemetry;
    idleTelemetry.type = WorkloadType::Idle;
    idleTelemetry.timestampMs = 0;
    idleTelemetry.kvCacheReductionFactor = 1.0f;
    
    governor.RecordWorkloadTelemetry(idleTelemetry);
    double idleTarget = governor.GetEffectiveTargetThroughput();
    std::cout << "[2] Idle workload target: " << (idleTarget / 1e9) << " B elem/s\n";
    assert(std::abs(idleTarget - 10.0e9) < 0.1e9);
    std::cout << "    ✓ Baseline target maintained\n\n";

    // 3. Parsing workload → target should drop to ~8.5B (85% of 10B)
    WorkloadTelemetry parseTelemetry;
    parseTelemetry.type = WorkloadType::Parsing;
    parseTelemetry.timestampMs = 200;  // Past hysteresis
    parseTelemetry.kvCacheReductionFactor = 1.0f;
    
    governor.RecordWorkloadTelemetry(parseTelemetry);
    double parseTarget = governor.GetEffectiveTargetThroughput();
    std::cout << "[3] Parsing workload target: " << (parseTarget / 1e9) << " B elem/s\n";
    assert(parseTarget < 9.0e9 && parseTarget > 8.0e9);
    std::cout << "    ✓ Parsing weight applied (85% of baseline)\n\n";

    // 4. Hotpatching workload → target should drop to ~6.0B (60% of 10B)
    WorkloadTelemetry hotpatchTelemetry;
    hotpatchTelemetry.type = WorkloadType::Hotpatching;
    hotpatchTelemetry.timestampMs = 400;
    hotpatchTelemetry.toolExecutionActive = true;
    hotpatchTelemetry.hotpatchToolCount = 1;
    hotpatchTelemetry.kvCacheReductionFactor = 1.0f;
    
    governor.RecordWorkloadTelemetry(hotpatchTelemetry);
    double hotpatchTarget = governor.GetEffectiveTargetThroughput();
    std::cout << "[4] Hotpatching workload target: " << (hotpatchTarget / 1e9) << " B elem/s\n";
    assert(hotpatchTarget < 6.5e9 && hotpatchTarget > 5.5e9);
    std::cout << "    ✓ Hotpatching weight applied (60% of baseline)\n\n";

    // 5. Mixed workload → target should drop to ~4.5B (45% of 10B)
    WorkloadTelemetry mixedTelemetry;
    mixedTelemetry.type = WorkloadType::Mixed;
    mixedTelemetry.timestampMs = 600;
    mixedTelemetry.toolExecutionActive = true;
    mixedTelemetry.hotpatchToolCount = 2;
    mixedTelemetry.activeToolCount = 3;
    mixedTelemetry.kvCacheReductionFactor = 1.0f;
    
    governor.RecordWorkloadTelemetry(mixedTelemetry);
    double mixedTarget = governor.GetEffectiveTargetThroughput();
    std::cout << "[5] Mixed workload target: " << (mixedTarget / 1e9) << " B elem/s\n";
    assert(mixedTarget < 5.0e9 && mixedTarget > 4.0e9);
    std::cout << "    ✓ Mixed weight applied (45% of baseline)\n\n";

    // 6. KV-cache pressure叠加 → target should drop further
    WorkloadTelemetry pressureTelemetry;
    pressureTelemetry.type = WorkloadType::Mixed;
    pressureTelemetry.timestampMs = 800;
    pressureTelemetry.kvCacheReductionFactor = 0.50f;  // 50% reduction from memory pressure
    
    governor.RecordWorkloadTelemetry(pressureTelemetry);
    double pressureTarget = governor.GetEffectiveTargetThroughput();
    std::cout << "[6] Mixed + KV pressure target: " << (pressureTarget / 1e9) << " B elem/s\n";
    // Mixed weight=0.45, KV factor=0.50 → effective weight=0.225
    // Target = 10B * 0.225 = 2.25B
    assert(pressureTarget < 3.0e9 && pressureTarget > 1.5e9);
    std::cout << "    ✓ KV-cache pressure compounded (weight * reduction)\n\n";

    // 7. Emergency throttle test
    std::cout << "[7] Emergency throttle test\n";
    governor.EmergencyThrottle();
    assert(governor.IsEmergencyThrottled());
    assert(governor.IsRenderReserveActive());
    assert(governor.GetRenderReserveCredits() == 50);
    std::cout << "    ✓ Emergency throttle activated, reserve=50 credits\n";
    
    governor.ClearEmergencyThrottle();
    assert(!governor.IsEmergencyThrottled());
    assert(!governor.IsRenderReserveActive());
    std::cout << "    ✓ Emergency throttle cleared\n\n";

    // 8. Workload type tracking
    std::cout << "[8] Workload type tracking\n";
    assert(governor.GetCurrentWorkloadType() == WorkloadType::Mixed);
    std::cout << "    ✓ Current workload: Mixed\n\n";

    // 9. Print adaptive report
    std::cout << "[9] Adaptive report:\n";
    governor.PrintAdaptiveReport();
    std::cout << "\n";

    std::cout << "=== ALL ADAPTIVE GOVERNOR TESTS PASSED ===\n";
    return 0;
}
