// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\monitoring\metrics_emitter.cpp
// Production metrics emission for Prometheus, structured logging, and distributed tracing

#include "metrics_emitter.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <atomic>

namespace RawrXD {
namespace Monitoring {

class MetricsEmitter::Impl {
public:
    struct MetricData {
        QString name;
        double value = 0.0;
        std::map<QString, QString> labels;
        QDateTime timestamp;
        MetricType type = MetricType::Gauge;
    };
    
    struct TraceSpan {
        QString spanId;
        QString traceId;
        QString parentSpanId;
        QString operationName;
        QDateTime startTime;
        QDateTime endTime;
        std::map<QString, QString> tags;
        std::vector<QString> logs;
        QString status = "OK";
    };
    
    std::map<QString, MetricData> metrics;
    std::vector<TraceSpan> traces;
    std::map<QString, std::atomic<long long>> counters;
    std::map<QString, std::vector<double>> histograms;
    std::map<QString, std::vector<double>> gauges;
    
    bool prometheusEnabled = true;
    bool jaegerEnabled = true;
    bool structuredLoggingEnabled = true;
    
    QString serviceName = "RawrXD-IDE";
    QString environment = "production";
    QString version = "1.0.0";
    
    QMutex metricsMutex;
};

MetricsEmitter::MetricsEmitter()
    : impl(std::make_unique<Impl>())
{
}

MetricsEmitter::~MetricsEmitter() = default;

void MetricsEmitter::recordCounter(const QString& metricName, long long value, 
                                    const QMap<QString, QString>& labels) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->prometheusEnabled) {
        impl->counters[metricName] += value;
        
        // Emit Prometheus metric
        qInfo() << QString("METRIC [COUNTER] %1=%2 %3 %4")
            .arg(metricName)
            .arg(impl->counters[metricName])
            .arg(labelsToString(labels))
            .arg(getCurrentTimestamp());
    }
}

void MetricsEmitter::recordGauge(const QString& metricName, double value,
                                  const QMap<QString, QString>& labels) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->prometheusEnabled) {
        Impl::MetricData metric;
        metric.name = metricName;
        metric.value = value;
        metric.type = MetricType::Gauge;
        metric.timestamp = QDateTime::currentDateTime();
        metric.labels = labels.toStdMap();
        
        impl->metrics[metricName] = metric;
        impl->gauges[metricName].push_back(value);
        
        qInfo() << QString("METRIC [GAUGE] %1=%2 %3 %4")
            .arg(metricName)
            .arg(value)
            .arg(labelsToString(labels))
            .arg(getCurrentTimestamp());
    }
}

void MetricsEmitter::recordHistogram(const QString& metricName, double value,
                                      const QMap<QString, QString>& labels) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->prometheusEnabled) {
        impl->histograms[metricName].push_back(value);
        
        // Calculate percentiles
        auto& data = impl->histograms[metricName];
        std::sort(data.begin(), data.end());
        
        double p50 = data[data.size() * 50 / 100];
        double p95 = data[data.size() * 95 / 100];
        double p99 = data[data.size() * 99 / 100];
        
        qInfo() << QString("METRIC [HISTOGRAM] %1 p50=%2 p95=%3 p99=%4 %5 %6")
            .arg(metricName)
            .arg(p50)
            .arg(p95)
            .arg(p99)
            .arg(labelsToString(labels))
            .arg(getCurrentTimestamp());
    }
}

void MetricsEmitter::recordLatency(const QString& operationName, long long durationMs,
                                    const QMap<QString, QString>& labels) {
    QMutexLocker lock(&impl->metricsMutex);
    
    recordHistogram(operationName + "_latency_ms", durationMs, labels);
    
    if (impl->structuredLoggingEnabled) {
        qInfo() << QString("{\"event\":\"operation_latency\",\"operation\":\"%1\",\"duration_ms\":%2,\"timestamp\":\"%3\"}")
            .arg(operationName)
            .arg(durationMs)
            .arg(getCurrentTimestamp());
    }
}

void MetricsEmitter::startSpan(const QString& traceId, const QString& spanId,
                                const QString& operationName,
                                const QMap<QString, QString>& tags) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->jaegerEnabled) {
        Impl::TraceSpan span;
        span.traceId = traceId;
        span.spanId = spanId;
        span.operationName = operationName;
        span.startTime = QDateTime::currentDateTime();
        span.tags = tags.toStdMap();
        span.tags["service.name"] = impl->serviceName;
        span.tags["environment"] = impl->environment;
        
        impl->traces.push_back(span);
        
        qDebug() << QString("[TRACE] START span=%1 trace=%2 operation=%3")
            .arg(spanId).arg(traceId).arg(operationName);
    }
}

void MetricsEmitter::endSpan(const QString& spanId, const QString& status) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->jaegerEnabled) {
        for (auto& span : impl->traces) {
            if (span.spanId == spanId) {
                span.endTime = QDateTime::currentDateTime();
                span.status = status;
                
                long long durationUs = span.startTime.msecsTo(span.endTime) * 1000;
                
                qDebug() << QString("[TRACE] END span=%1 status=%2 duration_us=%3")
                    .arg(spanId).arg(status).arg(durationUs);
                break;
            }
        }
    }
}

void MetricsEmitter::logStructured(LogLevel level, const QString& component,
                                    const QString& message,
                                    const QMap<QString, QString>& context) {
    QMutexLocker lock(&impl->metricsMutex);
    
    if (impl->structuredLoggingEnabled) {
        QJsonObject logObject;
        logObject["timestamp"] = getCurrentTimestamp();
        logObject["level"] = logLevelToString(level);
        logObject["component"] = component;
        logObject["message"] = message;
        logObject["service"] = impl->serviceName;
        logObject["environment"] = impl->environment;
        logObject["version"] = impl->version;
        
        for (const auto& [key, value] : context.toStdMap()) {
            logObject[key] = value;
        }
        
        QJsonDocument doc(logObject);
        std::cout << doc.toJson(QJsonDocument::Compact).toStdString() << std::endl;
    }
}

QString MetricsEmitter::labelsToString(const QMap<QString, QString>& labels) {
    QString result;
    for (const auto& [key, value] : labels.toStdMap()) {
        if (!result.isEmpty()) result += ",";
        result += key + "=" + value;
    }
    return "{" + result + "}";
}

QString MetricsEmitter::getCurrentTimestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzz");
}

QString MetricsEmitter::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace Monitoring
} // namespace RawrXD
