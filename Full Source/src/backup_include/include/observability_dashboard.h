#pragma once

// C++20 / Win32. Observability dashboard; no Qt. Profiler + callbacks.

#include <string>
#include <vector>
#include <functional>

class Profiler;

class ObservabilityDashboard
{
public:
    using RequestMetricsRefreshFn = std::function<void()>;

    explicit ObservabilityDashboard(Profiler* profiler);
    ~ObservabilityDashboard();

    void setOnRequestMetricsRefresh(RequestMetricsRefreshFn f) { m_onRequestRefresh = std::move(f); }
    void initialize();

    void onMetricsUpdated(float cpuPercent, float memoryMB, float gpuPercent, float gpuMemoryMB);
    void onThroughputUpdated(float batchLatencyMs, float throughputSamples);
    void onPerformanceWarning(const std::string& warning);
    void clearCharts();
    bool exportData(const std::string& filePath);

    void* getWidgetHandle() const { return m_handle; }

private:
    void setupUI();
    void createResourceChart();
    void createThroughputChart();
    void createLatencyChart();
    void createMetricsPanel();

    void* m_handle = nullptr;
    Profiler* m_profiler = nullptr;
    int m_dataPointCount = 0;
    int m_maxDataPoints = 300;
    std::vector<std::string> m_warnings;
    RequestMetricsRefreshFn m_onRequestRefresh;
};
