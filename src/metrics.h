#pragma once
#include <string>
#include <iostream>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

struct MetricsSnapshot {
    std::unordered_map<std::string, std::vector<double>> histograms;
    std::unordered_map<std::string, int64_t> counters;
    std::unordered_map<std::string, double> gauges;
    uint64_t totalHistogramSamples = 0;
    uint64_t totalCounterIncrements = 0;
    uint64_t gaugeUpdates = 0;
};

class Metrics {
public:
    virtual ~Metrics() = default;

    virtual void recordHistogram(const std::string& name, double value) {
        if (name.empty())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto& samples = m_histograms[name];
        samples.push_back(value);
        if (samples.size() > kMaxHistogramSamples)
        {
            samples.erase(samples.begin(), samples.begin() + (samples.size() - kMaxHistogramSamples));
        }
        ++m_totalHistogramSamples;
    }

    virtual void incrementCounter(const std::string& name, int count = 1) {
        if (name.empty() || count == 0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters[name] += static_cast<int64_t>(count);
        m_totalCounterIncrements.fetch_add(1, std::memory_order_relaxed);
    }

    virtual void recordGauge(const std::string& name, double value) {
        if (name.empty())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] = value;
        m_gaugeUpdates.fetch_add(1, std::memory_order_relaxed);
    }

    int64_t getCounterValue(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_counters.find(name);
        if (it == m_counters.end())
        {
            return 0;
        }
        return it->second;
    }

    double getGaugeValue(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gauges.find(name);
        if (it == m_gauges.end())
        {
            return 0.0;
        }
        return it->second;
    }

    MetricsSnapshot snapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        MetricsSnapshot s;
        s.histograms = m_histograms;
        s.counters = m_counters;
        s.gauges = m_gauges;
        s.totalHistogramSamples = m_totalHistogramSamples;
        s.totalCounterIncrements = m_totalCounterIncrements.load(std::memory_order_relaxed);
        s.gaugeUpdates = m_gaugeUpdates.load(std::memory_order_relaxed);
        return s;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_histograms.clear();
        m_counters.clear();
        m_gauges.clear();
        m_totalHistogramSamples = 0;
        m_totalCounterIncrements.store(0, std::memory_order_relaxed);
        m_gaugeUpdates.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr size_t kMaxHistogramSamples = 256;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<double>> m_histograms;
    std::unordered_map<std::string, int64_t> m_counters;
    std::unordered_map<std::string, double> m_gauges;
    uint64_t m_totalHistogramSamples = 0;
    std::atomic<uint64_t> m_totalCounterIncrements{0};
    std::atomic<uint64_t> m_gaugeUpdates{0};
};
