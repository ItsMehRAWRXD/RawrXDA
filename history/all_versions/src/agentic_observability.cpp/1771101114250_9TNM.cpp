// AgenticObservability Implementation (Qt-free)
#include "agentic_observability.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string GenerateHexId(int length = 32) {
    static thread_local std::mt19937 rng(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 15);
    const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result += hex[dist(rng)];
    }
    return result;
}

static double SamplingRoll() {
    static thread_local std::mt19937 rng(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count() ^ 0xBEEF));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

AgenticObservability& AgenticObservability::instance() {
    static AgenticObservability inst;
    return inst;
}

AgenticObservability::AgenticObservability()
    : m_systemStartTime(std::chrono::system_clock::now())
{
    fprintf(stderr, "[AgenticObservability] Initialized - Ready for comprehensive observability\n");
}

AgenticObservability::~AgenticObservability()
{
    fprintf(stderr, "[AgenticObservability] Destroyed - Logged %d entries and %d metrics\n",
            m_totalLogsWritten, m_totalMetricsRecorded);
}

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------

std::string AgenticObservability::timePointToISO(const TimePoint& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

// ===== STRUCTURED LOGGING =====

void AgenticObservability::log(
    LogLevel level,
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    // Apply sampling
    if (SamplingRoll() > m_samplingRate) {
        return;
    }

    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = generateTraceId();
    entry.spanId = generateSpanId();

    m_logs.push_back(entry);
    m_totalLogsWritten++;

    // Keep buffer bounded
    if (static_cast<int>(m_logs.size()) > m_maxLogEntries) {
        m_logs.erase(m_logs.begin());
    }

    if (m_logCb) m_logCb(entry, m_logCbData);
}

void AgenticObservability::logDebug(
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    log(LogLevel::ObsDebug, component, message, context);
}

void AgenticObservability::logInfo(
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    log(LogLevel::ObsError, component, message, context);
}

void AgenticObservability::logCritical(
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    log(LogLevel::ObsCritical, component, message, context);
}

void AgenticObservability::logError(
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    log(LogLevel::ERROR_LEVEL, component, message, context);
    m_errorCounts[component]++;
}

void AgenticObservability::logCritical(
    const std::string& component,
    const std::string& message,
    const nlohmann::json& context)
{
    log(LogLevel::CRITICAL_LEVEL, component, message, context);
    m_errorCounts[component]++;
}

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogs(
    int limit,
    LogLevel minLevel,
    const std::string& component) const
{
    std::vector<LogEntry> filtered;

    for (const auto& entry : m_logs) {
        if (entry.level < minLevel) continue;
        if (!component.empty() && entry.component != component) continue;
        filtered.push_back(entry);
    }

    if (limit > 0 && static_cast<int>(filtered.size()) > limit) {
        filtered.erase(filtered.begin(), filtered.end() - limit);
    }

    return filtered;
}

std::vector<AgenticObservability::LogEntry> AgenticObservability::getLogsByTimeRange(
    const TimePoint& start,
    const TimePoint& end,
    LogLevel minLevel) const
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
    const nlohmann::json& labels,
    const std::string& unit)
{
    MetricPoint point;
    point.metricName = metricName;
    point.value = value;
    point.labels = labels;
    point.timestamp = std::chrono::system_clock::now();
    point.unit = unit;

    m_metrics.push_back(point);
    m_totalMetricsRecorded++;

    // Keep buffer bounded
    if (static_cast<int>(m_metrics.size()) > m_metricsBufferSize) {
        m_metrics.erase(m_metrics.begin());
    }

    if (m_metricCb) m_metricCb(metricName, m_metricCbData);
}

void AgenticObservability::incrementCounter(
    const std::string& metricName,
    int delta,
    const nlohmann::json& labels)
{
    recordMetric(metricName, static_cast<float>(delta), labels, "count");
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
    const nlohmann::json& labels)
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
    const nlohmann::json& labels)
{
    recordMetric(metricName + "_histogram", value, labels, "histogram");
}

nlohmann::json AgenticObservability::getHistogramStats(const std::string& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName + "_histogram") {
            values.push_back(metric.value);
        }
    }

    nlohmann::json stats;

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
    float mean = sum / static_cast<float>(values.size());
    for (float v : values) {
        variance += (v - mean) * (v - mean);
    }
    stats["stddev"] = std::sqrt(variance / static_cast<float>(values.size()));

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
        m_obs->recordMetric(m_metricName + "_duration", static_cast<float>(duration.count()), {}, "ms");
    }
}

std::unique_ptr<AgenticObservability::TimingGuard> AgenticObservability::measureDuration(
    const std::string& metricName)
{
    return std::make_unique<TimingGuard>(this, metricName);
}

std::vector<AgenticObservability::MetricPoint> AgenticObservability::getMetrics(
    const std::string& pattern,
    int limit) const
{
    std::vector<MetricPoint> filtered;

    for (const auto& metric : m_metrics) {
        if (!pattern.empty() && metric.metricName.find(pattern) == std::string::npos) {
            continue;
        }
        filtered.push_back(metric);
    }

    if (limit > 0 && static_cast<int>(filtered.size()) > limit) {
        filtered.erase(filtered.begin(), filtered.end() - limit);
    }

    return filtered;
}

