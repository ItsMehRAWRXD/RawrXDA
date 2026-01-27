// AgenticObservability Implementation
#include "agentic_observability.h"
#include <QDebug>
#include <QUuid>
#include <QJsonDocument>
#include <algorithm>
#include <cmath>

AgenticObservability::AgenticObservability(QObject* parent)
    : QObject(parent),
      m_systemStartTime(QDateTime::currentDateTime())
{
    qDebug() << "[AgenticObservability] Initialized - Ready for comprehensive observability";
}

AgenticObservability::~AgenticObservability()
{
    qDebug() << "[AgenticObservability] Destroyed - Logged"
             << m_totalLogsWritten << "entries and"
             << m_totalMetricsRecorded << "metrics";
}

// ===== STRUCTURED LOGGING =====

void AgenticObservability::log(
    LogLevel level,
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    // Apply sampling
    if (QRandomGenerator::global()->generateDouble() > m_samplingRate) {
        return;
    }

    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = generateTraceId();
    entry.spanId = generateSpanId();

    m_logs.push_back(entry);
    m_totalLogsWritten++;

    // Keep buffer bounded
    if (m_logs.size() > m_maxLogEntries) {
        m_logs.erase(m_logs.begin());
    }

    emit logWritten(entry);
}

void AgenticObservability::logDebug(
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    log(LogLevel::DEBUG, component, message, context);
}

void AgenticObservability::logInfo(
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    log(LogLevel::INFO, component, message, context);
}

void AgenticObservability::logWarn(
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    log(LogLevel::WARN, component, message, context);
}

void AgenticObservability::logError(
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    log(LogLevel::ERROR, component, message, context);
    m_errorCounts[component.toStdString()]++;
}

void AgenticObservability::logCritical(
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    log(LogLevel::CRITICAL, component, message, context);
    m_errorCounts[component.toStdString()]++;
}

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogs(
    int limit,
    LogLevel minLevel,
    const QString& component)
{
    std::vector<LogEntry> filtered;

    for (const auto& entry : m_logs) {
        if (entry.level < minLevel) continue;
        if (!component.isEmpty() && entry.component != component) continue;
        filtered.push_back(entry);
    }

    if (limit > 0 && filtered.size() > limit) {
        filtered.erase(filtered.begin(), filtered.end() - limit);
    }

    return filtered;
}

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogsByTimeRange(
    const QDateTime& start,
    const QDateTime& end,
    LogLevel minLevel)
{
    std::vector<LogEntry> filtered;

    for (const auto& entry : m_logs) {
        if (entry.timestamp < start || entry.timestamp > end) continue;
        if (entry.level < minLevel) continue;
        filtered.push_back(entry);
    }

    return filtered;
}

// ===== METRICS =====

void AgenticObservability::recordMetric(
    const QString& metricName,
    float value,
    const QJsonObject& labels,
    const QString& unit)
{
    MetricPoint point;
    point.metricName = metricName;
    point.value = value;
    point.labels = labels;
    point.timestamp = QDateTime::currentDateTime();
    point.unit = unit;

    m_metrics.push_back(point);
    m_totalMetricsRecorded++;

    // Keep buffer bounded
    if (m_metrics.size() > m_metricsBufferSize) {
        m_metrics.erase(m_metrics.begin());
    }

    emit metricRecorded(metricName);
}

void AgenticObservability::incrementCounter(
    const QString& metricName,
    int delta,
    const QJsonObject& labels)
{
    recordMetric(metricName, delta, labels, "count");
}

float AgenticObservability::getCounterValue(const QString& metricName) const
{
    float sum = 0.0f;
    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName) {
            sum += metric.value;
        }
    }
    return sum;
}

void AgenticObservability::setGauge(
    const QString& metricName,
    float value,
    const QJsonObject& labels)
{
    recordMetric(metricName, value, labels, "gauge");
}

