// ============================================================================
// Main Entry Point: Unbounded Capacity Smoke Test
// Usage: slash_command_smoke_test [--model PATH] [--tier smoke|quick|standard|deep|stress|soak]
//                                 [--parallel] [--report PATH] [--auto-detect]
// ============================================================================

#include "smoke_test_orchestrator.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <string>
#include <filesystem>

using namespace RawrXD::Test;

void PrintUsage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --model PATH       Path to GGUF model file\n"
              << "  --tier TIER        Test depth: smoke, quick, standard, deep, stress, soak\n"
              << "  --parallel N       Test N models concurrently\n"
              << "  --report PATH      Save JSON/Markdown report to PATH\n"
              << "  --auto-detect      Auto-detect GGUF models in current directory\n"
              << "  --simulate         Run shard simulation for models exceeding hardware\n"
              << "  --verbose          Enable verbose output\n"
              << "  --help             Show this help\n"
              << "\n"
              << "Examples:\n"
              << "  " << argv0 << " --model d:\\phi3mini.gguf\n"
              << "  " << argv0 << " --model d:\\codestral22b.gguf --tier quick\n"
              << "  " << argv0 << " --auto-detect --parallel 2\n";
}

int main(int argc, char* argv[]) {
    SmokeTestOrchestrator::Configuration config;
    config.outputDir = ".";
    config.generateReport = true;
    
    TestDepth forcedDepth = TestDepth::Smoke;
    bool depthForced = false;
    bool verbose = false;
    bool simulate = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--model" && i + 1 < argc) {
            config.modelPaths.push_back(argv[++i]);
        }
        else if (arg == "--tier" && i + 1 < argc) {
            std::string tier = argv[++i];
            if (tier == "smoke") forcedDepth = TestDepth::Smoke;
            else if (tier == "quick") forcedDepth = TestDepth::Quick;
            else if (tier == "standard") forcedDepth = TestDepth::Standard;
            else if (tier == "deep") forcedDepth = TestDepth::Deep;
            else if (tier == "stress") forcedDepth = TestDepth::Stress;
            else if (tier == "soak") forcedDepth = TestDepth::Soak;
            else {
                std::cerr << "Unknown tier: " << tier << std::endl;
                return 1;
            }
            depthForced = true;
        }
        else if (arg == "--parallel" && i + 1 < argc) {
            config.parallelModelTesting = true;
            config.maxConcurrentModels = std::atoi(argv[++i]);
        }
        else if (arg == "--report" && i + 1 < argc) {
            config.outputDir = argv[++i];
        }
        else if (arg == "--auto-detect") {
            config.autoDetectModels = true;
        }
        else if (arg == "--simulate") {
            simulate = true;
        }
        else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        }
        else if (arg.starts_with("--model=")) {
            config.modelPaths.push_back(arg.substr(8));
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }
    
    // Default model paths if none specified
    if (config.modelPaths.empty() && !config.autoDetectModels) {
        // Try common locations
        std::vector<std::string> defaults = {
            "d:\\phi3mini.gguf",
            "d:\\codestral22b.gguf",
            "d:\\ministral3.gguf",
        };
        
        for (const auto& path : defaults) {
            if (std::filesystem::exists(path)) {
                config.modelPaths.push_back(path);
                std::cout << "Auto-selected model: " << path << std::endl;
                break;
            }
        }
    }
    
    if (config.modelPaths.empty() && !config.autoDetectModels) {
        std::cerr << "No models found! Use --model PATH or --auto-detect" << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    
    // Run tests
    SmokeTestOrchestrator orchestrator(config);
    
    bool success = orchestrator.RunAllTests();
    
    // Print summary
    auto results = orchestrator.GetResults();
    
    uint32_t passed = 0, failed = 0, degraded = 0;
    for (const auto& r : results) {
        if (r.passed) passed++;
        else failed++;
        if (r.wasDegraded) degraded++;
    }
    
    std::cout << "\n=== Final Summary ===" << std::endl;
    std::cout << "Total tests: " << results.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    if (degraded > 0) {
        std::cout << "Degraded (simulated): " << degraded << std::endl;
    }
    std::cout << "Success rate: " << (passed * 100.0 / results.size()) << "%" << std::endl;
    
    return success ? 0 : 1;
}
