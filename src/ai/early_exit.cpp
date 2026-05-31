// early_exit.cpp - Implementation of logit-level early exit optimization
// Part of the Copilot-like inference pipeline.

#include "early_exit.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {

EarlyExitManager::EarlyExitManager() {
    stats_ = {};
}

void EarlyExitManager::SetConfig(const Config& config) {
    config_ = config;
}

EarlyExitDecision EarlyExitManager::ShouldEarlyExit(
    const float* logits,
    size_t vocab_size,
    int token_index,
    const std::vector<float>& confidence_history
) {
    EarlyExitDecision decision;
    decision.should_exit = false;
    
    // Don't exit too early
    if (token_index < config_.min_tokens_before_exit) {
        decision.reason = "Too early to exit";
        return decision;
    }
    
    // Calculate confidence
    decision.confidence = CalculateConfidence(logits, vocab_size);
    decision.entropy = CalculateEntropy(logits, vocab_size);
    decision.margin = CalculateMargin(logits, vocab_size);
    
    // Check confidence threshold
    if (decision.confidence > config_.confidence_threshold) {
        decision.should_exit = true;
        decision.reason = "High confidence: " + std::to_string(decision.confidence);
        return decision;
    }
    
    // Check margin threshold
    if (config_.enable_margin_check && decision.margin > config_.margin_threshold) {
        decision.should_exit = true;
        decision.reason = "High margin: " + std::to_string(decision.margin);
        return decision;
    }
    
    // Check entropy threshold
    if (config_.enable_entropy_check && decision.entropy < config_.entropy_threshold) {
        decision.should_exit = true;
        decision.reason = "Low entropy: " + std::to_string(decision.entropy);
        return decision;
    }
    
    // Check stability
    if (config_.enable_stability_check && IsConfidenceStable(confidence_history)) {
        decision.should_exit = true;
        decision.reason = "Stable confidence";
        return decision;
    }
    
    decision.reason = "Not confident enough to exit";
    return decision;
}

void EarlyExitManager::Softmax(const float* logits, size_t size, float* probs) const {
    // Find max for numerical stability
    float max_logit = logits[0];
    for (size_t i = 1; i < size; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
        }
    }
    
    // Compute exp and sum
    float sum_exp = 0.0f;
    for (size_t i = 0; i < size; i++) {
        probs[i] = std::exp(logits[i] - max_logit);
        sum_exp += probs[i];
    }
    
    // Normalize
    for (size_t i = 0; i < size; i++) {
        probs[i] /= sum_exp;
    }
}

void EarlyExitManager::FindTopK(
    const float* values,
    size_t size,
    int k,
    float* top_values
) const {
    // Simple selection sort for top-k
    std::vector<float> temp(values, values + size);
    
    for (int i = 0; i < k && i < static_cast<int>(size); i++) {
        int max_idx = i;
        for (size_t j = i + 1; j < size; j++) {
            if (temp[j] > temp[max_idx]) {
                max_idx = static_cast<int>(j);
            }
        }
        std::swap(temp[i], temp[max_idx]);
        top_values[i] = temp[i];
    }
}

} // namespace RawrXD