nlohmann::json AgenticObservability::getMetricsSummary() const
{
    nlohmann::json summary;

    // Group metrics by name
    std::unordered_map<std::string, std::vector<float>> metricGroups;
    for (const auto& metric : m_metrics) {
        metricGroups[metric.metricName].push_back(metric.value);
    }

    nlohmann::json metrics;
    for (const auto& pair : metricGroups) {
        float sum = 0.0f;
        for (float v : pair.second) sum += v;

        nlohmann::json metricInfo;
        metricInfo["count"] = static_cast<int>(pair.second.size());
        metricInfo["latest"] = pair.second.back();
        metricInfo["avg"] = sum / static_cast<float>(pair.second.size());

        metrics[pair.first] = metricInfo;
    }

    summary["metrics"] = metrics;
    summary["total_recorded"] = m_totalMetricsRecorded;

    return summary;
}

nlohmann::json AgenticObservability::getPercentiles(const std::string& metricName) const
{
    std::vector<float> values;

    for (const auto& metric : m_metrics) {
        if (metric.metricName == metricName) {
            values.push_back(metric.value);
        }
    }

    nlohmann::json percentiles;

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
    m_traceSpans[traceId] = {};

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
    span.startTime = std::chrono::system_clock::now();
    span.hasError = false;
    span.statusCode = 0;

    m_spans[spanId] = span;

    return spanId;
}

void AgenticObservability::endSpan(
    const std::string& spanId,
    bool hasError,
    const std::string& errorMessage,
    int statusCode)
{
    auto it = m_spans.find(spanId);
    if (it != m_spans.end()) {
        it->second.endTime = std::chrono::system_clock::now();
        it->second.hasError = hasError;
        it->second.errorMessage = errorMessage;
        it->second.statusCode = statusCode;

        if (m_spanCb) m_spanCb(spanId, m_spanCbData);
    }
}

void AgenticObservability::setSpanAttribute(
    const std::string& spanId,
    const std::string& key,
    const nlohmann::json& value)
{
    auto it = m_spans.find(spanId);
    if (it != m_spans.end()) {
        it->second.attributes[key] = value;
    }
}

void AgenticObservability::addSpanEvent(
    const std::string& spanId,
    const std::string& eventName,
    const nlohmann::json& attributes)
{
    auto it = m_spans.find(spanId);
    if (it != m_spans.end()) {
        nlohmann::json event;
        event["name"] = eventName;
        event["timestamp"] = timePointToISO(std::chrono::system_clock::now());
        event["attributes"] = attributes;
        
        // Store event in attributes array
        if (!it->second.attributes.contains("events")) {
            it->second.attributes["events"] = nlohmann::json::array();
        }
        it->second.attributes["events"].push_back(event);
    }
}

