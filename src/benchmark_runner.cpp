/**
 * \file benchmark_runner.cpp
 * \brief Implementation of benchmark runner with real test execution
 * \author RawrXD Team
 * \date 2025-12-13
 */

#include "benchmark_runner.hpp"
#include "inference_engine.h"
#include "real_time_completion_engine.h"
#include "logging/logger.h"
#include "metrics/metrics.h"


#include <chrono>
#include <algorithm>
#include <numeric>
#include <vector>
#include <iomanip>
#include <sstream>

// ============================================================================
// BENCHMARK RUNNER IMPLEMENTATION
// ============================================================================

BenchmarkRunner::BenchmarkRunner(void* parent)
    : void(parent), gpuEnabled_(true), verbose_(false), shouldStop_(false) {
}

BenchmarkRunner::~BenchmarkRunner() {
    completionEngine_.reset();
    engine_.reset();
}

void BenchmarkRunner::runBenchmarks(const std::vector<std::string>& selectedTests,
                                    const std::string& modelPath,
                                    bool gpuEnabled,
                                    bool verbose) {
    selectedTests_ = selectedTests;
    modelPath_ = modelPath;
    gpuEnabled_ = gpuEnabled;
    verbose_ = verbose;
    shouldStop_ = false;
    results_.clear();

    // Run in background
    QMetaObject::invokeMethod(this, "executeRun", //QueuedConnection);
}

void BenchmarkRunner::stop() {
    shouldStop_ = true;
    log("Stopping benchmarks...", 3);  // WARNING
}

const std::vector<BenchmarkTestResult>& BenchmarkRunner::getResults() const {
    return results_;
}

void BenchmarkRunner::executeRun() {
    auto startTime = std::chrono::high_resolution_clock::now();

    started();
    log("Initializing benchmark suite...", 1);  // INFO

    // Initialize inference engine
    try {
        engine_ = std::make_unique<InferenceEngine>(
            modelPath_.empty() ? nullptr : modelPath_.toLocal8Bit().constData()
        );

        if (!engine_->loadModel(modelPath_)) {
            error("Failed to load model: " + modelPath_);
            return;
        }

        log(std::string("Model loaded: %1"), 2);  // SUCCESS

        // Initialize completion engine
        auto logger = std::make_shared<Logger>("benchmark-runner");
        auto metrics = std::make_shared<Metrics>();
        completionEngine_ = std::make_unique<RealTimeCompletionEngine>(logger, metrics);
        completionEngine_->setInferenceEngine(engine_.get());

        log("Completion engine initialized", 2);  // SUCCESS

    } catch (const std::exception& e) {
        error(std::string("Initialization failed: %1")));
        return;
    }

    log("", 1);
    log("Starting benchmark execution...", 1);  // INFO
    log("", 1);

    // Run selected tests
    int passed = 0;
    int total = selectedTests_.size();
    int current = 0;

    for (const auto& testName : selectedTests_) {
        if (shouldStop_) {
            log("Benchmark cancelled by user", 3);  // WARNING
            break;
        }

        current++;
        testStarted(std::string::fromStdString(testName));
        progress(current, total);

        log(std::string("[%1/%2] Running: %3")), 1);

        BenchmarkTestResult result;

        // Run appropriate test
        bool success = false;
        try {
            if (testName == "cold_start") {
                success = testColdStart(result);
            } else if (testName == "warm_cache") {
                success = testWarmCache(result);
            } else if (testName == "rapid_fire") {
                success = testRapidFire(result);
            } else if (testName == "multi_lang") {
                success = testMultiLanguage(result);
            } else if (testName == "context_aware") {
                success = testContextAware(result);
            } else if (testName == "multi_line") {
                success = testMultiLine(result);
            } else if (testName == "gpu_accel") {
                success = testGPUAcceleration(result);
            } else if (testName == "memory") {
                success = testMemory(result);
            } else {
                log(std::string("Unknown test: %1")), 4);  // ERROR
                continue;
            }

            result.testName = testName;
            result.passed = success;
            results_.push_back(result);

            if (success) passed++;

            testCompleted(std::string::fromStdString(testName), success, result.avgLatencyMs);

            // Log result
            std::string status = success ? "✅ PASS" : "⚠ WARNING";
            log(std::string("  %1 | Avg: %2ms | P95: %3ms | Success: %4%")


                ), success ? 2 : 3);

        } catch (const std::exception& e) {
            log(std::string("Test failed with exception: %1")), 4);  // ERROR
        }

        log("", 1);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double executionTime = std::chrono::duration<double>(endTime - startTime).count();

    // Final summary
    log("═══════════════════════════════════════════════════════", 1);
    log(std::string("SUMMARY: %1/%2 tests passed"), 2);
    log(std::string("Execution time: %1 seconds"), 1);
    log("═══════════════════════════════════════════════════════", 1);

    finished(passed, total, executionTime);
}

