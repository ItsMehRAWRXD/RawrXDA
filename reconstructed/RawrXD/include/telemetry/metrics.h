#ifndef METRICS_H
#define METRICS_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QMutex>

// Real-time metrics endpoint localhost:9090/metrics (Prometheus text format)
class Metrics : public QObject
{
    Q_OBJECT

public:
    explicit Metrics(QObject *parent = nullptr);
    ~Metrics();

    // Counter metric
    class Counter {
    public:
        Counter(Metrics *metrics, const QString &name, const QMap<QString, QString> &labels = QMap<QString, QString>());
        void increment(qint64 value = 1);
        void decrement(qint64 value = 1);
        qint64 value() const;

    private:
        Metrics *m_metrics;
        QString m_name;
        QMap<QString, QString> m_labels;
    };

    // Gauge metric
    class Gauge {
    public:
        Gauge(Metrics *metrics, const QString &name, const QMap<QString, QString> &labels = QMap<QString, QString>());
        void set(qreal value);
        void increment(qreal value = 1.0);
        void decrement(qreal value = 1.0);
        qreal value() const;

    private:
        Metrics *m_metrics;
        QString m_name;
        QMap<QString, QString> m_labels;
    };

    // Get or create a counter
    Counter& counter(const QString &name, const QMap<QString, QString> &labels = QMap<QString, QString>());

    // Get or create a gauge
    Gauge& gauge(const QString &name, const QMap<QString, QString> &labels = QMap<QString, QString>());

    // Generate Prometheus text format metrics
    QString generateMetricsText();

private:
    QMap<QString, Counter*> m_counters;
    QMap<QString, Gauge*> m_gauges;
    QMutex m_mutex;

    // Generate a unique key for a metric with labels
    QString metricKey(const QString &name, const QMap<QString, QString> &labels);
};

#endif // METRICS_H