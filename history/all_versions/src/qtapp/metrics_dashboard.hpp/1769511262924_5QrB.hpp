/**
 * @file metrics_dashboard.hpp
 * @brief Real-time monitoring dashboard for circuit breaker, ModelInvoker, and GGUF server
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <memory>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QTableWidget;
// class QChartView;
// class QLineSeries;
// class QChart;
class QPushButton;
class QComboBox;
QT_END_NAMESPACE

/*
namespace QtCharts {
    class QChart;
    class QChartView;
    class QLineSeries;
    class QValueAxis;
}
*/

/**
 * @struct MetricSnapshot
 * @brief Single metric data point with timestamp
 */
struct MetricSnapshot {
    QDateTime timestamp;
    QString metricName;
    double value;
    QMap<QString, QString> labels;  // For dimensions (backend, model, etc.)
};

/**
 * @struct CircuitBreakerMetrics
 * @brief Circuit breaker state and statistics
 */
struct CircuitBreakerMetrics {
    QString backend;
    bool isOpen = false;
    int failureCount = 0;
    QDateTime lastFailureTime;
    QDateTime lastSuccessTime;
    int totalRequests = 0;
    int totalFailures = 0;
    double failureRate = 0.0;  // Percentage
    QDateTime circuitOpenedAt;
    int timeUntilResetMs = 0;
};

/**
 * @struct ModelInvokerMetrics
 * @brief LLM invocation statistics
 */
struct ModelInvokerMetrics {
    QString backend;
    int requestsInProgress = 0;
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    double successRate = 0.0;
    double avgLatencyMs = 0.0;
    double p50LatencyMs = 0.0;
    double p95LatencyMs = 0.0;
    double p99LatencyMs = 0.0;
    int totalTokensGenerated = 0;
    double avgTokensPerRequest = 0.0;
    QDateTime lastRequestTime;
};

/**
 * @struct GGUFServerMetrics
 * @brief GGUF server load and performance
 */
struct GGUFServerMetrics {
    bool isRunning = false;
    int activeConnections = 0;
    int totalModelsLoaded = 0;
    int requestQueueDepth = 0;
    double memoryUsageMB = 0.0;
    double gpuMemoryUsageMB = 0.0;
    double cpuUtilization = 0.0;
    double gpuUtilization = 0.0;
    double avgInferenceTimeMs = 0.0;
    int totalInferences = 0;
    QDateTime serverStartTime;
    qint64 uptimeSeconds = 0;
};

/**
 * @class MetricsCollector
 * @brief Thread-safe collector for metrics from various components
 */
class MetricsCollector : public QObject {
    Q_OBJECT

public:
    explicit MetricsCollector(QObject* parent = nullptr);
    ~MetricsCollector();

    // Record metrics
    void recordCircuitBreakerMetric(const CircuitBreakerMetrics& metric);
    void recordModelInvokerMetric(const ModelInvokerMetrics& metric);
    void recordGGUFServerMetric(const GGUFServerMetrics& metric);
    void recordCustomMetric(const QString& name, double value, 
                          const QMap<QString, QString>& labels = {});

    // Query metrics
    QVector<CircuitBreakerMetrics> getCircuitBreakerHistory(const QString& backend = QString(), 
                                                            int lastNMinutes = 60) const;
    QVector<ModelInvokerMetrics> getModelInvokerHistory(const QString& backend = QString(),
                                                        int lastNMinutes = 60) const;
    QVector<GGUFServerMetrics> getGGUFServerHistory(int lastNMinutes = 60) const;
    QVector<MetricSnapshot> getCustomMetricHistory(const QString& metricName, 
                                                   int lastNMinutes = 60) const;

    // Current state
    CircuitBreakerMetrics getCurrentCircuitBreakerState(const QString& backend) const;
    ModelInvokerMetrics getCurrentModelInvokerState(const QString& backend) const;
    GGUFServerMetrics getCurrentGGUFServerState() const;

    // Export for Prometheus/OpenTelemetry
    QString exportPrometheusFormat() const;
    QByteArray exportOpenTelemetryFormat() const;

    // Configuration
    void setRetentionPeriod(int minutes);
    void setCollectionInterval(int milliseconds);

signals:
    void circuitBreakerStateChanged(const QString& backend, bool isOpen);
    void highFailureRateDetected(const QString& backend, double failureRate);
    void ggufServerDown();
    void ggufServerUp();
    void memoryThresholdExceeded(double usageMB);

private slots:
    void cleanup();  // Remove old metrics beyond retention period

private:
    mutable QMutex m_mutex;
    
