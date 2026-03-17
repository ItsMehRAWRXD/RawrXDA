#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <vector>

namespace RawrXD::Agentic::Observability {

/// Metric type
enum class MetricType {
    COUNTER,        // Monotonically increasing
    GAUGE,          // Can go up or down
    HISTOGRAM,      // Distribution
    SUMMARY         // Quantiles
};

/// Metric value
struct MetricValue {
    MetricType type;
    std::atomic<double> value{0.0};
    std::chrono::system_clock::time_point lastUpdated;
    
    // Histogram data
    std::vector<double> buckets;
    std::vector<std::atomic<uint64_t>> counts;
    
    // Labels
    std::unordered_map<std::string, std::string> labels;
};

/// Metrics collection engine (Prometheus-compatible)
class Metrics {
public:
    static Metrics& instance();
    
    /// Initialize metrics
    bool initialize(const std::string& appName);
    
    /// Register metric
    bool registerMetric(const std::string& name, MetricType type,
                       const std::string& help = "",
                       const std::vector<std::string>& labelNames = {});
    
    /// Increment counter
    void incrementCounter(const std::string& name, double value = 1.0,
                         const std::unordered_map<std::string, std::string>& labels = {});
    
    /// Set gauge
    void setGauge(const std::string& name, double value,
                  const std::unordered_map<std::string, std::string>& labels = {});
    
    /// Observe histogram
    void observeHistogram(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& labels = {});
    
    /// Get metric value
    double getMetricValue(const std::string& name,
                         const std::unordered_map<std::string, std::string>& labels = {}) const;
    
    /// Export to Prometheus text format
    std::string exportPrometheus() const;
    
    /// Export to JSON
    std::string exportJson() const;
    
    /// Start HTTP metrics server
    bool startMetricsServer(uint16_t port = 9090);
    
    /// Stop metrics server
    void stopMetricsServer();
    
    /// Clear all metrics
    void clear();
    
    /// Get all metric names
    std::vector<std::string> getMetricNames() const;
    
private:
    Metrics() = default;
    ~Metrics() { stopMetricsServer(); }
    
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    
    mutable std::mutex m_mutex;
    std::string m_appName;
    std::unordered_map<std::string, MetricValue> m_metrics;
    std::unordered_map<std::string, std::string> m_help;
    std::unordered_map<std::string, std::vector<std::string>> m_labelNames;
    
    // HTTP server state
    std::atomic<bool> m_serverRunning{false};
    uint16_t m_serverPort = 9090;
    
    std::string generateKey(const std::string& name,
                           const std::unordered_map<std::string, std::string>& labels) const;
};

/// RAII timer for automatic duration measurement
class Timer {
public:
    Timer(const std::string& metricName,
          const std::unordered_map<std::string, std::string>& labels = {})
        : m_metricName(metricName), m_labels(labels),
          m_start(std::chrono::high_resolution_clock::now()) {}
    
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - m_start).count();
        Metrics::instance().observeHistogram(m_metricName, duration, m_labels);
    }
    
private:
    std::string m_metricName;
    std::unordered_map<std::string, std::string> m_labels;
    std::chrono::high_resolution_clock::time_point m_start;
};

// Helper macro
#define MEASURE_TIME(metric_name) \
    RawrXD::Agentic::Observability::Timer _timer_##__LINE__(metric_name)

} // namespace RawrXD::Agentic::Observability
