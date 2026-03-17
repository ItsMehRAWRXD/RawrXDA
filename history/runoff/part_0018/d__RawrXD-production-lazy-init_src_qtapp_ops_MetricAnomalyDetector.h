#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QMutex>

// MetricAnomalyDetector keeps rolling windows and flags anomalies via z-score thresholding.
class MetricAnomalyDetector : public QObject {
    Q_OBJECT
public:
    struct DataPoint {
        double value{0.0};
        qint64 timestamp{0};
    };

    struct Stats {
        double mean{0.0};
        double stddev{0.0};
    };

    explicit MetricAnomalyDetector(QObject* parent = nullptr);
    ~MetricAnomalyDetector();

    void addSample(const QString& metric, double value, qint64 timestampMs);
    bool isAnomalous(const QString& metric, double zThreshold = 3.0) const;
    double mean(const QString& metric) const;
    double stddev(const QString& metric) const;

signals:
    void anomalyDetected(const QString& metric, double value, double zScore);

private:
    void prune(const QString& metric, qint64 nowMs, qint64 windowMs = 5 * 60 * 1000) const;
    Stats computeStats(const QVector<DataPoint>& series) const;

    mutable QMap<QString, QVector<DataPoint>> m_series;
    mutable QMutex m_mutex;
};
