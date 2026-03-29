#pragma once
#include <string>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <atomic>

class Metrics {
public:
    virtual ~Metrics() = default;
    
    virtual void recordHistogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& h = m_histograms[name];
        h.count++;
        h.sum += value;
        if (value < h.min) h.min = value;
        if (value > h.max) h.max = value;
    }
    
    virtual void incrementCounter(const std::string& name, int count = 1) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters[name] += count;
    }
    
    virtual void recordGauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] = value;
    }

    int64_t getCounter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_counters.find(name);
        return (it != m_counters.end()) ? it->second : 0;
    }

    double getGauge(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gauges.find(name);
        return (it != m_gauges.end()) ? it->second : 0.0;
    }

    void dump(std::ostream& out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        out << "=== Counters ===\n";
        for (const auto& [k, v] : m_counters) {
            out << "  " << k << ": " << v << "\n";
        }
        out << "=== Gauges ===\n";
        for (const auto& [k, v] : m_gauges) {
            out << "  " << k << ": " << v << "\n";
        }
        out << "=== Histograms ===\n";
        for (const auto& [k, v] : m_histograms) {
            double avg = (v.count > 0) ? (v.sum / v.count) : 0.0;
            out << "  " << k << ": count=" << v.count
                << " avg=" << avg << " min=" << v.min << " max=" << v.max << "\n";
        }
    }

private:
    struct HistogramData {
        int64_t count = 0;
        double sum = 0.0;
        double min = 1e300;
        double max = -1e300;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, int64_t> m_counters;
    std::unordered_map<std::string, double> m_gauges;
    std::unordered_map<std::string, HistogramData> m_histograms;
};
