#include "telemetry/ai_metrics.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>

namespace RawrXD {
namespace Telemetry {

AIMetricsCollector::AIMetricsCollector() 
    : m_session_start(std::chrono::system_clock::now()) {
}

AIMetricsCollector::~AIMetricsCollector() {
}

void AIMetricsCollector::recordOllamaRequest(const std::string& model, 
                                            uint64_t latency_ms,
                                            bool success, 
                                            uint64_t prompt_tokens,
                                            uint64_t completion_tokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_total_requests++;
    if (success) {
        m_successful_requests++;
    } else {
        m_failed_requests++;
    }
    
    // Record latency
    m_latency_samples.push_back(latency_ms);
    if (m_latency_samples.size() > MAX_TIME_SERIES_POINTS) {
        m_latency_samples.pop_front();
    }
    
    // Record tokens
    m_prompt_token_samples.push_back(prompt_tokens);
    m_completion_token_samples.push_back(completion_tokens);
    if (m_prompt_token_samples.size() > MAX_TIME_SERIES_POINTS) {
        m_prompt_token_samples.pop_front();
        m_completion_token_samples.pop_front();
    }
    
    // Update model metrics
    auto& metrics = m_model_metrics[model];
    metrics.model_name = model;
    metrics.request_count++;
    if (success) {
        metrics.success_count++;
    } else {
        metrics.error_count++;
    }
    metrics.success_rate = static_cast<double>(metrics.success_count) / metrics.request_count;
    metrics.tokens.total_prompt_tokens += prompt_tokens;
    metrics.tokens.total_completion_tokens += completion_tokens;
    metrics.tokens.total_tokens = metrics.tokens.total_prompt_tokens + 
                                  metrics.tokens.total_completion_tokens;
    metrics.tokens.avg_prompt_tokens = static_cast<double>(metrics.tokens.total_prompt_tokens) / 
                                       metrics.request_count;
    metrics.tokens.avg_completion_tokens = static_cast<double>(metrics.tokens.total_completion_tokens) / 
                                          metrics.request_count;
    
    // Time series
    auto now = std::chrono::system_clock::now();
    m_timeSeries["latency"].push_back({now, static_cast<double>(latency_ms), model});
    m_timeSeries["prompt_tokens"].push_back({now, static_cast<double>(prompt_tokens), model});
    m_timeSeries["completion_tokens"].push_back({now, static_cast<double>(completion_tokens), model});
    
    pruneTimeSeries("latency");
    pruneTimeSeries("prompt_tokens");
    pruneTimeSeries("completion_tokens");
}

void AIMetricsCollector::recordToolInvocation(const std::string& tool_name, 
                                             uint64_t latency_ms,
                                             bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& stats = m_tool_stats[tool_name];
    stats.tool_name = tool_name;
    stats.invocation_count++;
    if (success) {
        stats.success_count++;
    } else {
        stats.error_count++;
    }
    stats.success_rate = static_cast<double>(stats.success_count) / stats.invocation_count;
    
    // Update latency stats (simplified - store samples)
    auto now = std::chrono::system_clock::now();
    m_timeSeries["tool_" + tool_name].push_back({now, static_cast<double>(latency_ms), tool_name});
    pruneTimeSeries("tool_" + tool_name);
}

void AIMetricsCollector::recordError(const std::string& error_type, 
                                    const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_error_counts[error_type]++;
    
    std::string error_entry = error_type + ": " + message;
    m_recent_errors.push_back(error_entry);
    if (m_recent_errors.size() > MAX_RECENT_ERRORS) {
        m_recent_errors.pop_front();
    }
}

void AIMetricsCollector::recordCustomMetric(const std::string& metric_name, 
                                           double value,
                                           const std::string& label) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::system_clock::now();
    m_timeSeries[metric_name].push_back({now, value, label});
    pruneTimeSeries(metric_name);
}

LatencyStats AIMetricsCollector::getOllamaLatencyStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return calculateLatencyStats(m_latency_samples);
}

TokenStats AIMetricsCollector::getTokenStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getTokenStatsInternal();
}