AgenticObservability::TraceSpan* AgenticObservability::getSpan(const std::string& spanId)
{
    auto it = m_spans.find(spanId);
    if (it != m_spans.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AgenticObservability::TraceSpan> AgenticObservability::getTraceSpans(
    const std::string& traceId) const
{
    std::vector<TraceSpan> spans;

    auto it = m_traceSpans.find(traceId);
    if (it != m_traceSpans.end()) {
        for (const auto& spanId : it->second) {
            auto spanIt = m_spans.find(spanId);
            if (spanIt != m_spans.end()) {
                spans.push_back(spanIt->second);
            }
        }
    }

    return spans;
}

nlohmann::json AgenticObservability::getTraceVisualization(const std::string& traceId) const
{
    nlohmann::json visualization;

    auto spans = getTraceSpans(traceId);
    nlohmann::json spanArray = nlohmann::json::array();

    for (const auto& span : spans) {
        nlohmann::json spanObj;
        spanObj["spanId"] = span.spanId;
        spanObj["operation"] = span.operation;
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            span.endTime - span.startTime).count();
        spanObj["duration"] = static_cast<int>(durationMs);
        spanObj["hasError"] = span.hasError;

        spanArray.push_back(spanObj);
    }

    visualization["traceId"] = traceId;
    visualization["spans"] = spanArray;

    return visualization;
}

// ===== DIAGNOSTICS =====

nlohmann::json AgenticObservability::getSystemHealth() const
{
    nlohmann::json health;

    int errorCount = 0;
    for (const auto& pair : m_errorCounts) {
        errorCount += pair.second;
    }

    auto now = std::chrono::system_clock::now();
    float uptime = std::chrono::duration<float>(now - m_systemStartTime).count();

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
    auto health = getSystemHealth();
    return health.value("healthy", true);
}

nlohmann::json AgenticObservability::getPerformanceSummary() const
{
    nlohmann::json summary;

    // Find latency-related metrics
    for (const auto& metric : m_metrics) {
        if (metric.metricName.find("duration") != std::string::npos) {
            std::string histogramName = metric.metricName;
            auto pos = histogramName.find("_duration");
            if (pos != std::string::npos) {
                histogramName.erase(pos);
            }
            auto stats = getHistogramStats(histogramName);
            summary[metric.metricName] = stats;
        }
    }

    return summary;
}

nlohmann::json AgenticObservability::getErrorSummary() const
{
    nlohmann::json summary;

    nlohmann::json errorsByComponent;
    for (const auto& pair : m_errorCounts) {
        errorsByComponent[pair.first] = pair.second;
    }

    summary["errors_by_component"] = errorsByComponent;
    summary["total_errors"] = getSystemHealth().value("total_errors", 0);

    return summary;
}

std::vector<std::string> AgenticObservability::detectBottlenecks()
{
    std::vector<std::string> bottlenecks;

    // Aggregate duration metrics: sum and count per metric name
    std::unordered_map<std::string, float> sumDurations;
    std::unordered_map<std::string, int>   countDurations;

    for (const auto& metric : m_metrics) {
        if (metric.metricName.find("duration") != std::string::npos) {
            sumDurations[metric.metricName] += metric.value;
            countDurations[metric.metricName]++;
        }
    }

    // Compute averages and flag operations exceeding 100ms threshold
    constexpr float BOTTLENECK_THRESHOLD_MS = 100.0f;

    for (const auto& pair : sumDurations) {
        int count = countDurations[pair.first];
        if (count == 0) continue;
        float avg = pair.second / static_cast<float>(count);
        if (avg > BOTTLENECK_THRESHOLD_MS) {
            bottlenecks.push_back(pair.first + " avg=" +
                std::to_string(avg) + "ms (" +
                std::to_string(count) + " samples)");
        }
    }

    // Sort by severity (longest average first)
    std::sort(bottlenecks.begin(), bottlenecks.end(),
        [&](const std::string& a, const std::string& b) {
            // Extract avg value from string for ordering
            auto extractAvg = [](const std::string& s) -> float {
                auto pos = s.find("avg=");
                if (pos == std::string::npos) return 0.0f;
                return std::stof(s.substr(pos + 4));
            };
            return extractAvg(a) > extractAvg(b);
        });

    return bottlenecks;
}

nlohmann::json AgenticObservability::analyzeLatency()
{
    return getPerformanceSummary();
}

// ===== EXPORT/REPORTING =====

std::string AgenticObservability::generateReport(
    const TimePoint& startTime,
    const TimePoint& endTime) const
{
    std::ostringstream report;
    report << "=== OBSERVABILITY REPORT ===\n\n";

    report << "LOGS:\n";
    auto logs = getLogs(100, LogLevel::ObsDebug);
    for (const auto& log : logs) {
        if (log.timestamp < startTime || log.timestamp > endTime) continue;
        report << "[" << timePointToISO(log.timestamp) << "] "
               << log.component << ": " << log.message << "\n";
    }

    report << "\nMETRICS:\n";
    for (const auto& metric : getMetrics("", 50)) {
        if (metric.timestamp < startTime || metric.timestamp > endTime) continue;
        report << metric.metricName << " = " << metric.value << "\n";
    }

    return report.str();
}

std::string AgenticObservability::exportMetricsAsCsv() const
{
    std::ostringstream csv;
    csv << "timestamp,metric_name,value,unit\n";

    for (const auto& metric : m_metrics) {
        csv << timePointToISO(metric.timestamp) << ","
            << metric.metricName << ","
            << metric.value << ","
            << metric.unit << "\n";
    }

    return csv.str();
}

std::string AgenticObservability::exportTracesAsJson() const
{
    nlohmann::json traces = nlohmann::json::array();

    for (const auto& pair : m_traceSpans) {
        traces.push_back(getTraceVisualization(pair.first));
    }

    return traces.dump(2);
}

std::string AgenticObservability::exportLogsAsJson() const
{
    nlohmann::json logs = nlohmann::json::array();

    for (const auto& log : m_logs) {
        nlohmann::json obj;
        obj["timestamp"] = timePointToISO(log.timestamp);
        obj["level"] = levelToString(log.level);
        obj["component"] = log.component;
        obj["message"] = log.message;
        obj["context"] = log.context;

        logs.push_back(obj);
    }

    return logs.dump(2);
}

// ===== PRIVATE HELPERS =====

std::string AgenticObservability::generateTraceId()
{
    return GenerateHexId(32);
}

std::string AgenticObservability::generateSpanId()
{
    return GenerateHexId(16);
}

std::string AgenticObservability::levelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::ObsDebug: return "DEBUG";
        case LogLevel::ObsInfo: return "INFO";
        case LogLevel::ObsWarn: return "WARN";
        case LogLevel::ObsError: return "ERROR";
        case LogLevel::ObsCritical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void AgenticObservability::checkAndRotateLogs()
{
    if (static_cast<int>(m_logs.size()) > m_maxLogEntries) {
        int toRemove = static_cast<int>(m_logs.size()) - (m_maxLogEntries * 9 / 10);
        m_logs.erase(m_logs.begin(), m_logs.begin() + toRemove);
    }
}

void AgenticObservability::prune()
{
    checkAndRotateLogs();
}
