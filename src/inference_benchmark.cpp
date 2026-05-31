#include "inference_benchmark.h"
#include "gpu/speculative_inference_bridge.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <numeric>
#include <limits>
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

namespace RawrXD {

InferenceBenchmark::InferenceBenchmark()
    : m_backendSelector(std::make_unique<BackendSelector>()) {
}

std::vector<BenchmarkResult> InferenceBenchmark::runBenchmarkSuite(const BenchmarkConfig& config) {
    std::vector<BenchmarkResult> allResults;

    for (const auto& modelPath : config.modelPaths) {
        for (const auto& testPrompt : config.testPrompts) {
            for (int maxTokens : config.maxTokensList) {
                double bestBaselineTps = 0.0;

                // Get available backends
                auto availableBackends = m_backendSelector->detectAvailableBackends();

                for (const auto& backendInfo : availableBackends) {
                    if (!backendInfo.available) continue;

                    try {
                        std::cout << "Benchmarking " << backendInfo.name
                                  << " with model " << modelPath
                                  << " (max_tokens=" << maxTokens << ")" << std::endl;

                        auto result = benchmarkBackend(backendInfo.type, modelPath,
                                                     testPrompt, maxTokens,
                                                     config.benchmarkRuns);

                        if (config.enableMemoryTracking) {
                            // Track peak memory during inference
                            PROCESS_MEMORY_COUNTERS pmc;
                            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                                result.peakMemoryMB = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
                            }
                        }

                        bestBaselineTps = std::max(bestBaselineTps, result.tokensPerSec);
                        allResults.push_back(result);

                    } catch (const std::exception& e) {
                        std::cerr << "Benchmark failed for " << backendInfo.name
                                  << ": " << e.what() << std::endl;

                        BenchmarkResult failedResult;
                        failedResult.backend = backendInfo.type;
                        failedResult.backendName = backendInfo.name;
                        failedResult.modelPath = modelPath;
                        failedResult.testPrompt = testPrompt;
                        failedResult.maxTokens = maxTokens;
                        failedResult.acceptanceRate = 0.0;
                        failedResult.speedupVsBaseline = 0.0;
                        failedResult.speculativeMode = false;
                        failedResult.success = false;
                        failedResult.errorMessage = e.what();
                        allResults.push_back(failedResult);
                    }
                }

                if (config.enableSpeculativeBenchmarks && !config.draftModelPath.empty()) {
                    auto speculative = benchmarkSpeculative(modelPath,
                                                            config.draftModelPath,
                                                            testPrompt,
                                                            maxTokens,
                                                            config.benchmarkRuns,
                                                            config);
                    if (speculative.success && bestBaselineTps > 0.0) {
                        speculative.speedupVsBaseline = speculative.tokensPerSec / bestBaselineTps;
                    }
                    allResults.push_back(std::move(speculative));
                }
            }
        }
    }

    return allResults;
}