TokenStats AIMetricsCollector::getTokenStatsInternal() const {
    // No lock - assumes caller holds mutex
    TokenStats stats;
    
    if (!m_prompt_token_samples.empty()) {
        stats.total_prompt_tokens = std::accumulate(
            m_prompt_token_samples.begin(), m_prompt_token_samples.end(), 0ULL);
        stats.total_completion_tokens = std::accumulate(
            m_completion_token_samples.begin(), m_completion_token_samples.end(), 0ULL);
        stats.total_tokens = stats.total_prompt_tokens + stats.total_completion_tokens;
        
        stats.avg_prompt_tokens = static_cast<double>(stats.total_prompt_tokens) / 
                                 m_prompt_token_samples.size();
        stats.avg_completion_tokens = static_cast<double>(stats.total_completion_tokens) / 
                                     m_completion_token_samples.size();
    }
    
    return stats;
}

std::vector<ToolStats> AIMetricsCollector::getToolStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getToolStatsInternal();
}

std::vector<ToolStats> AIMetricsCollector::getToolStatsInternal() const {
    // No lock - assumes caller holds mutex
    std::vector<ToolStats> result;
    for (const auto& pair : m_tool_stats) {
        result.push_back(pair.second);
    }
    
    // Sort by invocation count descending
    std::sort(result.begin(), result.end(), [](const ToolStats& a, const ToolStats& b) {
        return a.invocation_count > b.invocation_count;
    });
    
    return result;
}

std::vector<ModelMetrics> AIMetricsCollector::getModelMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<ModelMetrics> result;
    for (auto& pair : m_model_metrics) {
        auto metrics = pair.second;
        
        // Calculate latency for this model
        std::deque<uint64_t> model_latencies;
        if (m_timeSeries.count("latency")) {
            for (const auto& point : m_timeSeries.at("latency")) {
                if (point.label == pair.first) {
                    model_latencies.push_back(static_cast<uint64_t>(point.value));
                }
            }
        }
        metrics.latency = calculateLatencyStats(model_latencies);
        
        result.push_back(metrics);
    }
    
    return result;
}

std::vector<MetricPoint> AIMetricsCollector::getMetricTimeSeries(
    const std::string& metric_name, size_t max_points) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<MetricPoint> result;
    if (m_timeSeries.count(metric_name)) {
        const auto& series = m_timeSeries.at(metric_name);
        size_t start = (series.size() > max_points) ? series.size() - max_points : 0;
        result.assign(series.begin() + start, series.end());
    }
    
    return result;
}

std::map<std::string, uint64_t> AIMetricsCollector::getErrorCounts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_error_counts;
}

AIMetricsCollector::DisplayMetrics AIMetricsCollector::getDisplayMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DisplayMetrics display;
    
    display.total_requests = m_total_requests;
    display.successful_requests = m_successful_requests;
    display.failed_requests = m_failed_requests;
    
    if (m_total_requests > 0) {
        display.success_rate = static_cast<double>(m_successful_requests) / m_total_requests * 100.0;
    }
    
    if (!m_latency_samples.empty()) {
        display.last_request_latency_ms = static_cast<double>(m_latency_samples.back());
    }
    
    display.latency_stats = calculateLatencyStats(m_latency_samples);
    display.token_stats = getTokenStatsInternal(); // Use internal version to avoid deadlock
    
    // Top 5 tools
    auto all_tools = getToolStatsInternal(); // Use internal version to avoid deadlock
    display.top_tools.assign(all_tools.begin(), 
                            all_tools.begin() + std::min(size_t(5), all_tools.size()));
    
    // Recent errors
    display.recent_errors.assign(m_recent_errors.begin(), m_recent_errors.end());
    
    // Current model (most recent)
    if (!m_model_metrics.empty()) {
        // Find model with most recent activity
        std::string latest_model;
        uint64_t max_count = 0;
        for (const auto& pair : m_model_metrics) {
            if (pair.second.request_count > max_count) {
                max_count = pair.second.request_count;
                latest_model = pair.first;
            }
        }
        display.current_model = latest_model;
        display.current_model_requests = max_count;
    }
    
    return display;
}

std::string AIMetricsCollector::exportMetrics(ExportFormat format) const {
    switch (format) {
        case ExportFormat::JSON: return toJSON();
        case ExportFormat::CSV: return toCSV();
        case ExportFormat::TEXT: return toText();
        default: return "";
    }
}