float AgenticObservability::getGaugeValue(const QString& metricName) const
{
    // Return most recent value
    for (auto it = m_metrics.rbegin(); it != m_metrics.rend(); ++it) {
        if (it->metricName == metricName) {
            return it->value;
        }
    }
    return 0.0f;
}

void AgenticObservability::recordHistogram(
    const QString& metricName,
    float value,
    const QJsonObject& labels)
{
    recordMetric(metricName + "_histogram", value, labels, "histogram");
}

QJsonObject AgenticObservability::getHistogramStats(const QString& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName + "_histogram") {
            values.push_back(metric.value);
        }
    }

    QJsonObject stats;

    if (values.empty()) {
        stats["count"] = 0;
        return stats;
    }

    std::sort(values.begin(), values.end());

    float sum = 0.0f;
    for (float v : values) sum += v;

    stats["count"] = static_cast<int>(values.size());
    stats["min"] = values.front();
    stats["max"] = values.back();
    stats["mean"] = sum / values.size();
    stats["median"] = values[values.size() / 2];

    // Calculate standard deviation
    float variance = 0.0f;
    float mean = sum / values.size();
    for (float v : values) {
        variance += (v - mean) * (v - mean);
    }
    stats["stddev"] = std::sqrt(variance / values.size());

    return stats;
}

AgenticObservability::TimingGuard::TimingGuard(
    AgenticObservability* obs,
    const QString& metricName)
    : m_obs(obs), m_metricName(metricName),
      m_start(std::chrono::high_resolution_clock::now())
{
}

AgenticObservability::TimingGuard::~TimingGuard()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
    
    if (m_obs) {
        m_obs->recordMetric(m_metricName + "_duration", duration.count(), {}, "ms");
    }
}

std::unique_ptr<AgenticObservability::TimingGuard> AgenticObservability::measureDuration(
    const QString& metricName)
{
    return std::make_unique<TimingGuard>(this, metricName);
}

std::vector<AgenticObservability::MetricPoint> AgenticObservability::getMetrics(
    const QString& pattern,
    int limit)
{
    std::vector<MetricPoint> filtered;

    for (const auto& metric : m_metrics) {
        if (!pattern.isEmpty() && !metric.metricName.contains(pattern)) {
            continue;
        }
        filtered.push_back(metric);
    }

    if (limit > 0 && filtered.size() > limit) {
        filtered.erase(filtered.begin(), filtered.end() - limit);
    }

    return filtered;
}

QJsonObject AgenticObservability::getMetricsSummary() const
{
    QJsonObject summary;

    // Group metrics by name
    std::unordered_map<std::string, std::vector<float>> metricGroups;
    for (const auto& metric : m_metrics) {
        metricGroups[metric.metricName.toStdString()].push_back(metric.value);
    }

    QJsonObject metrics;
    for (const auto& pair : metricGroups) {
        float sum = 0.0f;
        for (float v : pair.second) sum += v;

        QJsonObject metricInfo;
        metricInfo["count"] = static_cast<int>(pair.second.size());
        metricInfo["latest"] = pair.second.back();
        metricInfo["avg"] = sum / pair.second.size();

        metrics[QString::fromStdString(pair.first)] = metricInfo;
    }

    summary["metrics"] = metrics;
    summary["total_recorded"] = m_totalMetricsRecorded;

    return summary;
}

QJsonObject AgenticObservability::getPercentiles(const QString& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName) {
            values.push_back(metric.value);
        }
    }

    QJsonObject percentiles;

    if (values.empty()) {
        return percentiles;
    }

    std::sort(values.begin(), values.end());

    percentiles["p50"] = values[values.size() / 2];
    percentiles["p95"] = values[static_cast<size_t>(values.size() * 0.95)];
    percentiles["p99"] = values[static_cast<size_t>(values.size() * 0.99)];

    return percentiles;
}

// ===== DISTRIBUTED TRACING =====

