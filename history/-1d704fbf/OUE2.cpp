/**
 * @file telemetry_hooks.cpp
 * @brief Implementation of Prometheus/OpenTelemetry metrics instrumentation
 */

#include "telemetry_hooks.hpp"
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <algorithm>
#include <numeric>
#include <mutex>

namespace RawrXD {

// ─────────────────────────────────────────────────────────────────────
// MetricsCollector Implementation
// ─────────────────────────────────────────────────────────────────────

QMap<QString, MetricsCollector::MetricPoint> MetricsCollector::s_metrics;
std::mutex MetricsCollector::s_metricsMutex;

void MetricsCollector::recordCounter(const QString& name, double value,
                                     const QMap<QString, QString>& labels)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    QString key = name;
    if (!labels.isEmpty()) {
        for (auto it = labels.begin(); it != labels.end(); ++it) {
            key += QString("_%1_%2").arg(it.key(), it.value());
        }
    }

    MetricPoint& point = s_metrics[key];
    point.name = name;
    point.type = "counter";
    point.value += value;
    point.labels = labels;
    point.timestamp = QDateTime::currentDateTime();

    qDebug() << "[Metrics::Counter]" << name << "=" << point.value << "labels=" << labels;
}

void MetricsCollector::recordGauge(const QString& name, double value,
                                   const QMap<QString, QString>& labels)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    QString key = name;
    if (!labels.isEmpty()) {
        for (auto it = labels.begin(); it != labels.end(); ++it) {
            key += QString("_%1_%2").arg(it.key(), it.value());
        }
    }

    MetricPoint& point = s_metrics[key];
    point.name = name;
    point.type = "gauge";
    point.value = value;
    point.labels = labels;
    point.timestamp = QDateTime::currentDateTime();

    qDebug() << "[Metrics::Gauge]" << name << "=" << value << "labels=" << labels;
}

void MetricsCollector::recordHistogram(const QString& name, double value,
                                       const QMap<QString, QString>& labels,
                                       const QVector<double>& buckets)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    QString key = name;
    if (!labels.isEmpty()) {
        for (auto it = labels.begin(); it != labels.end(); ++it) {
            key += QString("_%1_%2").arg(it.key(), it.value());
        }
    }

    MetricPoint& point = s_metrics[key];
    point.name = name;
    point.type = "histogram";
    point.value = value;
    point.labels = labels;
    point.histogram_buckets = buckets;
    point.timestamp = QDateTime::currentDateTime();

    qDebug() << "[Metrics::Histogram]" << name << "=" << value << "labels=" << labels;
}

QString MetricsCollector::exportPrometheus()
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    QString output = "# HELP RawrXD IDE Metrics\n";
    output += "# TYPE gauge untyped\n\n";

    for (auto it = s_metrics.begin(); it != s_metrics.end(); ++it) {
        const MetricPoint& point = it.value();

        // Format: metric_name{label1="value1",label2="value2"} value timestamp
        output += point.name;

        if (!point.labels.isEmpty()) {
            output += "{";
            int count = 0;
            for (auto lit = point.labels.begin(); lit != point.labels.end(); ++lit) {
                if (count > 0) output += ",";
                output += QString("%1=\"%2\"").arg(lit.key(), lit.value());
                count++;
            }
            output += "}";
        }

        output += QString(" %1 %2\n")
            .arg(point.value)
            .arg(point.timestamp.toMSecsSinceEpoch());
    }

    return output;
}

QJsonObject MetricsCollector::exportJSON()
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    root["metric_count"] = static_cast<int>(s_metrics.size());

    QJsonObject metrics;
    for (auto it = s_metrics.begin(); it != s_metrics.end(); ++it) {
        const MetricPoint& point = it.value();

        QJsonObject metric;
        metric["type"] = point.type;
        metric["value"] = point.value;
        metric["timestamp"] = point.timestamp.toSecsSinceEpoch();

        if (!point.labels.isEmpty()) {
            QJsonObject labelsObj;
            for (auto lit = point.labels.begin(); lit != point.labels.end(); ++lit) {
                labelsObj[lit.key()] = lit.value();
            }
            metric["labels"] = labelsObj;
        }

        metrics[it.key()] = metric;
    }

    root["metrics"] = metrics;
    return root;
}

