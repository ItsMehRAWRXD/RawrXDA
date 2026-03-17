#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

/**
 * @class AgenticObservability
 * @brief Comprehensive observability for agentic systems (Qt-free)
 * 
 * Three pillars of observability:
 * 1. STRUCTURED LOGGING: Detailed context at key points
 * 2. METRICS: Quantifiable measurements for performance tracking
 * 3. DISTRIBUTED TRACING: Request flow visualization across components
 * 
 * Enables production monitoring, debugging, and performance optimization.
 * No Qt dependency. Uses function pointer callbacks instead of signals.
 */
class AgenticObservability
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    struct LogEntry {
        TimePoint timestamp;
        LogLevel level;
        std::string component;
        std::string message;
        nlohmann::json context;
        std::string traceId;
        std::string spanId;
    };

    struct MetricPoint {
        std::string metricName;
        float value;
        nlohmann::json labels;
        TimePoint timestamp;
        std::string unit;
    };

    struct TraceSpan {
        std::string spanId;
        std::string parentSpanId;
        std::string traceId;
        std::string operation;
        TimePoint startTime;
        TimePoint endTime;
        nlohmann::json attributes;
        bool hasError;
        std::string errorMessage;
        int statusCode;
    };

    // Callback types (replaces Qt signals)
    using LogCallback    = void(*)(const LogEntry& entry, void* userData);
    using MetricCallback = void(*)(const std::string& metricName, void* userData);
    using SpanCallback   = void(*)(const std::string& spanId, void* userData);
    using AnomalyCallback= void(*)(const std::string& description, void* userData);

