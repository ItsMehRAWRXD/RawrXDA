/**
 * @file metrics_dashboard.cpp
 * @brief Implementation of real-time metrics dashboard
 */

#include "metrics_dashboard.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QTimer>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <algorithm>

using namespace QtCharts;

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

MetricsCollector::MetricsCollector(QObject* parent)
    : QObject(parent)
{
    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(60000); // Cleanup every minute
    connect(m_cleanupTimer, &QTimer::timeout, this, &MetricsCollector::cleanup);
    m_cleanupTimer->start();
}

MetricsCollector::~MetricsCollector()
{
}

void MetricsCollector::recordCircuitBreakerMetric(const CircuitBreakerMetrics& metric)
{
    QMutexLocker locker(&m_mutex);
    
    m_circuitBreakerHistory.append(metric);
    m_currentCircuitBreakerState[metric.backend] = metric;
    
    // Emit signals for state changes
    if (metric.isOpen) {
        emit circuitBreakerStateChanged(metric.backend, true);
    }
    
    if (metric.failureRate > 50.0) {
        emit highFailureRateDetected(metric.backend, metric.failureRate);
    }
}

void MetricsCollector::recordModelInvokerMetric(const ModelInvokerMetrics& metric)
{
    QMutexLocker locker(&m_mutex);
    
    m_modelInvokerHistory.append(metric);
    m_currentModelInvokerState[metric.backend] = metric;
}

void MetricsCollector::recordGGUFServerMetric(const GGUFServerMetrics& metric)
{
    QMutexLocker locker(&m_mutex);
    
    bool wasRunning = m_currentGGUFServerState.isRunning;
    
    m_ggufServerHistory.append(metric);
    m_currentGGUFServerState = metric;
    
    // Emit state change signals
    if (!wasRunning && metric.isRunning) {
        emit ggufServerUp();
    } else if (wasRunning && !metric.isRunning) {
        emit ggufServerDown();
    }
    
    // Check memory threshold
    if (metric.memoryUsageMB > 8192.0) {  // 8GB threshold
        emit memoryThresholdExceeded(metric.memoryUsageMB);
    }
}

void MetricsCollector::recordCustomMetric(const QString& name, double value,
                                         const QMap<QString, QString>& labels)
{
    QMutexLocker locker(&m_mutex);
    
    MetricSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();
    snapshot.metricName = name;
    snapshot.value = value;
    snapshot.labels = labels;
    
    m_customMetrics[name].append(snapshot);
}

QVector<CircuitBreakerMetrics> MetricsCollector::getCircuitBreakerHistory(
    const QString& backend, int lastNMinutes) const
{
    QMutexLocker locker(&m_mutex);
    
    QVector<CircuitBreakerMetrics> result;
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-lastNMinutes * 60);
    
    for (const auto& metric : m_circuitBreakerHistory) {
        if (metric.lastFailureTime >= cutoff) {
            if (backend.isEmpty() || metric.backend == backend) {
                result.append(metric);
            }
        }
    }
    
    return result;
}

QVector<ModelInvokerMetrics> MetricsCollector::getModelInvokerHistory(
    const QString& backend, int lastNMinutes) const
{
    QMutexLocker locker(&m_mutex);
    
    QVector<ModelInvokerMetrics> result;
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-lastNMinutes * 60);
    
    for (const auto& metric : m_modelInvokerHistory) {
        if (metric.lastRequestTime >= cutoff) {
            if (backend.isEmpty() || metric.backend == backend) {
                result.append(metric);
            }
        }
    }
    
    return result;
}

QVector<GGUFServerMetrics> MetricsCollector::getGGUFServerHistory(int lastNMinutes) const
{
    QMutexLocker locker(&m_mutex);
    
    QVector<GGUFServerMetrics> result;
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-lastNMinutes * 60);
    
    for (const auto& metric : m_ggufServerHistory) {
        if (metric.serverStartTime >= cutoff) {
            result.append(metric);
        }
    }
    
    return result;
}

QVector<MetricSnapshot> MetricsCollector::getCustomMetricHistory(
    const QString& metricName, int lastNMinutes) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_customMetrics.contains(metricName)) {
        return {};
    }
    
    QVector<MetricSnapshot> result;
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-lastNMinutes * 60);
    
    for (const auto& snapshot : m_customMetrics[metricName]) {
        if (snapshot.timestamp >= cutoff) {
            result.append(snapshot);
        }
    }
    
    return result;
}