void MetricsCollector::clear()
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);
    s_metrics.clear();
    qInfo() << "[Metrics] Cleared all metrics";
}

double MetricsCollector::getValue(const QString& name)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex);

    auto it = s_metrics.find(name);
    if (it != s_metrics.end()) {
        return it.value().value;
    }
    return 0.0;
}

// ─────────────────────────────────────────────────────────────────────
// LatencyRecorder Implementation
// ─────────────────────────────────────────────────────────────────────

LatencyRecorder::LatencyRecorder(const QString& metricName,
                               const QMap<QString, QString>& labels)
    : m_metricName(metricName), m_labels(labels)
{
    m_start = std::chrono::high_resolution_clock::now();
}

LatencyRecorder::~LatencyRecorder()
{
    if (!m_recorded) {
        stop();
    }
}

qint64 LatencyRecorder::stop()
{
    auto end = std::chrono::high_resolution_clock::now();
    qint64 elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();

    QString metricName = m_metricName;
    if (!metricName.endsWith("_ms") && !metricName.endsWith("_us")) {
        metricName += "_ms";
    }

    MetricsCollector::recordHistogram(metricName, static_cast<double>(elapsed), m_labels);
    m_recorded = true;

    return elapsed;
}

// ─────────────────────────────────────────────────────────────────────
// LLMMetrics Implementation
// ─────────────────────────────────────────────────────────────────────

QMap<QString, LLMMetrics::Stats> LLMMetrics::s_backendStats;
QVector<qint64> LLMMetrics::s_allLatencies;
std::mutex LLMMetrics::s_statsMutex;

void LLMMetrics::recordRequest(const Request& request)
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    Stats& stats = s_backendStats[request.backend];
    stats.totalRequests++;

    if (request.success) {
        stats.successfulRequests++;
        stats.totalLatencyMs += request.latencyMs;
        stats.totalTokens += request.tokensUsed;
        s_allLatencies.append(request.latencyMs);
    } else {
        stats.failedRequests++;
    }

    if (request.cacheHit) {
        stats.cachedResponses++;
    }

    updatePercentiles();

    // Record to MetricsCollector
    MetricsCollector::recordCounter("llm_requests_total", 1.0,
        {{"backend", request.backend}, {"success", request.success ? "true" : "false"}});
    MetricsCollector::recordHistogram("llm_request_latency_ms", static_cast<double>(request.latencyMs),
        {{"backend", request.backend}});

    qDebug() << "[LLMMetrics]" << request.backend << "latency=" << request.latencyMs << "ms"
             << "tokens=" << request.tokensUsed << "success=" << request.success;
}

void LLMMetrics::updatePercentiles()
{
    if (s_allLatencies.isEmpty()) return;

    QVector<qint64> sorted = s_allLatencies;
    std::sort(sorted.begin(), sorted.end());

    size_t p50_idx = (sorted.size() * 50) / 100;
    size_t p95_idx = (sorted.size() * 95) / 100;
    size_t p99_idx = (sorted.size() * 99) / 100;

    for (auto& stats : s_backendStats) {
        stats.p50LatencyMs = p50_idx < sorted.size() ? sorted[p50_idx] : 0;
        stats.p95LatencyMs = p95_idx < sorted.size() ? sorted[p95_idx] : 0;
        stats.p99LatencyMs = p99_idx < sorted.size() ? sorted[p99_idx] : 0;
    }
}

