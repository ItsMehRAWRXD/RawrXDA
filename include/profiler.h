#pragma once

// C++20, no Qt. Performance profiler; callbacks replace signals.

#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

// Forward declaration for AdvancedPerformanceProfiler
namespace RawrXD { namespace Profiling { class AdvancedPerformanceProfiler; } }

struct ProfilerImpl;

struct ProfileSnapshot {
    int64_t timestamp = 0;
    float cpuUsagePercent = 0.0f;
    float memoryUsageMB = 0.0f;
    float gpuUsagePercent = 0.0f;
    float gpuMemoryMB = 0.0f;
    float throughputSamples = 0.0f;
    float throughputTokens = 0.0f;
    float batchLatencyMs = 0.0f;
    float loadLatencyMs = 0.0f;
    float tokenizeLatencyMs = 0.0f;
    float forwardPassMs = 0.0f;
    float backwardPassMs = 0.0f;
    float optimizerStepMs = 0.0f;
};

class Profiler
{
public:
    using MetricsUpdatedFn = std::function<void(const ProfileSnapshot&)>;
    using ProfilingCompletedFn = std::function<void(const std::string& reportJson)>;
    using PerformanceWarningFn = std::function<void(const std::string&)>;

    Profiler();
    ~Profiler();

    void setOnMetricsUpdated(MetricsUpdatedFn f)       { m_onMetricsUpdated = std::move(f); }
    void setOnProfilingCompleted(ProfilingCompletedFn f) { m_onProfilingCompleted = std::move(f); }
    void setOnPerformanceWarning(PerformanceWarningFn f) { m_onPerformanceWarning = std::move(f); }

    void startProfiling();
    void stopProfiling();
    bool isProfiling() const { return m_isProfiling; }

    void markPhaseStart(const std::string& phaseName);
    void markPhaseEnd(const std::string& phaseName);
    void recordBatchCompleted(int sampleCount, int tokenCount);
    void recordMemoryAllocation(size_t bytes);
    void recordMemoryDeallocation(size_t bytes);
    void updateGpuMetrics(float gpuUsagePercent, float gpuMemoryMB);

    ProfileSnapshot getCurrentSnapshot() const;
    std::string getProfilingReport() const;
    bool exportReport(const std::string& filePath) const;

    // Batch 3: Advanced Performance Profiling Enhancements
    void enableAdvancedProfiling();
    void disableAdvancedProfiling();
    bool isAdvancedProfilingEnabled() const;

    // Enhancement 1: Real-time Performance Monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    std::vector<ProfileSnapshot> getMetricsHistory(int minutes = 60) const;

    // Enhancement 2: Bottleneck Detection
    std::vector<std::string> detectBottlenecks();
    void analyzeComponentBottlenecks(const std::string& componentName);

    // Enhancement 3: Memory Usage Profiling
    void startMemoryProfiling();
    void stopMemoryProfiling();
    std::string getMemoryProfileReport() const;

    // Enhancement 4: Execution Path Tracing
    std::string startTrace(const std::string& operationName, const std::string& component = "unknown");
    void endTrace(const std::string& traceId);
    void addTraceEvent(const std::string& traceId, const std::string& eventName);
    std::string getTraceReport(const std::string& traceId) const;

    // Enhancement 5: Resource Contention Analysis
    void monitorResourceContention(const std::string& resourceName, const std::string& resourceType);
    std::string getResourceContentionReport() const;

    // Enhancement 6: Performance Regression Detection
    void establishPerformanceBaseline(const std::string& metricName, double value);
    std::vector<std::string> detectPerformanceRegressions();

    // Enhancement 7: Profiling Data Analytics
    std::string generateAnalyticsReport(const std::string& timeRange = "1h");
    void exportAdvancedProfilingData(const std::string& filePath);
    std::string getProfilingSummary() const;

private:
    void collectSystemMetrics();
    float getCpuUsagePercent() const;
    float getMemoryUsageMB() const;
    void analyzeMetrics();

    bool m_isProfiling = false;
    std::chrono::high_resolution_clock::time_point m_profilingStart;
    std::chrono::high_resolution_clock::time_point m_batchStart;

    struct PhaseData {
        std::chrono::high_resolution_clock::time_point startTime;
        std::vector<int64_t> durations;
        int64_t totalMs = 0;
    };
    std::map<std::string, PhaseData> m_phases;

    size_t m_totalAllocated = 0;
    size_t m_currentAllocated = 0;
    size_t m_peakAllocated = 0;
    int m_totalSamplesProcessed = 0;
    int m_totalTokensProcessed = 0;
    std::vector<int64_t> m_batchLatencies;
    std::chrono::high_resolution_clock::time_point m_lastMetricsCollection;
    float m_lastGpuUsagePercent = 0.0f;
    float m_lastGpuMemoryMB = 0.0f;
    mutable std::vector<ProfileSnapshot> m_snapshots;
    float m_cpuThresholdPercent = 95.0f;
    float m_memoryThresholdPercent = 85.0f;
    float m_gpuThresholdPercent = 95.0f;

    MetricsUpdatedFn     m_onMetricsUpdated;
    ProfilingCompletedFn m_onProfilingCompleted;
    PerformanceWarningFn m_onPerformanceWarning;

    // Batch 3: Advanced Performance Profiling
    bool m_advancedProfilingEnabled = false;
    std::unique_ptr<RawrXD::Profiling::AdvancedPerformanceProfiler> m_advancedProfiler;
};
