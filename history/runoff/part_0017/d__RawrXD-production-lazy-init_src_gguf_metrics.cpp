#include "gguf_metrics.h"
#include "logging/structured_logger.h"
#include "error_handler.h"
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpServer>
#include <QJsonDocument>
#include <math>

namespace RawrXD {

GGUFMetricsCollector& GGUFMetricsCollector::instance() {
    static GGUFMetricsCollector instance;
    return instance;
}

void GGUFMetricsCollector::initialize(int port, const QString& path) {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    port_ = port;
    path_ = path;
    
    // Start aggregation timer
    QObject::connect(&aggregationTimer_, &QTimer::timeout, this, [this]() {
        aggregateMetrics();
    });
    aggregationTimer_.start(DEFAULT_AGGREGATION_INTERVAL);
    
    // Start cleanup timer
    QObject::connect(&cleanupTimer_, &QTimer::timeout, this, [this]() {
        clearOldMetrics();
    });
    cleanupTimer_.start(DEFAULT_CLEANUP_INTERVAL);
    
    // Start metrics server
    startMetricsServer();
    
    initialized_ = true;
    
    LOG_INFO("GGUF metrics collector initialized", {
        {"port", port_},
        {"path", path_}
    });
}

void GGUFMetricsCollector::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        aggregationTimer_.stop();
        cleanupTimer_.stop();
        stopMetricsServer();
        metrics_.clear();
        initialized_ = false;
        
        LOG_INFO("GGUF metrics collector shut down");
    }
}

void GGUFMetricsCollector::recordCounter(const QString& name, double value, const QJsonObject& labels) {
    QMutexLocker lock(&mutex_);
    
    GGUFMetric metric(name, "counter", value, labels);
    metrics_[name].append(metric);
    
    LOG_DEBUG("Counter metric recorded", {{"name", name}, {"value", value}});
}

void GGUFMetricsCollector::recordGauge(const QString& name, double value, const QJsonObject& labels) {
    QMutexLocker lock(&mutex_);
    
    GGUFMetric metric(name, "gauge", value, labels);
    metrics_[name].append(metric);
    
    LOG_DEBUG("Gauge metric recorded", {{"name", name}, {"value", value}});
}

void GGUFMetricsCollector::recordHistogram(const QString& name, double value, const QJsonObject& labels) {
    QMutexLocker lock(&mutex_);
    
    GGUFMetric metric(name, "histogram", value, labels);
    metrics_[name].append(metric);
    
    LOG_DEBUG("Histogram metric recorded", {{"name", name}, {"value", value}});
}

void GGUFMetricsCollector::recordDuration(const QString& name, qint64 durationMs, const QJsonObject& labels) {
    QJsonObject enhancedLabels = labels;
    enhancedLabels["unit"] = "ms";
    
    recordHistogram(name + "_duration", durationMs, enhancedLabels);
}

QVector<GGUFMetric> GGUFMetricsCollector::getMetrics(const QString& name) const {
    QMutexLocker lock(&mutex_);
    
    if (name.isEmpty()) {
        QVector<GGUFMetric> allMetrics;
        for (const auto& vec : metrics_) {
            allMetrics.append(vec);
        }
        return allMetrics;
    }
    
    return metrics_.value(name, QVector<GGUFMetric>());
}

QJsonObject GGUFMetricsCollector::getMetricsAsJson() const {
    QMutexLocker lock(&mutex_);
    
    QJsonObject result;
    
    for (auto it = metrics_.begin(); it != metrics_.end(); ++it) {
        QJsonArray metricArray;
        
        for (const GGUFMetric& metric : it.value()) {
            QJsonObject metricObj;
            metricObj["name"] = metric.name;
            metricObj["type"] = metric.type;
            metricObj["value"] = metric.value;
            metricObj["timestamp"] = metric.timestamp.toString(Qt::ISODate);
            metricObj["labels"] = metric.labels;
            
            metricArray.append(metricObj);
        }
        
        result[it.key()] = metricArray;
    }
    
    return result;
}

QString GGUFMetricsCollector::getMetricsAsPrometheus() const {
    QMutexLocker lock(&mutex_);
    
    QString result;
    
    for (auto it = metrics_.begin(); it != metrics_.end(); ++it) {
        for (const GGUFMetric& metric : it.value()) {
            result += formatPrometheusMetric(metric) + "\n";
        }
    }
    
    return result;
}

