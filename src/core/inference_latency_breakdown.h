/**
 * inference_latency_breakdown.h
 * 
 * Structured latency measurement for GPU inference pipeline.
 * Separates TTFT (time-to-first-token) from decode latency to identify
 * kernel-specific vs constant overhead bottlenecks.
 * 
 * Phase 2C: GPU Performance Tuning
 */

#pragma once

#include <vector>
#include <chrono>
#include <cmath>

namespace RawrXD {
namespace GPU {

/**
 * LatencyBreakdown tracks per-phase timing for inference execution:
 * - TTFT (prefill): Model initialization + embedding + first attention
 * - Decode (generation): Token-by-token autoregressive generation
 * 
 * Used to identify whether kernel tuning affects TTFT (model loading overhead)
 * or decode throughput (steady-state token generation).
 */
struct LatencyBreakdown {
    // Timing in milliseconds
    uint32_t ttft_ms = 0;           // Time from start to first token emitted
    uint32_t first_token_ms = 0;    // Absolute timestamp of first token
    uint32_t total_ms = 0;          // Time from start to last token
    
    // Per-token decode latencies in microseconds
    std::vector<uint32_t> per_token_us;
    
    // Metadata
    uint32_t token_count = 0;
    std::string kernel_variant;     // "tg64_fused", "tg128_fallback", etc.
    std::string quantization;       // "Q2_K", "Q4_K", etc.
    
    /**
     * Calculate average TTFT (prefill latency)
     */
    double ttft_seconds() const {
        return static_cast<double>(ttft_ms) / 1000.0;
    }
    
    /**
     * Calculate average decode latency per token
     */
    double mean_decode_latency_us() const {
        if (per_token_us.empty()) return 0.0;
        
        double sum = 0.0;
        for (auto us : per_token_us) {
            sum += us;
        }
        return sum / per_token_us.size();
    }
    
    /**
     * Calculate standard deviation of decode latency
     */
    double stddev_decode_latency_us() const {
        if (per_token_us.size() < 2) return 0.0;
        
        double mean = mean_decode_latency_us();
        double sum_sq_diff = 0.0;
        for (auto us : per_token_us) {
            double diff = us - mean;
            sum_sq_diff += diff * diff;
        }
        double variance = sum_sq_diff / per_token_us.size();
        return std::sqrt(variance);
    }
    
    /**
     * Calculate steady-state throughput (tokens per second)
     * Based on average decode latency in stable generation phase
     */
    double steady_state_tok_s() const {
        double avg_us = mean_decode_latency_us();
        if (avg_us <= 0.0) return 0.0;
        
        // 1 token = avg_us microseconds → tokens/second = 1e6 / avg_us
        return 1e6 / avg_us;
    }
    
    /**
     * Overall throughput (all tokens from start to finish)
     */
    double overall_tok_s() const {
        if (total_ms == 0) return 0.0;
        return (static_cast<double>(token_count) * 1000.0) / total_ms;
    }
    
    /**
     * Percentile decode latency
     */
    uint32_t percentile_decode_latency_us(int percentile) const {
        if (per_token_us.empty()) return 0;
        if (percentile < 0 || percentile > 100) return 0;
        
        // Simple percentile (consider sorting if high accuracy needed)
        size_t index = (per_token_us.size() * percentile) / 100;
        return per_token_us[index];
    }
    
    /**
     * Check if latency is within SLA
     * SLA: TTFT < 250ms, per-token decode < 50µs
     */
    bool meets_sla() const {
        if (ttft_ms > 250) return false;
        if (percentile_decode_latency_us(99) > 50) return false;
        return true;
    }
    
    /**
     * Serialize to JSON for benchmarking
     */
    std::string to_json() const {
        std::string json = R"({
  "ttft_ms": )" + std::to_string(ttft_ms) + R"(,
  "first_token_ms": )" + std::to_string(first_token_ms) + R"(,
  "total_ms": )" + std::to_string(total_ms) + R"(,
  "token_count": )" + std::to_string(token_count) + R"(,
  "kernel_variant": ")" + kernel_variant + R"(",
  "quantization": ")" + quantization + R"(",
  "per_token_us": [)" + (per_token_us.empty() ? "" : std::to_string(per_token_us[0]));
        
        for (size_t i = 1; i < per_token_us.size(); ++i) {
            json += "," + std::to_string(per_token_us[i]);
        }
        json += R"(],
  "mean_decode_latency_us": )" + std::to_string(mean_decode_latency_us()) + R"(,
  "stddev_decode_latency_us": )" + std::to_string(stddev_decode_latency_us()) + R"(,
  "steady_state_tok_s": )" + std::to_string(steady_state_tok_s()) + R"(,
  "overall_tok_s": )" + std::to_string(overall_tok_s()) + R"(,
  "p99_decode_latency_us": )" + std::to_string(percentile_decode_latency_us(99)) + R"(,
  "meets_sla": )" + (meets_sla() ? "true" : "false") + R"(
})";
        return json;
    }
};

} // namespace GPU
} // namespace RawrXD
