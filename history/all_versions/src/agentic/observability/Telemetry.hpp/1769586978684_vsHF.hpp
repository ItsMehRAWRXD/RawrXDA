#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <iomanip>

namespace RawrXD::Agentic::Observability {

// Log levels matching AITK instructions
enum class LogLevel {
    DEBUG,
    INFO,
    ERROR
};

// Metric types
enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    SUMMARY
};

// Metric definition
struct Metric {
    std::string name;
    MetricType type;
    std::map<std::string, std::string> labels;
    std::chrono::steady_clock::time_point timestamp;
    double value;
};

// Trace span
struct Span {
    std::string name;
    std::string traceId;
    std::string spanId;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    std::map<std::string, std::string> tags;
    bool completed;
};

// Observability configuration
struct ObservabilityConfig {
    bool enableLogging = true;
    bool enableMetrics = true;
    bool enableTracing = true;
    std::string logLevel = "INFO";
    std::string metricsEndpoint;
    std::string tracingEndpoint;
    std::string serviceName;
};

// Logger interface
class Logger {
public:
    static Logger& instance();
    
    void log(LogLevel level, const std::string& category, const std::string& message);
    void debug(const std::string& category, const std::string& message);
    void info(const std::string& category, const std::string& message);
    void error(const std::string& category, const std::string& message);
    
    void setLevel(LogLevel level) { level_ = level; }
    void setOutput(std::function<void(LogLevel, const std::string&, const std::string&)> output);
    
private:
    Logger() = default;
    
    LogLevel level_ = LogLevel::INFO;
    std::function<void(LogLevel, const std::string&, const std::string&)> output_;
};

// Metrics collector
class Metrics {
public:
    static Metrics& instance();
    
    void counter(const std::string& name, double value = 1.0, 
                 const std::map<std::string, std::string>& labels = {});
    void gauge(const std::string& name, double value,
               const std::map<std::string, std::string>& labels = {});
    void histogram(const std::string& name, double value,
                   const std::map<std::string, std::string>& labels = {});
    
    std::vector<Metric> getMetrics() const;
    void clear();
    
    void setEndpoint(const std::string& endpoint) { endpoint_ = endpoint; }
    bool exportMetrics();
    
private:
    Metrics() = default;
    
    std::vector<Metric> metrics_;
    std::string endpoint_;
    mutable std::mutex mutex_;
};

// Distributed tracing
class Tracer {
public:
    static Tracer& instance();
    
    Span startSpan(const std::string& name, const std::map<std::string, std::string>& tags = {});
    void endSpan(Span& span);
    
    void setEndpoint(const std::string& endpoint) { endpoint_ = endpoint; }
    bool exportSpan(const Span& span);
    
private:
    Tracer() = default;
    
    std::string endpoint_;
    std::atomic<int> spanCounter_{0};
    mutable std::mutex mutex_;
};

// Main observability facade
class Telemetry {
public:
    static Telemetry& instance();
    
    bool initialize(const ObservabilityConfig& config);
    void shutdown();
    
    // Convenience methods
    void logFunctionCall(const std::string& functionName);
    void logError(const std::string& functionName, const std::string& error);
    void logWarning(const std::string& functionName, const std::string& warning);
    void logInfo(const std::string& message);
    
    void metric(const std::string& name, double value = 1.0,
                const std::map<std::string, std::string>& labels = {});
    
    Span trace(const std::string& name, const std::map<std::string, std::string>& tags = {});
    
private:
    Telemetry() = default;
    
    ObservabilityConfig config_;
    bool initialized_ = false;
};

// Convenience macros
#define TELEMETRY_LOG_DEBUG(category, message) \
    RawrXD::Agentic::Observability::Telemetry::instance().logDebug(category, message)

#define TELEMETRY_LOG_INFO(category, message) \
    RawrXD::Agentic::Observability::Telemetry::instance().logInfo(category, message)

#define TELEMETRY_LOG_ERROR(category, message) \
    RawrXD::Agentic::Observability::Telemetry::instance().logError(category, message)

#define TELEMETRY_METRIC(name, value) \
    RawrXD::Agentic::Observability::Telemetry::instance().metric(name, value)

#define TELEMETRY_TRACE(name) \
    auto span = RawrXD::Agentic::Observability::Telemetry::instance().trace(name)

} // namespace RawrXD::Agentic::Observability