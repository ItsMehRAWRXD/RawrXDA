// smoketest_main.cpp - Main entry point for comprehensive smoketest suite
// Runs all 7 tests and generates comprehensive report
//
// Usage: smoketest [--verbose] [--report path] [--timeout seconds]
//
// Part of the Copilot-like inference pipeline.

#include "smoketest_harness.h"
#include <iostream>
#include <string>
#include <cstdlib>

using namespace RawrXD;

void PrintUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --verbose          Enable verbose output\n";
    std::cout << "  --report path      Export report to file\n";
    std::cout << "  --timeout seconds  Test timeout (default: 30)\n";
    std::cout << "  --help             Show this help\n";
}

int main(int argc, char* argv[]) {
    SmoketestConfig config;
    config.verbose = true;
    config.generate_report = true;
    config.report_path = "smoketest_report.html";
    config.test_timeout = std::chrono::seconds(30);
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (std::strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            config.report_path = argv[++i];
        } else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            config.test_timeout = std::chrono::seconds(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           RawrXD Production Pipeline Smoketest Suite             ║\n";
    std::cout << "║                    Validation Framework v1.0                     ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    std::cout << "Configuration:\n";
    std::cout << "  Verbose: " << (config.verbose ? "yes" : "no") << "\n";
    std::cout << "  Report: " << config.report_path << "\n";
    std::cout << "  Timeout: " << config.test_timeout.count() << " seconds\n";
    std::cout << "  Warmup tokens: " << config.warmup_tokens << "\n";
    std::cout << "  Measurement tokens: " << config.measurement_tokens << "\n";
    std::cout << "  Long session tokens: " << config.long_session_tokens << "\n";
    std::cout << "  Concurrent requests: " << config.concurrent_requests << "\n";
    std::cout << "\n";
    
    // Initialize harness
    SmoketestHarness harness;
    if (!harness.Initialize(config)) {
        std::cerr << "Failed to initialize smoketest harness\n";
        return 1;
    }
    
    // Run all tests
    auto results = harness.RunAllTests();
    
    // Print final summary
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        FINAL RESULTS                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    std::cout << "Total tests: " << results.size() << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Success rate: " << (static_cast<float>(passed) / results.size() * 100.0f) << "%\n";
    std::cout << "\n";
    
    if (failed == 0) {
        std::cout << "✅ ALL TESTS PASSED - System is stable, predictive, and fair\n";
        std::cout << "✅ Production pipeline ready for deployment\n";
    } else {
        std::cout << "⚠️  SOME TESTS FAILED - Review report for details\n";
        std::cout << "⚠️  System may have stability issues under pressure\n";
    }
    
    std::cout << "\n";
    std::cout << "Report exported to: " << config.report_path << "\n";
    std::cout << "\n";
    
    return failed > 0 ? 1 : 0;
}