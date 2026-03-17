// AgenticObservability Implementation
#include "agentic_observability.h"
#include <algorithm>
#include <cmath>

#ifdef RAWRXD_NO_QT

#include <chrono>
#include <random>
#include <string>

static std::mt19937& rng() { static std::mt19937 r(std::random_device{}()); return r; }

AgenticObservability::AgenticObservability(void* parent)
    : m_systemStartTime(std::chrono::system_clock::now()) { (void)parent; }

AgenticObservability::~AgenticObservability() = default;

std::string AgenticObservability::generateTraceId() { return "t-" + std::to_string(rng()()); }
std::string AgenticObservability::generateSpanId() { return "s-" + std::to_string(rng()()); }

void AgenticObservability::log(LogLevel level, const std::string& component, const std::string& message, const AgenticObservability::json& context) {
    if (std::uniform_real_distribution<float>(0,1)(rng()) > m_samplingRate) return;
    LogEntry e;
    e.timestamp = std::chrono::system_clock::now();
    e.level = level;
    e.component = component;
    e.message = message;
    e.context = context;
    e.traceId = generateTraceId();
    e.spanId = generateSpanId();
    m_logs.push_back(e);
    m_totalLogsWritten++;
    if (m_logs.size() > (size_t)m_maxLogEntries) m_logs.erase(m_logs.begin());
}

void AgenticObservability::logDebug(const std::string& c, const std::string& m, const AgenticObservability::json& ctx) { log(LogLevel::DEBUG, c, m, ctx); }
void AgenticObservability::logInfo(const std::string& c, const std::string& m, const AgenticObservability::json& ctx) { log(LogLevel::INFO, c, m, ctx); }
void AgenticObservability::logWarn(const std::string& c, const std::string& m, const AgenticObservability::json& ctx) { log(LogLevel::WARN, c, m, ctx); }
void AgenticObservability::logError(const std::string& c, const std::string& m, const AgenticObservability::json& ctx) { log(LogLevel::ERROR, c, m, ctx); }
void AgenticObservability::logCritical(const std::string& c, const std::string& m, const AgenticObservability::json& ctx) { log(LogLevel::CRITICAL, c, m, ctx); }

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogs(int limit, LogLevel minLevel, const std::string& component) {
    std::vector<LogEntry> out;
    for (const auto& e : m_logs) {
        if (e.level < minLevel) continue;
        if (!component.empty() && e.component != component) continue;
        out.push_back(e);
    }
    if (limit > 0 && (int)out.size() > limit) out.erase(out.begin(), out.end() - limit);
    return out;
}

void AgenticObservability::recordMetric(const std::string&, float, const AgenticObservability::json&, const std::string&) { m_totalMetricsRecorded++; }
void AgenticObservability::incrementCounter(const std::string&, int, const AgenticObservability::json&) {}
float AgenticObservability::getCounterValue(const std::string&) const { return 0.f; }
void AgenticObservability::setGauge(const std::string&, float, const AgenticObservability::json&) {}
float AgenticObservability::getGaugeValue(const std::string&) const { return 0.f; }
void AgenticObservability::recordHistogram(const std::string&, float, const AgenticObservability::json&) {}
std::string AgenticObservability::startTrace(const std::string&) { return generateTraceId(); }
std::string AgenticObservability::startSpan(const std::string&, const std::string&) { return generateSpanId(); }
void AgenticObservability::endSpan(const std::string&, bool, const std::string&, int) {}

AgenticObservability::TimingGuard::TimingGuard(AgenticObservability* obs, const std::string& metricName)
    : m_obs(obs), m_metricName(metricName), m_start(std::chrono::high_resolution_clock::now()) {}

AgenticObservability::TimingGuard::~TimingGuard() {
    if (!m_obs) return;
    auto end = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(end - m_start).count();
    m_obs->recordHistogram(m_metricName, ms, AgenticObservability::json::object());
}

std::unique_ptr<AgenticObservability::TimingGuard> AgenticObservability::measureDuration(const std::string& metricName) {
    return std::make_unique<TimingGuard>(this, metricName);
}

#else

AgenticObservability::AgenticObservability(void* parent)
    : QObject(static_cast<QObject*>(parent)),
      m_systemStartTime(QDateTime::currentDateTime())
{
}

AgenticObservability::~AgenticObservability()
{
}

// ===== STRUCTURED LOGGING =====

void AgenticObservability::log(
    LogLevel level,
    const QString& component,
    const QString& message,
    const QJsonObject& context)
{
    if (QRandomGenerator::global()->generateDouble() > m_samplingRate) return;
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
    if (m_logs.size() > (size_t)m_maxLogEntries) m_logs.erase(m_logs.begin());
    emit logWritten(entry);
}

