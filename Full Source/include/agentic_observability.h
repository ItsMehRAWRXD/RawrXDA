#pragma once

#ifdef RAWRXD_NO_QT
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <string>

/** Stub observability for non-Qt (GMake/MinGW) builds */
class AgenticObservability {
public:
    enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, CRITICAL = 4 };
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string component;
        std::string message;
        std::string traceId;
        std::string spanId;
    };
    struct MetricPoint { std::string metricName; float value; std::string unit; std::chrono::system_clock::time_point timestamp; };
    struct TraceSpan { std::string spanId, parentSpanId, traceId, operation; bool hasError; std::string errorMessage; int statusCode; };

    explicit AgenticObservability(void* parent = nullptr);
    ~AgenticObservability();

    void log(LogLevel level, const std::string& component, const std::string& message, const void* context = nullptr);
    void logDebug(const std::string& component, const std::string& message, const void* context = nullptr);
    void logInfo(const std::string& component, const std::string& message, const void* context = nullptr);
    void logWarn(const std::string& component, const std::string& message, const void* context = nullptr);
    void logError(const std::string& component, const std::string& message, const void* context = nullptr);
    void logCritical(const std::string& component, const std::string& message, const void* context = nullptr);

    std::vector<LogEntry> getLogs(int limit = 100, LogLevel minLevel = LogLevel::DEBUG, const std::string& component = "");
    void recordMetric(const std::string& metricName, float value, const void* labels = nullptr, const std::string& unit = "");
    void incrementCounter(const std::string& metricName, int delta = 1, const void* labels = nullptr);
    float getCounterValue(const std::string& metricName) const;
    void setGauge(const std::string& metricName, float value, const void* labels = nullptr);
    float getGaugeValue(const std::string& metricName) const;
    void recordHistogram(const std::string& metricName, float value, const void* labels = nullptr);
    std::string startTrace(const std::string& operation);
    std::string startSpan(const std::string& spanName, const std::string& parentSpanId = "");
    void endSpan(const std::string& spanId, bool hasError = false, const std::string& errorMessage = "", int statusCode = 0);
    void setLogLevel(LogLevel level) { m_minLogLevel = level; }
    void setMaxLogEntries(int max) { m_maxLogEntries = max; }
    void setSamplingRate(float rate) { m_samplingRate = rate; }

private:
    std::string generateTraceId();
    std::string generateSpanId();
    std::vector<LogEntry> m_logs;
    LogLevel m_minLogLevel = LogLevel::DEBUG;
    int m_maxLogEntries = 10000;
    float m_samplingRate = 1.0f;
    std::chrono::system_clock::time_point m_systemStartTime;
    int m_totalLogsWritten = 0;
    int m_totalMetricsRecorded = 0;
};

#else

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>

/**
 * @class AgenticObservability
 * @brief Comprehensive observability for agentic systems
 */
class AgenticObservability : public QObject
{
    Q_OBJECT

public:
    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    struct LogEntry {
        QDateTime timestamp;
        LogLevel level;
        QString component;
        QString message;
        QJsonObject context;
        QString traceId;
        QString spanId;
    };

    struct MetricPoint {
        QString metricName;
        float value;
        QJsonObject labels;
        QDateTime timestamp;
        QString unit;
    };

    struct TraceSpan {
        QString spanId;
        QString parentSpanId;
        QString traceId;
        QString operation;
        QDateTime startTime;
        QDateTime endTime;
        QJsonObject attributes;
        bool hasError;
        QString errorMessage;
        int statusCode;
    };

public:
    explicit AgenticObservability(QObject* parent = nullptr);
    ~AgenticObservability();

    // ===== STRUCTURED LOGGING =====
    void log(
        LogLevel level,
        const QString& component,
        const QString& message,
        const QJsonObject& context = QJsonObject()
    );

