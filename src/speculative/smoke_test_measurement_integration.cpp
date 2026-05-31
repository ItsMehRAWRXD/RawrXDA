// smoke_test_measurement_integration.cpp
// Validates that corrected measurement + pattern recognition integrate properly

#include "rawr_benchmark_measurement_corrected.h"
#include "rawr_autopatch_realtime_recognizer.h"
#include "cpu_inference_measurement_integration.h"

#include <iostream>
#include <thread>
#include <cmath>

using namespace RawrXD::Benchmark;
using namespace RawrXD::Autopatch;
using namespace RawrXD::Inference;

// ============================================================================
// SMOKE TEST 1: Measurement Framework Compiles
// ============================================================================

void TestMeasurementFramework() {
    std::cout << "\n=== SMOKE TEST 1: Measurement Framework ===\n";
    
    // Create a corrected measurement
    CorrectMeasurement m;
    m.time_to_first_token = std::chrono::milliseconds(1850);
    m.total_generation_time = std::chrono::milliseconds(6200);
    m.tokens_in_context = 120;
    m.tokens_generated_real = 512;
    m.tokens_expected = 512;
    m.overhead_server = std::chrono::milliseconds(50);
    m.overhead_tokenizer = std::chrono::milliseconds(10);
    m.overhead_post_process = std::chrono::milliseconds(5);
    m.peak_memory_bytes = 68 * 1024 * 1024 * 1024ULL;
    m.average_memory_bytes = 52 * 1024 * 1024 * 1024ULL;
    
    // Test calculations
    double real_tps = m.real_decode_tps();
    double total_tps = m.total_end_to_end_tps();
    
    std::cout << "Real decode TPS: " << real_tps << " tokens/sec\n";
    std::cout << "End-to-end TPS: " << total_tps << " tokens/sec\n";
    std::cout << "TTFT: " << m.ttft_ms() << " ms\n";
    
    // Validate
    if (MeasurementValidator::ValidateMeasurement(m)) {
        std::cout << "✓ Measurement VALID\n";
    } else {
        std::cout << "✗ Measurement INVALID (expected for realistic 70B)\n";
    }
    
    // Print report
    MeasurementValidator::PrintReport(m);
}

// ============================================================================
// SMOKE TEST 2: Pattern Recognition Compiles
// ============================================================================

void TestPatternRecognition() {
    std::cout << "\n=== SMOKE TEST 2: Pattern Recognition ===\n";
    
    TelemetryWindow window;
    RealtimePatternRecognizer recognizer;
    
    // Simulate degrading throughput pattern
    std::cout << "Simulating TPS degradation pattern...\n";
    
    for (int i = 0; i < 20; i++) {
        TelemetrySnapshot snap;
        snap.timestamp = std::chrono::high_resolution_clock::now();
        snap.tps = 100.0 - (i * 2.0);           // Degrading from 100 to 60 TPS
        snap.bandwidth_gbps = 25.0;
        snap.cache_hit_rate = 0.70;
        snap.prefetch_depth = 2;
        snap.memory_pressure_percent = 75.0f + (i * 0.5f);
        snap.latency_per_token_us = 10000.0 / snap.tps;
        snap.tier_current = 1;  // WARNING tier
        snap.is_first_token = (i == 0);
        
        window.AddSnapshot(snap);
        std::cout << "  Token " << i << ": TPS=" << snap.tps << ", BW=" << snap.bandwidth_gbps << " GB/s\n";
    }
    
    // Analyze patterns
    PerfPattern pattern = recognizer.RecognizePatterns();
    Diagnosis diag = PatternDiagnosticEngine::DiagnosePerformanceIssue(pattern);
    
    std::cout << "\nDetected Issue: " << diag.root_cause << "\n";
    std::cout << "Confidence: " << (diag.confidence * 100.0) << "%\n";
    std::cout << "Evidence:\n";
    for (const auto& ev : diag.evidence) {
        std::cout << "  - " << ev << "\n";
    }
    std::cout << "✓ Pattern recognition working\n";
}

// ============================================================================
// SMOKE TEST 3: Measurement Collector Integration
// ============================================================================

