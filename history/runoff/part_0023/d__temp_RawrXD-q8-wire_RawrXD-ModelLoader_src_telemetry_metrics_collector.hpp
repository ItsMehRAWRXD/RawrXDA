#pragma once
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <vector>
#include <cstdint>

/**
 * @class MetricsCollector
 * @brief Production-grade telemetry and performance monitoring
 * 
 * Features:
 * - Real-time performance metrics
 * - Health checks and alerting
 * - SLA tracking
 * - Performance anomaly detection
 * - Structured logging (JSON)
 */
class MetricsCollector {
    
public:
    struct PerformanceMetrics {
        // Timing metrics (milliseconds)
        double avg_token_latency_ms = 0.0;
        double p99_token_latency_ms = 0.0;
        double p95_token_latency_ms = 0.0;
        double throughput_tokens_per_sec = 0.0;
        
        // Resource metrics
        uint64_t peak_memory_usage_bytes = 0;
        double avg_cpu_usage_percent = 0.0;
        double gpu_utilization_percent = 0.0;
        
        // Model metrics
        int tokens_generated = 0;
        int inference_requests = 0;
        int failed_requests = 0;
        double success_rate = 1.0;
    };
    
    struct HealthStatus {
        enum Status { Healthy, Degraded, Unhealthy };
        Status status = Healthy;
        QString message;
        QDateTime lastCheck;
        double responsiveness = 1.0; // 0-1
        bool gpu_available = false;
        bool models_loaded = false;
        double uptime_percent = 99.9;
    };
    
    struct Alert {
        enum Severity { Info, Warning, Critical };
        Severity severity;
        QString message;
        QDateTime timestamp;
        QString source; // component that raised alert
    };
    
    static MetricsCollector& instance();
    
    // Recording methods
    void recordInferenceRequest(qint64 latencyMs, int tokensGenerated, bool success);
    void recordModelLoad(const QString& modelName, qint64 loadTimeMs, uint64_t modelSizeBytes);
    void recordMemoryUsage(uint64_t usageBytes, uint64_t peakBytes);
    void recordCPUUsage(double percent);
    void recordGPUUsage(double percent);
    
    // Metrics retrieval
    PerformanceMetrics getPerformanceMetrics() const;
    HealthStatus checkHealth();
    QVector<Alert> getRecentAlerts(int count = 10) const;
    
    // Health monitoring
    void enableHealthChecks(bool enable);
    bool runHealthCheck();
    void addHealthCheckCallback(std::function<bool()> callback);
    
    // SLA tracking
    void setSLATarget(const QString& metric, double target);
    bool isSLAMet(const QString& metric) const;
    double getSLACompliance(const QString& metric) const;
    
    // Alert management
    void setAlertThreshold(const QString& metric, double threshold);
    void clearAlerts();
    
    // Export metrics
    QString exportMetricsJSON() const;
    QString exportHealthJSON() const;
    
private:
    MetricsCollector();
    ~MetricsCollector() = default;
    
    struct Measurement {
        qint64 timestamp;
        double value;
    };
    
    mutable QMutex m_mutex;
    
    // Time series data
    std::vector<Measurement> m_token_latencies;
    std::vector<Measurement> m_throughputs;
    std::vector<Measurement> m_memory_usage;
    std::vector<Measurement> m_cpu_usage;
    std::vector<Measurement> m_gpu_usage;
    
    // Aggregated stats
    int m_total_requests = 0;
    int m_failed_requests = 0;
    int m_total_tokens = 0;
    qint64 m_startup_time = 0;
    
    // Health & alerts
    QVector<Alert> m_alerts;
    bool m_health_checks_enabled = true;
    std::vector<std::function<bool()>> m_health_callbacks;
    
    // SLA targets
    QHash<QString, double> m_sla_targets;
    QHash<QString, double> m_alert_thresholds;
    
    // Helper methods
    double calculatePercentile(const std::vector<Measurement>& data, double percentile) const;
    void pruneOldData();
};