QJsonObject LLMMetrics::getStatistics()
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    QJsonObject root;
    root["total_requests"] = static_cast<int>(std::accumulate(
        s_backendStats.begin(), s_backendStats.end(), 0LL,
        [](qint64 sum, const Stats& s) { return sum + s.totalRequests; }));

    root["success_rate"] = static_cast<double>(std::accumulate(
        s_backendStats.begin(), s_backendStats.end(), 0LL,
        [](qint64 sum, const Stats& s) { return sum + s.successfulRequests; })) / std::max(1LL,
        std::accumulate(s_backendStats.begin(), s_backendStats.end(), 0LL,
        [](qint64 sum, const Stats& s) { return sum + s.totalRequests; }));

    QJsonArray backends;
    for (auto it = s_backendStats.begin(); it != s_backendStats.end(); ++it) {
        QJsonObject backend;
        backend["name"] = it.key();
        backend["total_requests"] = static_cast<int>(it.value().totalRequests);
        backend["success_count"] = static_cast<int>(it.value().successfulRequests);
        backend["failure_count"] = static_cast<int>(it.value().failedRequests);
        backend["cached_responses"] = static_cast<int>(it.value().cachedResponses);
        backend["total_tokens"] = static_cast<int>(it.value().totalTokens);
        backend["p50_latency_ms"] = it.value().p50LatencyMs;
        backend["p95_latency_ms"] = it.value().p95LatencyMs;
        backend["p99_latency_ms"] = it.value().p99LatencyMs;
        backends.append(backend);
    }
    root["backends"] = backends;

    return root;
}

QJsonObject LLMMetrics::getBackendStats(const QString& backend)
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    QJsonObject result;
    auto it = s_backendStats.find(backend);
    if (it != s_backendStats.end()) {
        const Stats& stats = it.value();
        result["backend"] = backend;
        result["total_requests"] = static_cast<int>(stats.totalRequests);
        result["successful"] = static_cast<int>(stats.successfulRequests);
        result["failed"] = static_cast<int>(stats.failedRequests);
        result["cached"] = static_cast<int>(stats.cachedResponses);
        result["success_rate"] = stats.totalRequests > 0 ? 
            static_cast<double>(stats.successfulRequests) / stats.totalRequests : 0.0;
        result["average_latency_ms"] = stats.totalRequests > 0 ? 
            static_cast<double>(stats.totalLatencyMs) / stats.totalRequests : 0.0;
        result["p50_latency_ms"] = stats.p50LatencyMs;
        result["p95_latency_ms"] = stats.p95LatencyMs;
        result["p99_latency_ms"] = stats.p99LatencyMs;
    }
    return result;
}

void LLMMetrics::reset()
{
    std::lock_guard<std::mutex> lock(s_statsMutex);
    s_backendStats.clear();
    s_allLatencies.clear();
    qInfo() << "[LLMMetrics] Reset all statistics";
}

// ─────────────────────────────────────────────────────────────────────
// CircuitBreakerMetrics Implementation
// ─────────────────────────────────────────────────────────────────────

QVector<CircuitBreakerMetrics::Event> CircuitBreakerMetrics::s_events;
std::mutex CircuitBreakerMetrics::s_eventsMutex;

void CircuitBreakerMetrics::recordEvent(const Event& event)
{
    std::lock_guard<std::mutex> lock(s_eventsMutex);

    s_events.append(event);

    MetricsCollector::recordCounter(QString("circuit_breaker_%1_total").arg(event.eventType), 1.0,
        {{"backend", event.backend}});

    if (event.eventType == "trip") {
        MetricsCollector::recordGauge("circuit_breaker_failures",
            static_cast<double>(event.failureCount), {{"backend", event.backend}});
    }

    qWarning() << "[CircuitBreakerMetrics]" << event.backend << event.eventType
               << "failures=" << event.failureCount;
}

QJsonObject CircuitBreakerMetrics::getStatistics()
{
    std::lock_guard<std::mutex> lock(s_eventsMutex);

    QJsonObject root;
    QJsonArray events;
    for (const Event& event : s_events) {
        QJsonObject e;
        e["backend"] = event.backend;
        e["type"] = event.eventType;
        e["failure_count"] = event.failureCount;
        e["timestamp"] = static_cast<int>(event.timestamp);
        events.append(e);
    }
    root["events"] = events;
    root["event_count"] = events.count();

    return root;
}

// ─────────────────────────────────────────────────────────────────────
// GGUFMetrics Implementation
// ─────────────────────────────────────────────────────────────────────

QMap<QString, GGUFMetrics::ModelStats> GGUFMetrics::s_modelStats;
std::mutex GGUFMetrics::s_statsMutex;

