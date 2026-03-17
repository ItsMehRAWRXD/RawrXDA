#include "Metrics.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace RawrXD::Agentic::Observability {

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

bool Metrics::initialize(const std::string& appName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_appName = appName;
    return true;
}

bool Metrics::registerMetric(const std::string& name, MetricType type,
                             const std::string& help,
                             const std::vector<std::string>& labelNames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    MetricValue metric;
    metric.type = type;
    metric.lastUpdated = std::chrono::system_clock::now();
    
    m_metrics[name] = metric;
    m_help[name] = help;
    m_labelNames[name] = labelNames;
    
    return true;
}

void Metrics::incrementCounter(const std::string& name, double value,
                               const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = generateKey(name, labels);
    
    if (m_metrics.find(key) == m_metrics.end()) {
        registerMetric(key, MetricType::COUNTER);
    }
    
    m_metrics[key].value.fetch_add(value);
    m_metrics[key].lastUpdated = std::chrono::system_clock::now();
}

void Metrics::setGauge(const std::string& name, double value,
                      const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = generateKey(name, labels);
    
    if (m_metrics.find(key) == m_metrics.end()) {
        registerMetric(key, MetricType::GAUGE);
    }
    
    m_metrics[key].value.store(value);
    m_metrics[key].lastUpdated = std::chrono::system_clock::now();
}

void Metrics::observeHistogram(const std::string& name, double value,
                              const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = generateKey(name, labels);
    
    if (m_metrics.find(key) == m_metrics.end()) {
        registerMetric(key, MetricType::HISTOGRAM);
    }
    
    // TODO: Implement histogram buckets
    m_metrics[key].value.store(value);
    m_metrics[key].lastUpdated = std::chrono::system_clock::now();
}

double Metrics::getMetricValue(const std::string& name,
                              const std::unordered_map<std::string, std::string>& labels) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = generateKey(name, labels);
    
    auto it = m_metrics.find(key);
    return (it != m_metrics.end()) ? it->second.value.load() : 0.0;
}

std::string Metrics::exportPrometheus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream prom;
    
    for (const auto& [name, metric] : m_metrics) {
        // Help text
        auto helpIt = m_help.find(name);
        if (helpIt != m_help.end() && !helpIt->second.empty()) {
            prom << "# HELP " << name << " " << helpIt->second << "\n";
        }
        
        // Type
        prom << "# TYPE " << name << " ";
        switch (metric.type) {
            case MetricType::COUNTER:   prom << "counter\n"; break;
            case MetricType::GAUGE:     prom << "gauge\n"; break;
            case MetricType::HISTOGRAM: prom << "histogram\n"; break;
            case MetricType::SUMMARY:   prom << "summary\n"; break;
        }
        
        // Value
        prom << name << " " << std::fixed << std::setprecision(2) << metric.value.load() << "\n";
    }
    
    return prom.str();
}

std::string Metrics::exportJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    
    json << "{\n  \"metrics\": [\n";
    
    bool first = true;
    for (const auto& [name, metric] : m_metrics) {
        if (!first) json << ",\n";
        first = false;
        
        json << "    {\n";
        json << "      \"name\": \"" << name << "\",\n";
        json << "      \"value\": " << metric.value.load() << "\n";
        json << "    }";
    }
    
    json << "\n  ]\n}";
    return json.str();
}

bool Metrics::startMetricsServer(uint16_t port) {
    // TODO: Implement HTTP server for Prometheus scraping
    m_serverPort = port;
    m_serverRunning.store(true);
    return false;
}

void Metrics::stopMetricsServer() {
    m_serverRunning.store(false);
}

void Metrics::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.clear();
    m_help.clear();
    m_labelNames.clear();
}

std::vector<std::string> Metrics::getMetricNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, _] : m_metrics) {
        names.push_back(name);
    }
    return names;
}

std::string Metrics::generateKey(const std::string& name,
                                const std::unordered_map<std::string, std::string>& labels) const {
    if (labels.empty()) {
        return name;
    }
    
    std::ostringstream key;
    key << name << "{";
    bool first = true;
    for (const auto& [k, v] : labels) {
        if (!first) key << ",";
        first = false;
        key << k << "=\"" << v << "\"";
    }
    key << "}";
    return key.str();
}

} // namespace RawrXD::Agentic::Observability
