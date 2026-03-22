#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>
#include <deque>

namespace RawrXD {
namespace Profiling {

// =============================================================================
// Batch 3: Performance Profiling Gaps (7 Enhancements)
// =============================================================================

// Enhancement 1: Real-time Performance Monitoring
struct PerformanceMetrics {
    std::chrono::steady_clock::time_point timestamp;
    double cpuUsagePercent = 0.0;
    double memoryUsageMB = 0.0;
    double gpuUsagePercent = 0.0;
    double gpuMemoryMB = 0.0;
    double networkBandwidthMbps = 0.0;
    double diskIOPerSec = 0.0;
    uint64_t activeThreads = 0;
    uint64_t contextSwitchesPerSec = 0;
};

// Enhancement 2: Bottleneck Detection
struct BottleneckInfo {
    std::string componentName;
    std::string bottleneckType; // "cpu", "memory", "io", "network", "lock_contention"
    double severityScore = 0.0; // 0.0-1.0
    std::chrono::steady_clock::time_point detectedAt;
    std::string description;
    std::vector<std::string> recommendations;
};

// Enhancement 3: Memory Usage Profiling
struct MemoryProfile {
    size_t totalAllocated = 0;
    size_t peakUsage = 0;
    size_t currentUsage = 0;
    std::unordered_map<std::string, size_t> allocationByComponent;
    std::unordered_map<std::string, size_t> allocationByType;
    std::vector<std::pair<std::string, size_t>> topAllocators;
    double fragmentationRatio = 0.0;
    std::chrono::steady_clock::time_point lastUpdated;
};

// Enhancement 4: Execution Path Tracing
struct TraceEvent {
    std::string eventId;
    std::string componentName;
    std::string operationName;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> childEvents;
    std::string parentEventId;
    double durationMs = 0.0;
};

struct ExecutionTrace {
    std::string traceId;
    std::string rootOperation;
    std::vector<TraceEvent> events;
    double totalDurationMs = 0.0;
    std::chrono::steady_clock::time_point startTime;
    bool completed = false;
};

// Enhancement 5: Resource Contention Analysis
struct ResourceContention {
    std::string resourceName;
    std::string resourceType; // "mutex", "semaphore", "memory", "cpu", "io"
    double contentionRatio = 0.0; // 0.0-1.0
    uint64_t waitCount = 0;
    double avgWaitTimeMs = 0.0;
    double maxWaitTimeMs = 0.0;
    std::vector<std::string> contendingComponents;
    std::chrono::steady_clock::time_point lastAnalyzed;
};

// Enhancement 6: Performance Regression Detection
struct PerformanceBaseline {
    std::string metricName;
    double baselineValue = 0.0;
    double thresholdPercent = 10.0; // Alert if deviation > 10%
    std::chrono::steady_clock::time_point establishedAt;
    std::vector<double> historicalValues;
};

struct RegressionAlert {
    std::string metricName;
    double currentValue = 0.0;
    double baselineValue = 0.0;
    double deviationPercent = 0.0;
    std::string severity; // "low", "medium", "high", "critical"
    std::chrono::steady_clock::time_point detectedAt;
    std::string description;
};

// Enhancement 7: Profiling Data Analytics
struct AnalyticsReport {
    std::string reportId;
    std::string timeRange;
    std::unordered_map<std::string, double> summaryStats;
    std::vector<std::string> topBottlenecks;
    std::vector<std::string> performanceTrends;
    std::vector<std::string> recommendations;
    std::string detailedMetrics; // JSON-like string instead of nlohmann::json
    std::chrono::steady_clock::time_point generatedAt;
};

// Core Advanced Performance Profiler
class AdvancedPerformanceProfiler {
public:
    static AdvancedPerformanceProfiler& instance();

    // Initialization
    bool initialize(const std::string& configPath = "");
    void shutdown();

    // Enhancement 1: Real-time Performance Monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    PerformanceMetrics getCurrentMetrics() const;
    std::vector<PerformanceMetrics> getMetricsHistory(int minutes = 60) const;

    // Enhancement 2: Bottleneck Detection
    std::vector<BottleneckInfo> detectBottlenecks();
    void analyzeComponentBottlenecks(const std::string& componentName);
    std::vector<BottleneckInfo> getActiveBottlenecks() const;

    // Enhancement 3: Memory Usage Profiling
    void startMemoryProfiling();
    void stopMemoryProfiling();
    MemoryProfile getCurrentMemoryProfile() const;
    void recordMemoryAllocation(const std::string& component, size_t bytes, const std::string& type = "unknown");
    void recordMemoryDeallocation(const std::string& component, size_t bytes);

    // Enhancement 4: Execution Path Tracing
    std::string startTrace(const std::string& operationName, const std::string& component = "unknown");
    void endTrace(const std::string& traceId);
    void addTraceEvent(const std::string& traceId, const std::string& eventName,
                      const std::unordered_map<std::string, std::string>& metadata = {});
    ExecutionTrace getTrace(const std::string& traceId) const;
    std::vector<ExecutionTrace> getRecentTraces(int count = 10) const;

    // Enhancement 5: Resource Contention Analysis
    void monitorResourceContention(const std::string& resourceName, const std::string& resourceType);
    std::vector<ResourceContention> analyzeResourceContention();
    ResourceContention getResourceContention(const std::string& resourceName) const;

    // Enhancement 6: Performance Regression Detection
    void establishBaseline(const std::string& metricName, double value);
    void updateBaseline(const std::string& metricName, double value);
    std::vector<RegressionAlert> detectRegressions();
    PerformanceBaseline getBaseline(const std::string& metricName) const;

    // Enhancement 7: Profiling Data Analytics
    AnalyticsReport generateAnalyticsReport(const std::string& timeRange = "1h");
    void exportProfilingData(const std::string& filePath);
    std::string getProfilingSummary() const;

    // Utility methods
    void setProfilingLevel(int level); // 0=minimal, 1=standard, 2=detailed, 3=comprehensive
    int getProfilingLevel() const;
    void clearCollectedData();

public:
    AdvancedPerformanceProfiler();
    ~AdvancedPerformanceProfiler();

private:
    AdvancedPerformanceProfiler(const AdvancedPerformanceProfiler&) = delete;
    AdvancedPerformanceProfiler& operator=(const AdvancedPerformanceProfiler&) = delete;

    // Core data structures
    std::deque<PerformanceMetrics> m_metricsHistory;
    std::vector<BottleneckInfo> m_activeBottlenecks;
    MemoryProfile m_currentMemoryProfile;
    std::unordered_map<std::string, ExecutionTrace> m_activeTraces;
    std::vector<ExecutionTrace> m_completedTraces;
    std::unordered_map<std::string, ResourceContention> m_resourceContention;
    std::unordered_map<std::string, PerformanceBaseline> m_baselines;

    // Synchronization
    mutable std::mutex m_mutex;
    std::atomic<bool> m_monitoringActive{false};
    std::atomic<int> m_profilingLevel{1};

    // Background threads
    std::thread m_monitoringThread;
    std::thread m_bottleneckDetectionThread;
    std::thread m_regressionDetectionThread;

    // Helper methods
    void monitoringLoop();
    void bottleneckDetectionLoop();
    void regressionDetectionLoop();
    PerformanceMetrics collectSystemMetrics();
    double calculateBottleneckSeverity(const std::string& component, const std::string& type);
    void updateMemoryProfile();
    void cleanupOldData();
    std::string generateUniqueId() const;
};

} // namespace Profiling
} // namespace RawrXD