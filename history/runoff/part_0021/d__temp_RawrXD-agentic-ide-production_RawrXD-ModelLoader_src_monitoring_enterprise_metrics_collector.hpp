#pragma once
#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>

class EnterpriseMetricsCollector : public QObject {
    Q_OBJECT

public:
    explicit EnterpriseMetricsCollector(QObject* parent = nullptr);
    ~EnterpriseMetricsCollector();

    // Metrics collection
    void recordMetric(const QString& name, double value, 
                     const std::map<QString, QString>& tags = {});
    void recordCounter(const QString& name, uint64_t value = 1,
                      const std::map<QString, QString>& tags = {});
    void recordHistogram(const QString& name, double value,
                        const std::map<QString, QString>& tags = {});
    void recordEvent(const QString& name, const std::map<QString, QVariant>& properties = {});
    
    // Backend configuration
    void setBackend(const QString& backend); // "prometheus", "influxdb", "cloudwatch", "custom"
    void setReportingInterval(std::chrono::seconds interval);
    void setEndpoint(const QString& endpoint);
    void setAuthentication(const QString& auth_token, const QString& auth_type = "Bearer");
    
    // Performance metrics
    struct PerformanceMetrics {
        double requests_per_second = 0.0;
        double avg_latency_ms = 0.0;
        double p50_latency_ms = 0.0;
        double p95_latency_ms = 0.0;
        double p99_latency_ms = 0.0;
        double error_rate = 0.0;
        double memory_utilization = 0.0;
        double cpu_utilization = 0.0;
        double gpu_utilization = 0.0;
        size_t active_connections = 0;
        size_t queue_depth = 0;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void recordPerformanceMetrics(const PerformanceMetrics& metrics);
    
    // System metrics
    struct SystemMetrics {
        size_t total_memory_bytes = 0;
        size_t used_memory_bytes = 0;
        size_t available_memory_bytes = 0;
        double memory_pressure = 0.0;
        size_t total_requests = 0;
        size_t failed_requests = 0;
        size_t active_models = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        double cache_hit_rate = 0.0;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void recordSystemMetrics(const SystemMetrics& metrics);
    
    // Business metrics
    struct BusinessMetrics {
        size_t models_deployed = 0;
        size_t models_undeployed = 0;
        size_t requests_processed = 0;
        size_t tokens_generated = 0;
        double avg_tokens_per_request = 0.0;
        std::chrono::seconds uptime{0};
        std::string deployment_environment;
        std::string software_version;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void recordBusinessMetrics(const BusinessMetrics& metrics);
    
    // Custom metrics collection
    void startCustomMetricCollection(const QString& metric_name, 
                                   std::function<double()> value_func,
                                   std::chrono::seconds interval);
    void stopCustomMetricCollection(const QString& metric_name);

signals:
    void metricsReported(bool success, const QString& error);
    void backendConnectionLost();
    void backendConnectionRestored();

private slots:
    void reportMetrics();
    void handleBackendResponse(QNetworkReply* reply);
    void checkBackendHealth();

private:
    // Backend configuration
    QString current_backend = "prometheus";
    QString metrics_endpoint;
    QString auth_token;
    QString auth_type = "Bearer";
    std::chrono::seconds reporting_interval{10};
    
    // HTTP client for metrics reporting
    QNetworkAccessManager* network_manager;
    QTimer* reporting_timer;
    QTimer* health_check_timer;
    
    // Metrics storage
    std::vector<std::map<QString, QVariant>> metric_buffer;
    std::mutex metric_buffer_mutex;
    std::atomic<bool> reporting_active{false};
    std::atomic<bool> backend_healthy{true};
    
    // Performance tracking
    std::map<QString, std::vector<double>> latency_histograms;
    std::map<QString, uint64_t> counters;
    std::map<QString, double> gauges;
    std::chrono::steady_clock::time_point start_time;
    
    // Custom metric collectors
    std::map<QString, QTimer*> custom_metric_timers;
    std::map<QString, std::function<double()>> custom_metric_functions;
    
    // Backend-specific formatters
    QByteArray formatPrometheusMetrics();
    QByteArray formatInfluxDBMetrics();
    QByteArray formatCloudWatchMetrics();
    QByteArray formatCustomMetrics();
    
    // Metric builders
    void buildPrometheusMetric(QByteArray& output, const QString& name, double value,
                              const std::map<QString, QString>& tags, const QString& type);
    void buildInfluxDBMetric(QByteArray& output, const QString& name, double value,
                            const std::map<QString, QString>& tags, const QString& measurement);
    
    // Utility methods
    QString escapeLabel(const QString& label) const;
    QString escapeMeasurement(const QString& measurement) const;
    QByteArray createAuthenticationHeader() const;
    bool validateMetricName(const QString& name) const;
    bool validateTagName(const QString& name) const;
    
    // Health monitoring
    void checkPrometheusHealth();
    void checkInfluxDBHealth();
    void checkCloudWatchHealth();
    void checkCustomBackendHealth();
    
    // Metric aggregation
    double calculateHistogramPercentile(const std::vector<double>& values, double percentile);
    void aggregateMetrics();
    void clearExpiredMetrics();
};
