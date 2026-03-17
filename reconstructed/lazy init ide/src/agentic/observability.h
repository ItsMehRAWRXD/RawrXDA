#ifndef AGENTIC_OBSERVABILITY_H
#define AGENTIC_OBSERVABILITY_H

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <vector>
#include <map>

class QNetworkAccessManager;
class QTimer;

class AgenticObservability : public QObject {
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

    explicit AgenticObservability(QObject* parent = nullptr);
    ~AgenticObservability();

    // Logging methods
    void log(AgenticObservability::LogLevel level,
             const QString& component,
             const QString& message,
             const QJsonObject& context = QJsonObject());
    
    void logDebug(const QString& component,
                  const QString& message,
                  const QJsonObject& context = QJsonObject());
    
    void logInfo(const QString& component,
                 const QString& message,
                 const QJsonObject& context = QJsonObject());
    
    void logWarn(const QString& component,
                 const QString& message,
                 const QJsonObject& context = QJsonObject());
    
    void logError(const QString& component,
                  const QString& message,
                  const QJsonObject& context = QJsonObject());
    
    void logCritical(const QString& component,
                     const QString& message,
                     const QJsonObject& context = QJsonObject());

    // Metric methods
    void recordMetric(const QString& metricName,
                      float value,
                      const QJsonObject& labels = QJsonObject(),
                      const QString& unit = QString());
    
    void incrementCounter(const QString& metricName,
                          int delta = 1,
                          const QJsonObject& labels = QJsonObject());
    
    void setGauge(const QString& metricName,
                  float value,
                  const QJsonObject& labels = QJsonObject());
    
    void recordHistogram(const QString& metricName,
                         float value,
                         float min,
                         float max,
                         int bucketCount,
                         const QJsonObject& labels = QJsonObject());

    // Query methods
    std::vector<AgenticObservability::LogEntry> getLogs(
        int limit = -1,
        AgenticObservability::LogLevel minLevel = AgenticObservability::LogLevel::DEBUG,
        const QString& component = QString()) const;
    
    std::vector<AgenticObservability::LogEntry> getLogsByTimeRange(
        const QDateTime& start,
        const QDateTime& end,
        AgenticObservability::LogLevel minLevel = AgenticObservability::LogLevel::DEBUG) const;
    
    std::vector<AgenticObservability::MetricPoint> getMetrics(
        const QString& metricName = QString(),
        const QDateTime& start = QDateTime(),
        const QDateTime& end = QDateTime()) const;
    
    float getCounterValue(const QString& metricName) const;
    float getGaugeValue(const QString& metricName) const;
    
    std::vector<float> getHistogramValues(
        const QString& metricName,
        const QDateTime& start = QDateTime(),
        const QDateTime& end = QDateTime()) const;

    // Configuration
    void setSamplingRate(double rate) { m_samplingRate = rate; }
    void setMaxLogEntries(int max) { m_maxLogEntries = max; }
    void setMetricsBufferSize(int size) { m_metricsBufferSize = size; }
    void setHistogramBucketCount(int count) { m_histogramBucketCount = count; }
    
    double samplingRate() const { return m_samplingRate; }
    int maxLogEntries() const { return m_maxLogEntries; }
    int metricsBufferSize() const { return m_metricsBufferSize; }
    int histogramBucketCount() const { return m_histogramBucketCount; }

    // Statistics
    int totalLogsWritten() const { return m_totalLogsWritten; }
    int totalMetricsRecorded() const { return m_totalMetricsRecorded; }
    int errorCount(const QString& component) const;
    QDateTime systemStartTime() const { return m_systemStartTime; }

signals:
    void logWritten(const AgenticObservability::LogEntry& entry);
    void metricRecorded(const QString& metricName);

private:
    QString generateTraceId() const;
    QString generateSpanId() const;
    QString logLevelToString(AgenticObservability::LogLevel level) const;
    
    // Data storage
    std::vector<AgenticObservability::LogEntry> m_logs;
    std::vector<AgenticObservability::MetricPoint> m_metrics;
    std::map<std::string, int> m_errorCounts;
    
    // Configuration
    double m_samplingRate = 1.0;
    int m_maxLogEntries = 10000;
    int m_metricsBufferSize = 10000;
    int m_histogramBucketCount = 10;
    
    // Statistics
    int m_totalLogsWritten = 0;
    int m_totalMetricsRecorded = 0;
    QDateTime m_systemStartTime;
};

#endif // AGENTIC_OBSERVABILITY_H