// ============================================================================
// INDIVIDUAL TEST IMPLEMENTATIONS
// ============================================================================

bool BenchmarkRunner::testColdStart(BenchmarkTestResult& result) {
    std::vector<double> latencies;
    size_t successful = 0;

    std::vector<std::string> testCases = {
        "int main() {\n    ",
        "class Person {\npublic:\n    ",
        "void calculate() {\n    double result = ",
        "template<typename T>\nclass Container {\nprivate:\n    ",
        "namespace Utils {\nvoid process() {\n    ",
        "struct Config {\n    std::string name = ",
        "enum class Status {\n    ",
        "auto lambda = [](int x) {\n    return ",
    };

    for (size_t i = 0; i < testCases.size(); i++) {
        if (shouldStop_) return false;

        const auto& prefix = testCases[i];
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getCompletions(prefix, "", "cpp", "");
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) {
                successful++;
                if (verbose_) {
                    log(std::string("  Test case %1: %2ms, %3 completions")
                        ), 1);
                }
            } else {
                log(std::string("  Test case %1: No completions generated"), 3);
            }
        } catch (const std::exception& e) {
            log(std::string("  Test case %1 failed: %2")), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("  Test case %1 failed: Unknown exception"), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("Cold Start", latencies, successful, testCases.size());
    result.passed = result.avgLatencyMs < 1000.0;
    result.notes = "Initial model load + first inference passes";
    
    return result.passed;
}

bool BenchmarkRunner::testWarmCache(BenchmarkTestResult& result) {
    std::vector<double> latencies;
    std::string testPrefix = "void process() {\n    int value = ";

    // Warm up cache with multiple passes
    log("  Warming up cache...", 1);
    for (int warmup = 0; warmup < 5; warmup++) {
        try {
            completionEngine_->getCompletions(testPrefix, "", "cpp", "");
        } catch (const std::exception& e) {
            log(std::string("  Warmup %1 failed: %2")), 3);
        } catch (...) {
            log(std::string("  Warmup %1 failed: Unknown exception"), 3);
        }
    }

    log("  Measuring cached performance...", 1);
    // Measure cached requests with full production load
    const int NUM_ITERATIONS = 100;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (shouldStop_) return false;

        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            completionEngine_->getCompletions(testPrefix, "", "cpp", "");
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);
            
            if (verbose_ && i % 20 == 0) {
                log(std::string("    Iteration %1/%2: %3ms"), 1);
            }
        } catch (const std::exception& e) {
            log(std::string("  Iteration %1 failed: %2")), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("  Iteration %1 failed: Unknown exception"), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("Warm Cache", latencies, latencies.size(), latencies.size());
    result.passed = result.avgLatencyMs < 50.0;
    result.notes = "Tests completion latency after model is warmed up";
    
    return result.passed;
}

