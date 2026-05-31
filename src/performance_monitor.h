// performance_monitor.h - Real-time Performance Monitoring and SLA Tracking
#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

struct MetricData {
    std::string metricId;
    std::string component;
    std::string operation;
    double value;
    std::string unit;
    std::map<std::string, std::string> tags;
    std::chrono::system_clock::time_point timestamp;
};

struct PerformanceThreshold {
    std::string component;
    std::string metric;
    double warningThreshold;
    double criticalThreshold;
    bool isEnabled;
};

struct SLADefinition {
    std::string slaId;
    std::string name;
    std::string component;
    std::string metric;
    double targetValue;
    double acceptableValue;
    std::string timeWindow; // "1h", "24h", "7d", "30d"
    bool isActive;
};

struct SLACompliance {
    std::string slaId;
    bool isCompliant;
    double currentValue;
    double targetValue;
    double compliancePercentage;
    int violationCount;
    std::chrono::system_clock::time_point lastViolation;
};

struct PerformanceSnapshot {
    std::chrono::system_clock::time_point timestamp;
    double cpuUsage;
    double memoryUsage;
    double diskUsage;
    double networkUsage;
    int activeConnections;
    double averageLatency;
    double requestsPerSecond;
};

struct Bottleneck {
    std::string bottleneckId;
    std::string component;
    std::string type;
    double severity;
    std::string description;
    std::chrono::system_clock::time_point detectedAt;
    std::vector<std::string> recommendations;
};

struct TrendAnalysis {
    std::string component;
    std::string operation;
    std::string trend;
    double changePercentage;
    double forecast;
    double confidence;
};

class PerformanceMonitor;

class ScopedTimer {
public:
    ScopedTimer(PerformanceMonitor* monitor, const std::string& component, const std::string& operation);
    ~ScopedTimer();

private:
    PerformanceMonitor* performanceMonitor;
    std::string component;
    std::string operation;
    std::string timerId;
};

class PerformanceMonitor {

public:
    explicit PerformanceMonitor(void* parent = nullptr);
    ~PerformanceMonitor();

    void recordMetric(const std::string& component, const std::string& operation, double value, const std::string& unit = "ms");
    void recordMetricWithTags(const std::string& component, const std::string& operation, double value, const std::string& unit, const std::map<std::string, std::string>& tags);
    void startTimer(const std::string& timerId, const std::string& component, const std::string& operation);
    double stopTimer(const std::string& timerId);
    ScopedTimer createScopedTimer(const std::string& component, const std::string& operation);

    std::vector<MetricData> getMetrics(const std::string& component, const std::string& operation,
                                   const std::chrono::system_clock::time_point& startTime = std::chrono::system_clock::time_point(), const std::chrono::system_clock::time_point& endTime = std::chrono::system_clock::time_point::currentDateTime()) const;
    double getAverageMetric(const std::string& component, const std::string& operation,
                            const std::chrono::system_clock::time_point& startTime = std::chrono::system_clock::time_point(), const std::chrono::system_clock::time_point& endTime = std::chrono::system_clock::time_point::currentDateTime()) const;
    double getP95Latency(const std::string& component, const std::string& operation) const;
    double getP99Latency(const std::string& component, const std::string& operation) const;
    double getPercentile(const std::string& component, const std::string& operation, double percentile) const;

    SLACompliance evaluateSLA(const std::string& slaId) const;
    std::vector<SLACompliance> evaluateAllSLAs() const;

    PerformanceSnapshot capturePerformanceSnapshot();
    std::vector<PerformanceSnapshot> getPerformanceHistory(int minutes = 0) const;

    std::vector<Bottleneck> detectBottlenecks() const;
    TrendAnalysis analyzeTrend(const std::string& component, const std::string& operation, int windowMinutes = 60) const;

    bool exportToPrometheus(const std::string& outputPath) const;

    void enableMonitoring(bool enable);
    void setSnapshotInterval(int milliseconds);
    void setMetricsRetention(int hours);
    void clearMetrics();


    void metricRecorded(const MetricData& metric);
    void thresholdViolation(const MetricData& metric, const std::string& severity);
    void snapshotCaptured(const PerformanceSnapshot& snapshot);

private:
    void setupDefaultSLAs();
    void setupDefaultThresholds();
    void checkThreshold(const MetricData& metric);
    void pruneOldMetrics(const std::string& metricKey);
    std::chrono::system_clock::time_point calculateTimeWindow(const std::chrono::system_clock::time_point& endTime, const std::string& window) const;

    double calculateUptimePercentage(const std::chrono::system_clock::time_point& startTime, const std::chrono::system_clock::time_point& endTime) const;
    double calculateErrorRate(const std::chrono::system_clock::time_point& startTime, const std::chrono::system_clock::time_point& endTime) const;

    double getCPUUsageWindows() const;
    double getMemoryUsageWindows() const;
    double getCPUUsageLinux() const;
    double getMemoryUsageLinux() const;
    double getCPUUsageMac() const;
    double getMemoryUsageMac() const;

    // Data members
    std::unordered_map<std::string, std::vector<MetricData>> metricHistory;
    std::vector<PerformanceSnapshot> performanceHistory;
    std::unordered_map<std::string, PerformanceThreshold> thresholds;
    std::unordered_map<std::string, SLADefinition> slaDefinitions;
    std::unordered_map<std::string, std::chrono::steady_clock> activeTimers;
    std::unordered_map<std::string, std::pair<std::string, std::string>> timerContext;

    bool monitoringEnabled;
    int snapshotIntervalMs;
    int metricsRetentionHours;
    void** snapshotTimer;

    // Add thread for snapshot
    std::thread m_monitorThread;
    std::atomic<bool> m_running;
    std::mutex m_mutex;
    int m_snapshotIntervalMs = 1000;

    void monitoringLoop();
};

#endif // PERFORMANCE_MONITOR_H

