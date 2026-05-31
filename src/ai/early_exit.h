// early_exit.h - Logit-level early exit optimization
// Implements early exit when model is confident, preventing unnecessary Q6_K calls
//
// Strategy:
//   - If top1 - top2 > threshold: skip refinement
//   - If entropy < threshold: skip refinement
//   - If confidence trend is stable: skip refinement
//
// This prevents wasting compute on Q6_K when Q4_K or Q5_K is already confident.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <vector>

namespace RawrXD {

// Early exit decision
struct EarlyExitDecision {
    bool should_exit;           // Whether to exit early
    float confidence;           // Model confidence (top1 - top2)
    float entropy;              // Logit distribution entropy
    float margin;               // Confidence margin
    std::string reason;         // Why we're exiting or not
};

// Early exit statistics
struct EarlyExitStats {
    int total_tokens;
    int early_exits;
    int full_generations;
    float avg_confidence_early_exit;
    float avg_confidence_full;
    std::chrono::microseconds time_saved;
    float exit_rate;            // Percentage of early exits
};

// Early exit manager
class EarlyExitManager {
public:
    EarlyExitManager();
    ~EarlyExitManager() = default;
    
    // Configure early exit thresholds
    struct Config {
        float confidence_threshold = 0.95f;    // Exit if confidence > this
        float margin_threshold = 0.8f;          // Exit if margin > this
        float entropy_threshold = 1.0f;        // Exit if entropy < this
        int min_tokens_before_exit = 3;        // Don't exit before N tokens
        int stability_window = 3;              // Check last N tokens for stability
        float stability_threshold = 0.05f;      // Max variance for stability
        bool enable_entropy_check = true;
        bool enable_margin_check = true;
        bool enable_stability_check = true;
    };
    void SetConfig(const Config& config);
    
    // Check if we should early exit based on logits
    EarlyExitDecision ShouldEarlyExit(
        const float* logits,
        size_t vocab_size,
        int token_index,
        const std::vector<float>& confidence_history
    );
    
    // Calculate confidence from logits (top1 - top2)
    float CalculateConfidence(const float* logits, size_t vocab_size) const;
    
    // Calculate entropy from logits
    float CalculateEntropy(const float* logits, size_t vocab_size) const;
    
    // Calculate margin (top1 - top2)
    float CalculateMargin(const float* logits, size_t vocab_size) const;
    
    // Check if confidence is stable
    bool IsConfidenceStable(const std::vector<float>& history) const;
    
    // Record early exit for statistics
    void RecordEarlyExit(float confidence);
    
    // Record full generation for statistics
    void RecordFullGeneration(float confidence);
    
    // Get statistics
    EarlyExitStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Softmax for probability calculation
    void Softmax(const float* logits, size_t size, float* probs) const;
    
    // Find top-k values
    void FindTopK(
        const float* values,
        size_t size,
        int k,
        float* top_values
    ) const;
    
    // Members
    Config config_;
    mutable std::mutex mutex_;
    EarlyExitStats stats_;
};

// Inline implementations

inline float EarlyExitManager::CalculateConfidence(const float* logits, size_t vocab_size) const {
    // Confidence = top1 probability - top2 probability
    float top1 = -INFINITY, top2 = -INFINITY;
    
    for (size_t i = 0; i < vocab_size; i++) {
        if (logits[i] > top1) {
            top2 = top1;
            top1 = logits[i];
        } else if (logits[i] > top2) {
            top2 = logits[i];
        }
    }
    
    // Convert to probabilities via softmax
    float exp_top1 = std::exp(top1);
    float exp_top2 = std::exp(top2);
    float sum_exp = 0.0f;
    
    for (size_t i = 0; i < vocab_size; i++) {
        sum_exp += std::exp(logits[i]);
    }
    
    float prob_top1 = exp_top1 / sum_exp;
    float prob_top2 = exp_top2 / sum_exp;
    
    return prob_top1 - prob_top2;
}

inline float EarlyExitManager::CalculateEntropy(const float* logits, size_t vocab_size) const {
    // Shannon entropy: -sum(p * log(p))
    // Lower entropy = more confident
    
    std::vector<float> probs(vocab_size);
    Softmax(logits, vocab_size, probs.data());
    
    float entropy = 0.0f;
    for (size_t i = 0; i < vocab_size; i++) {
        if (probs[i] > 1e-10f) {
            entropy -= probs[i] * std::log2(probs[i]);
        }
    }
    
    return entropy;
}

inline float EarlyExitManager::CalculateMargin(const float* logits, size_t vocab_size) const {
    // Margin = top1 - top2 (in logit space)
    float top1 = -INFINITY, top2 = -INFINITY;
    
    for (size_t i = 0; i < vocab_size; i++) {
        if (logits[i] > top1) {
            top2 = top1;
            top1 = logits[i];
        } else if (logits[i] > top2) {
            top2 = logits[i];
        }
    }
    
    return top1 - top2;
}

inline bool EarlyExitManager::IsConfidenceStable(const std::vector<float>& history) const {
    if (history.size() < static_cast<size_t>(config_.stability_window)) {
        return false;
    }
    
    // Check last N tokens for stability
    int start = static_cast<int>(history.size()) - config_.stability_window;
    
    // Calculate variance
    float mean = 0.0f;
    for (int i = start; i < static_cast<int>(history.size()); i++) {
        mean += history[i];
    }
    mean /= config_.stability_window;
    
    float variance = 0.0f;
    for (int i = start; i < static_cast<int>(history.size()); i++) {
        float diff = history[i] - mean;
        variance += diff * diff;
    }
    variance /= config_.stability_window;
    
    // Stable if variance is low
    return variance < config_.stability_threshold;
}

inline void EarlyExitManager::RecordEarlyExit(float confidence) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_tokens++;
    stats_.early_exits++;
    
    stats_.avg_confidence_early_exit = 
        (stats_.avg_confidence_early_exit * (stats_.early_exits - 1) + confidence) / 
        stats_.early_exits;
    
    if (stats_.total_tokens > 0) {
        stats_.exit_rate = static_cast<float>(stats_.early_exits) / stats_.total_tokens;
    }
}

inline void EarlyExitManager::RecordFullGeneration(float confidence) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_tokens++;
    stats_.full_generations++;
    
    stats_.avg_confidence_full = 
        (stats_.avg_confidence_full * (stats_.full_generations - 1) + confidence) / 
        stats_.full_generations;
    
    if (stats_.total_tokens > 0) {
        stats_.exit_rate = static_cast<float>(stats_.early_exits) / stats_.total_tokens;
    }
}

inline EarlyExitStats EarlyExitManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

inline void EarlyExitManager::ResetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = {};
}

} // namespace RawrXD