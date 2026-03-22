#include "inference_benchmark.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <numeric>
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
                            // Additional memory measurement could be added here
                        }

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
                        failedResult.success = false;
                        failedResult.errorMessage = e.what();
                        allResults.push_back(failedResult);
                    }
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
    result.testPrompt = testPrompt;
    result.maxTokens = maxTokens;

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
        result.latencyP50Ms = latencies[latencies.size() / 2];
        result.latencyP95Ms = latencies[static_cast<size_t>(latencies.size() * 0.95)];
        result.latencyP99Ms = latencies[static_cast<size_t>(latencies.size() * 0.99)];

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

size_t InferenceBenchmark::measureMemoryUsage(std::unique_ptr<InferenceEngine>& engine) {
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
        } catch (const std::exception&) {
            // Ignore warmup failures
        }
    }
}

} // namespace RawrXD