void AgenticObservability::logDebug(const QString& c, const QString& m, const QJsonObject& ctx) { log(LogLevel::DEBUG, c, m, ctx); }
void AgenticObservability::logInfo(const QString& c, const QString& m, const QJsonObject& ctx) { log(LogLevel::INFO, c, m, ctx); }
void AgenticObservability::logWarn(const QString& c, const QString& m, const QJsonObject& ctx) { log(LogLevel::WARN, c, m, ctx); }
void AgenticObservability::logError(const QString& c, const QString& m, const QJsonObject& ctx) { log(LogLevel::ERROR, c, m, ctx); m_errorCounts[c.toStdString()]++; }
void AgenticObservability::logCritical(const QString& c, const QString& m, const QJsonObject& ctx) { log(LogLevel::CRITICAL, c, m, ctx); m_errorCounts[c.toStdString()]++; }

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogs(int limit, LogLevel minLevel, const QString& component) {
    std::vector<LogEntry> filtered;
    for (const auto& entry : m_logs) {
        if (entry.level < minLevel) continue;
        if (!component.isEmpty() && entry.component != component) continue;
        filtered.push_back(entry);
    }
    if (limit > 0 && (int)filtered.size() > limit)
        filtered.erase(filtered.begin(), filtered.end() - limit);
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
    const std::string& metricName,
    float value,
    const void*& labels,
    const std::string& unit)
{
    MetricPoint point;
    point.metricName = metricName;
    point.value = value;
    point.labels = labels;
    point.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    point.unit = unit;

    m_metrics.push_back(point);
    m_totalMetricsRecorded++;

    // Keep buffer bounded
    if (m_metrics.size() > m_metricsBufferSize) {
        m_metrics.erase(m_metrics.begin());
    }

    metricRecorded(metricName);
}

void AgenticObservability::incrementCounter(
    const std::string& metricName,
    int delta,
    const void*& labels)
{
    recordMetric(metricName, delta, labels, "count");
}

float AgenticObservability::getCounterValue(const std::string& metricName) const
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
    const std::string& metricName,
    float value,
    const void*& labels)
{
    recordMetric(metricName, value, labels, "gauge");
}

float AgenticObservability::getGaugeValue(const std::string& metricName) const
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
    const std::string& metricName,
    float value,
    const void*& labels)
{
    recordMetric(metricName + "_histogram", value, labels, "histogram");
}

void* AgenticObservability::getHistogramStats(const std::string& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName + "_histogram") {
            values.push_back(metric.value);
        }
    }

    void* stats;

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
    const std::string& metricName)
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
    const std::string& metricName)
{
    return std::make_unique<TimingGuard>(this, metricName);
}

std::vector<AgenticObservability::MetricPoint> AgenticObservability::getMetrics(
    const std::string& pattern,
    int limit)
{
    std::vector<MetricPoint> filtered;

    for (const auto& metric : m_metrics) {
        if (!pattern.empty() && !metric.metricName.contains(pattern)) {
            continue;
        }
        filtered.push_back(metric);
    }

    if (limit > 0 && filtered.size() > limit) {
        filtered.erase(filtered.begin(), filtered.end() - limit);
    }

    return filtered;
}

void* AgenticObservability::getMetricsSummary() const
{
    void* summary;

    // Group metrics by name
    std::unordered_map<std::string, std::vector<float>> metricGroups;
    for (const auto& metric : m_metrics) {
        metricGroups[metric.metricName.toStdString()].push_back(metric.value);
    }

    void* metrics;
    for (const auto& pair : metricGroups) {
        float sum = 0.0f;
        for (float v : pair.second) sum += v;

        void* metricInfo;
        metricInfo["count"] = static_cast<int>(pair.second.size());
        metricInfo["latest"] = pair.second.back();
        metricInfo["avg"] = sum / pair.second.size();

        metrics[std::string::fromStdString(pair.first)] = metricInfo;
    }

    summary["metrics"] = metrics;
    summary["total_recorded"] = m_totalMetricsRecorded;

    return summary;
}

void* AgenticObservability::getPercentiles(const std::string& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName) {
            values.push_back(metric.value);
        }
    }

    void* percentiles;

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

std::string AgenticObservability::startTrace(const std::string& operation)
{
    if (!m_tracingEnabled) return "";

    std::string traceId = generateTraceId();
    m_traceSpans[traceId.toStdString()] = {};

    return traceId;
}

std::string AgenticObservability::startSpan(const std::string& spanName, const std::string& parentSpanId)
{
    if (!m_tracingEnabled) return "";

    std::string spanId = generateSpanId();
    
    TraceSpan span;
    span.spanId = spanId;
    span.parentSpanId = parentSpanId;
    span.operation = spanName;
    span.startTime = std::chrono::system_clock::time_point::currentDateTime();
    span.hasError = false;
    span.statusCode = 0;

    m_spans[spanId.toStdString()] = span;

    return spanId;
}