QString AgenticObservability::startTrace(const QString& operation)
{
    if (!m_tracingEnabled) return "";

    QString traceId = generateTraceId();
    m_traceSpans[traceId.toStdString()] = {};

    return traceId;
}

QString AgenticObservability::startSpan(const QString& spanName, const QString& parentSpanId)
{
    if (!m_tracingEnabled) return "";

    QString spanId = generateSpanId();
    
    TraceSpan span;
    span.spanId = spanId;
    span.parentSpanId = parentSpanId;
    span.operation = spanName;
    span.startTime = QDateTime::currentDateTime();
    span.hasError = false;
    span.statusCode = 0;

    m_spans[spanId.toStdString()] = span;

    return spanId;
}

void AgenticObservability::endSpan(
    const QString& spanId,
    bool hasError,
    const QString& errorMessage,
    int statusCode)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        it->second.endTime = QDateTime::currentDateTime();
        it->second.hasError = hasError;
        it->second.errorMessage = errorMessage;
        it->second.statusCode = statusCode;

        emit spanCompleted(spanId);
    }
}

void AgenticObservability::setSpanAttribute(
    const QString& spanId,
    const QString& key,
    const QVariant& value)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        it->second.attributes[key] = QJsonValue::fromVariant(value);
    }
}

void AgenticObservability::addSpanEvent(
    const QString& spanId,
    const QString& eventName,
    const QJsonObject& attributes)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        QJsonObject event;
        event["name"] = eventName;
        event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        event["attributes"] = attributes;
        
        // Store event in attributes array
        QJsonArray events = it->second.attributes.value("events").toArray();
        events.append(event);
        it->second.attributes["events"] = events;
    }
}

AgenticObservability::TraceSpan* AgenticObservability::getSpan(const QString& spanId)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AgenticObservability::TraceSpan> AgenticObservability::getTraceSpans(
    const QString& traceId)
{
    std::vector<TraceSpan> spans;

    auto it = m_traceSpans.find(traceId.toStdString());
    if (it != m_traceSpans.end()) {
        for (const auto& spanId : it->second) {
            auto spanIt = m_spans.find(spanId.toStdString());
            if (spanIt != m_spans.end()) {
                spans.push_back(spanIt->second);
            }
        }
    }

    return spans;
}

QJsonObject AgenticObservability::getTraceVisualization(const QString& traceId)
{
    QJsonObject visualization;

    auto spans = getTraceSpans(traceId);
    QJsonArray spanArray;

    for (const auto& span : spans) {
        QJsonObject spanObj;
        spanObj["spanId"] = span.spanId;
        spanObj["operation"] = span.operation;
        spanObj["duration"] = static_cast<int>(
            span.startTime.msecsTo(span.endTime)
        );
        spanObj["hasError"] = span.hasError;

        spanArray.append(spanObj);
    }

    visualization["traceId"] = traceId;
    visualization["spans"] = spanArray;

    return visualization;
}

// ===== DIAGNOSTICS =====

QJsonObject AgenticObservability::getSystemHealth() const
{
    QJsonObject health;

    int errorCount = 0;
    for (const auto& pair : m_errorCounts) {
        errorCount += pair.second;
    }

    float uptime = m_systemStartTime.msecsTo(QDateTime::currentDateTime()) / 1000.0f;

    health["uptime_seconds"] = uptime;
    health["total_logs"] = m_totalLogsWritten;
    health["total_metrics"] = m_totalMetricsRecorded;
    health["total_errors"] = errorCount;
    health["error_rate"] = errorCount / std::max(1.0f, uptime / 60.0f); // errors per minute

    health["healthy"] = errorCount < 10;

    return health;
}

bool AgenticObservability::isHealthy() const
{
    return getSystemHealth().value("healthy").toBool(true);
}