public:
    AgenticObservability();
    ~AgenticObservability();

    // ===== STRUCTURED LOGGING =====
    void log(
        LogLevel level,
        const std::string& component,
        const std::string& message,
        const nlohmann::json& context = nlohmann::json::object()
    );

    void logDebug(const std::string& component, const std::string& message, const nlohmann::json& context = nlohmann::json::object());
    void logInfo(const std::string& component, const std::string& message, const nlohmann::json& context = nlohmann::json::object());
    void logWarn(const std::string& component, const std::string& message, const nlohmann::json& context = nlohmann::json::object());
    void logError(const std::string& component, const std::string& message, const nlohmann::json& context = nlohmann::json::object());
    void logCritical(const std::string& component, const std::string& message, const nlohmann::json& context = nlohmann::json::object());

    // Retrieve logs
    std::vector<LogEntry> getLogs(
        int limit = 100,
        LogLevel minLevel = LogLevel::DEBUG,
        const std::string& component = ""
    ) const;
    std::vector<LogEntry> getLogsByTimeRange(
        const TimePoint& start,
        const TimePoint& end,
        LogLevel minLevel = LogLevel::DEBUG
    ) const;

    // ===== METRICS =====
    
    // Record metric points
    void recordMetric(
        const std::string& metricName,
        float value,
        const nlohmann::json& labels = nlohmann::json::object(),
        const std::string& unit = ""
    );

    // Counters
    void incrementCounter(const std::string& metricName, int delta = 1, const nlohmann::json& labels = nlohmann::json::object());
    float getCounterValue(const std::string& metricName) const;

    // Gauges (point-in-time measurements)
    void setGauge(const std::string& metricName, float value, const nlohmann::json& labels = nlohmann::json::object());
    float getGaugeValue(const std::string& metricName) const;

    // Histograms (distributions)
    void recordHistogram(
        const std::string& metricName,
        float value,
        const nlohmann::json& labels = nlohmann::json::object()
    );
    nlohmann::json getHistogramStats(const std::string& metricName) const;

    // Timing measurements
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

    // Get metrics
    std::vector<MetricPoint> getMetrics(const std::string& pattern = "", int limit = 100) const;
    nlohmann::json getMetricsSummary() const;
    nlohmann::json getPercentiles(const std::string& metricName) const;

    // ===== DISTRIBUTED TRACING =====
    
    // Start trace
    std::string startTrace(const std::string& operation);
    
    // Start span (child of current trace)
    std::string startSpan(const std::string& spanName, const std::string& parentSpanId = "");
    void endSpan(
        const std::string& spanId,
        bool hasError = false,
        const std::string& errorMessage = "",
        int statusCode = 0
    );
    
    // Set span attributes
    void setSpanAttribute(const std::string& spanId, const std::string& key, const nlohmann::json& value);
    void addSpanEvent(const std::string& spanId, const std::string& eventName, const nlohmann::json& attributes);

    // Get trace information
    TraceSpan* getSpan(const std::string& spanId);
    std::vector<TraceSpan> getTraceSpans(const std::string& traceId) const;
    nlohmann::json getTraceVisualization(const std::string& traceId) const;

    // ===== DIAGNOSTICS =====
    
    // Health checks
    nlohmann::json getSystemHealth() const;
    bool isHealthy() const;
    
    // Performance summary
    nlohmann::json getPerformanceSummary() const;
    nlohmann::json getErrorSummary() const;
    
    // Bottleneck detection
    std::vector<std::string> detectBottlenecks();
    nlohmann::json analyzeLatency();

    // ===== EXPORT/REPORTING =====
    std::string generateReport(
        const TimePoint& startTime,
        const TimePoint& endTime
    ) const;
    
    std::string exportMetricsAsCsv() const;
    std::string exportTracesAsJson() const;
    std::string exportLogsAsJson() const;

    // ===== CONFIGURATION =====
    void setLogLevel(LogLevel level) { m_minLogLevel = level; }
    void setMaxLogEntries(int max) { m_maxLogEntries = max; }
    void setMetricsBufferSize(int size) { m_metricsBufferSize = size; }
    void setTracingEnabled(bool enabled) { m_tracingEnabled = enabled; }
    void setSamplingRate(float rate) { m_samplingRate = rate; }

    // ===== CALLBACKS (replaces Qt signals) =====
    void setLogCallback(LogCallback cb, void* userData = nullptr) { m_logCb = cb; m_logCbData = userData; }
    void setMetricCallback(MetricCallback cb, void* userData = nullptr) { m_metricCb = cb; m_metricCbData = userData; }
    void setSpanCallback(SpanCallback cb, void* userData = nullptr) { m_spanCb = cb; m_spanCbData = userData; }
    void setAnomalyCallback(AnomalyCallback cb, void* userData = nullptr) { m_anomalyCb = cb; m_anomalyCbData = userData; }

private:
    // Helper methods
    std::string generateTraceId();
    std::string generateSpanId();
    std::string levelToString(LogLevel level) const;
    static std::string timePointToISO(const TimePoint& tp);
    void checkAndRotateLogs();
    void prune();

    // Storage
    std::vector<LogEntry> m_logs;
    std::vector<MetricPoint> m_metrics;
    std::unordered_map<std::string, TraceSpan> m_spans;
    std::unordered_map<std::string, std::vector<std::string>> m_traceSpans; // trace_id -> span_ids

    // Configuration
    LogLevel m_minLogLevel = LogLevel::DEBUG;
    int m_maxLogEntries = 10000;
    int m_metricsBufferSize = 5000;
    bool m_tracingEnabled = true;
    float m_samplingRate = 1.0f;
    
    // State
    TimePoint m_systemStartTime;
    int m_totalLogsWritten = 0;
    int m_totalMetricsRecorded = 0;
    std::unordered_map<std::string, int> m_errorCounts; // component -> count

    // Callbacks (function pointers, per project rules — no std::function)
    LogCallback    m_logCb = nullptr;       void* m_logCbData = nullptr;
    MetricCallback m_metricCb = nullptr;    void* m_metricCbData = nullptr;
    SpanCallback   m_spanCb = nullptr;      void* m_spanCbData = nullptr;
    AnomalyCallback m_anomalyCb = nullptr;  void* m_anomalyCbData = nullptr;
};
