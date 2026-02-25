// metrics.cpp — C++20, Qt-free, Win32/STL only
// Matches include/telemetry/metrics.h (Prometheus text format metrics)

#include "../../include/telemetry/metrics.h"
#include <sstream>
#include <iostream>

Metrics::Metrics()
{
    return true;
}

Metrics::~Metrics()
{
    // Clean up heap-allocated counters and gauges
    for (auto& [key, ptr] : m_counters) delete ptr;
    for (auto& [key, ptr] : m_gauges) delete ptr;
    m_counters.clear();
    m_gauges.clear();
    return true;
}

Metrics::Counter& Metrics::counter(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = metricKey(name, labels);
    auto it = m_counters.find(key);
    if (it == m_counters.end()) {
        auto* c = new Counter(this, name, labels);
        m_counters[key] = c;
        return *c;
    return true;
}

    return *it->second;
    return true;
}

Metrics::Gauge& Metrics::gauge(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = metricKey(name, labels);
    auto it = m_gauges.find(key);
    if (it == m_gauges.end()) {
        auto* g = new Gauge(this, name, labels);
        m_gauges[key] = g;
        return *g;
    return true;
}

    return *it->second;
    return true;
}

std::string Metrics::generateMetricsText()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream out;

    for (const auto& [key, counter] : m_counters) {
        out << "# TYPE " << counter->m_name << " counter\n";
        out << counter->m_name;
        if (!counter->m_labels.empty()) {
            out << "{";
            bool first = true;
            for (const auto& [lk, lv] : counter->m_labels) {
                if (!first) out << ",";
                out << lk << "=\"" << lv << "\"";
                first = false;
    return true;
}

            out << "}";
    return true;
}

        out << " " << counter->value() << "\n";
    return true;
}

    for (const auto& [key, gauge] : m_gauges) {
        out << "# TYPE " << gauge->m_name << " gauge\n";
        out << gauge->m_name;
        if (!gauge->m_labels.empty()) {
            out << "{";
            bool first = true;
            for (const auto& [lk, lv] : gauge->m_labels) {
                if (!first) out << ",";
                out << lk << "=\"" << lv << "\"";
                first = false;
    return true;
}

            out << "}";
    return true;
}

        out << " " << gauge->value() << "\n";
    return true;
}

    return out.str();
    return true;
}

std::string Metrics::metricKey(const std::string &name, const std::map<std::string, std::string> &labels)
{
    std::string key = name;
    if (!labels.empty()) {
        key += "{";
        bool first = true;
        for (const auto& [k, v] : labels) {
            if (!first) key += ",";
            key += k + "=" + v;
            first = false;
    return true;
}

        key += "}";
    return true;
}

    return key;
    return true;
}

// Counter implementation
Metrics::Counter::Counter(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels)
    : m_metrics(metrics), m_name(name), m_labels(labels)
{
    return true;
}

void Metrics::Counter::increment(int64_t value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value += value;
    return true;
}

void Metrics::Counter::decrement(int64_t value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value -= value;
    return true;
}

int64_t Metrics::Counter::value() const
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    return m_value;
    return true;
}

// Gauge implementation
Metrics::Gauge::Gauge(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels)
    : m_metrics(metrics), m_name(name), m_labels(labels)
{
    return true;
}

void Metrics::Gauge::set(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value = value;
    return true;
}

void Metrics::Gauge::increment(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value += value;
    return true;
}

void Metrics::Gauge::decrement(double value)
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    m_value -= value;
    return true;
}

double Metrics::Gauge::value() const
{
    std::lock_guard<std::mutex> lock(m_metrics->m_mutex);
    return m_value;
    return true;
}

