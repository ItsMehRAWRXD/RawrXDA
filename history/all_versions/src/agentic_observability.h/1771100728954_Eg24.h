// =============================================================================
// agentic_observability.h — Comprehensive Observability System (Qt-free)
// =============================================================================
// Structured logging, metrics, distributed tracing, and diagnostics.
// Used by ToolRegistry, DiskRecoveryAgent, and all agentic subsystems.
//
// No exceptions. No Qt. Uses nlohmann::json for structured context.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// Log level enum
// ---------------------------------------------------------------------------
enum class LogLevel : int {
    DEBUG_LEVEL    = 0,
    INFO_LEVEL     = 1,
    WARN_LEVEL     = 2,
    ERROR_LEVEL    = 3,
    CRITICAL_LEVEL = 4
};

// ---------------------------------------------------------------------------
// AgenticObservability — Singleton-friendly observability hub
// ---------------------------------------------------------------------------
class AgenticObservability {
public:
    using TimePoint = std::chrono::system_clock::time_point;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    AgenticObservability();
    ~AgenticObservability();

    // -----------------------------------------------------------------------
    // Structured Logging
    // -----------------------------------------------------------------------
    struct LogEntry {
        TimePoint       timestamp;
        LogLevel        level;
        std::string     component;
        std::string     message;
        nlohmann::json  context;
        std::string     traceId;
        std::string     spanId;
    };

    void log(LogLevel level,
             const std::string& component,
             const std::string& message,
             const nlohmann::json& context = {});

    void logDebug(const std::string& component, const std::string& message,
                  const nlohmann::json& context = {});
    void logInfo(const std::string& component, const std::string& message,
                 const nlohmann::json& context = {});
    void logWarn(const std::string& component, const std::string& message,
                 const nlohmann::json& context = {});
    void logError(const std::string& component, const std::string& message,
                  const nlohmann::json& context = {});
    void logCritical(const std::string& component, const std::string& message,
                     const nlohmann::json& context = {});

    std::vector<LogEntry> getLogs(int limit = 100,
                                 LogLevel minLevel = LogLevel::DEBUG,
                                 const std::string& component = "") const;

    std::vector<LogEntry> getLogsByTimeRange(const TimePoint& start,
                                             const TimePoint& end,
                                             LogLevel minLevel = LogLevel::DEBUG) const;

    // Log callback
    using LogCallback = void(*)(const LogEntry& entry, void* userData);
    void setLogCallback(LogCallback cb, void* userData = nullptr) {
        m_logCb = cb; m_logCbData = userData;
    }

    // -----------------------------------------------------------------------
    // Metrics
    // -----------------------------------------------------------------------
    struct MetricPoint {
        std::string     metricName;
        float           value;
        nlohmann::json  labels;
        TimePoint       timestamp;
        std::string     unit;
    };

    void recordMetric(const std::string& metricName, float value,
                      const nlohmann::json& labels = {},
                      const std::string& unit = "");

    void incrementCounter(const std::string& metricName, int delta = 1,
                          const nlohmann::json& labels = {});
    float getCounterValue(const std::string& metricName) const;

    void setGauge(const std::string& metricName, float value,
                  const nlohmann::json& labels = {});
    float getGaugeValue(const std::string& metricName) const;

    void recordHistogram(const std::string& metricName, float value,
                         const nlohmann::json& labels = {});
    nlohmann::json getHistogramStats(const std::string& metricName) const;

    std::vector<MetricPoint> getMetrics(const std::string& pattern = "",
                                        int limit = 100) const;
    nlohmann::json getMetricsSummary() const;
    nlohmann::json getPercentiles(const std::string& metricName) const;

    // Metric callback
    using MetricCallback = void(*)(const std::string& metricName, void* userData);
    void setMetricCallback(MetricCallback cb, void* userData = nullptr) {
        m_metricCb = cb; m_metricCbData = userData;
    }

