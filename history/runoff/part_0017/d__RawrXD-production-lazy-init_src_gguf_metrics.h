#ifndef GGUF_METRICS_H
#define GGUF_METRICS_H

#include <QString>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QSharedPointer>
#include <QTimer>

namespace RawrXD {

struct GGUFMetric {
    QString name;
    QString type; // "counter", "gauge", "histogram"
    double value;
    QDateTime timestamp;
    QJsonObject labels;
    
    GGUFMetric(const QString& n, const QString& t, double v, const QJsonObject& l = QJsonObject())
        : name(n), type(t), value(v), timestamp(QDateTime::currentDateTime()), labels(l) {}
};

class GGUFMetricsCollector {
public:
    static GGUFMetricsCollector& instance();
    
    void initialize(int port = 8080, const QString& path = "/metrics");
    void shutdown();
    
    // Metric recording
    void recordCounter(const QString& name, double value = 1.0, const QJsonObject& labels = QJsonObject());
    void recordGauge(const QString& name, double value, const QJsonObject& labels = QJsonObject());
    void recordHistogram(const QString& name, double value, const QJsonObject& labels = QJsonObject());
    void recordDuration(const QString& name, qint64 durationMs, const QJsonObject& labels = QJsonObject());
    
    // Metrics query
    QVector<GGUFMetric> getMetrics(const QString& name = QString()) const;
    QJsonObject getMetricsAsJson() const;
    QString getMetricsAsPrometheus() const;
    
    // Aggregation
    void aggregateMetrics();
    void clearOldMetrics(qint64 maxAgeSeconds = 3600); // 1 hour default
    
private:
    GGUFMetricsCollector() = default;
    ~GGUFMetricsCollector();
    
    void startMetricsServer();
    void stopMetricsServer();
    QString formatPrometheusMetric(const GGUFMetric& metric) const;
    
    QHash<QString, QVector<GGUFMetric>> metrics_;
    mutable QMutex mutex_;
    QTimer aggregationTimer_;
    QTimer cleanupTimer_;
    int port_;
    QString path_;
    bool initialized_ = false;
    
    static const int DEFAULT_AGGREGATION_INTERVAL = 60000; // 1 minute
    static const int DEFAULT_CLEANUP_INTERVAL = 300000; // 5 minutes
};

// Convenience macros
#define GGUF_METRIC_COUNTER(name, value, labels) RawrXD::GGUFMetricsCollector::instance().recordCounter(name, value, labels)
#define GGUF_METRIC_GAUGE(name, value, labels) RawrXD::GGUFMetricsCollector::instance().recordGauge(name, value, labels)
#define GGUF_METRIC_HISTOGRAM(name, value, labels) RawrXD::GGUFMetricsCollector::instance().recordHistogram(name, value, labels)
#define GGUF_METRIC_DURATION(name, duration, labels) RawrXD::GGUFMetricsCollector::instance().recordDuration(name, duration, labels)

} // namespace RawrXD

#endif // GGUF_METRICS_H