bool AIMetricsCollector::saveMetricsToFile(const std::string& filepath, 
                                          ExportFormat format) const {
    std::ofstream file(filepath);
    if (!file) return false;
    
    file << exportMetrics(format);
    return file.good();
}

void AIMetricsCollector::clearMetrics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_timeSeries.clear();
    m_latency_samples.clear();
    m_prompt_token_samples.clear();
    m_completion_token_samples.clear();
    m_model_metrics.clear();
    m_tool_stats.clear();
    m_error_counts.clear();
    m_recent_errors.clear();
}

void AIMetricsCollector::resetMetrics() {
    clearMetrics();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_session_start = std::chrono::system_clock::now();
    m_total_requests = 0;
    m_successful_requests = 0;
    m_failed_requests = 0;
}

size_t AIMetricsCollector::getTotalRecordedMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t total = m_latency_samples.size();
    for (const auto& pair : m_timeSeries) {
        total += pair.second.size();
    }
    return total;
}

LatencyStats AIMetricsCollector::calculateLatencyStats(
    const std::deque<uint64_t>& samples) const {
    
    LatencyStats stats;
    
    if (samples.empty()) return stats;
    
    std::vector<uint64_t> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    
    stats.sample_count = sorted.size();
    
    // Percentiles
    size_t p50_idx = sorted.size() * 50 / 100;
    size_t p95_idx = sorted.size() * 95 / 100;
    size_t p99_idx = sorted.size() * 99 / 100;
    
    stats.p50_ms = static_cast<double>(sorted[p50_idx]);
    stats.p95_ms = static_cast<double>(sorted[std::min(p95_idx, sorted.size() - 1)]);
    stats.p99_ms = static_cast<double>(sorted[std::min(p99_idx, sorted.size() - 1)]);
    
    // Mean
    uint64_t sum = std::accumulate(sorted.begin(), sorted.end(), 0ULL);
    stats.mean_ms = static_cast<double>(sum) / sorted.size();
    
    // Max
    stats.max_ms = static_cast<double>(sorted.back());
    
    return stats;
}

void AIMetricsCollector::pruneTimeSeries(const std::string& metric_name) {
    auto& series = m_timeSeries[metric_name];
    while (series.size() > MAX_TIME_SERIES_POINTS) {
        series.pop_front();
    }
}

std::string AIMetricsCollector::toJSON() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "{\n";
    oss << "  \"session\": {\n";
    oss << "    \"total_requests\": " << m_total_requests << ",\n";
    oss << "    \"successful_requests\": " << m_successful_requests << ",\n";
    oss << "    \"failed_requests\": " << m_failed_requests << ",\n";
    
    if (m_total_requests > 0) {
        double success_rate = static_cast<double>(m_successful_requests) / m_total_requests * 100.0;
        oss << "    \"success_rate\": " << success_rate << "\n";
    } else {
        oss << "    \"success_rate\": 0.0\n";
    }
    
    oss << "  },\n";
    
    auto latency = calculateLatencyStats(m_latency_samples);
    oss << "  \"latency\": {\n";
    oss << "    \"p50_ms\": " << latency.p50_ms << ",\n";
    oss << "    \"p95_ms\": " << latency.p95_ms << ",\n";
    oss << "    \"p99_ms\": " << latency.p99_ms << ",\n";
    oss << "    \"mean_ms\": " << latency.mean_ms << ",\n";
    oss << "    \"max_ms\": " << latency.max_ms << "\n";
    oss << "  },\n";
    
    auto tokens = getTokenStats();
    oss << "  \"tokens\": {\n";
    oss << "    \"total_prompt\": " << tokens.total_prompt_tokens << ",\n";
    oss << "    \"total_completion\": " << tokens.total_completion_tokens << ",\n";
    oss << "    \"total\": " << tokens.total_tokens << ",\n";
    oss << "    \"avg_prompt\": " << tokens.avg_prompt_tokens << ",\n";
    oss << "    \"avg_completion\": " << tokens.avg_completion_tokens << "\n";
    oss << "  },\n";
    
    oss << "  \"tools\": [\n";
    auto tools = getToolStats();
    for (size_t i = 0; i < tools.size(); ++i) {
        const auto& tool = tools[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << tool.tool_name << "\",\n";
        oss << "      \"invocations\": " << tool.invocation_count << ",\n";
        oss << "      \"successes\": " << tool.success_count << ",\n";
        oss << "      \"errors\": " << tool.error_count << ",\n";
        oss << "      \"success_rate\": " << (tool.success_rate * 100.0) << "\n";
        oss << "    }" << (i < tools.size() - 1 ? "," : "") << "\n";
    }
    oss << "  ]\n";
    
    oss << "}\n";
    
    return oss.str();
}

