#include "metrics.h"
#include <QTextStream>
#include <QDebug>

Metrics::Metrics(QObject *parent)
    : QObject(parent)
{
}

Metrics::~Metrics()
{
    // Clean up counters and gauges
    qDeleteAll(m_counters);
    qDeleteAll(m_gauges);
}

Metrics::Counter& Metrics::counter(const QString &name, const QMap<QString, QString> &labels)
{
    QMutexLocker locker(&m_mutex);
    QString key = metricKey(name, labels);
    if (!m_counters.contains(key)) {
        m_counters[key] = new Counter(this, name, labels);
    }
    return *m_counters[key];
}

Metrics::Gauge& Metrics::gauge(const QString &name, const QMap<QString, QString> &labels)
{
    QMutexLocker locker(&m_mutex);
    QString key = metricKey(name, labels);
    if (!m_gauges.contains(key)) {
        m_gauges[key] = new Gauge(this, name, labels);
    }
    return *m_gauges[key];
}

QString Metrics::generateMetricsText()
{
    QMutexLocker locker(&m_mutex);
    QString result;
    QTextStream stream(&result);

    // Generate counters
    for (auto it = m_counters.constBegin(); it != m_counters.constEnd(); ++it) {
        const Counter *counter = it.value();
        stream << "# TYPE " << counter->m_name << " counter\n";
        stream << counter->m_name;
        if (!counter->m_labels.isEmpty()) {
            stream << "{";
            bool first = true;
            for (auto labelIt = counter->m_labels.constBegin(); labelIt != counter->m_labels.constEnd(); ++labelIt) {
                if (!first) stream << ",";
                stream << labelIt.key() << "=\"" << labelIt.value() << "\"";
                first = false;
            }
            stream << "}";
        }
        stream << " " << counter->value() << "\n";
    }

    // Generate gauges
    for (auto it = m_gauges.constBegin(); it != m_gauges.constEnd(); ++it) {
        const Gauge *gauge = it.value();
        stream << "# TYPE " << gauge->m_name << " gauge\n";
        stream << gauge->m_name;
        if (!gauge->m_labels.isEmpty()) {
            stream << "{";
            bool first = true;
            for (auto labelIt = gauge->m_labels.constBegin(); labelIt != gauge->m_labels.constEnd(); ++labelIt) {
                if (!first) stream << ",";
                stream << labelIt.key() << "=\"" << labelIt.value() << "\"";
                first = false;
            }
            stream << "}";
        }
        stream << " " << gauge->value() << "\n";
    }

    return result;
}

QString Metrics::metricKey(const QString &name, const QMap<QString, QString> &labels)
{
    QString key = name;
    if (!labels.isEmpty()) {
        key += "{";
        QStringList labelPairs;
        for (auto it = labels.constBegin(); it != labels.constEnd(); ++it) {
            labelPairs << (it.key() + "=" + it.value());
        }
        key += labelPairs.join(",");
        key += "}";
    }
    return key;
}

// Counter implementation
Metrics::Counter::Counter(Metrics *metrics, const QString &name, const QMap<QString, QString> &labels)
    : m_metrics(metrics)
    , m_name(name)
    , m_labels(labels)
{
}

void Metrics::Counter::increment(qint64 value)
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would update the counter value
    // For this example, we'll just print a message
    qDebug() << "Incrementing counter" << m_name << "by" << value;
}

void Metrics::Counter::decrement(qint64 value)
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would update the counter value
    // For this example, we'll just print a message
    qDebug() << "Decrementing counter" << m_name << "by" << value;
}

qint64 Metrics::Counter::value() const
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would return the current counter value
    // For this example, we'll just return a dummy value
    return 0;
}

// Gauge implementation
Metrics::Gauge::Gauge(Metrics *metrics, const QString &name, const QMap<QString, QString> &labels)
    : m_metrics(metrics)
    , m_name(name)
    , m_labels(labels)
{
}

void Metrics::Gauge::set(qreal value)
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would update the gauge value
    // For this example, we'll just print a message
    qDebug() << "Setting gauge" << m_name << "to" << value;
}

void Metrics::Gauge::increment(qreal value)
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would update the gauge value
    // For this example, we'll just print a message
    qDebug() << "Incrementing gauge" << m_name << "by" << value;
}

void Metrics::Gauge::decrement(qreal value)
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would update the gauge value
    // For this example, we'll just print a message
    qDebug() << "Decrementing gauge" << m_name << "by" << value;
}

qreal Metrics::Gauge::value() const
{
    QMutexLocker locker(&m_metrics->m_mutex);
    Q_UNUSED(locker)
    // In a real implementation, this would return the current gauge value
    // For this example, we'll just return a dummy value
    return 0.0;
}