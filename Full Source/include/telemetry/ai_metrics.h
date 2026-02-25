#pragma once

/**
 * @file telemetry/ai_metrics.h
 * AI Model Inference Metrics Collection
 * Tracks request latency, token usage, tool invocations, and error rates
 */

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <cstdint>
#include <chrono>
#include <mutex>

namespace RawrXD {
namespace Telemetry {

// Constants
constexpr size_t MAX_TIME_SERIES_POINTS = 1000;
constexpr size_t MAX_RECENT_ERRORS = 100;

// Forward declarations
struct MetricPoint;
struct LatencyStats;
struct TokenStats;
struct ToolStats;
struct ModelMetrics;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct MetricPoint {
    std::chrono::system_clock::time_point timestamp;
    double value = 0.0;
    std::string label;
};

struct LatencyStats {
    size_t sample_count = 0;
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double mean_ms = 0.0;
    double max_ms = 0.0;
};

struct TokenStats {
    uint64_t total_prompt_tokens = 0;
    uint64_t total_completion_tokens = 0;
    uint64_t total_tokens = 0;
    double avg_prompt_tokens = 0.0;
    double avg_completion_tokens = 0.0;
};

struct ToolStats {
    std::string tool_name;
    uint64_t invocation_count = 0;
    uint64_t success_count = 0;
    uint64_t error_count = 0;
    double success_rate = 0.0;
};

struct ModelMetrics {
    std::string model_name;
    uint64_t request_count = 0;
    uint64_t success_count = 0;
    uint64_t error_count = 0;
    double success_rate = 0.0;
    TokenStats tokens;
    LatencyStats latency;
};

// ============================================================================
// METRICS COLLECTOR CLASS
// ============================================================================

enum class ExportFormat {
    JSON,
    CSV,
    TEXT
};

class AIMetricsCollector {
public:
    AIMetricsCollector();
    ~AIMetricsCollector();
    
    // Ollama/LLM metrics
    void recordOllamaRequest(const std::string& model, 
                            uint64_t latency_ms,
                            bool success, 
                            uint64_t prompt_tokens = 0,
                            uint64_t completion_tokens = 0);
    
    // Tool invocation metrics
    void recordToolInvocation(const std::string& tool_name, 
                             uint64_t latency_ms,
                             bool success);
    
    // Error tracking
    void recordError(const std::string& error_type, 
                    const std::string& message);
    
    // Custom metrics
    void recordCustomMetric(const std::string& metric_name, 
                           double value,
                           const std::string& label = "");
    
    // Data retrieval
    LatencyStats getOllamaLatencyStats() const;
    TokenStats getTokenStats() const;
    std::vector<ToolStats> getToolStats() const;
    std::vector<ModelMetrics> getModelMetrics() const;
    std::vector<MetricPoint> getMetricTimeSeries(const std::string& metric_name, 
                                                 size_t max_points = 1000) const;
    std::map<std::string, uint64_t> getErrorCounts() const;
    
    // Display metrics (aggregated for UI)
    struct DisplayMetrics {
        uint64_t total_requests = 0;
        uint64_t successful_requests = 0;
        uint64_t failed_requests = 0;
        double success_rate = 0.0;
        double last_request_latency_ms = 0.0;
        LatencyStats latency_stats;
        TokenStats token_stats;
        std::vector<ToolStats> top_tools;
        std::vector<std::string> recent_errors;
        std::string current_model;
        uint64_t current_model_requests = 0;
    };
    
    DisplayMetrics getDisplayMetrics() const;
    
    // Export
    std::string exportMetrics(ExportFormat format) const;
    bool saveMetricsToFile(const std::string& filepath, 
                          ExportFormat format) const;
    
    // Management
    void clearMetrics();
    void resetMetrics();
    size_t getTotalRecordedMetrics() const;

private:
    // Internal helpers
    TokenStats getTokenStatsInternal() const;
    std::vector<ToolStats> getToolStatsInternal() const;
    LatencyStats calculateLatencyStats(const std::deque<uint64_t>& samples) const;
    void pruneTimeSeries(const std::string& metric_name);
    
    // Export formatters
    std::string toJSON() const;
    std::string toCSV() const;
    std::string toText() const;
    
    // Data members
    mutable std::mutex m_mutex;
    std::chrono::system_clock::time_point m_session_start;
    
    uint64_t m_total_requests = 0;
    uint64_t m_successful_requests = 0;
    uint64_t m_failed_requests = 0;
    
    std::deque<uint64_t> m_latency_samples;
    std::deque<uint64_t> m_prompt_token_samples;
    std::deque<uint64_t> m_completion_token_samples;
    
    std::map<std::string, ModelMetrics> m_model_metrics;
    std::map<std::string, ToolStats> m_tool_stats;
    std::map<std::string, uint64_t> m_error_counts;
    std::deque<std::string> m_recent_errors;
    
    std::map<std::string, std::deque<MetricPoint>> m_timeSeries;
};

// Global metrics collector accessor
AIMetricsCollector& GetMetricsCollector();

} // namespace Telemetry
} // namespace RawrXD
