// ============================================================================
// saturation_test_driver.cpp — Main Entry Point for Saturation Testing
// ============================================================================
//
// Usage: saturation_test_driver.exe [--output-dir d:\results] [--requests 100]
//
// Runs two sequential saturation tests:
//   1. Dual-lane baseline (Primary + Verifier only)
//   2. Tri-lane with Librarian enabled
//
// Compares latency overhead and generates verdict.
//
// ============================================================================
#include "swarm_saturation_test.hpp"
#include "swarm_orchestrator.h"
#include <iostream>
#include <cstring>
#include <chrono>

using namespace RawrXD;
using namespace RawrXD::test;

int main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(80, '#') << "\n"
              << "# RAWRXD SWARM SATURATION TEST DRIVER\n"
              << "#\n"
              << "# Purpose: Measure latency impact of Librarian (RAG) lane\n"
              << "# Builds: Dual-lane baseline → Tri-lane comparison\n"
              << "#\n"
              << std::string(80, '#') << "\n";

    // Parse command-line arguments
    SaturationTestConfig baselineConfig;
    SaturationTestConfig triLaneConfig;

    std::string outputDir = "d:\\saturation_test_results";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            outputDir = argv[i + 1];
            ++i;
        } else if (std::strcmp(argv[i], "--requests") == 0 && i + 1 < argc) {
            baselineConfig.numConcurrentRequests = std::strtoul(argv[i + 1], nullptr, 10);
            ++i;
        }
    }

    triLaneConfig = baselineConfig;
    triLaneConfig.enableLibrarian = true;

    baselineConfig.enableLibrarian = false;
    baselineConfig.outputDir = outputDir;
    triLaneConfig.outputDir = outputDir;

    std::cout << "\n✓ Configuration:\n"
              << "  Output Dir:      " << outputDir << "\n"
              << "  Test Requests:   " << baselineConfig.numConcurrentRequests << "\n"
              << "  Tokens/Request:  " << baselineConfig.tokensPerRequest << "\n"
              << "  Librarian Policy: dispatch every " << triLaneConfig.librarianDispatchEveryTokens
              << " tokens, max pending: " << triLaneConfig.librarianMaxPendingLookups << "\n\n";

    // Initialize SwarmOrchestrator
    std::cout << "🔧 Initializing SwarmOrchestrator...\n";
    SwarmOrchestrator orchestra(8);  // 8 worker threads

    SwarmSaturationTester tester(&orchestra);

    // Run BASELINE (dual-lane)
    std::cout << "\n" << std::string(80, '#') << "\n";
    std::cout << "# PHASE 1: DUAL-LANE BASELINE (No Librarian)\n";
    std::cout << std::string(80, '#') << "\n";

    auto baselineResult = tester.runSaturationTest(baselineConfig);

    // Export baseline results
    SwarmSaturationTester::exportResults(baselineResult, outputDir);
    std::cout << SwarmSaturationTester::generateReport(baselineResult);

    // Small cooldown
    std::cout << "\n⏳ Cooling down (2 seconds)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Run TRI-LANE with LIBRARIAN
    std::cout << "\n" << std::string(80, '#') << "\n";
    std::cout << "# PHASE 2: TRI-LANE WITH LIBRARIAN (Enabled)\n";
    std::cout << std::string(80, '#') << "\n";

    auto triLaneResult = tester.runSaturationTest(triLaneConfig);

    // Export tri-lane results
    SwarmSaturationTester::exportResults(triLaneResult, outputDir);
    std::cout << SwarmSaturationTester::generateReport(triLaneResult);

    // Compare
    std::cout << "\n" << std::string(80, '#') << "\n";
    std::cout << "# COMPARISON: TRI-LANE vs. BASELINE\n";
    std::cout << std::string(80, '#') << "\n";

    SaturationTestResult comparison;
    SwarmSaturationTester::compareResults(baselineResult, triLaneResult, comparison);

    std::cout << "\n📊 Overhead Analysis:\n"
              << "  Baseline P99 TTFT:        " << std::fixed << std::setprecision(2) << baselineResult.p99_ms << " ms\n"
              << "  Tri-Lane P99 TTFT:        " << std::fixed << std::setprecision(2) << triLaneResult.p99_ms << " ms\n"
              << "  P99 Overhead:             " << std::fixed << std::setprecision(2) 
              << (triLaneResult.p99_ms - baselineResult.p99_ms) << " ms ("
              << std::fixed << std::setprecision(1) 
              << (((triLaneResult.p99_ms - baselineResult.p99_ms) / baselineResult.p99_ms) * 100.0)
              << "%)\n\n";

    std::cout << "  Baseline Mean TTFT:       " << std::fixed << std::setprecision(2) << baselineResult.meanTtft_ms << " ms\n"
              << "  Tri-Lane Mean TTFT:       " << std::fixed << std::setprecision(2) << triLaneResult.meanTtft_ms << " ms\n"
              << "  Mean Overhead:            " << std::fixed << std::setprecision(2) 
              << (triLaneResult.meanTtft_ms - baselineResult.meanTtft_ms) << " ms ("
              << std::fixed << std::setprecision(1) << comparison.overheadPercent << "%)\n\n";

    // Final verdict
    std::cout << "\n" << std::string(80, '-') << "\n";
    std::cout << "🎯 EXECUTIVE VERDICT:\n\n";

    double p99Overhead_ms = triLaneResult.p99_ms - baselineResult.p99_ms;

    if (triLaneResult.p99_ms < 150.0 && p99Overhead_ms < 15.0) {
        std::cout << "✅ VERDICT: RAG INTEGRATION IS SAFE\n\n"
                  << "The Librarian thread adds only " << std::fixed << std::setprecision(1) << p99Overhead_ms
                  << "ms overhead at P99.\n"
                  << "→ RECOMMENDATION: Implement full LLM-based re-ranker on Librarian lane.\n"
                  << "→ Config: Dispatch every 32 tokens, min 1KB pulse, max 8 pending lookups.\n"
                  << "→ Expected RAG latency budget: ~20-50ms per vector search (fits in interval).\n";
    } else if (triLaneResult.p99_ms < 200.0 && p99Overhead_ms < 40.0) {
        std::cout << "⚠️  VERDICT: PROCEED WITH CAUTION\n\n"
                  << "The Librarian thread adds " << std::fixed << std::setprecision(1) << p99Overhead_ms
                  << "ms overhead at P99.\n"
                  << "→ RECOMMENDATION: Use lightweight BM25/Vector search only (no LLM re-ranker).\n"
                  << "→ Optimization: Reduce dispatchEveryTokens from 32 to 64.\n"
                  << "→ Alternative: Implement RAG in async post-processing lane (not real-time).\n";
    } else {
        std::cout << "❌ VERDICT: LIBRARIAN DISABLED (SYSTEM SATURATED)\n\n"
                  << "The trio-lane adds " << std::fixed << std::setprecision(1) << p99Overhead_ms
                  << "ms overhead at P99 (P99 = " << triLaneResult.p99_ms << "ms).\n"
                  << "→ RECOMMENDATION: Defer RAG integration until infrastructure scales.\n"
                  << "→ Next steps:\n"
                  << "   1. Increase VRAM (16GB → 24GB or more)\n"
                  << "   2. Use KV-cache quantization (4-bit) on Verifier lane\n"
                  << "   3. Implement Librarian as offline batch process\n";
    }

    std::cout << "\n" << std::string(80, '#') << "\n"
              << "# TEST COMPLETE\n"
              << std::string(80, '#') << "\n\n";

    return 0;
}
