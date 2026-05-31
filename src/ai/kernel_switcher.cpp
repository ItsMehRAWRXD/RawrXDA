// kernel_switcher.cpp - Implementation of mid-generation kernel switching
// Part of the Copilot-like inference pipeline.

#include "kernel_switcher.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {

KernelSwitcher::KernelSwitcher()
    : switches_this_gen_(0)
{
    stats_ = {};
}

void KernelSwitcher::SetConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

SwitchDecision KernelSwitcher::ShouldSwitch(
    int current_kernel,
    const TokenMetrics& metrics,
    const std::vector<TokenMetrics>& history
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SwitchDecision decision;
    decision.new_kernel = current_kernel;
    decision.should_switch = false;
    decision.confidence_threshold = config_.confidence_threshold;
    
    // Don't switch too early
    if (metrics.token_index < config_.min_tokens_before_switch) {
        decision.reason = "Too early to switch";
        return decision;
    }
    
    // Don't switch too many times
    if (switches_this_gen_ >= config_.max_switches_per_gen) {
        decision.reason = "Max switches reached";
        return decision;
    }
    
    // Calculate confidence trend
    float trend = CalculateConfidenceTrend(history);
    
    // Decision logic based on current kernel
    if (current_kernel == 1) {  // Q4_K
        // Switch to Q5_K if confidence is dropping
        if (metrics.confidence < config_.confidence_threshold) {
            decision.new_kernel = 2;  // Q5_K
            decision.should_switch = true;
            decision.reason = "Low confidence, switching to Q5_K";
            switches_this_gen_++;
            
            // Update stats
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(1, 2, 0);
            
            return decision;
        }
        
        // Switch to Q5_K if entropy is high
        if (metrics.entropy > config_.entropy_threshold) {
            decision.new_kernel = 2;  // Q5_K
            decision.should_switch = true;
            decision.reason = "High entropy, switching to Q5_K";
            switches_this_gen_++;
            
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(1, 2, 0);
            
            return decision;
        }
        
        // Switch to Q5_K if confidence trend is negative
        if (trend < -0.1f) {
            decision.new_kernel = 2;  // Q5_K
            decision.should_switch = true;
            decision.reason = "Declining confidence trend, switching to Q5_K";
            switches_this_gen_++;
            
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(1, 2, 0);
            
            return decision;
        }
        
        decision.reason = "Q4_K performing well, staying";
        
    } else if (current_kernel == 2) {  // Q5_K
        // Switch to Q6_K if confidence is still low
        if (metrics.confidence < config_.confidence_threshold * 0.8f) {
            decision.new_kernel = 4;  // Q6_K
            decision.should_switch = true;
            decision.reason = "Very low confidence, switching to Q6_K";
            switches_this_gen_++;
            
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(2, 4, 0);
            
            return decision;
        }
        
        // Switch to Q6_K if entropy is very high
        if (metrics.entropy > config_.entropy_threshold * 1.5f) {
            decision.new_kernel = 4;  // Q6_K
            decision.should_switch = true;
            decision.reason = "Very high entropy, switching to Q6_K";
            switches_this_gen_++;
            
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(2, 4, 0);
            
            return decision;
        }
        
        // Switch to Q6_K if trend is strongly negative
        if (trend < -0.2f) {
            decision.new_kernel = 4;  // Q6_K
            decision.should_switch = true;
            decision.reason = "Strongly declining confidence, switching to Q6_K";
            switches_this_gen_++;
            
            stats_.total_switches++;
            stats_.time_saved += EstimateTimeSaved(2, 4, 0);
            
            return decision;
        }
        
        decision.reason = "Q5_K performing well, staying";
        
    } else {  // Q6_K
        // Already at highest quality, no need to switch
        decision.reason = "Already at Q6_K, no switch needed";
    }
    
    return decision;
}