    void logDebug(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logInfo(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logWarn(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logError(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logCritical(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());

    // Retrieve logs
    std::vector<LogEntry> getLogs(
        int limit = 100,
        LogLevel minLevel = LogLevel::DEBUG,
        const QString& component = ""
    );
    std::vector<LogEntry> getLogsByTimeRange(
        const QDateTime& start,
        const QDateTime& end,
        LogLevel minLevel = LogLevel::DEBUG
    );

    // ===== METRICS =====
    
    // Record metric points
    void recordMetric(
        const QString& metricName,
        float value,
        const QJsonObject& labels = QJsonObject(),
        const QString& unit = ""
    );

    // Counters
    void incrementCounter(const QString& metricName, int delta = 1, const QJsonObject& labels = QJsonObject());
    float getCounterValue(const QString& metricName) const;

    // Gauges (point-in-time measurements)
    void setGauge(const QString& metricName, float value, const QJsonObject& labels = QJsonObject());
    float getGaugeValue(const QString& metricName) const;

    // Histograms (distributions)
    void recordHistogram(
        const QString& metricName,
        float value,
        const QJsonObject& labels = QJsonObject()
    );
    QJsonObject getHistogramStats(const QString& metricName) const;

    // Timing measurements
    class TimingGuard {
    public:
        TimingGuard(AgenticObservability* obs, const QString& metricName);
        ~TimingGuard();
    private:
        AgenticObservability* m_obs;
        QString m_metricName;
        std::chrono::high_resolution_clock::time_point m_start;
    };

    std::unique_ptr<TimingGuard> measureDuration(const QString& metricName);

    // Get metrics
    std::vector<MetricPoint> getMetrics(const QString& pattern = "", int limit = 100);
    QJsonObject getMetricsSummary() const;
    QJsonObject getPercentiles(const QString& metricName) const;

    // ===== DISTRIBUTED TRACING =====
    
    // Start trace
    QString startTrace(const QString& operation);
    
    // Start span (child of current trace)
    QString startSpan(const QString& spanName, const QString& parentSpanId = "");
    void endSpan(
        const QString& spanId,
        bool hasError = false,
        const QString& errorMessage = "",
        int statusCode = 0
    );
    
    // Set span attributes
    void setSpanAttribute(const QString& spanId, const QString& key, const QVariant& value);
    void addSpanEvent(const QString& spanId, const QString& eventName, const QJsonObject& attributes);

    // Get trace information
    TraceSpan* getSpan(const QString& spanId);
    std::vector<TraceSpan> getTraceSpans(const QString& traceId);
    QJsonObject getTraceVisualization(const QString& traceId);

    // ===== DIAGNOSTICS =====
    
    // Health checks
    QJsonObject getSystemHealth() const;
    bool isHealthy() const;
    
    // Performance summary
    QJsonObject getPerformanceSummary() const;
    QJsonObject getErrorSummary() const;
    
    // Bottleneck detection
    std::vector<QString> detectBottlenecks();
    QJsonObject analyzeLatency();

    // ===== EXPORT/REPORTING =====
    QString generateReport(
        const QDateTime& startTime,
        const QDateTime& endTime
    ) const;
    
    QString exportMetricsAsCsv() const;
    QString exportTracesAsJson() const;
    QString exportLogsAsJson() const;

    // ===== CONFIGURATION =====
    void setLogLevel(LogLevel level) { m_minLogLevel = level; }
    void setMaxLogEntries(int max) { m_maxLogEntries = max; }
    void setMetricsBufferSize(int size) { m_metricsBufferSize = size; }
    void setTracingEnabled(bool enabled) { m_tracingEnabled = enabled; }
    void setSamplingRate(float rate) { m_samplingRate = rate; }

signals:
    void logWritten(const LogEntry& entry);
    void metricRecorded(const QString& metricName);
    void spanCompleted(const QString& spanId);
    void anomalyDetected(const QString& description);

private:
    // Helper methods
    QString generateTraceId();
    QString generateSpanId();
    QString levelToString(LogLevel level) const;
    void checkAndRotateLogs();
    void prune();

    // Storage
    std::vector<LogEntry> m_logs;
    std::vector<MetricPoint> m_metrics;
    std::unordered_map<std::string, TraceSpan> m_spans;
    std::unordered_map<std::string, std::vector<QString>> m_traceSpans; // trace_id -> span_ids

    // Configuration
    LogLevel m_minLogLevel = LogLevel::DEBUG;
    int m_maxLogEntries = 10000;
    int m_metricsBufferSize = 5000;
    bool m_tracingEnabled = true;
    float m_samplingRate = 1.0f;
    
    // Metrics
    QDateTime m_systemStartTime;
    int m_totalLogsWritten = 0;
    int m_totalMetricsRecorded = 0;
    std::unordered_map<std::string, int> m_errorCounts; // error_type -> count
};

#endif /* !RAWRXD_NO_QT */