CircuitBreakerMetrics MetricsCollector::getCurrentCircuitBreakerState(const QString& backend) const
{
    QMutexLocker locker(&m_mutex);
    return m_currentCircuitBreakerState.value(backend, CircuitBreakerMetrics());
}

ModelInvokerMetrics MetricsCollector::getCurrentModelInvokerState(const QString& backend) const
{
    QMutexLocker locker(&m_mutex);
    return m_currentModelInvokerState.value(backend, ModelInvokerMetrics());
}

GGUFServerMetrics MetricsCollector::getCurrentGGUFServerState() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGGUFServerState;
}

QString MetricsCollector::exportPrometheusFormat() const
{
    QMutexLocker locker(&m_mutex);
    
    QString output;
    QTextStream stream(&output);
    
    // Circuit Breaker Metrics
    stream << "# HELP circuit_breaker_open Circuit breaker open state (1=open, 0=closed)\n";
    stream << "# TYPE circuit_breaker_open gauge\n";
    for (auto it = m_currentCircuitBreakerState.begin(); 
         it != m_currentCircuitBreakerState.end(); ++it) {
        stream << "circuit_breaker_open{backend=\"" << it.key() << "\"} "
               << (it.value().isOpen ? 1 : 0) << "\n";
    }
    
    stream << "# HELP circuit_breaker_failure_count Current failure count\n";
    stream << "# TYPE circuit_breaker_failure_count gauge\n";
    for (auto it = m_currentCircuitBreakerState.begin();
         it != m_currentCircuitBreakerState.end(); ++it) {
        stream << "circuit_breaker_failure_count{backend=\"" << it.key() << "\"} "
               << it.value().failureCount << "\n";
    }
    
    stream << "# HELP circuit_breaker_failure_rate Failure rate percentage\n";
    stream << "# TYPE circuit_breaker_failure_rate gauge\n";
    for (auto it = m_currentCircuitBreakerState.begin();
         it != m_currentCircuitBreakerState.end(); ++it) {
        stream << "circuit_breaker_failure_rate{backend=\"" << it.key() << "\"} "
               << it.value().failureRate << "\n";
    }
    
    // Model Invoker Metrics
    stream << "# HELP model_invoker_requests_total Total requests\n";
    stream << "# TYPE model_invoker_requests_total counter\n";
    for (auto it = m_currentModelInvokerState.begin();
         it != m_currentModelInvokerState.end(); ++it) {
        stream << "model_invoker_requests_total{backend=\"" << it.key() << "\"} "
               << it.value().totalRequests << "\n";
    }
    
    stream << "# HELP model_invoker_requests_successful Successful requests\n";
    stream << "# TYPE model_invoker_requests_successful counter\n";
    for (auto it = m_currentModelInvokerState.begin();
         it != m_currentModelInvokerState.end(); ++it) {
        stream << "model_invoker_requests_successful{backend=\"" << it.key() << "\"} "
               << it.value().successfulRequests << "\n";
    }
    
    stream << "# HELP model_invoker_latency_avg Average latency in milliseconds\n";
    stream << "# TYPE model_invoker_latency_avg gauge\n";
    for (auto it = m_currentModelInvokerState.begin();
         it != m_currentModelInvokerState.end(); ++it) {
        stream << "model_invoker_latency_avg{backend=\"" << it.key() << "\"} "
               << it.value().avgLatencyMs << "\n";
    }
    
    stream << "# HELP model_invoker_latency_p95 95th percentile latency\n";
    stream << "# TYPE model_invoker_latency_p95 gauge\n";
    for (auto it = m_currentModelInvokerState.begin();
         it != m_currentModelInvokerState.end(); ++it) {
        stream << "model_invoker_latency_p95{backend=\"" << it.key() << "\"} "
               << it.value().p95LatencyMs << "\n";
    }
    
    // GGUF Server Metrics
    stream << "# HELP gguf_server_running Server running state\n";
    stream << "# TYPE gguf_server_running gauge\n";
    stream << "gguf_server_running " << (m_currentGGUFServerState.isRunning ? 1 : 0) << "\n";
    
    stream << "# HELP gguf_server_active_connections Active connections\n";
    stream << "# TYPE gguf_server_active_connections gauge\n";
    stream << "gguf_server_active_connections " << m_currentGGUFServerState.activeConnections << "\n";
    
    stream << "# HELP gguf_server_memory_mb Memory usage in MB\n";
    stream << "# TYPE gguf_server_memory_mb gauge\n";
    stream << "gguf_server_memory_mb " << m_currentGGUFServerState.memoryUsageMB << "\n";
    
    stream << "# HELP gguf_server_cpu_utilization CPU utilization percentage\n";
    stream << "# TYPE gguf_server_cpu_utilization gauge\n";
    stream << "gguf_server_cpu_utilization " << m_currentGGUFServerState.cpuUtilization << "\n";
    
    stream << "# HELP gguf_server_gpu_utilization GPU utilization percentage\n";
    stream << "# TYPE gguf_server_gpu_utilization gauge\n";
    stream << "gguf_server_gpu_utilization " << m_currentGGUFServerState.gpuUtilization << "\n";
    
    return output;
}

