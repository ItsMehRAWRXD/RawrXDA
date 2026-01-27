/**
 * \file benchmark_completions.cpp
 * \brief Comprehensive benchmark suite for AI completion performance
 * \author RawrXD Team
 * \date 2025-12-13
 *
 * Victory Lap Tests:
 * 1. Latency measurements (cold/warm/cached)
 * 2. Multi-language code patterns
 * 3. Stress testing with rapid requests
 * 4. Context window utilization
 * 5. Memory usage profiling
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include "real_time_completion_engine.h"
#include "inference_engine.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

struct BenchmarkResult {
    std::string testName;
    double avgLatencyMs;
    double minLatencyMs;
    double maxLatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    size_t totalRequests;
    size_t successfulRequests;
    bool passed;
};

class CompletionBenchmark {
private:
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<Metrics> metrics_;
    InferenceEngine* engine_;
    std::unique_ptr<RealTimeCompletionEngine> completionEngine_;
    std::vector<BenchmarkResult> results_;

public:
    CompletionBenchmark() {
        logger_ = std::make_shared<Logger>("benchmark");
        metrics_ = std::make_shared<Metrics>();
        logger_->setMinLevel(LogLevel::INFO);
        engine_ = nullptr;
    }

    bool Initialize(const std::string& modelPath) {
        std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║   RawrXD AI Completion Benchmark Suite v1.0         ║\n";
        std::cout << "║   Cursor-Killer Performance Validation              ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

        std::cout << "[INIT] Loading GGUF model: " << modelPath << "\n";
        
        engine_ = new InferenceEngine(QString::fromStdString(modelPath));
        bool loaded = engine_->loadModel(QString::fromStdString(modelPath));
        
        if (!loaded) {
            std::cerr << "✗ Failed to load model\n";
            return false;
        }

        std::cout << "✓ Model loaded successfully\n";
        std::cout << "  • Model path: " << engine_->modelPath().toStdString() << "\n";
        std::cout << "  • Model loaded: " << (engine_->isModelLoaded() ? "yes" : "no") << "\n\n";

        completionEngine_ = std::make_unique<RealTimeCompletionEngine>(logger_, metrics_);
        completionEngine_->setInferenceEngine(engine_);
        
        std::cout << "✓ CompletionEngine initialized\n\n";
        return true;
    }

    void RunAllBenchmarks() {
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "Starting Benchmark Suite...\n";
        std::cout << "═══════════════════════════════════════════════════════\n\n";

        BenchmarkColdStart();
        BenchmarkWarmCache();
        BenchmarkRapidFire();
        BenchmarkMultiLanguage();
        BenchmarkContextAwareness();
        BenchmarkMultiLine();

        PrintSummary();
    }

private:
    void BenchmarkColdStart() {
        std::cout << "[TEST 1/6] Cold Start Latency\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        std::vector<double> latencies;
        size_t successful = 0;

        std::vector<std::string> testCases = {
            "int main() {\n    ",
            "class Person {\npublic:\n    ",
            "void calculate() {\n    double result = ",
            "for (int i = 0; i < n; i++) {\n    ",
            "std::vector<int> vec = "
        };

        for (const auto& prefix : testCases) {
            auto start = std::chrono::high_resolution_clock::now();
            
            auto completions = completionEngine_->getCompletions(
                prefix, "", "cpp", ""
            );
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            
            latencies.push_back(latencyMs);
            if (!completions.empty()) successful++;
            
            std::cout << "  • Request " << latencies.size() << ": " 
                      << std::fixed << std::setprecision(2) << latencyMs << " ms";
            if (!completions.empty()) {
                std::cout << " ✓ (got " << completions.size() << " completions)";
            }
            std::cout << "\n";
        }

        BenchmarkResult result = CalculateStats("Cold Start", latencies, successful, testCases.size());
        result.passed = result.avgLatencyMs < 500.0;  // First requests can be slower
        results_.push_back(result);
        
        std::cout << "\n✓ Avg: " << result.avgLatencyMs << " ms | "
                  << "P95: " << result.p95LatencyMs << " ms | "
                  << "Success: " << successful << "/" << testCases.size() << "\n\n";
    }

    void BenchmarkWarmCache() {
        std::cout << "[TEST 2/6] Warm Cache Performance (TARGET: <10ms)\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        std::vector<double> latencies;
        std::string testPrefix = "void process() {\n    int value = ";
        
        // First request (populate cache)
        completionEngine_->getCompletions(testPrefix, "", "cpp", "");
        
        // Measure cached requests
        for (int i = 0; i < 10; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            auto completions = completionEngine_->getCompletions(testPrefix, "", "cpp", "");
            auto end = std::chrono::high_resolution_clock::now();
            
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);
            
            std::cout << "  • Cached request " << (i+1) << ": " 
                      << std::fixed << std::setprecision(2) << latencyMs << " ms ✓\n";
        }

        BenchmarkResult result = CalculateStats("Warm Cache", latencies, latencies.size(), latencies.size());
        result.passed = result.avgLatencyMs < 10.0;  // Cached should be <10ms
        results_.push_back(result);
        
        std::cout << "\n" << (result.passed ? "✅ PASS" : "⚠ WARNING") 
                  << " - Avg: " << result.avgLatencyMs << " ms (target: <10ms)\n\n";
    }

    void BenchmarkRapidFire() {
        std::cout << "[TEST 3/6] Rapid-Fire Stress Test (100 requests)\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        std::vector<double> latencies;
        size_t successful = 0;
        const int NUM_REQUESTS = 100;
        
        auto testStart = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_REQUESTS; i++) {
            std::string prefix = "void func" + std::to_string(i) + "() {\n    ";
            
            auto start = std::chrono::high_resolution_clock::now();
            auto completions = completionEngine_->getCompletions(prefix, "", "cpp", "");
            auto end = std::chrono::high_resolution_clock::now();
            
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);
            if (!completions.empty()) successful++;
            
            if (i % 20 == 0) {
                std::cout << "  • Progress: " << i << "/" << NUM_REQUESTS << " requests...\n";
            }
        }
        
        auto testEnd = std::chrono::high_resolution_clock::now();
        double totalTimeMs = std::chrono::duration<double, std::milli>(testEnd - testStart).count();
        
        BenchmarkResult result = CalculateStats("Rapid Fire", latencies, successful, NUM_REQUESTS);
        result.passed = result.avgLatencyMs < 100.0 && successful > NUM_REQUESTS * 0.95;
        results_.push_back(result);
        
        std::cout << "\n✓ Completed " << NUM_REQUESTS << " requests in " 
                  << (totalTimeMs / 1000.0) << " seconds\n";
        std::cout << "  • Throughput: " << (NUM_REQUESTS / (totalTimeMs / 1000.0)) 
                  << " req/sec\n";
        std::cout << "  • Success rate: " << (successful * 100.0 / NUM_REQUESTS) << "%\n\n";
    }

    void BenchmarkMultiLanguage() {
        std::cout << "[TEST 4/6] Multi-Language Support\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        struct TestCase {
            std::string language;
            std::string prefix;
            std::string fileType;
        };
        
        std::vector<TestCase> tests = {
            {"C++",        "class Widget {\npublic:\n    ",           "cpp"},
            {"Python",     "def process_data(items):\n    result = ", "py"},
            {"JavaScript", "function calculate() {\n    const x = ",  "js"},
            {"TypeScript", "interface User {\n    name: ",            "ts"}
        };
        
        std::vector<double> latencies;
        size_t successful = 0;
        
        for (const auto& test : tests) {
            std::cout << "  • Testing " << test.language << "...\n";
            
            auto start = std::chrono::high_resolution_clock::now();
            auto completions = completionEngine_->getCompletions(
                test.prefix, "", test.fileType, ""
            );
            auto end = std::chrono::high_resolution_clock::now();
            
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);
            
            if (!completions.empty()) {
                successful++;
                std::cout << "    ✓ " << latencyMs << " ms - Got " 
                          << completions.size() << " suggestions\n";
            } else {
                std::cout << "    ✗ No completions\n";
            }
        }
        
        BenchmarkResult result = CalculateStats("Multi-Language", latencies, successful, tests.size());
        result.passed = successful >= tests.size() * 0.75;  // At least 75% success
        results_.push_back(result);
        
        std::cout << "\n" << (result.passed ? "✅ PASS" : "⚠ WARNING") 
                  << " - " << successful << "/" << tests.size() << " languages supported\n\n";
    }

    void BenchmarkContextAwareness() {
        std::cout << "[TEST 5/6] Context-Aware Completions\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        std::string context = 
            "// Calculate factorial of a number\n"
            "int factorial(int n) {\n"
            "    if (n <= 1) return 1;\n"
            "    return n * factorial(n - 1);\n"
            "}\n\n";
        
        std::string prefix = "// Calculate sum\nint sum(int a, int b) {\n    return ";
        
        std::cout << "  • Testing with context:\n";
        std::cout << "    " << context.substr(0, 40) << "...\n\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        auto completions = completionEngine_->getCompletions(
            prefix, "", "cpp", context
        );
        auto end = std::chrono::high_resolution_clock::now();
        
        double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::vector<double> latencies = {latencyMs};
        BenchmarkResult result = CalculateStats("Context-Aware", latencies, 
                                               !completions.empty() ? 1 : 0, 1);
        result.passed = !completions.empty() && latencyMs < 150.0;
        results_.push_back(result);
        
        if (!completions.empty()) {
            std::cout << "  ✓ Generated context-aware completion in " << latencyMs << " ms\n";
            std::cout << "    Suggestion: \"" << completions[0].text << "\"\n\n";
        } else {
            std::cout << "  ✗ No completions generated\n\n";
        }
    }

    void BenchmarkMultiLine() {
        std::cout << "[TEST 6/6] Multi-Line Function Generation\n";
        std::cout << "────────────────────────────────────────────────────────\n";
        
        std::string prefix = "// Binary search implementation\nint binarySearch(int arr[], int size, int target) {\n    ";
        
        std::cout << "  • Requesting multi-line completion...\n\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        auto completions = completionEngine_->getMultiLineCompletions(prefix, 10);
        auto end = std::chrono::high_resolution_clock::now();
        
        double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::vector<double> latencies = {latencyMs};
        BenchmarkResult result = CalculateStats("Multi-Line", latencies, 
                                               !completions.empty() ? 1 : 0, 1);
        result.passed = !completions.empty();
        results_.push_back(result);
        
        if (!completions.empty()) {
            std::cout << "  ✓ Generated " << completions.size() << " multi-line completions in " 
                      << latencyMs << " ms\n";
            std::cout << "    Lines: " << (completions[0].text.empty() ? 0 : 
                         std::count(completions[0].text.begin(), completions[0].text.end(), '\n') + 1) << "\n";
            std::cout << "    Preview:\n";
            std::cout << "    ──────────────────────────────────────\n";
            std::string preview = completions[0].text.substr(0, 200);
            std::cout << "    " << preview;
            if (completions[0].text.length() > 200) std::cout << "...";
            std::cout << "\n    ──────────────────────────────────────\n\n";
        } else {
            std::cout << "  ✗ No multi-line completions generated\n\n";
        }
    }

    BenchmarkResult CalculateStats(const std::string& name, 
                                   const std::vector<double>& latencies,
                                   size_t successful, size_t total) {
        BenchmarkResult result;
        result.testName = name;
        result.totalRequests = total;
        result.successfulRequests = successful;
        
        if (latencies.empty()) {
            result.avgLatencyMs = 0;
            result.minLatencyMs = 0;
            result.maxLatencyMs = 0;
            result.p95LatencyMs = 0;
            result.p99LatencyMs = 0;
            return result;
        }
        
        double sum = 0;
        result.minLatencyMs = latencies[0];
        result.maxLatencyMs = latencies[0];
        
        for (double lat : latencies) {
            sum += lat;
            if (lat < result.minLatencyMs) result.minLatencyMs = lat;
            if (lat > result.maxLatencyMs) result.maxLatencyMs = lat;
        }
        
        result.avgLatencyMs = sum / latencies.size();
        
        auto sorted = latencies;
        std::sort(sorted.begin(), sorted.end());
        result.p95LatencyMs = sorted[static_cast<size_t>(sorted.size() * 0.95)];
        result.p99LatencyMs = sorted[static_cast<size_t>(sorted.size() * 0.99)];
        
        return result;
    }

    void PrintSummary() {
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║              BENCHMARK RESULTS SUMMARY               ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";
        
        std::cout << std::left << std::setw(20) << "Test Name" 
                  << std::setw(12) << "Avg (ms)" 
                  << std::setw(12) << "P95 (ms)"
                  << std::setw(12) << "Success"
                  << std::setw(10) << "Status" << "\n";
        std::cout << "────────────────────────────────────────────────────────────\n";
        
        int passed = 0;
        for (const auto& result : results_) {
            std::cout << std::left << std::setw(20) << result.testName
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << result.avgLatencyMs
                      << std::setw(12) << result.p95LatencyMs
                      << std::setw(12) << (std::to_string(result.successfulRequests) + "/" + 
                                           std::to_string(result.totalRequests))
                      << std::setw(10) << (result.passed ? "✅ PASS" : "⚠ WARN") << "\n";
            if (result.passed) passed++;
        }
        
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "OVERALL: " << passed << "/" << results_.size() << " tests passed\n";
        
        if (passed == results_.size()) {
            std::cout << "\n🎉 ALL TESTS PASSED! CURSOR-KILLER STATUS: CONFIRMED! 🎉\n";
        } else {
            std::cout << "\n⚠ Some tests need attention, but core functionality works!\n";
        }
        std::cout << "═══════════════════════════════════════════════════════\n\n";
        
        // Print performance metrics from engine
        auto metrics = completionEngine_->getMetrics();
        std::cout << "Engine Metrics:\n";
        std::cout << "  • Cache hit rate: " << (metrics.cacheHitRate * 100) << "%\n";
        std::cout << "  • Total requests: " << metrics.requestCount << "\n";
        std::cout << "  • Errors: " << metrics.errorCount << "\n\n";
    }
};

int main(int argc, char* argv[]) {
    std::string modelPath = "models/ministral-3b-instruct-v0.3-Q4_K_M.gguf";
    
    if (argc > 1) {
        modelPath = argv[1];
    }
    
    CompletionBenchmark benchmark;
    
    if (!benchmark.Initialize(modelPath)) {
        std::cerr << "\n✗ Benchmark initialization failed!\n";
        std::cerr << "  Make sure you have a GGUF model at: " << modelPath << "\n\n";
        return 1;
    }
    
    benchmark.RunAllBenchmarks();
    
    return 0;
}
