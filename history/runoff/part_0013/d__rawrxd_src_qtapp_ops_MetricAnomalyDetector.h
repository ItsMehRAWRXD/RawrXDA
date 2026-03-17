#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>

class MetricAnomalyDetector {

public:
    struct DataPoint {
        double value{0.0};
        int64_t timestamp{0};
    };

    struct Stats {
        double mean{0.0};
        double stddev{0.0};
    };

    MetricAnomalyDetector() = default;
    ~MetricAnomalyDetector() = default;

    void addSample(const std::string& metric, double value, int64_t timestampMs);
    bool isAnomalous(const std::string& metric, double zThreshold = 3.0) const;
    double mean(const std::string& metric) const;
    double stddev(const std::string& metric) const;

    void anomalyDetected(const std::string& metric, double value, double zScore);

private:
    void prune(const std::string& metric, int64_t nowMs, int64_t windowMs = 5 * 60 * 1000) const;
    Stats computeStats(const std::deque<DataPoint>& series) const;

    mutable std::map<std::string, std::deque<DataPoint>> m_series;
    mutable std::mutex m_mutex;
};


