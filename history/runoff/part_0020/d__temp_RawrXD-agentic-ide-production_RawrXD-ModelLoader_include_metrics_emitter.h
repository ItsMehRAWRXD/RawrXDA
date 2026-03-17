// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\include\metrics_emitter.h
// Metrics emitter header for observability

#pragma once

#include <QString>
#include <QMap>
#include <memory>
#include <QMutex>

namespace RawrXD {
namespace Monitoring {

enum class MetricType {
    Counter,
    Gauge,
    Histogram,
    Summary
};

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class MetricsEmitter {
public:
    class Impl;
    
    MetricsEmitter();
    ~MetricsEmitter();
    
    // Prometheus metrics
    void recordCounter(const QString& metricName, long long value = 1,
                      const QMap<QString, QString>& labels = {});
    void recordGauge(const QString& metricName, double value,
                    const QMap<QString, QString>& labels = {});
    void recordHistogram(const QString& metricName, double value,
                        const QMap<QString, QString>& labels = {});
    
    // Latency tracking
    void recordLatency(const QString& operationName, long long durationMs,
                     const QMap<QString, QString>& labels = {});
    
    // Distributed tracing (Jaeger)
    void startSpan(const QString& traceId, const QString& spanId,
                  const QString& operationName,
                  const QMap<QString, QString>& tags = {});
    void endSpan(const QString& spanId, const QString& status = "OK");
    
    // Structured logging (JSON)
    void logStructured(LogLevel level, const QString& component,
                      const QString& message,
                      const QMap<QString, QString>& context = {});
    
private:
    QString labelsToString(const QMap<QString, QString>& labels);
    QString getCurrentTimestamp();
    QString logLevelToString(LogLevel level);
    
    std::unique_ptr<Impl> impl;
};

} // namespace Monitoring
} // namespace RawrXD
