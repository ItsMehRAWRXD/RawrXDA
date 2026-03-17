#include "MetricAnomalyDetector.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QtMath>
#include <algorithm>

namespace {
constexpr qint64 kDefaultWindowMs = 5 * 60 * 1000;
}

Q_LOGGING_CATEGORY(opsAnomalyLog, "ops.anomaly")

MetricAnomalyDetector::MetricAnomalyDetector(QObject* parent)
    : QObject(parent) {
}

MetricAnomalyDetector::~MetricAnomalyDetector() = default;

void MetricAnomalyDetector::addSample(const QString& metric, double value, qint64 timestampMs) {
    QMutexLocker locker(&m_mutex);
    auto& series = m_series[metric];
    series.push_back({value, timestampMs});
    prune(metric, timestampMs, kDefaultWindowMs);

    const Stats stats = computeStats(series);
    if (stats.stddev <= 0.0) {
        return;
    }
    const double z = (value - stats.mean) / stats.stddev;
    if (qAbs(z) >= 3.0) {
        qInfo(opsAnomalyLog) << "anomaly" << metric << "value" << value << "z" << z;
        emit anomalyDetected(metric, value, z);
    }
}

bool MetricAnomalyDetector::isAnomalous(const QString& metric, double zThreshold) const {
    QMutexLocker locker(&m_mutex);
    if (!m_series.contains(metric) || m_series[metric].isEmpty()) {
        return false;
    }
    const auto& series = m_series[metric];
    const auto last = series.last();
    const Stats stats = computeStats(series);
    if (stats.stddev <= 0.0) {
        return false;
    }
    const double z = (last.value - stats.mean) / stats.stddev;
    return qAbs(z) >= zThreshold;
}

double MetricAnomalyDetector::mean(const QString& metric) const {
    QMutexLocker locker(&m_mutex);
    if (!m_series.contains(metric) || m_series[metric].isEmpty()) {
        return 0.0;
    }
    return computeStats(m_series[metric]).mean;
}

double MetricAnomalyDetector::stddev(const QString& metric) const {
    QMutexLocker locker(&m_mutex);
    if (!m_series.contains(metric) || m_series[metric].isEmpty()) {
        return 0.0;
    }
    return computeStats(m_series[metric]).stddev;
}

void MetricAnomalyDetector::prune(const QString& metric, qint64 nowMs, qint64 windowMs) const {
    auto& series = m_series[metric];
    const qint64 minTime = nowMs - windowMs;
    while (!series.isEmpty() && series.front().timestamp < minTime) {
        series.pop_front();
    }
}

MetricAnomalyDetector::Stats MetricAnomalyDetector::computeStats(const QVector<DataPoint>& series) const {
    Stats stats;
    if (series.isEmpty()) {
        return stats;
    }

    double sum = 0.0;
    for (const auto& dp : series) {
        sum += dp.value;
    }
    stats.mean = sum / series.size();

    double accum = 0.0;
    for (const auto& dp : series) {
        const double diff = dp.value - stats.mean;
        accum += diff * diff;
    }
    const double variance = accum / series.size();
    stats.stddev = qSqrt(variance);
    return stats;
}
