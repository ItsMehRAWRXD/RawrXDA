// ============================================================================
// test_credit_governor.cpp - Unit tests for auto-tuning credit governor
// ============================================================================

#include "flow_control/credit_governor.hpp"
#include <cstdio>

using namespace RawrXD::FlowControl;

// Test 1: Basic initialization and config retrieval
bool TestInitialization() {
    printf("\n[Test] Governor initialization...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.targetThroughput = 10.0e9;
    gov.kp = 0.3;
    gov.ki = 0.05;
    gov.kd = 0.1;
    
    CreditGovernor governor;
    bool pass1 = governor.Initialize(base, gov);
    printf("  Initialize: %s\n", pass1 ? "SUCCESS" : "FAILED");
    
    auto config = governor.GetCurrentConfig();
    bool pass2 = (config.minCredits == 100);
    printf("  Initial minCredits: %u (expected 100)\n", config.minCredits);
    
    governor.Shutdown();
    
    bool pass = pass1 && pass2;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 2: Telemetry-driven tuning (above target → more aggressive)
bool TestTuningAboveTarget() {
    printf("\n[Test] Tuning above target...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.targetThroughput = 10.0e9;
    gov.kp = 0.5;
    gov.updateIntervalMs = 50;  // Fast update for test
    
    CreditGovernor governor;
    governor.Initialize(base, gov);
    
    // Feed telemetry showing 150% of target (above target)
    // This should DECREASE minCredits (more aggressive)
    for (int i = 0; i < 5; i++) {
        GovernorTelemetry tel;
        tel.throughputElemPerSec = 15.0e9;  // 150% of target
        tel.timestampMs = 100 + i * 60;  // Start at 100, increment by 60
        governor.RecordTelemetry(tel);
    }
    
    auto config = governor.GetCurrentConfig();
    printf("  minCredits after above-target: %u (started at 100)\n", config.minCredits);
    bool pass = (config.minCredits < 100);  // Should have decreased
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Test 3: Telemetry-driven tuning (below target → more conservative)
bool TestTuningBelowTarget() {
    printf("\n[Test] Tuning below target...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.targetThroughput = 10.0e9;
    gov.kp = 0.5;
    gov.updateIntervalMs = 50;
    
    CreditGovernor governor;
    governor.Initialize(base, gov);
    
    // Feed telemetry showing 50% of target (below target)
    // This should INCREASE minCredits (more conservative)
    for (int i = 0; i < 5; i++) {
        GovernorTelemetry tel;
        tel.throughputElemPerSec = 5.0e9;  // 50% of target
        tel.timestampMs = 100 + i * 60;
        governor.RecordTelemetry(tel);
    }
    
    auto config = governor.GetCurrentConfig();
    printf("  minCredits after below-target: %u (started at 100)\n", config.minCredits);
    bool pass = (config.minCredits > 100);  // Should have increased
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Test 4: Override and reset
bool TestOverride() {
    printf("\n[Test] Manual override...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.minCreditsFloor = 20;
    gov.minCreditsCeiling = 500;
    
    CreditGovernor governor;
    governor.Initialize(base, gov);
    
    // Override to 250
    governor.OverrideMinCredits(250);
    auto config = governor.GetCurrentConfig();
    bool pass1 = (config.minCredits == 250);
    printf("  After override: %u (expected 250)\n", config.minCredits);
    
    // Try to override beyond ceiling (should clamp)
    governor.OverrideMinCredits(1000);
    config = governor.GetCurrentConfig();
    bool pass2 = (config.minCredits == 500);  // Clamped to ceiling
    printf("  After ceiling override: %u (expected 500)\n", config.minCredits);
    
    // Reset
    governor.ResetOverride();
    config = governor.GetCurrentConfig();
    bool pass3 = (config.minCredits == 100);  // Back to base
    printf("  After reset: %u (expected 100)\n", config.minCredits);
    
    bool pass = pass1 && pass2 && pass3;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Test 5: Safety bounds (floor and ceiling)
bool TestSafetyBounds() {
    printf("\n[Test] Safety bounds...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.targetThroughput = 10.0e9;
    gov.kp = 2.0;  // Very aggressive to test clamping
    gov.minCreditsFloor = 50;
    gov.minCreditsCeiling = 200;
    gov.updateIntervalMs = 50;
    
    CreditGovernor governor;
    governor.Initialize(base, gov);
    
    // Extreme above-target (should hit floor)
    for (int i = 0; i < 10; i++) {
        GovernorTelemetry tel;
        tel.throughputElemPerSec = 50.0e9;  // Way above target
        tel.timestampMs = 100 + i * 60;
        governor.RecordTelemetry(tel);
    }
    
    auto config = governor.GetCurrentConfig();
    printf("  After extreme above-target: %u (floor=%u)\n", config.minCredits, gov.minCreditsFloor);
    bool pass1 = (config.minCredits >= gov.minCreditsFloor);
    
    // Extreme below-target (should hit ceiling)
    governor.ResetOverride();
    for (int i = 0; i < 10; i++) {
        GovernorTelemetry tel;
        tel.throughputElemPerSec = 1.0e9;  // Way below target
        tel.timestampMs = 100 + i * 60;
        governor.RecordTelemetry(tel);
    }
    
    config = governor.GetCurrentConfig();
    printf("  After extreme below-target: %u (ceiling=%u)\n", config.minCredits, gov.minCreditsCeiling);
    bool pass2 = (config.minCredits <= gov.minCreditsCeiling);
    
    bool pass = pass1 && pass2;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Test 6: EMA smoothing
bool TestEMASmoothing() {
    printf("\n[Test] EMA smoothing...\n");
    
    CreditConfig base;
    base.initialCredits = 1000;
    base.maxCredits = 1000;
    base.minCredits = 100;
    
    GovernorConfig gov;
    gov.targetThroughput = 10.0e9;
    gov.emaAlpha = 0.5;  // High alpha for responsive test
    gov.updateIntervalMs = 50;
    
    CreditGovernor governor;
    governor.Initialize(base, gov);
    
    // Feed oscillating telemetry
    for (int i = 0; i < 10; i++) {
        GovernorTelemetry tel;
        // Oscillate between 5B and 15B
        tel.throughputElemPerSec = (i % 2 == 0) ? 5.0e9 : 15.0e9;
        tel.timestampMs = i * 60;
        governor.RecordTelemetry(tel);
    }
    
    // With EMA=0.5, the smoothed value should be near the average (~10B)
    // So minCredits should stay near the base (100)
    auto config = governor.GetCurrentConfig();
    printf("  minCredits after oscillation: %u (base=100)\n", config.minCredits);
    bool pass = (config.minCredits >= 80 && config.minCredits <= 120);  // Should be near base
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Main test runner
int main() {
    printf("========================================\n");
    printf("Credit Governor Auto-Tuner Tests\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 6;
    
    if (TestInitialization()) passed++;
    if (TestTuningAboveTarget()) passed++;
    if (TestTuningBelowTarget()) passed++;
    if (TestOverride()) passed++;
    if (TestSafetyBounds()) passed++;
    if (TestEMASmoothing()) passed++;
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}
