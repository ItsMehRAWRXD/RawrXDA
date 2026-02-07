#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <cmath>
#include <algorithm>

struct HistogramBucket {
    double boundary;
    int64_t count;
};

class Metrics {
private:
    std::unordered_map<std::string, std::atomic<int64_t>> m_counters;
    std::unordered_map<std::string, std::vector<double>> m_histograms;
    std::unordered_map<std::string, std::atomic<double>> m_gauges;
    mutable std::mutex m_mutex;

public:
    Metrics() = default;

    // Counter operations
    void incrementCounter(const std::string& name, int64_t value = 1) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_counters.find(name) == m_counters.end()) {
            m_counters[name] = 0;
        }
        m_counters[name] += value;
    }

    int64_t getCounter(const std::string& name) const {
        auto it = m_counters.find(name);
        if (it == m_counters.end()) return 0;
        return it->second.load();
    }

    // Histogram operations
    void recordHistogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_histograms.find(name) == m_histograms.end()) {
            m_histograms[name] = std::vector<double>();
        }
        m_histograms[name].push_back(value);
    }

    double getHistogramAverage(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_histograms.find(name);
        if (it == m_histograms.end() || it->second.empty()) return 0.0;

        double sum = 0.0;
        for (double val : it->second) {
            sum += val;
        }
        return sum / it->second.size();
    }

    double getHistogramPercentile(const std::string& name, double percentile) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_histograms.find(name);
        if (it == m_histograms.end() || it->second.empty()) return 0.0;

        auto values = it->second;
        std::sort(values.begin(), values.end());

        int index = static_cast<int>(values.size() * percentile / 100.0);
        return values[std::min(index, static_cast<int>(values.size() - 1))];
    }

    int64_t getHistogramCount(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_histograms.find(name);
        if (it == m_histograms.end()) return 0;
        return it->second.size();
    }

    // Gauge operations
    void setGauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] = value;
    }

    void incrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gauges.find(name) == m_gauges.end()) {
            m_gauges[name] = 0.0;
        }
        m_gauges[name] = m_gauges[name].load() + value;
    }

    void decrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gauges.find(name) == m_gauges.end()) {
            m_gauges[name] = 0.0;
        }
        m_gauges[name] = m_gauges[name].load() - value;
    }

    double getGauge(const std::string& name) const {
        auto it = m_gauges.find(name);
        if (it == m_gauges.end()) return 0.0;
        return it->second.load();
    }

    // Reset
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.clear();
        m_histograms.clear();
        m_gauges.clear();
    }

    void resetMetric(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.erase(name);
        m_histograms.erase(name);
        m_gauges.erase(name);
    }
};

