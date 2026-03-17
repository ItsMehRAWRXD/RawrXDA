#include "metrics.h"
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>

Metrics::Metrics()
{
}

Metrics::~Metrics()
{
    // Clean up counters and gauges
    for (auto &[key, ptr] : m_counters) {
        delete ptr;
    }
    m_counters.clear();

    for (auto &[key, ptr] : m_gauges) {
        delete ptr;
    }
    m_gauges.clear();
}

Metrics::Counter& Metrics::counter(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = metricKey(name, labels);
    auto it = m_counters.find(key);
    if (it == m_counters.end()) {
        m_counters[key] = new Counter(this, name, labels);
        return *m_counters[key];
    }
    return *it->second;
}

Metrics::Gauge& Metrics::gauge(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = metricKey(name, labels);
    auto it = m_gauges.find(key);
    if (it == m_gauges.end()) {
        m_gauges[key] = new Gauge(this, name, labels);
        return *m_gauges[key];
    }
    return *it->second;
}

std::string Metrics::generateMetricsText()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream stream;

    // Generate counters in Prometheus text format
    for (const auto &[key, counter] : m_counters) {
        stream << "# TYPE " << counter->m_name << " counter\n";
        stream << counter->m_name;
        if (!counter->m_labels.empty()) {
            stream << "{";
            bool first = true;
            for (const auto &[lk, lv] : counter->m_labels) {
                if (!first) stream << ",";
                stream << lk << "=\"" << lv << "\"";
                first = false;
            }
            stream << "}";
        }
        stream << " " << counter->m_value << "\n";
    }

    // Generate gauges in Prometheus text format
    for (const auto &[key, gauge] : m_gauges) {
        stream << "# TYPE " << gauge->m_name << " gauge\n";
        stream << gauge->m_name;
        if (!gauge->m_labels.empty()) {
            stream << "{";
            bool first = true;
            for (const auto &[lk, lv] : gauge->m_labels) {
                if (!first) stream << ",";
                stream << lk << "=\"" << lv << "\"";
                first = false;
            }
            stream << "}";
        }
        stream << " " << gauge->m_value << "\n";
    }

    return stream.str();
}

std::string Metrics::metricKey(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::string key = name;
    if (!labels.empty()) {
        key += "{";
        bool first = true;
        for (const auto &[lk, lv] : labels) {
            if (!first) key += ",";
            key += lk + "=" + lv;
            first = false;
        }
        key += "}";
    }
    return key;
}

// Counter implementation
Metrics::Counter::Counter(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels)
    : m_metrics(metrics)
    , m_name(name)
    , m_labels(labels)
    , m_value(0)
{
}

void Metrics::Counter::increment(int64_t value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value += value;
    fprintf(stderr, "[INFO] Counter %s incremented by %lld -> now %lld\n",
            m_name.c_str(), static_cast<long long>(value), static_cast<long long>(m_value));
}

void Metrics::Counter::decrement(int64_t value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value -= value;
    fprintf(stderr, "[INFO] Counter %s decremented by %lld -> now %lld\n",
            m_name.c_str(), static_cast<long long>(value), static_cast<long long>(m_value));
}

int64_t Metrics::Counter::value() const
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    return m_value;
}

// Gauge implementation
Metrics::Gauge::Gauge(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels)
    : m_metrics(metrics)
    , m_name(name)
    , m_labels(labels)
    , m_value(0.0)
{
}

void Metrics::Gauge::set(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value = value;
    fprintf(stderr, "[INFO] Gauge %s set to %f\n", m_name.c_str(), m_value);
}

void Metrics::Gauge::increment(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value += value;
    fprintf(stderr, "[INFO] Gauge %s incremented by %f -> now %f\n",
            m_name.c_str(), value, m_value);
}

void Metrics::Gauge::decrement(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value -= value;
    fprintf(stderr, "[INFO] Gauge %s decremented by %f -> now %f\n",
            m_name.c_str(), value, m_value);
}

double Metrics::Gauge::value() const
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    return m_value;
}
