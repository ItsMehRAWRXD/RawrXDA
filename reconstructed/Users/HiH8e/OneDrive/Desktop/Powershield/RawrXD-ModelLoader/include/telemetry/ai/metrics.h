#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <deque>

namespace RawrXD {
namespace Telemetry {

// Metric data point for time-series tracking
struct MetricPoint {
    std::chrono::system_clock::time_point timestamp;
    double value;
    std::string label;
};

// Latency percentile statistics
struct LatencyStats {
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double mean_ms = 0.0;
    double max_ms = 0.0;
    size_t sample_count = 0;
};

// Token usage statistics
struct TokenStats {
    uint64_t total_prompt_tokens = 0;
    uint64_t total_completion_tokens = 0;
    uint64_t total_tokens = 0;
    double avg_prompt_tokens = 0.0;
    double avg_completion_tokens = 0.0;
};

// Tool invocation tracking
struct ToolStats {
    std::string tool_name;
    uint64_t invocation_count = 0;
    uint64_t success_count = 0;
    uint64_t error_count = 0;
    double success_rate = 0.0;
    LatencyStats latency;
};

// AI model performance metrics
struct ModelMetrics {
    std::string model_name;
    uint64_t request_count = 0;
    uint64_t success_count = 0;
    uint64_t error_count = 0;
    double success_rate = 0.0;
    LatencyStats latency;
    TokenStats tokens;
};

// Export formats
enum class ExportFormat {
    JSON,
    CSV,
    TEXT
};

class AIMetricsCollector {
public:
    AIMetricsCollector();
    ~AIMetricsCollector();

    // Record metrics
    void recordOllamaRequest(const std::string& model, uint64_t latency_ms, 
                            bool success, uint64_t prompt_tokens, 
                            uint64_t completion_tokens);
    
    void recordToolInvocation(const std::string& tool_name, uint64_t latency_ms, 
                             bool success);
    
    void recordError(const std::string& error_type, const std::string& message);
    
    void recordCustomMetric(const std::string& metric_name, double value, 
                           const std::string& label = "");

    // Query metrics
    LatencyStats getOllamaLatencyStats() const;
    TokenStats getTokenStats() const;
    std::vector<ToolStats> getToolStats() const;
    std::vector<ModelMetrics> getModelMetrics() const;
    
    std::vector<MetricPoint> getMetricTimeSeries(const std::string& metric_name, 
                                                 size_t max_points = 100) const;
    
    std::map<std::string, uint64_t> getErrorCounts() const;

    // Export
    std::string exportMetrics(ExportFormat format) const;
    bool saveMetricsToFile(const std::string& filepath, ExportFormat format) const;

    // Management
    void clearMetrics();
    void resetMetrics();
    size_t getTotalRecordedMetrics() const;

    // Real-time display data
    struct DisplayMetrics {
        // Current session
        uint64_t total_requests = 0;
        uint64_t successful_requests = 0;
        uint64_t failed_requests = 0;
        double success_rate = 0.0;
        
        // Latest latency
        double last_request_latency_ms = 0.0;
        LatencyStats latency_stats;
        
        // Token counts
        TokenStats token_stats;
        
        // Top tools
        std::vector<ToolStats> top_tools; // by invocation count
        
        // Recent errors
        std::vector<std::string> recent_errors; // last 10
        
        // Active model
        std::string current_model;
        uint64_t current_model_requests = 0;
    };
    
    DisplayMetrics getDisplayMetrics() const;

private:
    // Internal storage
    mutable std::mutex m_mutex;
    
    // Time-series data (limited buffer)
    std::map<std::string, std::deque<MetricPoint>> m_timeSeries;
    static constexpr size_t MAX_TIME_SERIES_POINTS = 1000;
    
    // Ollama metrics
    std::deque<uint64_t> m_latency_samples; // milliseconds
    std::deque<uint64_t> m_prompt_token_samples;
    std::deque<uint64_t> m_completion_token_samples;
    std::map<std::string, ModelMetrics> m_model_metrics;
    
    // Tool metrics
    std::map<std::string, ToolStats> m_tool_stats;
    
    // Error tracking
    std::map<std::string, uint64_t> m_error_counts;
    std::deque<std::string> m_recent_errors;
    static constexpr size_t MAX_RECENT_ERRORS = 10;
    
    // Session tracking
    std::chrono::system_clock::time_point m_session_start;
    uint64_t m_total_requests = 0;
    uint64_t m_successful_requests = 0;
    uint64_t m_failed_requests = 0;
    
    // Helper methods
    LatencyStats calculateLatencyStats(const std::deque<uint64_t>& samples) const;
    void pruneTimeSeries(const std::string& metric_name);
    std::string toJSON() const;
    std::string toCSV() const;
    std::string toText() const;
    
    // Internal helpers (assume mutex is already held)
    TokenStats getTokenStatsInternal() const;
    std::vector<ToolStats> getToolStatsInternal() const;
};

// Global singleton instance
AIMetricsCollector& GetMetricsCollector();

} // namespace Telemetry
} // namespace RawrXD