    // -----------------------------------------------------------------------
    // Timing guard (RAII duration measurement)
    // -----------------------------------------------------------------------
    class TimingGuard {
    public:
        TimingGuard(AgenticObservability* obs, const std::string& metricName);
        ~TimingGuard();
    private:
        AgenticObservability* m_obs;
        std::string m_metricName;
        std::chrono::high_resolution_clock::time_point m_start;
    };

    std::unique_ptr<TimingGuard> measureDuration(const std::string& metricName);

    // -----------------------------------------------------------------------
    // Distributed Tracing
    // -----------------------------------------------------------------------
    struct TraceSpan {
        std::string     spanId;
        std::string     parentSpanId;
        std::string     operation;
        TimePoint       startTime;
        TimePoint       endTime;
        bool            hasError = false;
        std::string     errorMessage;
        int             statusCode = 0;
        nlohmann::json  attributes;
    };

    std::string startTrace(const std::string& operation);
    std::string startSpan(const std::string& spanName,
                          const std::string& parentSpanId = "");
    void endSpan(const std::string& spanId,
                 bool hasError = false,
                 const std::string& errorMessage = "",
                 int statusCode = 0);

    void setSpanAttribute(const std::string& spanId,
                          const std::string& key,
                          const nlohmann::json& value);
    void addSpanEvent(const std::string& spanId,
                      const std::string& eventName,
                      const nlohmann::json& attributes = {});

    TraceSpan* getSpan(const std::string& spanId);
    std::vector<TraceSpan> getTraceSpans(const std::string& traceId) const;
    nlohmann::json getTraceVisualization(const std::string& traceId) const;

    void setTracingEnabled(bool enabled) { m_tracingEnabled = enabled; }

    // Span callback
    using SpanCallback = void(*)(const std::string& spanId, void* userData);
    void setSpanCallback(SpanCallback cb, void* userData = nullptr) {
        m_spanCb = cb; m_spanCbData = userData;
    }

    // -----------------------------------------------------------------------
    // Diagnostics / Health
    // -----------------------------------------------------------------------
    nlohmann::json getSystemHealth() const;
    bool isHealthy() const;
    nlohmann::json getPerformanceSummary() const;
    nlohmann::json getErrorSummary() const;
    std::vector<std::string> detectBottlenecks();
    nlohmann::json analyzeLatency();

    // -----------------------------------------------------------------------
    // Export / Reporting
    // -----------------------------------------------------------------------
    std::string generateReport(const TimePoint& startTime,
                               const TimePoint& endTime) const;
    std::string exportMetricsAsCsv() const;
    std::string exportTracesAsJson() const;
    std::string exportLogsAsJson() const;

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    void setSamplingRate(double rate) { m_samplingRate = rate; }
    void setMaxLogEntries(int max) { m_maxLogEntries = max; }
    void setMetricsBufferSize(int size) { m_metricsBufferSize = size; }
    void prune();

    // -----------------------------------------------------------------------
    // Time helpers
    // -----------------------------------------------------------------------
    static std::string timePointToISO(const TimePoint& tp);

private:
    std::string generateTraceId();
    std::string generateSpanId();
    std::string levelToString(LogLevel level) const;
    void checkAndRotateLogs();

    // Logs
    std::vector<LogEntry>   m_logs;
    int                     m_maxLogEntries = 10000;
    int                     m_totalLogsWritten = 0;

    // Metrics
    std::vector<MetricPoint> m_metrics;
    int                     m_metricsBufferSize = 50000;
    int                     m_totalMetricsRecorded = 0;

    // Traces
    std::unordered_map<std::string, std::vector<std::string>> m_traceSpans;
    std::unordered_map<std::string, TraceSpan>                m_spans;
    bool                    m_tracingEnabled = true;

    // Error tracking
    std::unordered_map<std::string, int> m_errorCounts;

    // Timing
    TimePoint               m_systemStartTime;

    // Sampling
    double                  m_samplingRate = 1.0;

    // Callbacks
    LogCallback             m_logCb = nullptr;
    void*                   m_logCbData = nullptr;
    MetricCallback          m_metricCb = nullptr;
    void*                   m_metricCbData = nullptr;
    SpanCallback            m_spanCb = nullptr;
    void*                   m_spanCbData = nullptr;
};
