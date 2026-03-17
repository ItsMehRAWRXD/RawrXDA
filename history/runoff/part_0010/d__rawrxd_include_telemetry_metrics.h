#ifndef METRICS_H
#define METRICS_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

// Real-time metrics endpoint localhost:9090/metrics (Prometheus text format)
class Metrics
{
public:
    Metrics();
    ~Metrics();

    // Counter metric
    class Counter {
    public:
        Counter(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels = {});
        void increment(int64_t value = 1);
        void decrement(int64_t value = 1);
        int64_t value() const;

    private:
        friend class Metrics;
        Metrics *m_metrics;
        std::string m_name;
        std::map<std::string, std::string> m_labels;
        int64_t m_value = 0;
    };

    // Gauge metric
    class Gauge {
    public:
        Gauge(Metrics *metrics, const std::string &name, const std::map<std::string, std::string> &labels = {});
        void set(double value);
        void increment(double value = 1.0);
        void decrement(double value = 1.0);
        double value() const;

    private:
        friend class Metrics;
        Metrics *m_metrics;
        std::string m_name;
        std::map<std::string, std::string> m_labels;
        double m_value = 0.0;
    };

    // Get or create a counter
    Counter& counter(const std::string &name, const std::map<std::string, std::string> &labels = {});

    // Get or create a gauge
    Gauge& gauge(const std::string &name, const std::map<std::string, std::string> &labels = {});

    // Generate Prometheus text format metrics
    std::string generateMetricsText();

private:
    std::map<std::string, Counter*> m_counters;
    std::map<std::string, Gauge*> m_gauges;
    std::mutex m_mutex;

    // Generate a unique key for a metric with labels
    std::string metricKey(const std::string &name, const std::map<std::string, std::string> &labels);
};

#endif // METRICS_H