QJsonObject AgenticObservability::getPerformanceSummary() const
{
    QJsonObject summary;

    // Find latency-related metrics
    for (const auto& metric : m_metrics) {
        if (metric.metricName.contains("duration")) {
            auto stats = getHistogramStats(metric.metricName.replace("_duration", ""));
            summary[metric.metricName] = stats;
        }
    }

    return summary;
}

QJsonObject AgenticObservability::getErrorSummary() const
{
    QJsonObject summary;

    QJsonObject errorsByComponent;
    for (const auto& pair : m_errorCounts) {
        errorsByComponent[QString::fromStdString(pair.first)] = pair.second;
    }

    summary["errors_by_component"] = errorsByComponent;
    summary["total_errors"] = getSystemHealth().value("total_errors");

    return summary;
}

std::vector<QString> AgenticObservability::detectBottlenecks()
{
    std::vector<QString> bottlenecks;

    // Find slowest operations
    std::unordered_map<std::string, float> avgDurations;

    for (const auto& metric : m_metrics) {
        if (metric.metricName.contains("duration")) {
            // Calculate average
        }
    }

    return bottlenecks;
}

QJsonObject AgenticObservability::analyzeLatency()
{
    return getPerformanceSummary();
}

// ===== EXPORT/REPORTING =====

QString AgenticObservability::generateReport(
    const QDateTime& startTime,
    const QDateTime& endTime) const
{
    QString report;
    report += "=== OBSERVABILITY REPORT ===\n\n";

    report += "LOGS:\n";
    auto logs = getLogs(100, LogLevel::DEBUG);
    for (const auto& log : logs) {
        if (log.timestamp < startTime || log.timestamp > endTime) continue;
        report += QString("[%1] %2: %3\n")
                  .arg(log.timestamp.toString("hh:mm:ss"))
                  .arg(log.component)
                  .arg(log.message);
    }

    report += "\nMETRICS:\n";
    for (const auto& metric : getMetrics("", 50)) {
        if (metric.timestamp < startTime || metric.timestamp > endTime) continue;
        report += QString("%1 = %2\n").arg(metric.metricName).arg(metric.value);
    }

    return report;
}

QString AgenticObservability::exportMetricsAsCsv() const
{
    QString csv = "timestamp,metric_name,value,unit\n";

    for (const auto& metric : m_metrics) {
        csv += QString("%1,%2,%3,%4\n")
               .arg(metric.timestamp.toString(Qt::ISODate))
               .arg(metric.metricName)
               .arg(metric.value)
               .arg(metric.unit);
    }

    return csv;
}

QString AgenticObservability::exportTracesAsJson() const
{
    QJsonArray traces;

    for (const auto& pair : m_traceSpans) {
        traces.append(getTraceVisualization(QString::fromStdString(pair.first)));
    }

    return QString::fromUtf8(QJsonDocument(traces).toJson());
}

QString AgenticObservability::exportLogsAsJson() const
{
    QJsonArray logs;

    for (const auto& log : m_logs) {
        QJsonObject obj;
        obj["timestamp"] = log.timestamp.toString(Qt::ISODate);
        obj["level"] = levelToString(log.level);
        obj["component"] = log.component;
        obj["message"] = log.message;
        obj["context"] = log.context;

        logs.append(obj);
    }

    return QString::fromUtf8(QJsonDocument(logs).toJson());
}

// ===== PRIVATE HELPERS =====

QString AgenticObservability::generateTraceId()
{
    return QUuid::createUuid().toString();
}

QString AgenticObservability::generateSpanId()
{
    return QUuid::createUuid().toString();
}

QString AgenticObservability::levelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void AgenticObservability::checkAndRotateLogs()
{
    if (m_logs.size() > m_maxLogEntries) {
        int toRemove = m_logs.size() - (m_maxLogEntries * 9 / 10);
        m_logs.erase(m_logs.begin(), m_logs.begin() + toRemove);
    }
}

void AgenticObservability::prune()
{
    checkAndRotateLogs();
}