QByteArray MetricsCollector::exportOpenTelemetryFormat() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject root;
    QJsonArray resourceMetrics;
    
    // Circuit Breaker Metrics
    QJsonObject cbMetrics;
    QJsonArray cbData;
    for (auto it = m_currentCircuitBreakerState.begin();
         it != m_currentCircuitBreakerState.end(); ++it) {
        QJsonObject metric;
        metric["backend"] = it.key();
        metric["isOpen"] = it.value().isOpen;
        metric["failureCount"] = it.value().failureCount;
        metric["failureRate"] = it.value().failureRate;
        metric["totalRequests"] = it.value().totalRequests;
        cbData.append(metric);
    }
    cbMetrics["circuitBreakers"] = cbData;
    
    // Model Invoker Metrics
    QJsonObject miMetrics;
    QJsonArray miData;
    for (auto it = m_currentModelInvokerState.begin();
         it != m_currentModelInvokerState.end(); ++it) {
        QJsonObject metric;
        metric["backend"] = it.key();
        metric["totalRequests"] = it.value().totalRequests;
        metric["successfulRequests"] = it.value().successfulRequests;
        metric["avgLatencyMs"] = it.value().avgLatencyMs;
        metric["p95LatencyMs"] = it.value().p95LatencyMs;
        miData.append(metric);
    }
    miMetrics["modelInvokers"] = miData;
    
    // GGUF Server Metrics
    QJsonObject ggufMetrics;
    ggufMetrics["isRunning"] = m_currentGGUFServerState.isRunning;
    ggufMetrics["activeConnections"] = m_currentGGUFServerState.activeConnections;
    ggufMetrics["memoryUsageMB"] = m_currentGGUFServerState.memoryUsageMB;
    ggufMetrics["cpuUtilization"] = m_currentGGUFServerState.cpuUtilization;
    ggufMetrics["gpuUtilization"] = m_currentGGUFServerState.gpuUtilization;
    
    root["circuitBreakerMetrics"] = cbMetrics;
    root["modelInvokerMetrics"] = miMetrics;
    root["ggufServerMetrics"] = ggufMetrics;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void MetricsCollector::setRetentionPeriod(int minutes)
{
    QMutexLocker locker(&m_mutex);
    m_retentionMinutes = minutes;
}

void MetricsCollector::setCollectionInterval(int milliseconds)
{
    m_cleanupTimer->setInterval(milliseconds);
}

void MetricsCollector::cleanup()
{
    QMutexLocker locker(&m_mutex);
    
    pruneOldMetrics(m_circuitBreakerHistory, m_retentionMinutes);
    pruneOldMetrics(m_modelInvokerHistory, m_retentionMinutes);
    pruneOldMetrics(m_ggufServerHistory, m_retentionMinutes);
    
    // Clean up custom metrics
    for (auto it = m_customMetrics.begin(); it != m_customMetrics.end(); ++it) {
        QDateTime cutoff = QDateTime::currentDateTime().addSecs(-m_retentionMinutes * 60);
        it.value().erase(
            std::remove_if(it.value().begin(), it.value().end(),
                [&cutoff](const MetricSnapshot& snap) {
                    return snap.timestamp < cutoff;
                }),
            it.value().end()
        );
    }
}

void MetricsCollector::pruneOldMetrics(QVector<CircuitBreakerMetrics>& metrics, int minutes) const
{
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-minutes * 60);
    metrics.erase(
        std::remove_if(metrics.begin(), metrics.end(),
            [&cutoff](const CircuitBreakerMetrics& m) {
                return m.lastFailureTime < cutoff;
            }),
        metrics.end()
    );
}

