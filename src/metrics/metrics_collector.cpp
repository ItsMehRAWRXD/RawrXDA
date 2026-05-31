// ============================================================================
// Metrics Collector — Comprehensive Metrics and Analytics
// Collects, aggregates, and analyzes system metrics
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::Metrics {

enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    SUMMARY
};

struct MetricValue {
    std::string name;
    double value;
    MetricType type;
    std::map<std::string, std::string> labels;
    std::chrono::system_clock::time_point timestamp;
};

struct TimeSeries {
    std::string metricName;
    std::vector<MetricValue> values;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

struct AggregationResult {
    std::string metricName;
    double min;
    double max;
    double avg;
    double sum;
    double count;
    double p50;
    double p95;
    double p99;
    std::chrono::system_clock::time_point calculatedAt;
};

struct AlertRule {
    std::string id;
    std::string metricName;
    std::string condition;
    double threshold;
    std::string severity;
    bool isActive;
    std::chrono::system_clock::time_point createdAt;
};

class MetricsCollector {
public:
    explicit MetricsCollector(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    void RecordMetric(const MetricValue& metric) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_metrics[metric.name].push_back(metric);
        
        // Trim old data (keep last 24 hours)
        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto& values = m_metrics[metric.name];
        values.erase(std::remove_if(values.begin(), values.end(),
            [cutoff](const MetricValue& v) { return v.timestamp < cutoff; }),
            values.end());
        
        // Check alerts
        CheckAlerts(metric);
    }

    void IncrementCounter(const std::string& name, 
                         const std::map<std::string, std::string>& labels = {}) {
        MetricValue metric;
        metric.name = name;
        metric.value = 1;
        metric.type = MetricType::COUNTER;
        metric.labels = labels;
        metric.timestamp = std::chrono::system_clock::now();
        
        RecordMetric(metric);
    }

    void SetGauge(const std::string& name, double value,
                 const std::map<std::string, std::string>& labels = {}) {
        MetricValue metric;
        metric.name = name;
        metric.value = value;
        metric.type = MetricType::GAAUGE;
        metric.labels = labels;
        metric.timestamp = std::chrono::system_clock::now();
        
        RecordMetric(metric);
    }

    void RecordHistogram(const std::string& name, double value,
                        const std::map<std::string, std::string>& labels = {}) {
        MetricValue metric;
        metric.name = name;
        metric.value = value;
        metric.type = MetricType::HISTOGRAM;
        metric.labels = labels;
        metric.timestamp = std::chrono::system_clock::now();
        
        RecordMetric(metric);
    }

    TimeSeries GetTimeSeries(const std::string& metricName,
                            std::chrono::system_clock::time_point start,
                            std::chrono::system_clock::time_point end) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        TimeSeries series;
        series.metricName = metricName;
        series.startTime = start;
        series.endTime = end;
        
        auto it = m_metrics.find(metricName);
        if (it != m_metrics.end()) {
            for (const auto& value : it->second) {
                if (value.timestamp >= start && value.timestamp <= end) {
                    series.values.push_back(value);
                }
            }
        }
        
        return series;
    }

    AggregationResult Aggregate(const std::string& metricName,
                                std::chrono::system_clock::time_point start,
                                std::chrono::system_clock::time_point end) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        AggregationResult result;
        result.metricName = metricName;
        result.calculatedAt = std::chrono::system_clock::now();
        
        auto series = GetTimeSeries(metricName, start, end);
        
        if (series.values.empty()) {
            return result;
        }
        
        // Calculate statistics
        std::vector<double> values;
        for (const auto& v : series.values) {
            values.push_back(v.value);
            result.sum += v.value;
        }
        
        result.count = values.size();
        result.avg = result.sum / result.count;
        result.min = *std::min_element(values.begin(), values.end());
        result.max = *std::max_element(values.begin(), values.end());
        
        // Calculate percentiles
        std::sort(values.begin(), values.end());
        result.p50 = CalculatePercentile(values, 0.5);
        result.p95 = CalculatePercentile(values, 0.95);
        result.p99 = CalculatePercentile(values, 0.99);
        
        return result;
    }

    void AddAlertRule(const AlertRule& rule) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_alertRules[rule.id] = rule;
    }

    void RemoveAlertRule(const std::string& ruleId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_alertRules.erase(ruleId);
    }

    std::vector<AlertRule> GetActiveAlerts() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<AlertRule> active;
        for (const auto& [id, rule] : m_alertRules) {
            if (rule.isActive) {
                active.push_back(rule);
            }
        }
        return active;
    }

    std::string GenerateMetricsReport(std::chrono::system_clock::time_point start,
                                     std::chrono::system_clock::time_point end) {
        std::ostringstream report;
        report << "# Metrics Report\n\n";
        report << "**Period:** " << FormatTime(start) << " to " << FormatTime(end) << "\n\n";
        
        report << "## Metric Summaries\n";
        for (const auto& [name, values] : m_metrics) {
            if (values.empty()) continue;
            
            auto result = Aggregate(name, start, end);
            report << "### " << name << "\n";
            report << "- **Count:** " << result.count << "\n";
            report << "- **Min:** " << result.min << "\n";
            report << "- **Max:** " << result.max << "\n";
            report << "- **Avg:** " << result.avg << "\n";
            report << "- **P95:** " << result.p95 << "\n";
            report << "- **P99:** " << result.p99 << "\n\n";
        }
        
        auto activeAlerts = GetActiveAlerts();
        if (!activeAlerts.empty()) {
            report << "## Active Alerts\n";
            for (const auto& alert : activeAlerts) {
                report << "- **" << alert.metricName << "**: " << alert.condition << " " << alert.threshold << "\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, std::vector<MetricValue>> m_metrics;
    std::map<std::string, AlertRule> m_alertRules;

    double CalculatePercentile(const std::vector<double>& sortedValues, double percentile) {
        if (sortedValues.empty()) return 0.0;
        
        size_t index = static_cast<size_t>(percentile * sortedValues.size());
        return sortedValues[std::min(index, sortedValues.size() - 1)];
    }

    void CheckAlerts(const MetricValue& metric) {
        for (const auto& [id, rule] : m_alertRules) {
            if (rule.metricName != metric.name || !rule.isActive) continue;
            
            bool triggered = false;
            if (rule.condition == ">" && metric.value > rule.threshold) {
                triggered = true;
            } else if (rule.condition == "<" && metric.value < rule.threshold) {
                triggered = true;
            } else if (rule.condition == "==" && metric.value == rule.threshold) {
                triggered = true;
            }
            
            if (triggered) {
                // Trigger alert notification
            }
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Metrics
