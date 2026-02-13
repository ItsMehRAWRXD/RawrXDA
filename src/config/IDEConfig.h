// ============================================================================
// IDEConfig.h - External Configuration Management & Feature Toggles
// Enterprise-grade configuration system for RawrXD IDE.
// Loads settings from rawrxd.config.json, environment variables, and defaults.
// ============================================================================
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <chrono>

// ============================================================================
// Feature Toggle System
// ============================================================================
class FeatureToggle {
public:
    static FeatureToggle& getInstance() {
        static FeatureToggle instance;
        return instance;
    }

    bool isEnabled(const std::string& feature) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_features.find(feature);
        return it != m_features.end() && it->second;
    }

    void setEnabled(const std::string& feature, bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_features[feature] = enabled;
    }

    std::vector<std::pair<std::string, bool>> getAllFeatures() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::pair<std::string, bool>> result;
        for (const auto& [key, val] : m_features) {
            result.push_back({key, val});
        }
        return result;
    }

private:
    FeatureToggle() = default;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, bool> m_features;
};

// ============================================================================
// Performance Metrics Collector
// ============================================================================
class MetricsCollector {
public:
    static MetricsCollector& getInstance() {
        static MetricsCollector instance;
        return instance;
    }

    // Counter: increment a named metric
    void increment(const std::string& name, int64_t delta = 1) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters[name] += delta;
    }

    // Gauge: set a metric to a specific value
    void gauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gauges[name] = value;
    }

    // Histogram: record a duration or value for distribution tracking
    void recordDuration(const std::string& name, double milliseconds) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& hist = m_histograms[name];
        hist.count++;
        hist.sum += milliseconds;
        if (milliseconds < hist.min) hist.min = milliseconds;
        if (milliseconds > hist.max) hist.max = milliseconds;
    }

    int64_t getCounter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_counters.find(name);
        return it != m_counters.end() ? it->second : 0;
    }

    double getGauge(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gauges.find(name);
        return it != m_gauges.end() ? it->second : 0.0;
    }

    struct HistogramStats {
        int64_t count = 0;
        double sum = 0.0;
        double min = 1e18;
        double max = 0.0;
        double avg() const { return count > 0 ? sum / count : 0.0; }
    };

    HistogramStats getHistogram(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_histograms.find(name);
        return it != m_histograms.end() ? it->second : HistogramStats{};
    }

    // Prometheus-compatible text export
    std::string exportPrometheus() const;

    // Reset all metrics
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters.clear();
        m_gauges.clear();
        m_histograms.clear();
    }

private:
    MetricsCollector() = default;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, int64_t> m_counters;
    std::unordered_map<std::string, double> m_gauges;
    std::unordered_map<std::string, HistogramStats> m_histograms;
};

// ============================================================================
// Scoped Timer — automatically records duration to MetricsCollector
// ============================================================================
class ScopedTimer {
public:
    ScopedTimer(const std::string& metricName)
        : m_name(metricName)
        , m_start(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
        MetricsCollector::getInstance().recordDuration(m_name, ms);
    }

private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

// ============================================================================
// IDE Configuration — loaded from rawrxd.config.json
// ============================================================================
class IDEConfig {
public:
    static IDEConfig& getInstance() {
        static IDEConfig instance;
        return instance;
    }

    // Load configuration from file (and environment overrides)
    bool loadFromFile(const std::string& configPath);

    // Save current configuration to file
    bool saveToFile(const std::string& configPath) const;

    // String getters/setters with defaults
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;

    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);

    // Load feature toggles from config into FeatureToggle singleton
    void applyFeatureToggles() const;

    // Apply environment variable overrides (RAWRXD_* prefix)
    void applyEnvironmentOverrides();

    // Get all config keys
    std::vector<std::string> getAllKeys() const;

private:
    IDEConfig() { setDefaults(); }
    void setDefaults();

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_values;
};

// ============================================================================
// Convenience macros
// ============================================================================
#define METRICS MetricsCollector::getInstance()
#define FEATURE_ENABLED(name) FeatureToggle::getInstance().isEnabled(name)
#define SCOPED_METRIC(name) ScopedTimer _timer_##__LINE__(name)
#define CONFIG IDEConfig::getInstance()