void MetricsCollector::pruneOldMetrics(QVector<ModelInvokerMetrics>& metrics, int minutes) const
{
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-minutes * 60);
    metrics.erase(
        std::remove_if(metrics.begin(), metrics.end(),
            [&cutoff](const ModelInvokerMetrics& m) {
                return m.lastRequestTime < cutoff;
            }),
        metrics.end()
    );
}

void MetricsCollector::pruneOldMetrics(QVector<GGUFServerMetrics>& metrics, int minutes) const
{
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-minutes * 60);
    metrics.erase(
        std::remove_if(metrics.begin(), metrics.end(),
            [&cutoff](const GGUFServerMetrics& m) {
                return m.serverStartTime < cutoff;
            }),
        metrics.end()
    );
}

// ============================================================================
// MetricsDashboard Implementation
// ============================================================================

MetricsDashboard::MetricsDashboard(MetricsCollector* collector, QWidget* parent)
    : QWidget(parent)
    , m_collector(collector)
{
    setupUI();
    createConnections();
    
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2000); // Refresh every 2 seconds
    connect(m_refreshTimer, &QTimer::timeout, this, &MetricsDashboard::refreshDashboard);
    m_refreshTimer->start();
    
    refreshDashboard();
}

MetricsDashboard::~MetricsDashboard()
{
}

void MetricsDashboard::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Top control bar
    auto* controlBar = new QHBoxLayout();
    
    m_timeRangeCombo = new QComboBox();
    m_timeRangeCombo->addItem("Last 15 minutes", 15);
    m_timeRangeCombo->addItem("Last hour", 60);
    m_timeRangeCombo->addItem("Last 4 hours", 240);
    m_timeRangeCombo->addItem("Last day", 1440);
    m_timeRangeCombo->setCurrentIndex(1);
    
    m_backendFilterCombo = new QComboBox();
    m_backendFilterCombo->addItem("All Backends");
    m_backendFilterCombo->addItem("Ollama");
    m_backendFilterCombo->addItem("Claude");
    m_backendFilterCombo->addItem("OpenAI");
    
    m_exportBtn = new QPushButton("Export Metrics");
    m_resetBtn = new QPushButton("Reset");
    
    controlBar->addWidget(new QLabel("Time Range:"));
    controlBar->addWidget(m_timeRangeCombo);
    controlBar->addWidget(new QLabel("Backend:"));
    controlBar->addWidget(m_backendFilterCombo);
    controlBar->addStretch();
    controlBar->addWidget(m_exportBtn);
    controlBar->addWidget(m_resetBtn);
    
    mainLayout->addLayout(controlBar);
    
    // Content sections
    setupCircuitBreakerSection();
    setupModelInvokerSection();
    setupGGUFServerSection();
    setupChartsSection();
}

void MetricsDashboard::setupCircuitBreakerSection()
{
    auto* groupBox = new QGroupBox("Circuit Breaker Status");
    auto* layout = new QVBoxLayout(groupBox);
    
    m_circuitBreakerStatus = new QLabel("Initializing...");
    m_circuitBreakerStatus->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(m_circuitBreakerStatus);
    
    m_circuitBreakerTable = new QTableWidget(0, 6);
    m_circuitBreakerTable->setHorizontalHeaderLabels({
        "Backend", "State", "Failure Count", "Total Requests", "Failure Rate", "Last Failure"
    });
    m_circuitBreakerTable->horizontalHeader()->setStretchLastSection(true);
    m_circuitBreakerTable->setAlternatingRowColors(true);
    m_circuitBreakerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_circuitBreakerTable);
    
    qobject_cast<QVBoxLayout*>(this->layout())->addWidget(groupBox);
}

