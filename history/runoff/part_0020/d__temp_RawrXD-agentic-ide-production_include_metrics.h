#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <sstream>

// Forward declaration for HTTP server
class MetricsHttpServer;

class Metrics {
public:
    static Metrics& instance();

    // Counters
    void incMessagesProcessed();
    void incVoiceStart();
    void incErrors();
    void incModelCalls();
    void incTokensGenerated(size_t count);
    void incBytesStreamed(size_t bytes);
    
    // Gauges
    void setActiveModels(int count);
    void setMemoryUsageMB(double mb);
    void setGpuUsagePercent(double percent);
    void setActiveConnections(int count);

    // Histograms
    void observeModelCallMs(int ms);
    void observeTokenLatencyMs(int ms);
    void observeRequestLatencyMs(int ms);

    // Labels support for multi-dimensional metrics
    void incWithLabel(const std::string& metric, const std::string& label, size_t value = 1);

    // Snapshot for export
    struct Snapshot {
        unsigned long long messagesProcessed;
        unsigned long long voiceStarts;
        unsigned long long errors;
        unsigned long long modelCalls;
        unsigned long long tokensGenerated;
        unsigned long long bytesStreamed;
        int activeModels;
        double memoryUsageMB;
        double gpuUsagePercent;
        int activeConnections;
        std::vector<unsigned long long> latencyBuckets;
        std::vector<unsigned long long> tokenLatencyBuckets;
        std::vector<unsigned long long> requestLatencyBuckets;
        std::unordered_map<std::string, unsigned long long> labeledMetrics;
    };

    Snapshot snapshot() const;
    
    // Export formats
    bool exportToFile(const std::string& path) const;      // JSON format
    std::string exportPrometheus() const;                   // Prometheus text format
    std::string exportOpenMetrics() const;                  // OpenMetrics format
    
    // HTTP endpoint for scraping
    bool startHttpServer(int port = 9090);
    void stopHttpServer();
    bool isHttpServerRunning() const;

private:
    Metrics();
    ~Metrics();

    // Counters
    std::atomic<unsigned long long> m_messagesProcessed{0};
    std::atomic<unsigned long long> m_voiceStarts{0};
    std::atomic<unsigned long long> m_errors{0};
    std::atomic<unsigned long long> m_modelCalls{0};
    std::atomic<unsigned long long> m_tokensGenerated{0};
    std::atomic<unsigned long long> m_bytesStreamed{0};
    
    // Gauges
    std::atomic<int> m_activeModels{0};
    std::atomic<double> m_memoryUsageMB{0.0};
    std::atomic<double> m_gpuUsagePercent{0.0};
    std::atomic<int> m_activeConnections{0};

    // Histogram buckets: <10, <20, <50, <100, <200, <500, <1000, >=1000
    std::vector<std::atomic<unsigned long long>> m_latencyBuckets;
    std::vector<std::atomic<unsigned long long>> m_tokenLatencyBuckets;
    std::vector<std::atomic<unsigned long long>> m_requestLatencyBuckets;
    
    // Labeled metrics
    mutable std::mutex m_labelMutex;
    std::unordered_map<std::string, std::atomic<unsigned long long>> m_labeledMetrics;
    
    // HTTP server (stub)
    std::atomic<bool> m_httpServerRunning{false};
    
    // Helper for bucket indexing
    size_t getBucketIndex(int ms) const;
    std::string formatPrometheusMetric(const std::string& name, const std::string& type,
                                        const std::string& help, unsigned long long value) const;
};