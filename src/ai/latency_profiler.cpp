// latency_profiler.cpp - Implementation of cycle-accurate latency profiler
// Part of the Copilot-like inference pipeline.

#include "latency_profiler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace RawrXD {

LatencyProfiler::LatencyProfiler() {
    stats_ = {};
}

LatencyProfiler::~LatencyProfiler() {
    if (output_file_.is_open()) {
        output_file_.close();
    }
}

void LatencyProfiler::SetConfig(const Config& config) {
    config_ = config;
    
    if (config_.enable_file_output) {
        output_file_.open(config_.output_file, std::ios::out | std::ios::app);
    }
}

GenerationLatency LatencyProfiler::GetCurrentGeneration() const {
    return current_generation_;
}

void LatencyProfiler::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
    generation_history_.clear();
    token_history_.clear();
}

void LatencyProfiler::CalculatePercentiles() {
    if (token_history_.empty()) {
        return;
    }
    
    // Collect all latencies
    std::vector<std::chrono::microseconds> latencies;
    latencies.reserve(token_history_.size());
    
    for (const auto& token : token_history_) {
        latencies.push_back(token.total_latency);
    }
    
    // Sort for percentiles
    std::sort(latencies.begin(), latencies.end());
    
    // Calculate percentiles
    size_t p50_idx = latencies.size() * 50 / 100;
    size_t p90_idx = latencies.size() * 90 / 100;
    size_t p99_idx = latencies.size() * 99 / 100;
    
    stats_.p50_latency = latencies[p50_idx];
    stats_.p90_latency = latencies[p90_idx];
    stats_.p99_latency = latencies[p99_idx];
}

void LatencyProfiler::UpdateStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (generation_history_.empty() || token_history_.empty()) {
        return;
    }
    
    // Update counts
    stats_.total_generations = static_cast<int>(generation_history_.size());
    stats_.total_tokens = static_cast<int>(token_history_.size());
    
    // Calculate averages
    std::chrono::microseconds total_first_token{0};
    std::chrono::microseconds total_per_token{0};
    std::chrono::microseconds total_total{0};
    
    std::chrono::microseconds total_debounce{0};
    std::chrono::microseconds total_context_extraction{0};
    std::chrono::microseconds total_kv_cache_lookup{0};
    std::chrono::microseconds total_tokenization{0};
    std::chrono::microseconds total_dispatch_preparation{0};
    std::chrono::microseconds total_gpu_dispatch{0};
    std::chrono::microseconds total_kernel_execution{0};
    std::chrono::microseconds total_result_retrieval{0};
    std::chrono::microseconds total_sampling{0};
    std::chrono::microseconds total_detokenization{0};
    std::chrono::microseconds total_ghost_text{0};
    
    for (const auto& gen : generation_history_) {
        total_first_token += gen.first_token_latency;
        total_total += gen.total_latency;
    }
    
    for (const auto& token : token_history_) {
        total_per_token += token.total_latency;
        total_debounce += token.debounce_latency;
        total_context_extraction += token.context_extraction_latency;
        total_kv_cache_lookup += token.kv_cache_lookup_latency;
        total_tokenization += token.tokenization_latency;
        total_dispatch_preparation += token.dispatch_preparation_latency;
        total_gpu_dispatch += token.gpu_dispatch_latency;
        total_kernel_execution += token.kernel_execution_latency;
        total_result_retrieval += token.result_retrieval_latency;
        total_sampling += token.sampling_latency;
        total_detokenization += token.detokenization_latency;
        total_ghost_text += token.ghost_text_latency;
    }
    
    stats_.avg_first_token = total_first_token / stats_.total_generations;
    stats_.avg_per_token = total_per_token / stats_.total_tokens;
    stats_.avg_total = total_total / stats_.total_generations;
    
    stats_.avg_debounce = total_debounce / stats_.total_tokens;
    stats_.avg_context_extraction = total_context_extraction / stats_.total_tokens;
    stats_.avg_kv_cache_lookup = total_kv_cache_lookup / stats_.total_tokens;
    stats_.avg_tokenization = total_tokenization / stats_.total_tokens;
    stats_.avg_dispatch_preparation = total_dispatch_preparation / stats_.total_tokens;
    stats_.avg_gpu_dispatch = total_gpu_dispatch / stats_.total_tokens;
    stats_.avg_kernel_execution = total_kernel_execution / stats_.total_tokens;
    stats_.avg_result_retrieval = total_result_retrieval / stats_.total_tokens;
    stats_.avg_sampling = total_sampling / stats_.total_tokens;
    stats_.avg_detokenization = total_detokenization / stats_.total_tokens;
    stats_.avg_ghost_text = total_ghost_text / stats_.total_tokens;
    
    // Calculate percentiles
    if (config_.enable_percentiles) {
        CalculatePercentiles();
    }
}