BenchmarkResult InferenceBenchmark::benchmarkBackend(BackendType backend,
                                                   const std::string& modelPath,
                                                   const std::string& testPrompt,
                                                   int maxTokens,
                                                   int numRuns) {
    BenchmarkResult result;
    result.backend = backend;
    result.backendName = m_backendSelector->getBackendInfo(backend).name;
    result.modelPath = modelPath;
    result.draftModelPath.clear();
    result.testPrompt = testPrompt;
    result.maxTokens = maxTokens;
    result.acceptanceRate = 0.0;
    result.speedupVsBaseline = 1.0;
    result.speculativeMode = false;

    try {
        auto engine = m_backendSelector->createInferenceEngine(backend);

        if (!engine) {
            throw std::runtime_error("Failed to create inference engine");
        }

        // Warmup
        warmupEngine(engine, modelPath, testPrompt, 3);

        // Run benchmarks
        auto latencies = measureLatencies(engine, engine->Tokenize(testPrompt), maxTokens, numRuns);

        if (latencies.empty()) {
            throw std::runtime_error("No latency measurements collected");
        }

        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        const size_t p50 = latencies.size() / 2;
        const size_t p95 = std::min(latencies.size() - 1,
            static_cast<size_t>(latencies.size() * 0.95));
        const size_t p99 = std::min(latencies.size() - 1,
            static_cast<size_t>(latencies.size() * 0.99));
        result.latencyP50Ms = latencies[p50];
        result.latencyP95Ms = latencies[p95];
        result.latencyP99Ms = latencies[p99];

        double totalLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        result.totalTimeMs = totalLatency / numRuns;

        // Calculate tokens per second
        double avgLatencySec = result.totalTimeMs / 1000.0;
        result.tokensPerSec = maxTokens / avgLatencySec;

        // Memory usage
        result.memoryUsageBytes = measureMemoryUsage(engine);

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

BenchmarkResult InferenceBenchmark::benchmarkSpeculative(const std::string& modelPath,
                                                       const std::string& draftModelPath,
                                                       const std::string& testPrompt,
                                                       int maxTokens,
                                                       int numRuns,
                                                       const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.backend = BackendType::CPU;
    result.backendName = "SpeculativeDraftVerify";
    result.modelPath = modelPath;
    result.draftModelPath = draftModelPath;
    result.testPrompt = testPrompt;
    result.maxTokens = maxTokens;
    result.acceptanceRate = 0.0;
    result.speedupVsBaseline = 1.0;
    result.speculativeMode = true;

    try {
        RawrXD::Speculative::SpeculativeInferenceBridge bridge;
        RawrXD::Speculative::SpeculationConfig speculativeCfg;
        speculativeCfg.maxDraftTokens = std::max(1, config.speculativeDraftTokens);
        speculativeCfg.minDraftTokens = 1;
        speculativeCfg.acceptanceThreshold = config.speculativeAcceptanceThreshold;
        speculativeCfg.adaptiveDraftLen = true;

        bridge.setDraftModel(draftModelPath);
        bridge.setTargetModel(modelPath);
        bridge.configure(speculativeCfg);

        for (int i = 0; i < config.warmupRuns; ++i) {
            (void)bridge.generateFromText(testPrompt, std::min(maxTokens, 8));
        }

        auto latencies = measureSpeculativeLatencies(modelPath,
                                                    draftModelPath,
                                                    testPrompt,
                                                    maxTokens,
                                                    numRuns,
                                                    result.acceptanceRate,
                                                    config);
        if (latencies.empty()) {
            throw std::runtime_error("No speculative latency measurements collected");
        }

        std::sort(latencies.begin(), latencies.end());
        const size_t p50 = latencies.size() / 2;
        const size_t p95 = std::min(latencies.size() - 1,
            static_cast<size_t>(latencies.size() * 0.95));
        const size_t p99 = std::min(latencies.size() - 1,
            static_cast<size_t>(latencies.size() * 0.99));
        result.latencyP50Ms = latencies[p50];
        result.latencyP95Ms = latencies[p95];
        result.latencyP99Ms = latencies[p99];

        double totalLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        result.totalTimeMs = totalLatency / latencies.size();
        result.tokensPerSec = maxTokens / (result.totalTimeMs / 1000.0);
        result.memoryUsageBytes = measureMemoryUsage(std::unique_ptr<InferenceEngine>());
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

std::string InferenceBenchmark::generateComparisonReport(const std::vector<BenchmarkResult>& results) {
    std::ostringstream report;

    report << "=== Inference Backend Benchmark Report ===\n\n";

    // Group by model
    std::unordered_map<std::string, std::vector<BenchmarkResult>> modelGroups;
    for (const auto& result : results) {
        modelGroups[result.modelPath].push_back(result);
    }

    for (const auto& [modelPath, modelResults] : modelGroups) {
        report << "Model: " << modelPath << "\n";
        report << "----------------------------------------\n";

        // Sort by tokens per second
        auto sortedResults = modelResults;
        std::sort(sortedResults.begin(), sortedResults.end(),
                 [](const BenchmarkResult& a, const BenchmarkResult& b) {
                     return a.tokensPerSec > b.tokensPerSec;
                 });

        for (const auto& result : sortedResults) {
            if (!result.success) {
                report << result.backendName << ": FAILED - " << result.errorMessage << "\n";
                continue;
            }

            report << result.backendName << ":\n";
            if (result.speculativeMode) {
                report << "  Mode: Speculative draft+verify\n";
                report << "  Draft model: " << result.draftModelPath << "\n";
                report << "  Acceptance rate: " << (result.acceptanceRate * 100.0) << "%\n";
                report << "  Speedup vs best baseline: " << result.speedupVsBaseline << "x\n";
            }
            report << "  Tokens/sec: " << result.tokensPerSec << "\n";
            report << "  Latency P50: " << result.latencyP50Ms << "ms\n";
            report << "  Latency P95: " << result.latencyP95Ms << "ms\n";
            report << "  Latency P99: " << result.latencyP99Ms << "ms\n";
            report << "  Memory: " << (result.memoryUsageBytes / 1024.0 / 1024.0) << " MB\n";
            report << "\n";
        }
        report << "\n";
    }

    return report.str();
}

BackendType InferenceBenchmark::getRecommendedBackend(const std::string& modelPath,
                                                     const std::vector<BenchmarkResult>& results) {
    // Find results for this model
    std::vector<BenchmarkResult> modelResults;
    for (const auto& result : results) {
        if (result.modelPath == modelPath && result.success) {
            modelResults.push_back(result);
        }
    }

    if (modelResults.empty()) {
        return BackendType::CPU; // Fallback
    }

    // Find backend with best performance (tokens/sec)
    auto bestResult = std::max_element(modelResults.begin(), modelResults.end(),
                                     [](const BenchmarkResult& a, const BenchmarkResult& b) {
                                         return a.tokensPerSec < b.tokensPerSec;
                                     });

    return bestResult->backend;
}

// Private implementation methods

BenchmarkResult InferenceBenchmark::runSingleBenchmark(BackendType backend,
                                                     const std::string& modelPath,
                                                     const std::string& testPrompt,
                                                     int maxTokens) {
    return benchmarkBackend(backend, modelPath, testPrompt, maxTokens, 1);
}

std::vector<double> InferenceBenchmark::measureLatencies(std::unique_ptr<InferenceEngine>& engine,
                                                       const std::vector<int32_t>& inputTokens,
                                                       int maxTokens,
                                                       int numRuns) {
    std::vector<double> latencies;

    for (int i = 0; i < numRuns; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        try {
            auto result = engine->Generate(inputTokens, maxTokens);
            (void)result; // Suppress unused variable warning
        } catch (const std::exception&) {
            // Skip failed runs
            continue;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(latencyMs);
    }

    return latencies;
}

std::vector<double> InferenceBenchmark::measureSpeculativeLatencies(const std::string& modelPath,
                                                                  const std::string& draftModelPath,
                                                                  const std::string& testPrompt,
                                                                  int maxTokens,
                                                                  int numRuns,
                                                                  double& acceptanceRate,
                                                                  const BenchmarkConfig& config) {
    std::vector<double> latencies;
    double acceptanceAccumulator = 0.0;
    int successfulRuns = 0;

    for (int i = 0; i < numRuns; ++i) {
        RawrXD::Speculative::SpeculativeInferenceBridge bridge;
        RawrXD::Speculative::SpeculationConfig speculativeCfg;
        speculativeCfg.maxDraftTokens = std::max(1, config.speculativeDraftTokens);
        speculativeCfg.minDraftTokens = 1;
        speculativeCfg.acceptanceThreshold = config.speculativeAcceptanceThreshold;
        speculativeCfg.adaptiveDraftLen = true;

        bridge.setDraftModel(draftModelPath);
        bridge.setTargetModel(modelPath);
        bridge.configure(speculativeCfg);

        auto start = std::chrono::high_resolution_clock::now();
        auto result = bridge.generateFromText(testPrompt, maxTokens);
        auto end = std::chrono::high_resolution_clock::now();

        if (!result.success || result.tokens.empty()) {
            continue;
        }

        latencies.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        acceptanceAccumulator += result.stats.acceptanceRate;
        ++successfulRuns;
    }

    acceptanceRate = successfulRuns > 0
        ? (acceptanceAccumulator / successfulRuns)
        : 0.0;
    return latencies;
}

size_t InferenceBenchmark::measureMemoryUsage(const std::unique_ptr<InferenceEngine>& engine) {
    (void)engine;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

void InferenceBenchmark::warmupEngine(std::unique_ptr<InferenceEngine>& engine,
                                    const std::string& modelPath,
                                    const std::string& testPrompt,
                                    int warmupRuns) {
    if (!engine->LoadModel(modelPath)) {
        throw std::runtime_error("Failed to load model for warmup");
    }

    auto tokens = engine->Tokenize(testPrompt);

    for (int i = 0; i < warmupRuns; ++i) {
        try {
            engine->Generate(tokens, 5); // Short generation for warmup
        } catch (const std::exception& e) {
            fprintf(stderr, "[InferenceBenchmark] Warmup failed: %s\n", e.what());
        }
    }
}

} // namespace RawrXD
