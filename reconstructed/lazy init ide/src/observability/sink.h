#ifndef OBSERVABILITY_SINK_H
#define OBSERVABILITY_SINK_H

#include <QObject>
#include <QFile>
#include <QDateTime>
#include <QJsonObject>
#include <mutex>
#include <vector>
#include "agentic_observability.h"

class QNetworkAccessManager;
class QTimer;

namespace RawrXD {

class ObservabilitySink : public QObject
{
    Q_OBJECT

public:
    explicit ObservabilitySink(QObject* parent = nullptr);
    ~ObservabilitySink();

    void setOtlpEndpoint(const QString& endpoint);
    void setPrometheusEndpoint(const QString& endpoint);
    void enableOtlp(bool enable);
    void enablePrometheus(bool enable);
    void enableFileRotation(bool enable);
    void setMaxLogFileSize(qint64 size);
    void setMaxMetricFileSize(qint64 size);

    void writeLog(const AgenticObservability::LogEntry& entry);
    void writeMetric(const AgenticObservability::MetricPoint& metric);

    void flush();

private slots:
    void flushLogs();
    void flushMetrics();
    void checkRotation();
    void exportToOtlp();
    void exportToPrometheus();

private:
    void rotateLogFile();
    void rotateMetricFile();
    void closeFiles();
    QString logLevelToString(AgenticObservability::LogLevel level) const;

    // File management
    QFile logFile_;
    QFile metricFile_;
    QString logFilePath_;
    QString metricFilePath_;
    qint64 maxLogFileSize_;
    qint64 maxMetricFileSize_;
    bool fileRotationEnabled_;
    QDateTime lastRotationDate_;

    // Buffers
    std::vector<AgenticObservability::LogEntry> logBuffer_;
    std::vector<AgenticObservability::MetricPoint> metricBuffer_;
    std::mutex logBufferMutex_;
    std::mutex metricBufferMutex_;

    // Export endpoints
    QString otlpEndpoint_;
    QString prometheusEndpoint_;
    bool otlpEnabled_;
    bool prometheusEnabled_;
    QNetworkAccessManager* nam_;
    QTimer* flushTimer_;
    QTimer* rotationTimer_;
};

} // namespace RawrXD

#endif // OBSERVABILITY_SINK_H