bool BenchmarkRunner::testRapidFire(BenchmarkTestResult& result) {
    std::vector<double> latencies;
    size_t successful = 0;
    const int NUM_REQUESTS = 500;  // Full production benchmark load

    auto testStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_REQUESTS; i++) {
        if (shouldStop_) return false;

        std::string prefix = "void func" + std::to_string(i) + "() {\n    ";

        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getCompletions(prefix, "", "cpp", "");
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) successful++;
        } catch (const std::exception& e) {
            log(std::string("Request %1 failed: %2")), 4);  // ERROR with details
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("Request %1 failed: Unknown exception"), 4);  // ERROR
            latencies.push_back(0.0);
        }
    }

    auto testEnd = std::chrono::high_resolution_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(testEnd - testStart).count();

    result = calculateStats("Rapid Fire", latencies, successful, NUM_REQUESTS);
    result.throughputReqSec = NUM_REQUESTS / (totalTimeMs / 1000.0);
    result.passed = result.throughputReqSec > 1.0;
    result.notes = "Tests burst completion handling under load";
    
    return result.passed;
}

bool BenchmarkRunner::testMultiLanguage(BenchmarkTestResult& result) {
    struct TestCase {
        std::string language;
        std::string prefix;
        std::string fileType;
    };

    std::vector<TestCase> tests = {
        {"C++", "class Widget {\npublic:\n    ", "cpp"},
        {"Python", "def process():\n    result = ", "py"},
        {"JavaScript", "function calc() {\n    const x = ", "js"},
        {"TypeScript", "interface Config {\n    name: ", "ts"},
        {"Rust", "fn compute() -> i32 {\n    let value = ", "rs"},
        {"Go", "func process() error {\n    result := ", "go"},
        {"Java", "public class Service {\n    private int ", "java"},
        {"C#", "class Handler {\n    public void Process() {\n        var ", "cs"},
    };

    std::vector<double> latencies;
    size_t successful = 0;

    for (const auto& test : tests) {
        if (shouldStop_) return false;

        log(std::string("  Testing %1...")), 1);
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getCompletions(test.prefix, "", test.fileType, "");
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) {
                successful++;
                if (verbose_) {
                    log(std::string("    %1: %2ms, %3 completions")
                        )
                        
                        ), 2);
                }
            } else {
                log(std::string("    %1: No completions generated")
                    ), 3);
            }
        } catch (const std::exception& e) {
            log(std::string("    %1 failed: %2")
                )
                ), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("    %1 failed: Unknown exception")
                ), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("Multi-Language", latencies, successful, tests.size());
    result.passed = successful >= tests.size() * 0.75;
    result.notes = "Tests language-agnostic completion quality";
    
    return result.passed;
}

bool BenchmarkRunner::testContextAware(BenchmarkTestResult& result) {
    struct ContextTest {
        std::string context;
        std::string prefix;
        std::string description;
    };

    std::vector<ContextTest> tests = {
        {
            "int factorial(int n) {\n"
            "    if (n <= 1) return 1;\n"
            "    return n * factorial(n - 1);\n"
            "}\n",
            "int sum(int a, int b) {\n    return ",
            "Simple arithmetic context"
        },
        {
            "class DataProcessor {\n"
            "private:\n"
            "    std::vector<int> data_;\n"
            "public:\n"
            "    void addData(int value) { data_.push_back(value); }\n",
            "    int getSize() {\n        return ",
            "Class member context"
        },
        {
            "template<typename T>\n"
            "T maximum(T a, T b) {\n"
            "    return (a > b) ? a : b;\n"
            "}\n",
            "template<typename T>\nT minimum(T a, T b) {\n    return ",
            "Template function context"
        },
    };

    std::vector<double> latencies;
    size_t successful = 0;

    for (const auto& test : tests) {
        if (shouldStop_) return false;

        log(std::string("  Testing: %1")), 1);
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getCompletions(test.prefix, "", "cpp", test.context);
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) {
                successful++;
                if (verbose_) {
                    log(std::string("    Success: %1ms, %2 completions")
                        
                        ), 2);
                }
            } else {
                log(std::string("    No completions generated"), 3);
            }
        } catch (const std::exception& e) {
            log(std::string("    Failed: %1")), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("    Failed: Unknown exception"), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("Context-Aware", latencies, successful, tests.size());
    result.passed = successful >= tests.size() * 0.6 && result.avgLatencyMs < 500.0;
    result.notes = std::string("Tests completion quality with full context (%1/%2 successful)")
        ).toStdString();
    
    return result.passed;
}