void AgenticObservability::endSpan(
    const std::string& spanId,
    bool hasError,
    const std::string& errorMessage,
    int statusCode)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        it->second.endTime = std::chrono::system_clock::time_point::currentDateTime();
        it->second.hasError = hasError;
        it->second.errorMessage = errorMessage;
        it->second.statusCode = statusCode;

        spanCompleted(spanId);
    }
}

void AgenticObservability::setSpanAttribute(
    const std::string& spanId,
    const std::string& key,
    const std::any& value)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        it->second.attributes[key] = void*::fromVariant(value);
    }
}

void AgenticObservability::addSpanEvent(
    const std::string& spanId,
    const std::string& eventName,
    const void*& attributes)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        void* event;
        event["name"] = eventName;
        event["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        event["attributes"] = attributes;
        
        // Store event in attributes array
        void* events = it->second.attributes.value("events").toArray();
        events.append(event);
        it->second.attributes["events"] = events;
    }
}

AgenticObservability::TraceSpan* AgenticObservability::getSpan(const std::string& spanId)
{
    auto it = m_spans.find(spanId.toStdString());
    if (it != m_spans.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AgenticObservability::TraceSpan> AgenticObservability::getTraceSpans(
    const std::string& traceId)
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

void* AgenticObservability::getTraceVisualization(const std::string& traceId)
{
    void* visualization;

    auto spans = getTraceSpans(traceId);
    void* spanArray;

    for (const auto& span : spans) {
        void* spanObj;
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

void* AgenticObservability::getSystemHealth() const
{
    void* health;

    int errorCount = 0;
    for (const auto& pair : m_errorCounts) {
        errorCount += pair.second;
    }

    float uptime = m_systemStartTime.msecsTo(std::chrono::system_clock::time_point::currentDateTime()) / 1000.0f;

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

void* AgenticObservability::getPerformanceSummary() const
{
    void* summary;

    // Find latency-related metrics
    for (const auto& metric : m_metrics) {
        if (metric.metricName.contains("duration")) {
            auto stats = getHistogramStats(metric.metricName.replace("_duration", ""));
            summary[metric.metricName] = stats;
        }
    }

    return summary;
}

void* AgenticObservability::getErrorSummary() const
{
    void* summary;

    void* errorsByComponent;
    for (const auto& pair : m_errorCounts) {
        errorsByComponent[std::string::fromStdString(pair.first)] = pair.second;
    }

    summary["errors_by_component"] = errorsByComponent;
    summary["total_errors"] = getSystemHealth().value("total_errors");

    return summary;
}

std::vector<std::string> AgenticObservability::detectBottlenecks()
{
    std::vector<std::string> bottlenecks;

    // Find slowest operations
    std::unordered_map<std::string, float> avgDurations;

    for (const auto& metric : m_metrics) {
        if (metric.metricName.contains("duration")) {
            // Calculate average
        }
    }

    return bottlenecks;
}

void* AgenticObservability::analyzeLatency()
{
    return getPerformanceSummary();
}

// ===== EXPORT/REPORTING =====

std::string AgenticObservability::generateReport(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime) const
{
    std::string report;
    report += "=== OBSERVABILITY REPORT ===\n\n";

    report += "LOGS:\n";
    auto logs = getLogs(100, LogLevel::DEBUG);
    for (const auto& log : logs) {
        if (log.timestamp < startTime || log.timestamp > endTime) continue;
        report += std::string("[%1] %2: %3\n")
                  )
                  
                  ;
    }

    report += "\nMETRICS:\n";
    for (const auto& metric : getMetrics("", 50)) {
        if (metric.timestamp < startTime || metric.timestamp > endTime) continue;
        report += std::string("%1 = %2\n");
    }

    return report;
}

std::string AgenticObservability::exportMetricsAsCsv() const
{
    std::string csv = "timestamp,metric_name,value,unit\n";

    for (const auto& metric : m_metrics) {
        csv += std::string("%1,%2,%3,%4\n")
               )


               ;
    }

    return csv;
}

std::string AgenticObservability::exportTracesAsJson() const
{
    void* traces;

    for (const auto& pair : m_traceSpans) {
        traces.append(getTraceVisualization(std::string::fromStdString(pair.first)));
    }

    return std::string::fromUtf8(void*(traces).toJson());
}

std::string AgenticObservability::exportLogsAsJson() const
{
    void* logs;

    for (const auto& log : m_logs) {
        void* obj;
        obj["timestamp"] = log.timestamp.toString(//ISODate);
        obj["level"] = levelToString(log.level);
        obj["component"] = log.component;
        obj["message"] = log.message;
        obj["context"] = log.context;

        logs.append(obj);
    }

    return std::string::fromUtf8(void*(logs).toJson());
}

// ===== PRIVATE HELPERS =====

std::string AgenticObservability::generateTraceId()
{
    return QUuid::createUuid().toString();
}

std::string AgenticObservability::generateSpanId()
{
    return QUuid::createUuid().toString();
}

std::string AgenticObservability::levelToString(LogLevel level) const
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

#endif /* RAWRXD_NO_QT */