void MetricsDashboard::setupModelInvokerSection()
{
    auto* groupBox = new QGroupBox("Model Invoker Statistics");
    auto* layout = new QVBoxLayout(groupBox);
    
    auto* statsLayout = new QHBoxLayout();
    
    m_successRateLabel = new QLabel("Success Rate: --");
    m_avgLatencyLabel = new QLabel("Avg Latency: --");
    m_requestProgressBar = new QProgressBar();
    m_requestProgressBar->setMaximum(100);
    m_requestProgressBar->setFormat("Requests in Progress: %v");
    
    statsLayout->addWidget(m_successRateLabel);
    statsLayout->addWidget(m_avgLatencyLabel);
    statsLayout->addWidget(m_requestProgressBar);
    
    layout->addLayout(statsLayout);
    
    m_modelInvokerTable = new QTableWidget(0, 7);
    m_modelInvokerTable->setHorizontalHeaderLabels({
        "Backend", "Total", "Success", "Failed", "Success Rate", "Avg Latency", "P95 Latency"
    });
    m_modelInvokerTable->horizontalHeader()->setStretchLastSection(true);
    m_modelInvokerTable->setAlternatingRowColors(true);
    m_modelInvokerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_modelInvokerTable);
    
    qobject_cast<QVBoxLayout*>(this->layout())->addWidget(groupBox);
}

void MetricsDashboard::setupGGUFServerSection()
{
    auto* groupBox = new QGroupBox("GGUF Server Status");
    auto* layout = new QGridLayout(groupBox);
    
    m_ggufStatusLabel = new QLabel("Status: Unknown");
    m_ggufConnectionsLabel = new QLabel("Connections: 0");
    m_ggufMemoryLabel = new QLabel("Memory: 0 MB");
    m_ggufUptimeLabel = new QLabel("Uptime: --");
    
    m_cpuUsageBar = new QProgressBar();
    m_cpuUsageBar->setMaximum(100);
    m_cpuUsageBar->setFormat("CPU: %p%");
    
    m_gpuUsageBar = new QProgressBar();
    m_gpuUsageBar->setMaximum(100);
    m_gpuUsageBar->setFormat("GPU: %p%");
    
    layout->addWidget(m_ggufStatusLabel, 0, 0);
    layout->addWidget(m_ggufConnectionsLabel, 0, 1);
    layout->addWidget(m_ggufMemoryLabel, 1, 0);
    layout->addWidget(m_ggufUptimeLabel, 1, 1);
    layout->addWidget(m_cpuUsageBar, 2, 0, 1, 2);
    layout->addWidget(m_gpuUsageBar, 3, 0, 1, 2);
    
    qobject_cast<QVBoxLayout*>(this->layout())->addWidget(groupBox);
}

void MetricsDashboard::setupChartsSection()
{
    auto* groupBox = new QGroupBox("Performance Charts");
    auto* layout = new QGridLayout(groupBox);
    
    // Latency chart
    m_latencyChart = new QChart();
    m_latencyChart->setTitle("Latency Over Time");
    m_latencyChart->legend()->setVisible(true);
    m_latencyChartView = new QChartView(m_latencyChart);
    m_latencyChartView->setRenderHint(QPainter::Antialiasing);
    
    // Request rate chart
    m_requestRateChart = new QChart();
    m_requestRateChart->setTitle("Request Rate");
    m_requestRateChartView = new QChartView(m_requestRateChart);
    m_requestRateChartView->setRenderHint(QPainter::Antialiasing);
    
    // Failure rate chart
    m_failureRateChart = new QChart();
    m_failureRateChart->setTitle("Failure Rate");
    m_failureRateChartView = new QChartView(m_failureRateChart);
    m_failureRateChartView->setRenderHint(QPainter::Antialiasing);
    
    // Memory chart
    m_memoryChart = new QChart();
    m_memoryChart->setTitle("Memory Usage");
    m_memoryChartView = new QChartView(m_memoryChart);
    m_memoryChartView->setRenderHint(QPainter::Antialiasing);
    
    layout->addWidget(m_latencyChartView, 0, 0);
    layout->addWidget(m_requestRateChartView, 0, 1);
    layout->addWidget(m_failureRateChartView, 1, 0);
    layout->addWidget(m_memoryChartView, 1, 1);
    
    qobject_cast<QVBoxLayout*>(this->layout())->addWidget(groupBox);
}

void MetricsDashboard::createConnections()
{
    connect(m_timeRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MetricsDashboard::onTimeRangeChanged);
    connect(m_backendFilterCombo, &QComboBox::currentTextChanged,
            this, &MetricsDashboard::onBackendFilterChanged);
    connect(m_exportBtn, &QPushButton::clicked, this, &MetricsDashboard::exportMetrics);
    connect(m_resetBtn, &QPushButton::clicked, this, &MetricsDashboard::resetMetrics);
    
    connect(m_collector, &MetricsCollector::circuitBreakerStateChanged,
            this, &MetricsDashboard::onCircuitBreakerStateChanged);
    connect(m_collector, &MetricsCollector::highFailureRateDetected,
            this, &MetricsDashboard::onHighFailureRateDetected);
}