void KernelSwitcher::RecordMetrics(const TokenMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    current_history_.push_back(metrics);
    
    // Update stats based on kernel used
    if (metrics.kernel_used == 1) {  // Q4_K
        stats_.q4k_tokens++;
        stats_.avg_confidence_q4k = (stats_.avg_confidence_q4k * (stats_.q4k_tokens - 1) + 
                                      metrics.confidence) / stats_.q4k_tokens;
    } else if (metrics.kernel_used == 2) {  // Q5_K
        stats_.q5k_tokens++;
        stats_.avg_confidence_q5k = (stats_.avg_confidence_q5k * (stats_.q5k_tokens - 1) + 
                                      metrics.confidence) / stats_.q5k_tokens;
    } else if (metrics.kernel_used == 4) {  // Q6_K
        stats_.q6k_tokens++;
        stats_.avg_confidence_q6k = (stats_.avg_confidence_q6k * (stats_.q6k_tokens - 1) + 
                                      metrics.confidence) / stats_.q6k_tokens;
    }
}

bool KernelSwitcher::ShouldEarlyExit(
    const std::vector<TokenMetrics>& history,
    int max_tokens
) {
    if (!config_.enable_early_exit) {
        return false;
    }
    
    if (history.size() < static_cast<size_t>(config_.early_exit_min_tokens)) {
        return false;
    }
    
    // Check if last few tokens have very high confidence
    int check_count = std::min(3, static_cast<int>(history.size()));
    float avg_confidence = 0.0f;
    
    for (int i = history.size() - check_count; i < static_cast<int>(history.size()); i++) {
        avg_confidence += history[i].confidence;
    }
    avg_confidence /= check_count;
    
    if (avg_confidence > config_.early_exit_confidence) {
        // High confidence, can exit early
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.early_exits++;
        return true;
    }
    
    return false;
}

float KernelSwitcher::CalculateConfidenceTrend(const std::vector<TokenMetrics>& history) {
    if (history.size() < 3) {
        return 0.0f;
    }
    
    // Calculate linear regression slope of confidence over last N tokens
    int n = std::min(5, static_cast<int>(history.size()));
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    
    for (int i = history.size() - n; i < static_cast<int>(history.size()); i++) {
        float x = static_cast<float>(i - (history.size() - n));
        float y = history[i].confidence;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    // Slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x)
    float numerator = n * sum_xy - sum_x * sum_y;
    float denominator = n * sum_x2 - sum_x * sum_x;
    
    if (std::abs(denominator) < 1e-6f) {
        return 0.0f;
    }
    
    return numerator / denominator;
}

float KernelSwitcher::CalculateEntropy(const std::vector<float>& logits) {
    // Shannon entropy: -sum(p * log(p))
    // Higher entropy = more uncertain
    
    float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum_exp = 0.0f;
    
    for (float logit : logits) {
        sum_exp += std::exp(logit - max_logit);
    }
    
    float entropy = 0.0f;
    for (float logit : logits) {
        float prob = std::exp(logit - max_logit) / sum_exp;
        if (prob > 1e-10f) {
            entropy -= prob * std::log2(prob);
        }
    }
    
    return entropy;
}

std::chrono::microseconds KernelSwitcher::EstimateTimeSaved(
    int from_kernel,
    int to_kernel,
    int tokens_remaining
) {
    // Estimated per-token latencies (microseconds)
    // These are approximate and should be calibrated from benchmarks
    static const int q4k_latency = 2000;   // 2ms per token
    static const int q5k_latency = 3000;   // 3ms per token
    static const int q6k_latency = 5000;   // 5ms per token
    
    int from_latency = 0;
    int to_latency = 0;
    
    switch (from_kernel) {
        case 1: from_latency = q4k_latency; break;
        case 2: from_latency = q5k_latency; break;
        case 4: from_latency = q6k_latency; break;
    }
    
    switch (to_kernel) {
        case 1: to_latency = q4k_latency; break;
        case 2: to_latency = q5k_latency; break;
        case 4: to_latency = q6k_latency; break;
    }
    
    // Time saved = (from_latency - to_latency) * tokens_remaining
    // Note: This is negative if switching to slower kernel
    int saved_us = (from_latency - to_latency) * tokens_remaining;
    
    return std::chrono::microseconds(saved_us);
}

} // namespace RawrXD