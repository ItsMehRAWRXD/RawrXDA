#pragma once

// C++20, no Qt. Performance profiler; callbacks replace signals.

#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

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

    Profiler() = default;
    ~Profiler() = default;

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
    std::vector<ProfileSnapshot> m_snapshots;
    float m_cpuThresholdPercent = 95.0f;
    float m_memoryThresholdPercent = 85.0f;
    float m_gpuThresholdPercent = 95.0f;

    MetricsUpdatedFn     m_onMetricsUpdated;
    ProfilingCompletedFn m_onProfilingCompleted;
    PerformanceWarningFn m_onPerformanceWarning;
};