void MetricsDashboard::refreshDashboard()
{
    updateCircuitBreakerDisplay();
    updateModelInvokerDisplay();
    updateGGUFServerDisplay();
    updateCharts();
}

void MetricsDashboard::updateCircuitBreakerDisplay()
{
    auto history = m_collector->getCircuitBreakerHistory(
        m_selectedBackend == "All" ? QString() : m_selectedBackend,
        m_selectedTimeRangeMinutes
    );
    
    m_circuitBreakerTable->setRowCount(0);
    
    QSet<QString> backends;
    for (const auto& metric : history) {
        backends.insert(metric.backend);
    }
    
    int openCount = 0;
    for (const QString& backend : backends) {
        auto current = m_collector->getCurrentCircuitBreakerState(backend);
        if (current.isOpen) openCount++;
        
        int row = m_circuitBreakerTable->rowCount();
        m_circuitBreakerTable->insertRow(row);
        
        m_circuitBreakerTable->setItem(row, 0, new QTableWidgetItem(backend));
        
        auto* stateItem = new QTableWidgetItem(current.isOpen ? "OPEN" : "CLOSED");
        stateItem->setForeground(current.isOpen ? Qt::red : Qt::green);
        m_circuitBreakerTable->setItem(row, 1, stateItem);
        
        m_circuitBreakerTable->setItem(row, 2, 
            new QTableWidgetItem(QString::number(current.failureCount)));
        m_circuitBreakerTable->setItem(row, 3,
            new QTableWidgetItem(QString::number(current.totalRequests)));
        m_circuitBreakerTable->setItem(row, 4,
            new QTableWidgetItem(QString::number(current.failureRate, 'f', 1) + "%"));
        m_circuitBreakerTable->setItem(row, 5,
            new QTableWidgetItem(current.lastFailureTime.toString("hh:mm:ss")));
    }
    
    QString status = openCount > 0 
        ? QString("<span style='color:red;'>%1 Circuit Breaker(s) OPEN</span>").arg(openCount)
        : "<span style='color:green;'>All Circuit Breakers CLOSED</span>";
    m_circuitBreakerStatus->setText(status);
}

void MetricsDashboard::updateModelInvokerDisplay()
{
    auto history = m_collector->getModelInvokerHistory(
        m_selectedBackend == "All" ? QString() : m_selectedBackend,
        m_selectedTimeRangeMinutes
    );
    
    m_modelInvokerTable->setRowCount(0);
    
    QSet<QString> backends;
    for (const auto& metric : history) {
        backends.insert(metric.backend);
    }
    
    double totalSuccessRate = 0.0;
    double totalAvgLatency = 0.0;
    int totalInProgress = 0;
    
    for (const QString& backend : backends) {
        auto current = m_collector->getCurrentModelInvokerState(backend);
        totalSuccessRate += current.successRate;
        totalAvgLatency += current.avgLatencyMs;
        totalInProgress += current.requestsInProgress;
        
        int row = m_modelInvokerTable->rowCount();
        m_modelInvokerTable->insertRow(row);
        
        m_modelInvokerTable->setItem(row, 0, new QTableWidgetItem(backend));
        m_modelInvokerTable->setItem(row, 1,
            new QTableWidgetItem(QString::number(current.totalRequests)));
        m_modelInvokerTable->setItem(row, 2,
            new QTableWidgetItem(QString::number(current.successfulRequests)));
        m_modelInvokerTable->setItem(row, 3,
            new QTableWidgetItem(QString::number(current.failedRequests)));
        m_modelInvokerTable->setItem(row, 4,
            new QTableWidgetItem(QString::number(current.successRate, 'f', 1) + "%"));
        m_modelInvokerTable->setItem(row, 5,
            new QTableWidgetItem(QString::number(current.avgLatencyMs, 'f', 0) + " ms"));
        m_modelInvokerTable->setItem(row, 6,
            new QTableWidgetItem(QString::number(current.p95LatencyMs, 'f', 0) + " ms"));
    }
    
    if (!backends.isEmpty()) {
        m_successRateLabel->setText(QString("Success Rate: %1%")
            .arg(totalSuccessRate / backends.size(), 0, 'f', 1));
        m_avgLatencyLabel->setText(QString("Avg Latency: %1 ms")
            .arg(totalAvgLatency / backends.size(), 0, 'f', 0));
    }
    m_requestProgressBar->setValue(totalInProgress);
}