    // Metric storage
    QVector<CircuitBreakerMetrics> m_circuitBreakerHistory;
    QVector<ModelInvokerMetrics> m_modelInvokerHistory;
    QVector<GGUFServerMetrics> m_ggufServerHistory;
    QMap<QString, QVector<MetricSnapshot>> m_customMetrics;
    
    // Current state cache
    QMap<QString, CircuitBreakerMetrics> m_currentCircuitBreakerState;
    QMap<QString, ModelInvokerMetrics> m_currentModelInvokerState;
    GGUFServerMetrics m_currentGGUFServerState;
    
    // Configuration
    int m_retentionMinutes = 120;  // 2 hours default
    QTimer* m_cleanupTimer = nullptr;
    
    void pruneOldMetrics(QVector<CircuitBreakerMetrics>& metrics, int minutes) const;
    void pruneOldMetrics(QVector<ModelInvokerMetrics>& metrics, int minutes) const;
    void pruneOldMetrics(QVector<GGUFServerMetrics>& metrics, int minutes) const;
};

/**
 * @class MetricsDashboard
 * @brief Real-time dashboard UI for monitoring system health
 */
class MetricsDashboard : public QWidget {
    Q_OBJECT

public:
    explicit MetricsDashboard(MetricsCollector* collector, QWidget* parent = nullptr);
    ~MetricsDashboard();

public slots:
    void refreshDashboard();
    void exportMetrics();
    void resetMetrics();

private slots:
    void onTimeRangeChanged(int index);
    void onBackendFilterChanged(const QString& backend);
    void onCircuitBreakerStateChanged(const QString& backend, bool isOpen);
    void onHighFailureRateDetected(const QString& backend, double rate);

private:
    void setupUI();
    void setupCircuitBreakerSection();
    void setupModelInvokerSection();
    void setupGGUFServerSection();
    void setupChartsSection();
    void createConnections();
    
    void updateCircuitBreakerDisplay();
    void updateModelInvokerDisplay();
    void updateGGUFServerDisplay();
    void updateCharts();
    
    void updateLatencyChart();
    void updateRequestRateChart();
    void updateFailureRateChart();
    void updateMemoryChart();
    
    MetricsCollector* m_collector;
    QTimer* m_refreshTimer;
    
    // UI Components
    QComboBox* m_timeRangeCombo;
    QComboBox* m_backendFilterCombo;
    QPushButton* m_exportBtn;
    QPushButton* m_resetBtn;
    
    // Circuit Breaker Section
    QTableWidget* m_circuitBreakerTable;
    QLabel* m_circuitBreakerStatus;
    
    // Model Invoker Section
    QTableWidget* m_modelInvokerTable;
    QLabel* m_successRateLabel;
    QLabel* m_avgLatencyLabel;
    QProgressBar* m_requestProgressBar;
    
    // GGUF Server Section
    QLabel* m_ggufStatusLabel;
    QLabel* m_ggufConnectionsLabel;
    QLabel* m_ggufMemoryLabel;
    QLabel* m_ggufUptimeLabel;
    QProgressBar* m_cpuUsageBar;
    QProgressBar* m_gpuUsageBar;
    
    // Charts
    QtCharts::QChartView* m_latencyChartView;
    QtCharts::QChartView* m_requestRateChartView;
    QtCharts::QChartView* m_failureRateChartView;
    QtCharts::QChartView* m_memoryChartView;
    
    QtCharts::QChart* m_latencyChart;
    QtCharts::QChart* m_requestRateChart;
    QtCharts::QChart* m_failureRateChart;
    QtCharts::QChart* m_memoryChart;
    
    QMap<QString, QtCharts::QLineSeries*> m_latencySeries;
    QMap<QString, QtCharts::QLineSeries*> m_requestRateSeries;
    QMap<QString, QtCharts::QLineSeries*> m_failureRateSeries;
    
    // State
    int m_selectedTimeRangeMinutes = 60;
    QString m_selectedBackend = "All";
};

/**
 * @class PrometheusExporter
 * @brief Exposes metrics in Prometheus format via HTTP endpoint
 */
class PrometheusExporter : public QObject {
    Q_OBJECT

public:
    explicit PrometheusExporter(MetricsCollector* collector, QObject* parent = nullptr);
    ~PrometheusExporter();

    bool start(quint16 port = 9090);
    void stop();
    bool isRunning() const { return m_isRunning; }

signals:
    void scrapeRequested();
    void exportFailed(const QString& error);

private:
    void handleRequest(const QString& path, const QString& method);
    QString generateMetricsOutput() const;
    
    MetricsCollector* m_collector;
    bool m_isRunning = false;
    quint16 m_port = 9090;
    // HTTP server implementation would go here
    // QTcpServer* m_server = nullptr;
};