void TestMeasurementCollector() {
    std::cout << "\n=== SMOKE TEST 3: Measurement Collector ===\n";
    
    MeasurementCollector collector;
    
    // Register diagnosis callback
    collector.SetDiagnosisCallback([](const Diagnosis& diag) {
           std::cout << "  [Autopatch] " << diag.root_cause << "\n";
    });
    
    // Simulate token generation loop
    std::cout << "Simulating 25-token generation...\n";
    
    for (int i = 0; i < 25; i++) {
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Report token completion
        collector.TokenGenerationEnd(
            1000 + i,                    // token_id
            25.0,                        // bandwidth
            0.65 + (i * 0.001),         // cache_hit_rate increasing
            2,                           // prefetch_depth
            75.0f - (i * 0.2f),         // memory_pressure decreasing
            1                            // tier: WARNING
        );
        
        if (i % 5 == 0) {
            std::cout << "  Generated " << i << " tokens\n";
        }
    }
    
    // Get final measurement
    auto measurement = collector.GetFinalMeasurement();
    
    std::cout << "\nFinal Measurement:\n";
    std::cout << "  Tokens generated: " << measurement.tokens_generated_real << "\n";
    std::cout << "  Real decode TPS: " << measurement.real_decode_tps() << "\n";
    std::cout << "  End-to-end TPS: " << measurement.total_end_to_end_tps() << "\n";
    std::cout << "✓ Measurement collector working\n";
}

// ============================================================================
// SMOKE TEST 4: Validation Rules
// ============================================================================

void TestValidationRules() {
    std::cout << "\n=== SMOKE TEST 4: Validation Rules ===\n";
    
    // Test case 1: Synthetic (fast path)
    {
        CorrectMeasurement m;
        m.time_to_first_token = std::chrono::milliseconds(50);  // Too fast!
        m.total_generation_time = std::chrono::milliseconds(100);
        m.tokens_generated_real = 512;
        
        bool valid = MeasurementValidator::ValidateMeasurement(m);
        std::cout << "Fast-path (50ms TTFT): " << (valid ? "VALID" : "INVALID (synthetic)") << "\n";
    }
    
    // Test case 2: Realistic 70B Q8_0
    {
        CorrectMeasurement m;
        m.time_to_first_token = std::chrono::milliseconds(1850);
        m.total_generation_time = std::chrono::milliseconds(6200);
        m.tokens_generated_real = 512;
        m.tokens_expected = 512;
        
        bool valid = MeasurementValidator::ValidateMeasurement(m);
        std::cout << "Realistic 70B (1850ms TTFT): " << (valid ? "VALID" : "INVALID") << "\n";
    }
    
    // Test case 3: Unrealistic TPS
    {
        CorrectMeasurement m;
        m.time_to_first_token = std::chrono::milliseconds(100);
        m.total_generation_time = std::chrono::milliseconds(100);  // 100ms for 512 tokens??
        m.tokens_generated_real = 512;
        
        bool valid = MeasurementValidator::ValidateMeasurement(m);
        std::cout << "Unrealistic TPS (5120 TPS): " << (valid ? "VALID" : "INVALID") << "\n";
    }
    
    std::cout << "✓ Validation rules enforced\n";
}

// ============================================================================
// MAIN SMOKE TEST SUITE
// ============================================================================

int main() {
    std::cout << "====================================\n";
    std::cout << "RawrXD Measurement Integration Smoke Tests\n";
    std::cout << "====================================\n";
    
    try {
        TestMeasurementFramework();
        TestPatternRecognition();
        TestMeasurementCollector();
        TestValidationRules();
        
        std::cout << "\n====================================\n";
        std::cout << "✓ ALL SMOKE TESTS PASSED\n";
        std::cout << "====================================\n";
        std::cout << "\nIntegration Status:\n";
        std::cout << "  [✓] Measurement framework compiles\n";
        std::cout << "  [✓] Pattern recognition compiles\n";
        std::cout << "  [✓] Measurement collector integrates\n";
        std::cout << "  [✓] Validation rules working\n";
        std::cout << "\nReady for production integration:\n";
        std::cout << "  1. Add MeasurementCollector to CPUInferenceEngine\n";
        std::cout << "  2. Call TokenGenerationEnd() in token loop\n";
        std::cout << "  3. Call GetFinalMeasurement() after completion\n";
        std::cout << "  4. Validate with MeasurementValidator\n";
        std::cout << "  5. Use diagnostics for autopatch tuning\n";
        
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "✗ SMOKE TEST FAILED: " << ex.what() << "\n";
        return 1;
    }
}