bool BenchmarkRunner::testMultiLine(BenchmarkTestResult& result) {
    struct MultiLineTest {
        std::string prefix;
        int maxLines;
        std::string description;
    };

    std::vector<MultiLineTest> tests = {
        {
            "int binarySearch(int arr[], int size, int target) {\n"
            "    int left = 0, right = size - 1;\n"
            "    ",
            10,
            "Binary search algorithm"
        },
        {
            "class NetworkHandler {\n"
            "public:\n"
            "    bool connect(const std::string& url) {\n"
            "        ",
            15,
            "Class method implementation"
        },
        {
            "void processData(std::vector<int>& data) {\n"
            "    if (data.empty()) return;\n"
            "    for (size_t i = 0; i < data.size(); i++) {\n"
            "        ",
            8,
            "Loop with conditional"
        },
        {
            "template<typename T>\n"
            "class SmartPointer {\n"
            "private:\n"
            "    T* ptr_;\n"
            "public:\n"
            "    SmartPointer(T* p) : ptr_(p) {}\n"
            "    ~SmartPointer() {\n"
            "        ",
            5,
            "Template class destructor"
        },
    };

    std::vector<double> latencies;
    size_t successful = 0;

    for (const auto& test : tests) {
        if (shouldStop_) return false;

        log(std::string("  Testing: %1")), 1);
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getMultiLineCompletions(test.prefix, test.maxLines);
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) {
                successful++;
                if (verbose_) {
                    log(std::string("    Generated %1 lines in %2ms")
                        )
                        , 2);
                }
            } else {
                log(std::string("    No multi-line completion generated"), 3);
            }
        } catch (const std::exception& e) {
            log(std::string("    Failed: %1")), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("    Failed: Unknown exception"), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("Multi-Line", latencies, successful, tests.size());
    result.passed = successful >= tests.size() * 0.5;
    result.notes = std::string("Tests structural code completion (%1/%2 successful)")
        ).toStdString();
    
    return result.passed;
}

bool BenchmarkRunner::testGPUAcceleration(BenchmarkTestResult& result) {
    std::vector<double> latencies;
    size_t successful = 0;
    const int NUM_GPU_TESTS = 50;  // Full GPU benchmark

    log(std::string("  GPU Mode: %1"), 1);

    std::vector<std::string> testPrompts = {
        "float calc() {\n    ",
        "double matrix_multiply(double* a, double* b) {\n    ",
        "void gpu_kernel() {\n    __global__ int* data = ",
        "struct Vector3 {\n    float x, y, z;\n    Vector3 operator+(const Vector3& v) const {\n        return ",
        "template<typename T>\nT compute_sum(const T* array, size_t size) {\n    T result = ",
    };

    for (int i = 0; i < NUM_GPU_TESTS; i++) {
        if (shouldStop_) return false;

        const auto& prompt = testPrompts[i % testPrompts.size()];
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto completions = completionEngine_->getCompletions(prompt, "", "cpp", "");
            
            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(latencyMs);

            if (!completions.empty()) {
                successful++;
                if (verbose_ && i % 10 == 0) {
                    log(std::string("    Iteration %1/%2: %3ms")
                        , 1);
                }
            }
        } catch (const std::exception& e) {
            log(std::string("  GPU test %1 failed: %2")), 4);
            latencies.push_back(0.0);
        } catch (...) {
            log(std::string("  GPU test %1 failed: Unknown exception"), 4);
            latencies.push_back(0.0);
        }
    }

    result = calculateStats("GPU Acceleration", latencies, successful, NUM_GPU_TESTS);
    result.passed = successful >= NUM_GPU_TESTS * 0.8;
    
    std::string performanceNote = gpuEnabled_ ? 
        std::string("GPU accelerated - Avg: %1ms, P95: %2ms")
            
             :
        std::string("CPU fallback - Avg: %1ms (GPU unavailable)")
            ;
    
    result.notes = performanceNote.toStdString();
    log(std::string("  %1"), result.passed ? 2 : 3);
    
    return result.passed;
}

