#include "Telemetry.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <random>

namespace RawrXD::Agentic::Observability {

// Logger implementation
Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const std::string& category, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return; // Filter by level
    }
    
    if (output_) {
        output_(level, category, message);
    } else {
        // Default output to console
        std::string levelStr;
        switch (level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO: levelStr = "INFO"; break;
            case LogLevel::ERROR: levelStr = "ERROR"; break;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                  << " [" << levelStr << "] " << category << ": " << message << std::endl;
    }
}

void Logger::debug(const std::string& category, const std::string& message) {
    log(LogLevel::DEBUG, category, message);
}

void Logger::info(const std::string& category, const std::string& message) {
    log(LogLevel::INFO, category, message);
}

void Logger::error(const std::string& category, const std::string& message) {
    log(LogLevel::ERROR, category, message);
}

void Logger::setOutput(std::function<void(LogLevel, const std::string&, const std::string&)> output) {
    output_ = output;
}

// Metrics implementation
Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

void Metrics::counter(const std::string& name, double value,
                      const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Metric metric;
    metric.name = name;
    metric.type = MetricType::COUNTER;
    metric.labels = labels;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.value = value;
    
    metrics_.push_back(metric);
}

void Metrics::gauge(const std::string& name, double value,
                   const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Metric metric;
    metric.name = name;
    metric.type = MetricType::GAUGE;
    metric.labels = labels;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.value = value;
    
    metrics_.push_back(metric);
}

void Metrics::histogram(const std::string& name, double value,
                        const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Metric metric;
    metric.name = name;
    metric.type = MetricType::HISTOGRAM;
    metric.labels = labels;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.value = value;
    
    metrics_.push_back(metric);
}

std::vector<Metric> Metrics::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void Metrics::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

bool Metrics::exportMetrics() {
    if (endpoint_.empty()) {
        return false;
    }
    
    // Simple file export for now
    std::ofstream file("metrics.txt", std::ios::app);
    if (!file.is_open()) {
        return false;
    }
    
    auto metrics = getMetrics();
    for (const auto& metric : metrics) {
        file << metric.name << " " << metric.value << "\n";
    }
    
    clear();
    return true;
}

// Tracer implementation
Tracer& Tracer::instance() {
    static Tracer instance;
    return instance;
}

Span Tracer::startSpan(const std::string& name, const std::map<std::string, std::string>& tags) {
    Span span;
    span.name = name;
    span.startTime = std::chrono::steady_clock::now();
    span.tags = tags;
    span.completed = false;
    
    // Generate trace and span IDs
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream traceId, spanId;
    for (int i = 0; i < 16; ++i) {
        traceId << std::hex << dis(gen);
        spanId << std::hex << dis(gen);
    }
    
    span.traceId = traceId.str();
    span.spanId = spanId.str();
    
    return span;
}

void Tracer::endSpan(Span& span) {
    span.endTime = std::chrono::steady_clock::now();
    span.completed = true;
    
    exportSpan(span);
}

bool Tracer::exportSpan(const Span& span) {
    if (endpoint_.empty()) {
        return false;
    }
    
    // Simple file export for now
    std::ofstream file("traces.txt", std::ios::app);
    if (!file.is_open()) {
        return false;
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        span.endTime - span.startTime);
    
    file << span.name << " " << duration.count() << "ms\n";
    return true;
}

// Telemetry implementation
Telemetry& Telemetry::instance() {
    static Telemetry instance;
    return instance;
}

bool Telemetry::initialize(const ObservabilityConfig& config) {
    config_ = config;
    
    // Initialize subsystems
    if (config_.enableLogging) {
        Logger::instance().setLevel(LogLevel::INFO); // Default level
    }
    
    if (config_.enableMetrics) {
        Metrics::instance().setEndpoint(config_.metricsEndpoint);
    }
    
    if (config_.enableTracing) {
        Tracer::instance().setEndpoint(config_.tracingEndpoint);
    }
    
    initialized_ = true;
    
    // Log initialization
    Logger::instance().info("Telemetry", "Observability system initialized");
    Metrics::instance().counter("telemetry.initialized");
    
    return true;
}

void Telemetry::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Export final metrics
    if (config_.enableMetrics) {
        Metrics::instance().exportMetrics();
    }
    
    Logger::instance().info("Telemetry", "Observability system shutdown");
    initialized_ = false;
}

void Telemetry::logFunctionCall(const std::string& functionName) {
    if (!config_.enableLogging) return;
    
    Logger::instance().debug("Function", functionName);
    
    if (config_.enableMetrics) {
        Metrics::instance().counter("function.calls", 1.0, {{"function", functionName}});
    }
}

void Telemetry::logError(const std::string& functionName, const std::string& error) {
    if (!config_.enableLogging) return;
    
    Logger::instance().error(functionName, error);
    
    if (config_.enableMetrics) {
        Metrics::instance().counter("errors", 1.0, {{"function", functionName}, {"error", error}});
    }
}

void Telemetry::logWarning(const std::string& functionName, const std::string& warning) {
    if (!config_.enableLogging) return;
    
    Logger::instance().info(functionName + ".warning", warning);
}

void Telemetry::logInfo(const std::string& message) {
    if (!config_.enableLogging) return;
    
    Logger::instance().info("Info", message);
}

void Telemetry::metric(const std::string& name, double value,
                       const std::map<std::string, std::string>& labels) {
    if (!config_.enableMetrics) return;
    
    Metrics::instance().counter(name, value, labels);
}

Span Telemetry::trace(const std::string& name, const std::map<std::string, std::string>& tags) {
    if (!config_.enableTracing) {
        return Span{};
    }
    
    return Tracer::instance().startSpan(name, tags);
}

} // namespace RawrXD::Agentic::Observability