std::string AIMetricsCollector::toCSV() const {
    std::ostringstream oss;
    
    oss << "Metric,Value\n";
    oss << "Total Requests," << m_total_requests << "\n";
    oss << "Successful Requests," << m_successful_requests << "\n";
    oss << "Failed Requests," << m_failed_requests << "\n";
    
    auto latency = calculateLatencyStats(m_latency_samples);
    oss << "Latency P50 (ms)," << latency.p50_ms << "\n";
    oss << "Latency P95 (ms)," << latency.p95_ms << "\n";
    oss << "Latency P99 (ms)," << latency.p99_ms << "\n";
    oss << "Latency Mean (ms)," << latency.mean_ms << "\n";
    
    auto tokens = getTokenStats();
    oss << "Total Prompt Tokens," << tokens.total_prompt_tokens << "\n";
    oss << "Total Completion Tokens," << tokens.total_completion_tokens << "\n";
    oss << "Average Prompt Tokens," << tokens.avg_prompt_tokens << "\n";
    oss << "Average Completion Tokens," << tokens.avg_completion_tokens << "\n";
    
    oss << "\nTool,Invocations,Successes,Errors,Success Rate\n";
    auto tools = getToolStats();
    for (const auto& tool : tools) {
        oss << tool.tool_name << "," << tool.invocation_count << ","
            << tool.success_count << "," << tool.error_count << ","
            << (tool.success_rate * 100.0) << "%\n";
    }
    
    return oss.str();
}

std::string AIMetricsCollector::toText() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "=== RawrXD AI Metrics Report ===\n\n";
    
    oss << "Session Summary:\n";
    oss << "  Total Requests:      " << m_total_requests << "\n";
    oss << "  Successful:          " << m_successful_requests << "\n";
    oss << "  Failed:              " << m_failed_requests << "\n";
    
    if (m_total_requests > 0) {
        double success_rate = static_cast<double>(m_successful_requests) / m_total_requests * 100.0;
        oss << "  Success Rate:        " << success_rate << "%\n";
    }
    
    oss << "\nLatency Statistics:\n";
    auto latency = calculateLatencyStats(m_latency_samples);
    oss << "  P50 (median):        " << latency.p50_ms << " ms\n";
    oss << "  P95:                 " << latency.p95_ms << " ms\n";
    oss << "  P99:                 " << latency.p99_ms << " ms\n";
    oss << "  Mean:                " << latency.mean_ms << " ms\n";
    oss << "  Max:                 " << latency.max_ms << " ms\n";
    
    oss << "\nToken Usage:\n";
    auto tokens = getTokenStats();
    oss << "  Total Prompt:        " << tokens.total_prompt_tokens << "\n";
    oss << "  Total Completion:    " << tokens.total_completion_tokens << "\n";
    oss << "  Total:               " << tokens.total_tokens << "\n";
    oss << "  Avg Prompt:          " << tokens.avg_prompt_tokens << "\n";
    oss << "  Avg Completion:      " << tokens.avg_completion_tokens << "\n";
    
    oss << "\nTool Statistics:\n";
    auto tools = getToolStats();
    for (const auto& tool : tools) {
        oss << "  " << tool.tool_name << ":\n";
        oss << "    Invocations:       " << tool.invocation_count << "\n";
        oss << "    Success Rate:      " << (tool.success_rate * 100.0) << "%\n";
    }
    
    if (!m_recent_errors.empty()) {
        oss << "\nRecent Errors:\n";
        for (const auto& error : m_recent_errors) {
            oss << "  - " << error << "\n";
        }
    }
    
    return oss.str();
}

// Global singleton
static AIMetricsCollector g_metricsCollector;

AIMetricsCollector& GetMetricsCollector() {
    return g_metricsCollector;
}

} // namespace Telemetry
} // namespace RawrXD
