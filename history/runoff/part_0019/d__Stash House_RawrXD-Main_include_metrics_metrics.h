#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class Metrics {
public:
    virtual ~Metrics() = default;

    virtual void incrementCounter(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters[name] += static_cast<uint64_t>(value);
    }

    virtual void decrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] -= value;
    }

    virtual void incrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] += value;
    }

    virtual void recordHistogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_histograms[name].push_back(value);
    }

    void increment(const std::string& name, double value = 1.0) {
        incrementCounter(name, value);
    }

    virtual nlohmann::json snapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json out = nlohmann::json::object();

        nlohmann::json counters = nlohmann::json::object();
        for (const auto& entry : m_counters) {
            counters[entry.first] = entry.second;
        }

        nlohmann::json gauges = nlohmann::json::object();
        for (const auto& entry : m_gauges) {
            gauges[entry.first] = entry.second;
        }

        nlohmann::json histograms = nlohmann::json::object();
        for (const auto& entry : m_histograms) {
            histograms[entry.first] = entry.second;
        }

        out["counters"] = counters;
        out["gauges"] = gauges;
        out["histograms"] = histograms;
        return out;
    }

protected:
    mutable std::mutex m_mutex;
    std::map<std::string, uint64_t> m_counters;
    std::map<std::string, std::vector<double>> m_histograms;
    std::map<std::string, double> m_gauges;
};

#ifndef METRICS
class ScopedMetricTimer {
public:
    explicit ScopedMetricTimer(const std::string& name)
        : m_name(name), m_start(std::chrono::steady_clock::now()) {}

    ~ScopedMetricTimer() {
        auto end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - m_start).count();
        METRICS.recordHistogram(m_name, elapsed_ms);
    }

private:
    std::string m_name;
    std::chrono::steady_clock::time_point m_start;
};

inline Metrics METRICS;

#ifndef SCOPED_METRIC
#define SCOPED_METRIC(name) ScopedMetricTimer scoped_metric_timer_##__LINE__(name)
#endif
#endif
