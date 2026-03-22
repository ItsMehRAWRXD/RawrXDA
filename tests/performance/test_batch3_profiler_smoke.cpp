#include "profiler.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class Batch3SmokeTest {
public:
    Batch3SmokeTest() : profiler(std::make_unique<Profiler>()) {}

    bool runAllTests() {
        std::cout << "=== Batch 3 Profiler Smoke Test Suite ===\n\n";

        bool allPassed = true;
        allPassed &= testBasicProfiling();
        allPassed &= testAdvancedProfilingToggle();
        allPassed &= testRealTimeMonitoring();
        allPassed &= testMemoryProfiling();
        allPassed &= testExecutionTracing();
        allPassed &= testBottleneckDetection();
        allPassed &= testResourceContention();
        allPassed &= testRegressionDetection();
        allPassed &= testAnalyticsReporting();
        allPassed &= testFullIntegration();

        std::cout << "\n=== Smoke Test Results ===\n";
        std::cout << "Overall: " << (allPassed ? "PASSED" : "FAILED") << "\n";
        return allPassed;
    }

private:
    std::unique_ptr<Profiler> profiler;

    bool testBasicProfiling() {
        std::cout << "Test 1: Basic Profiling... ";
        try {
            profiler->startProfiling();
            if (!profiler->isProfiling()) {
                std::cout << "FAILED (not profiling)\n";
                return false;
            }

            profiler->recordMemoryAllocation(1024 * 1024);
            profiler->recordBatchCompleted(10, 100);

            const auto snapshot = profiler->getCurrentSnapshot();
            if (snapshot.memoryUsageMB < 0) {
                std::cout << "FAILED (invalid memory reading)\n";
                return false;
            }

            profiler->stopProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testAdvancedProfilingToggle() {
        std::cout << "Test 2: Advanced Profiling Toggle... ";
        try {
            if (profiler->isAdvancedProfilingEnabled()) {
                std::cout << "FAILED (should start disabled)\n";
                return false;
            }

            profiler->enableAdvancedProfiling();
            if (!profiler->isAdvancedProfilingEnabled()) {
                std::cout << "FAILED (enable failed)\n";
                return false;
            }

            profiler->disableAdvancedProfiling();
            if (profiler->isAdvancedProfilingEnabled()) {
                std::cout << "FAILED (disable failed)\n";
                return false;
            }

            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testRealTimeMonitoring() {
        std::cout << "Test 3: Real-time Monitoring... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->startRealTimeMonitoring();
            std::this_thread::sleep_for(std::chrono::seconds(2));

            const auto history = profiler->getMetricsHistory(1);
            if (history.empty()) {
                std::cout << "FAILED (no metrics collected)\n";
                return false;
            }

            profiler->stopRealTimeMonitoring();
            profiler->disableAdvancedProfiling();
            std::cout << "PASSED (" << history.size() << " samples)\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testMemoryProfiling() {
        std::cout << "Test 4: Memory Profiling... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->startMemoryProfiling();
            profiler->recordMemoryAllocation(2048 * 1024);
            profiler->recordMemoryAllocation(1024 * 1024);
            profiler->recordMemoryDeallocation(512 * 1024);

            const auto report = profiler->getMemoryProfileReport();
            if (report.find("Memory Profile") == std::string::npos) {
                std::cout << "FAILED (invalid report)\n";
                return false;
            }

            profiler->stopMemoryProfiling();
            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testExecutionTracing() {
        std::cout << "Test 5: Execution Tracing... ";
        try {
            profiler->enableAdvancedProfiling();
            const auto traceId = profiler->startTrace("test_operation", "smoke_test");
            if (traceId.empty()) {
                std::cout << "FAILED (no trace ID)\n";
                return false;
            }

            profiler->addTraceEvent(traceId, "step1");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            profiler->addTraceEvent(traceId, "step2");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            profiler->endTrace(traceId);

            const auto traceReport = profiler->getTraceReport(traceId);
            if (traceReport.find("Execution Trace") == std::string::npos) {
                std::cout << "FAILED (invalid trace report)\n";
                return false;
            }

            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testBottleneckDetection() {
        std::cout << "Test 6: Bottleneck Detection... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->recordMemoryAllocation(100 * 1024 * 1024);
            (void)profiler->detectBottlenecks();
            profiler->analyzeComponentBottlenecks("test_component");
            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testResourceContention() {
        std::cout << "Test 7: Resource Contention... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->monitorResourceContention("test_mutex", "mutex");
            profiler->monitorResourceContention("test_semaphore", "semaphore");

            const auto report = profiler->getResourceContentionReport();
            if (report.find("Resource Contention") == std::string::npos) {
                std::cout << "FAILED (invalid report)\n";
                return false;
            }

            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testRegressionDetection() {
        std::cout << "Test 8: Regression Detection... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->establishPerformanceBaseline("test_metric", 100.0);
            profiler->establishPerformanceBaseline("cpu_usage", 50.0);
            (void)profiler->detectPerformanceRegressions();
            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testAnalyticsReporting() {
        std::cout << "Test 9: Analytics Reporting... ";
        try {
            profiler->enableAdvancedProfiling();
            const auto analytics = profiler->generateAnalyticsReport("1h");
            if (analytics.find("Analytics Report") == std::string::npos) {
                std::cout << "FAILED (invalid analytics)\n";
                return false;
            }

            const auto summary = profiler->getProfilingSummary();
            if (summary.empty()) {
                std::cout << "FAILED (empty summary)\n";
                return false;
            }

            profiler->disableAdvancedProfiling();
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }

    bool testFullIntegration() {
        std::cout << "Test 10: Full Integration... ";
        try {
            profiler->enableAdvancedProfiling();
            profiler->startProfiling();
            profiler->startRealTimeMonitoring();
            profiler->startMemoryProfiling();

            const auto traceId = profiler->startTrace("integration_test", "full");
            profiler->recordMemoryAllocation(1024 * 1024);
            profiler->addTraceEvent(traceId, "memory_allocated");
            profiler->recordBatchCompleted(5, 50);
            profiler->addTraceEvent(traceId, "batch_completed");
            profiler->endTrace(traceId);

            const auto analytics = profiler->generateAnalyticsReport("1h");
            const auto summary = profiler->getProfilingSummary();

            profiler->stopMemoryProfiling();
            profiler->stopRealTimeMonitoring();
            profiler->stopProfiling();
            profiler->disableAdvancedProfiling();

            if (analytics.empty() || summary.empty()) {
                std::cout << "FAILED (missing data)\n";
                return false;
            }

            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            return false;
        }
    }
};

int main() {
    Batch3SmokeTest test;
    const bool success = test.runAllTests();
    std::cout << "\nBatch 3 Smoke Test " << (success ? "PASSED" : "FAILED") << "\n";
    return success ? 0 : 1;
}