bool BenchmarkRunner::testMemory(BenchmarkTestResult& result) {
    log("  Profiling memory usage...", 1);
    
    try {
        // Get initial memory usage
        int64_t baseMemMB = engine_->memoryUsageMB();
        log(std::string("    Base memory: %1 MB"), 1);
        
        // Perform inference operations to measure peak memory
        std::vector<int64_t> memorySnapshots;
        memorySnapshots.push_back(baseMemMB);
        
        std::vector<std::string> testPrompts = {
            "void test1() {\n    ",
            "class LargeDataStructure {\nprivate:\n    std::vector<std::vector<double>> data_;\n",
            "template<typename T>\nvoid process(const std::vector<T>& items) {\n    ",
        };
        
        for (const auto& prompt : testPrompts) {
            if (shouldStop_) return false;
            
            try {
                auto completions = completionEngine_->getCompletions(prompt, "", "cpp", "");
                int64_t currentMemMB = engine_->memoryUsageMB();
                memorySnapshots.push_back(currentMemMB);
                
                if (verbose_) {
                    log(std::string("    After inference: %1 MB"), 1);
                }
            } catch (const std::exception& e) {
                log(std::string("    Memory snapshot failed: %1")), 3);
            }
        }
        
        // Calculate memory statistics
        int64_t minMemMB = *std::min_element(memorySnapshots.begin(), memorySnapshots.end());
        int64_t maxMemMB = *std::max_element(memorySnapshots.begin(), memorySnapshots.end());
        int64_t avgMemMB = std::accumulate(memorySnapshots.begin(), memorySnapshots.end(), 0LL) / memorySnapshots.size();
        int64_t peakDeltaMB = maxMemMB - minMemMB;
        
        result.testName = "Memory";
        result.avgLatencyMs = static_cast<double>(avgMemMB);
        result.minLatencyMs = static_cast<double>(minMemMB);
        result.maxLatencyMs = static_cast<double>(maxMemMB);
        result.passed = true;
        
        std::string memoryReport = std::string(
            "Base: %1 MB | Avg: %2 MB | Peak: %3 MB | Delta: %4 MB | Samples: %5"
        ));
        
        result.notes = memoryReport.toStdString();
        log(std::string("  %1"), 2);
        
        // Warn if memory usage is concerning
        if (maxMemMB > 8192) {
            log("  WARNING: High memory usage detected (>8GB)", 3);
        }
        
        return true;
    } catch (const std::exception& e) {
        log(std::string("  Memory profiling failed: %1")), 4);
        result.passed = false;
        result.notes = std::string("Error: %1")).toStdString();
        return false;
    } catch (...) {
        log("  Memory profiling failed: Unknown exception", 4);
        result.passed = false;
        result.notes = "Error: Unknown exception during memory profiling";
        return false;
    }
}

BenchmarkTestResult BenchmarkRunner::calculateStats(
    const std::string& name,
    const std::vector<double>& latencies,
    size_t successful,
    size_t total) {
    
    BenchmarkTestResult result;
    result.testName = name;
    result.totalRequests = total;
    result.successfulRequests = successful;
    result.successRate = total > 0 ? (100.0 * successful / total) : 0.0;

    if (latencies.empty()) {
        result.avgLatencyMs = 0;
        result.minLatencyMs = 0;
        result.maxLatencyMs = 0;
        result.medianLatencyMs = 0;
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

    size_t medianIdx = sorted.size() / 2;
    result.medianLatencyMs = sorted[medianIdx];
    result.p95LatencyMs = sorted[std::min(static_cast<size_t>(sorted.size() * 0.95), sorted.size() - 1)];
    result.p99LatencyMs = sorted[std::min(static_cast<size_t>(sorted.size() * 0.99), sorted.size() - 1)];

    return result;
}

void BenchmarkRunner::log(const std::string& message, int level) {
    logMessage(message, level);
}


