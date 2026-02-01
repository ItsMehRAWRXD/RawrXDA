#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Profiler
{
public:
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

    explicit Profiler();
    virtual ~Profiler() = default;

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
    json getProfilingReport() const;
    bool exportReport(const std::string& filePath) const;

    std::function<void(const ProfileSnapshot&)> onMetricsUpdated;
    std::function<void(const std::string&)> onPerformanceWarning;

private:
    bool m_isProfiling;
    std::chrono::steady_clock::time_point m_startTime;
    std::map<std::string, std::chrono::steady_clock::time_point> m_phaseStarts;
    
    // Simple state
    size_t m_memoryAllocated = 0;
};