void LatencyProfiler::ExportCSV(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    
    if (!file.is_open()) {
        return;
    }
    
    // Header
    file << "token_index,debounce_us,context_us,kv_cache_us,tokenization_us,"
         << "dispatch_prep_us,gpu_dispatch_us,kernel_us,result_us,"
         << "sampling_us,detoken_us,ghost_us,total_us,kernel\n";
    
    // Data
    for (const auto& token : token_history_) {
        file << token.token_index << ","
             << token.debounce_latency.count() << ","
             << token.context_extraction_latency.count() << ","
             << token.kv_cache_lookup_latency.count() << ","
             << token.tokenization_latency.count() << ","
             << token.dispatch_preparation_latency.count() << ","
             << token.gpu_dispatch_latency.count() << ","
             << token.kernel_execution_latency.count() << ","
             << token.result_retrieval_latency.count() << ","
             << token.sampling_latency.count() << ","
             << token.detokenization_latency.count() << ","
             << token.ghost_text_latency.count() << ","
             << token.total_latency.count() << ","
             << token.kernel_used << "\n";
    }
    
    file.close();
}

std::string LatencyProfiler::GetBreakdownReport() const {
    std::ostringstream report;
    
    report << "=== Latency Breakdown Report ===\n\n";
    
    report << "Total Generations: " << stats_.total_generations << "\n";
    report << "Total Tokens: " << stats_.total_tokens << "\n\n";
    
    report << "Average Latencies:\n";
    report << "  First Token: " << stats_.avg_first_token.count() << " us\n";
    report << "  Per Token: " << stats_.avg_per_token.count() << " us\n";
    report << "  Total: " << stats_.avg_total.count() << " us\n\n";
    
    report << "Stage Breakdown:\n";
    report << "  Debounce: " << stats_.avg_debounce.count() << " us\n";
    report << "  Context Extraction: " << stats_.avg_context_extraction.count() << " us\n";
    report << "  KV Cache Lookup: " << stats_.avg_kv_cache_lookup.count() << " us\n";
    report << "  Tokenization: " << stats_.avg_tokenization.count() << " us\n";
    report << "  Dispatch Prep: " << stats_.avg_dispatch_preparation.count() << " us\n";
    report << "  GPU Dispatch: " << stats_.avg_gpu_dispatch.count() << " us\n";
    report << "  Kernel Execution: " << stats_.avg_kernel_execution.count() << " us\n";
    report << "  Result Retrieval: " << stats_.avg_result_retrieval.count() << " us\n";
    report << "  Sampling: " << stats_.avg_sampling.count() << " us\n";
    report << "  Detokenization: " << stats_.avg_detokenization.count() << " us\n";
    report << "  Ghost Text: " << stats_.avg_ghost_text.count() << " us\n\n";
    
    report << "Percentiles:\n";
    report << "  P50: " << stats_.p50_latency.count() << " us\n";
    report << "  P90: " << stats_.p90_latency.count() << " us\n";
    report << "  P99: " << stats_.p99_latency.count() << " us\n\n";
    
    report << "Kernel Usage:\n";
    report << "  Q4_K: " << stats_.q4k_tokens << " tokens\n";
    report << "  Q5_K: " << stats_.q5k_tokens << " tokens\n";
    report << "  Q6_K: " << stats_.q6k_tokens << " tokens\n\n";
    
    report << "KV Cache:\n";
    report << "  Hits: " << stats_.kv_cache_hits << "\n";
    report << "  Misses: " << stats_.kv_cache_misses << "\n";
    report << "  Hit Rate: " << std::fixed << std::setprecision(2) 
           << (stats_.kv_cache_hit_rate * 100.0f) << "%\n";
    
    return report.str();
}