void GGUFMetrics::recordInference(const InferenceRequest& request)
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    ModelStats& stats = s_modelStats[request.model];
    stats.totalRequests++;

    if (request.success) {
        stats.successfulRequests++;
        stats.totalPromptTokens += request.promptTokens;
        stats.totalGeneratedTokens += request.generatedTokens;
        stats.totalLatencyMs += request.latencyMs;

        if (request.latencyMs > 0) {
            stats.averageTokensPerSecond = (stats.totalGeneratedTokens * 1000.0) / stats.totalLatencyMs;
        }
    }

    MetricsCollector::recordCounter("gguf_inference_requests_total", 1.0,
        {{"model", request.model}, {"success", request.success ? "true" : "false"}});
    MetricsCollector::recordHistogram("gguf_inference_latency_ms",
        static_cast<double>(request.latencyMs), {{"model", request.model}});

    qDebug() << "[GGUFMetrics]" << request.model << "latency=" << request.latencyMs << "ms"
             << "tokens=" << request.generatedTokens << "success=" << request.success;
}

QJsonObject GGUFMetrics::getStatistics()
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    QJsonObject root;
    QJsonArray models;
    for (auto it = s_modelStats.begin(); it != s_modelStats.end(); ++it) {
        QJsonObject model;
        model["name"] = it.key();
        model["total_requests"] = static_cast<int>(it.value().totalRequests);
        model["successful_requests"] = static_cast<int>(it.value().successfulRequests);
        model["success_rate"] = it.value().totalRequests > 0 ?
            static_cast<double>(it.value().successfulRequests) / it.value().totalRequests : 0.0;
        model["total_prompt_tokens"] = static_cast<int>(it.value().totalPromptTokens);
        model["total_generated_tokens"] = static_cast<int>(it.value().totalGeneratedTokens);
        model["average_latency_ms"] = it.value().totalRequests > 0 ?
            static_cast<double>(it.value().totalLatencyMs) / it.value().totalRequests : 0.0;
        model["tokens_per_second"] = it.value().averageTokensPerSecond;
        models.append(model);
    }
    root["models"] = models;
    root["model_count"] = models.count();

    return root;
}

QJsonObject GGUFMetrics::getModelStats(const QString& model)
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    QJsonObject result;
    auto it = s_modelStats.find(model);
    if (it != s_modelStats.end()) {
        const ModelStats& stats = it.value();
        result["model"] = model;
        result["total_requests"] = static_cast<int>(stats.totalRequests);
        result["successful_requests"] = static_cast<int>(stats.successfulRequests);
        result["success_rate"] = stats.totalRequests > 0 ?
            static_cast<double>(stats.successfulRequests) / stats.totalRequests : 0.0;
        result["total_prompt_tokens"] = static_cast<int>(stats.totalPromptTokens);
        result["total_generated_tokens"] = static_cast<int>(stats.totalGeneratedTokens);
        result["average_latency_ms"] = stats.totalRequests > 0 ?
            static_cast<double>(stats.totalLatencyMs) / stats.totalRequests : 0.0;
        result["tokens_per_second"] = stats.averageTokensPerSecond;
        result["p50_latency_ms"] = stats.p50LatencyMs;
        result["p95_latency_ms"] = stats.p95LatencyMs;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────
// HotpatchMetrics Implementation
// ─────────────────────────────────────────────────────────────────────

QMap<QString, HotpatchMetrics::Stats> HotpatchMetrics::s_opStats;
std::mutex HotpatchMetrics::s_statsMutex;

void HotpatchMetrics::recordOperation(const Operation& op)
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    Stats& stats = s_opStats[op.operationType];
    stats.totalOperations++;

    if (op.success) {
        stats.successfulOperations++;
        stats.totalLatencyMs += op.latencyMs;
        stats.totalBytesModified += op.bytesModified;
    }

    MetricsCollector::recordCounter("hotpatch_operations_total", 1.0,
        {{"operation", op.operationType}, {"success", op.success ? "true" : "false"}});
    MetricsCollector::recordHistogram("hotpatch_operation_latency_ms",
        static_cast<double>(op.latencyMs), {{"operation", op.operationType}});

    qDebug() << "[HotpatchMetrics]" << op.operationType << "latency=" << op.latencyMs << "ms"
             << "success=" << op.success;
}