void MetricsDashboard::updateGGUFServerDisplay()
{
    auto current = m_collector->getCurrentGGUFServerState();
    
    m_ggufStatusLabel->setText(current.isRunning ? 
        "<span style='color:green;'>Status: Running</span>" :
        "<span style='color:red;'>Status: Stopped</span>");
    
    m_ggufConnectionsLabel->setText(QString("Connections: %1").arg(current.activeConnections));
    m_ggufMemoryLabel->setText(QString("Memory: %1 MB").arg(current.memoryUsageMB, 0, 'f', 0));
    
    qint64 uptimeMin = current.uptimeSeconds / 60;
    m_ggufUptimeLabel->setText(QString("Uptime: %1:%2")
        .arg(uptimeMin / 60, 2, 10, QChar('0'))
        .arg(uptimeMin % 60, 2, 10, QChar('0')));
    
    m_cpuUsageBar->setValue(static_cast<int>(current.cpuUtilization));
    m_gpuUsageBar->setValue(static_cast<int>(current.gpuUtilization));
}

void MetricsDashboard::updateCharts()
{
    updateLatencyChart();
    updateRequestRateChart();
    updateFailureRateChart();
    updateMemoryChart();
}

void MetricsDashboard::updateLatencyChart()
{
    // Implementation for latency chart update
    // Would plot avgLatencyMs, p95LatencyMs, p99LatencyMs over time
}

void MetricsDashboard::updateRequestRateChart()
{
    // Implementation for request rate chart
}

void MetricsDashboard::updateFailureRateChart()
{
    // Implementation for failure rate chart
}

void MetricsDashboard::updateMemoryChart()
{
    // Implementation for memory usage chart
}

void MetricsDashboard::exportMetrics()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Metrics", QString(), "JSON Files (*.json);;Prometheus Format (*.prom)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export Failed", "Could not write to file");
        return;
    }
    
    if (fileName.endsWith(".prom")) {
        file.write(m_collector->exportPrometheusFormat().toUtf8());
    } else {
        file.write(m_collector->exportOpenTelemetryFormat());
    }
    
    file.close();
    QMessageBox::information(this, "Export Complete", "Metrics exported successfully");
}

void MetricsDashboard::resetMetrics()
{
    // Implementation for resetting metrics
}

void MetricsDashboard::onTimeRangeChanged(int index)
{
    m_selectedTimeRangeMinutes = m_timeRangeCombo->itemData(index).toInt();
    refreshDashboard();
}

void MetricsDashboard::onBackendFilterChanged(const QString& backend)
{
    m_selectedBackend = backend;
    refreshDashboard();
}

void MetricsDashboard::onCircuitBreakerStateChanged(const QString& backend, bool isOpen)
{
    qDebug() << "[MetricsDashboard] Circuit breaker state changed:" << backend << isOpen;
    refreshDashboard();
}

void MetricsDashboard::onHighFailureRateDetected(const QString& backend, double rate)
{
    QMessageBox::warning(this, "High Failure Rate",
        QString("Backend '%1' has high failure rate: %2%").arg(backend).arg(rate, 0, 'f', 1));
}

// ============================================================================
// PrometheusExporter Implementation
// ============================================================================

PrometheusExporter::PrometheusExporter(MetricsCollector* collector, QObject* parent)
    : QObject(parent)
    , m_collector(collector)
{
}

PrometheusExporter::~PrometheusExporter()
{
    stop();
}

bool PrometheusExporter::start(quint16 port)
{
    m_port = port;
    m_isRunning = true;
    
    // HTTP server implementation would start here
    qDebug() << "[PrometheusExporter] Started on port" << port;
    
    return true;
}

void PrometheusExporter::stop()
{
    m_isRunning = false;
    qDebug() << "[PrometheusExporter] Stopped";
}

void PrometheusExporter::handleRequest(const QString& path, const QString& method)
{
    if (path == "/metrics" && method == "GET") {
        emit scrapeRequested();
        // Return m_collector->exportPrometheusFormat()
    }
}

QString PrometheusExporter::generateMetricsOutput() const
{
    return m_collector->exportPrometheusFormat();
}