void GGUFMetricsCollector::aggregateMetrics() {
    QMutexLocker lock(&mutex_);
    
    QHash<QString, QVector<GGUFMetric>> aggregated;
    
    for (auto it = metrics_.begin(); it != metrics_.end(); ++it) {
        const QString& name = it.key();
        const QVector<GGUFMetric>& metrics = it.value();
        
        if (metrics.isEmpty()) continue;
        
        // Aggregate based on metric type
        QString type = metrics.first().type;
        
        if (type == "counter") {
            // Sum all counter values
            double sum = 0.0;
            for (const GGUFMetric& metric : metrics) {
                sum += metric.value;
            }
            
            GGUFMetric aggregatedMetric(name, type, sum, metrics.last().labels);
            aggregated[name].append(aggregatedMetric);
            
        } else if (type == "gauge") {
            // Keep the latest gauge value
            GGUFMetric latest = metrics.last();
            aggregated[name].append(latest);
            
        } else if (type == "histogram") {
            // Calculate histogram statistics
            QVector<double> values;
            for (const GGUFMetric& metric : metrics) {
                values.append(metric.value);
            }
            
            std::sort(values.begin(), values.end());
            
            double sum = 0.0;
            for (double val : values) {
                sum += val;
            }
            
            double avg = values.isEmpty() ? 0.0 : sum / values.size();
            double min = values.isEmpty() ? 0.0 : values.first();
            double max = values.isEmpty() ? 0.0 : values.last();
            double p50 = values.isEmpty() ? 0.0 : values[values.size() * 0.5];
            double p95 = values.isEmpty() ? 0.0 : values[values.size() * 0.95];
            double p99 = values.isEmpty() ? 0.0 : values[values.size() * 0.99];
            
            // Store aggregated histogram
            QJsonObject labels = metrics.last().labels;
            labels["statistic"] = "summary";
            
            aggregated[name + "_count"].append(GGUFMetric(name + "_count", "gauge", values.size(), labels));
            aggregated[name + "_sum"].append(GGUFMetric(name + "_sum", "gauge", sum, labels));
            aggregated[name + "_avg"].append(GGUFMetric(name + "_avg", "gauge", avg, labels));
            aggregated[name + "_min"].append(GGUFMetric(name + "_min", "gauge", min, labels));
            aggregated[name + "_max"].append(GGUFMetric(name + "_max", "gauge", max, labels));
            aggregated[name + "_p50"].append(GGUFMetric(name + "_p50", "gauge", p50, labels));
            aggregated[name + "_p95"].append(GGUFMetric(name + "_p95", "gauge", p95, labels));
            aggregated[name + "_p99"].append(GGUFMetric(name + "_p99", "gauge", p99, labels));
        }
    }
    
    metrics_ = aggregated;
    
    LOG_DEBUG("Metrics aggregated", {{"metric_count", metrics_.size()}});
}

void GGUFMetricsCollector::clearOldMetrics(qint64 maxAgeSeconds) {
    QMutexLocker lock(&mutex_);
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-maxAgeSeconds);
    int removedCount = 0;
    
    for (auto it = metrics_.begin(); it != metrics_.end(); ++it) {
        QVector<GGUFMetric>& metrics = it.value();
        
        for (int i = metrics.size() - 1; i >= 0; --i) {
            if (metrics[i].timestamp < cutoff) {
                metrics.remove(i);
                removedCount++;
            }
        }
        
        if (metrics.isEmpty()) {
            metrics_.remove(it.key());
        }
    }
    
    if (removedCount > 0) {
        LOG_DEBUG("Old metrics cleared", {{"removed_count", removedCount}});
    }
}

void GGUFMetricsCollector::startMetricsServer() {
    // Note: QHttpServer is not available in standard Qt, so we'll simulate
    // In production, you would use a proper HTTP server library
    
    LOG_INFO("Metrics server would start on port", {{"port", port_}, {"path", path_}});
    
    // For now, we'll just log that the server would start
    // In a real implementation, you would:
    // 1. Create an HTTP server
    // 2. Set up the metrics endpoint
    // 3. Handle requests and return Prometheus format
}

void GGUFMetricsCollector::stopMetricsServer() {
    LOG_INFO("Metrics server would stop");
}

QString GGUFMetricsCollector::formatPrometheusMetric(const GGUFMetric& metric) const {
    QString result = metric.name;
    
    // Add labels
    if (!metric.labels.isEmpty()) {
        result += "{";
        bool first = true;
        for (auto it = metric.labels.begin(); it != metric.labels.end(); ++it) {
            if (!first) {
                result += ",";
            }
            result += it.key() + "=\"" + it.value().toString() + "\"";
            first = false;
        }
        result += "}";
    }
    
    result += " " + QString::number(metric.value);
    
    // Add timestamp in milliseconds
    qint64 timestamp = metric.timestamp.toMSecsSinceEpoch();
    result += " " + QString::number(timestamp);
    
    return result;
}

GGUFMetricsCollector::~GGUFMetricsCollector() {
    shutdown();
}

} // namespace RawrXD