std::vector<std::string> LatencyProfiler::GetOptimizationSuggestions() const {
    std::vector<std::string> suggestions;
    
    // Analyze stage breakdown
    auto total = stats_.avg_debounce + stats_.avg_context_extraction + 
                 stats_.avg_kv_cache_lookup + stats_.avg_tokenization + 
                 stats_.avg_dispatch_preparation + stats_.avg_gpu_dispatch + 
                 stats_.avg_kernel_execution + stats_.avg_result_retrieval + 
                 stats_.avg_sampling + stats_.avg_detokenization + stats_.avg_ghost_text;
    
    if (total.count() == 0) {
        return suggestions;
    }
    
    // Check for high-latency stages
    float debounce_percent = static_cast<float>(stats_.avg_debounce.count()) / total.count() * 100.0f;
    if (debounce_percent > 20.0f) {
        suggestions.push_back("High debounce latency (" + std::to_string(debounce_percent) + 
                            "%). Consider adaptive debounce based on typing speed.");
    }
    
    float kv_percent = static_cast<float>(stats_.avg_kv_cache_lookup.count()) / total.count() * 100.0f;
    if (kv_percent > 10.0f && stats_.kv_cache_hit_rate < 0.5f) {
        suggestions.push_back("Low KV cache hit rate (" + std::to_string(stats_.kv_cache_hit_rate * 100.0f) + 
                            "%). Consider increasing cache size or improving hash strategy.");
    }
    
    float kernel_percent = static_cast<float>(stats_.avg_kernel_execution.count()) / total.count() * 100.0f;
    if (kernel_percent > 50.0f) {
        suggestions.push_back("High kernel execution time (" + std::to_string(kernel_percent) + 
                            "%). Consider using Q4_K for faster tokens or speculative decoding.");
    }
    
    float gpu_percent = static_cast<float>((stats_.avg_gpu_dispatch + stats_.avg_result_retrieval).count()) / 
                        total.count() * 100.0f;
    if (gpu_percent > 15.0f) {
        suggestions.push_back("High GPU dispatch overhead (" + std::to_string(gpu_percent) + 
                            "%). Consider persistent buffers or async dispatch.");
    }
    
    // Check for first token latency
    if (stats_.avg_first_token.count() > 100000) {  // > 100ms
        suggestions.push_back("High first token latency (" + std::to_string(stats_.avg_first_token.count() / 1000.0) + 
                            " ms). Consider model residency tiers or KV cache reuse.");
    }
    
    // Check for P99 latency
    if (stats_.p99_latency.count() > 200000) {  // > 200ms
        suggestions.push_back("High P99 latency (" + std::to_string(stats_.p99_latency.count() / 1000.0) + 
                            " ms). Consider early exit optimization or cancellation fast-path.");
    }
    
    // Check kernel distribution
    int total_kernels = stats_.q4k_tokens + stats_.q5k_tokens + stats_.q6k_tokens;
    if (total_kernels > 0) {
        float q6k_percent = static_cast<float>(stats_.q6k_tokens) / total_kernels * 100.0f;
        if (q6k_percent > 50.0f) {
            suggestions.push_back("High Q6_K usage (" + std::to_string(q6k_percent) + 
                                "%). Consider adaptive kernel switching for better latency.");
        }
    }
    
    return suggestions;
}

} // namespace RawrXD