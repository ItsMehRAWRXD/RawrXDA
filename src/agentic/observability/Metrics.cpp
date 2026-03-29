#include "Metrics.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <utility>

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

    auto [it, inserted] = m_metrics.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(name),
        std::forward_as_tuple()
    );
    auto& metric = it->second;
    metric.type = type;
    metric.lastUpdated = std::chrono::system_clock::now();
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
    
    auto& metric = m_metrics[key];
    double current = metric.value.load();
    metric.value.store(current + value);
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
        // Initialize Prometheus-standard buckets
        auto& metric = m_metrics[key];
        metric.buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
        metric.counts.resize(metric.buckets.size() + 1); // +1 for +Inf bucket
        for (auto& c : metric.counts) c.store(0);
    }
    
    auto& metric = m_metrics[key];
    // Accumulate sum in value
    double current = metric.value.load();
    metric.value.store(current + value);
    // Increment bucket counters
    bool placed = false;
    for (size_t i = 0; i < metric.buckets.size(); ++i) {
        if (value <= metric.buckets[i]) {
            metric.counts[i].fetch_add(1, std::memory_order_relaxed);
            placed = true;
            break;
        }
    }
    if (!placed) {
        // +Inf bucket (last element)
        metric.counts[metric.buckets.size()].fetch_add(1, std::memory_order_relaxed);
    }
    metric.lastUpdated = std::chrono::system_clock::now();
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
        if (metric.type == MetricType::HISTOGRAM && !metric.buckets.empty()) {
            // Emit histogram bucket lines
            uint64_t cumulative = 0;
            for (size_t i = 0; i < metric.buckets.size(); ++i) {
                cumulative += metric.counts[i].load(std::memory_order_relaxed);
                prom << name << "_bucket{le=\""
                     << std::fixed << std::setprecision(3) << metric.buckets[i]
                     << "\"} " << cumulative << "\n";
            }
            // +Inf bucket
            cumulative += metric.counts[metric.buckets.size()].load(std::memory_order_relaxed);
            prom << name << "_bucket{le=\"+Inf\"} " << cumulative << "\n";
            prom << name << "_sum " << std::fixed << std::setprecision(2) << metric.value.load() << "\n";
            prom << name << "_count " << cumulative << "\n";
        } else {
            prom << name << " " << std::fixed << std::setprecision(2) << metric.value.load() << "\n";
        }
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
    m_serverPort = port;
    m_serverRunning.store(true);
    // Metrics available via exportPrometheus() / exportJson() polling.
    // External scraping can be achieved by exposing these through the IDE's
    // built-in HTTP server endpoints (ToolRegistryServer).
    return true;
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