QJsonObject HotpatchMetrics::getStatistics()
{
    std::lock_guard<std::mutex> lock(s_statsMutex);

    QJsonObject root;
    QJsonArray operations;
    for (auto it = s_opStats.begin(); it != s_opStats.end(); ++it) {
        QJsonObject op;
        op["type"] = it.key();
        op["total"] = static_cast<int>(it.value().totalOperations);
        op["successful"] = static_cast<int>(it.value().successfulOperations);
        op["success_rate"] = it.value().totalOperations > 0 ?
            static_cast<double>(it.value().successfulOperations) / it.value().totalOperations : 0.0;
        op["average_latency_ms"] = it.value().totalOperations > 0 ?
            static_cast<double>(it.value().totalLatencyMs) / it.value().totalOperations : 0.0;
        op["total_bytes_modified"] = static_cast<int>(it.value().totalBytesModified);
        operations.append(op);
    }
    root["operations"] = operations;

    return root;
}

// ─────────────────────────────────────────────────────────────────────
// StartupMetrics Implementation
// ─────────────────────────────────────────────────────────────────────

QVector<StartupMetrics::CheckResult> StartupMetrics::s_checks;
std::mutex StartupMetrics::s_checksMutex;

void StartupMetrics::recordCheck(const CheckResult& result)
{
    std::lock_guard<std::mutex> lock(s_checksMutex);

    s_checks.append(result);

    MetricsCollector::recordGauge(QString("startup_check_%1_latency_ms").arg(result.checkName),
        static_cast<double>(result.latencyMs));
    MetricsCollector::recordGauge(QString("startup_check_%1_success").arg(result.checkName),
        result.success ? 1.0 : 0.0);

    qInfo() << "[StartupMetrics]" << result.checkName << "status=" << result.status
            << "latency=" << result.latencyMs << "ms";
}

QJsonObject StartupMetrics::getStatistics()
{
    std::lock_guard<std::mutex> lock(s_checksMutex);

    QJsonObject root;
    QJsonArray checks;
    qint64 totalLatency = 0;

    for (const CheckResult& check : s_checks) {
        QJsonObject c;
        c["name"] = check.checkName;
        c["success"] = check.success;
        c["status"] = check.status;
        c["latency_ms"] = static_cast<int>(check.latencyMs);
        c["attempts"] = check.attempts;
        checks.append(c);
        totalLatency += check.latencyMs;
    }

    root["checks"] = checks;
    root["total_latency_ms"] = static_cast<int>(totalLatency);
    root["check_count"] = checks.count();

    return root;
}

// ─────────────────────────────────────────────────────────────────────
// Global Telemetry Functions
// ─────────────────────────────────────────────────────────────────────

void initializeTelemetry()
{
    qInfo() << "[Telemetry] Initializing metrics collection system";
    MetricsCollector::clear();
    LLMMetrics::reset();

    // Record initialization timestamp
    MetricsCollector::recordGauge("telemetry_initialization_timestamp",
        static_cast<double>(QDateTime::currentMSecsSinceEpoch()));

    qInfo() << "[Telemetry] Ready for metrics collection";
}

QString exportMetrics(const QString& format)
{
    if (format.toLower() == "prometheus") {
        return MetricsCollector::exportPrometheus();
    } else if (format.toLower() == "json") {
        return QJsonDocument(MetricsCollector::exportJSON()).toJson(QJsonDocument::Indented);
    }
    return QString();
}

bool exportMetricsToFile(const QString& filePath, const QString& format)
{
    QString metrics = exportMetrics(format);
    if (metrics.isEmpty()) {
        qWarning() << "[Telemetry] Failed to export metrics - empty result";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[Telemetry] Failed to open file for export:" << filePath;
        return false;
    }

    file.write(metrics.toUtf8());
    file.close();

    qInfo() << "[Telemetry] Metrics exported to" << filePath << "(" << format << ")";
    return true;
}

}  // namespace RawrXD
