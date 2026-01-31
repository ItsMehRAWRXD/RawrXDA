#include "MetricAnomalyDetector.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
constexpr int64_t kDefaultWindowMs = 5 * 60 * 1000;
}

namespace {
void LogInfo(const std::string& event, const std::string& detail) {
    std::cout << "[INFO] ops.anomaly " << event << " " << detail << "\n";
}
}

void MetricAnomalyDetector::addSample(const std::string& metric, double value, int64_t timestampMs) {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto& series = m_series[metric];
    series.push_back({value, timestampMs});
    prune(metric, timestampMs, kDefaultWindowMs);

    const Stats stats = computeStats(series);
    if (stats.stddev <= 0.0) {
        return;
    }
    const double z = (value - stats.mean) / stats.stddev;
    if (std::abs(z) >= 3.0) {
        LogInfo("anomaly", metric + " value=" + std::to_string(value) + " z=" + std::to_string(z));
        anomalyDetected(metric, value, z);
    }
}

bool MetricAnomalyDetector::isAnomalous(const std::string& metric, double zThreshold) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_series.find(metric);
    if (it == m_series.end() || it->second.empty()) {
        return false;
    }
    const auto& series = it->second;
    const auto last = series.back();
    const Stats stats = computeStats(series);
    if (stats.stddev <= 0.0) {
        return false;
    }
    const double z = (last.value - stats.mean) / stats.stddev;
    return std::abs(z) >= zThreshold;
}

double MetricAnomalyDetector::mean(const std::string& metric) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_series.find(metric);
    if (it == m_series.end() || it->second.empty()) {
        return 0.0;
    }
    return computeStats(it->second).mean;
}

double MetricAnomalyDetector::stddev(const std::string& metric) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_series.find(metric);
    if (it == m_series.end() || it->second.empty()) {
        return 0.0;
    }
    return computeStats(it->second).stddev;
}

void MetricAnomalyDetector::prune(const std::string& metric, int64_t nowMs, int64_t windowMs) const {
    auto& series = m_series[metric];
    const int64_t minTime = nowMs - windowMs;
    while (!series.empty() && series.front().timestamp < minTime) {
        series.pop_front();
    }
}

MetricAnomalyDetector::Stats MetricAnomalyDetector::computeStats(const std::deque<DataPoint>& series) const {
    Stats stats;
    if (series.empty()) {
        return stats;
    }

    double sum = 0.0;
    for (const auto& dp : series) {
        sum += dp.value;
    }
    stats.mean = sum / series.size();

    double accum = 0.0;
    for (const auto& dp : series) {
        const double diff = dp.value - stats.mean;
        accum += diff * diff;
    }
    const double variance = accum / series.size();
    stats.stddev = std::sqrt(variance);
    return stats;
}

void MetricAnomalyDetector::anomalyDetected(const std::string& metric, double value, double zScore) {
    (void)metric;
    (void)value;
    (void)zScore;
}

