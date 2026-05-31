#pragma once

#include "../RawrXD_Interfaces.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace RawrXD {

/**
 * SpeculativeDecoder - Predictive token generation optimization
 *
 * Predicts the next token(s) during current token processing,
 * reducing latency by overlapping computation with I/O.
 *
 * Enterprise targets (Day 13):
 * - 30-50% reduction in token generation latency
 * - <5ms per speculation round
 * - <2% accuracy impact on generation quality
 */
class SpeculativeDecoder {
public:
    struct PredictionStats {
        uint64_t total_predictions = 0;
        uint64_t correct_predictions = 0;
        uint64_t incorrect_predictions = 0;
        double accuracy_rate = 0.0;
        double avg_speculation_time_ms = 0.0;
        double latency_reduction_percent = 0.0;
    };

    struct SpeculationHint {
        uint32_t predicted_token_id;
        float confidence;
        bool accept_result;
    };

    SpeculativeDecoder();
    ~SpeculativeDecoder() = default;

    /**
     * Generate predictive token candidates for speculative execution.
     * Called after main token is generated to predict next candidates.
     *
     * @param context - Recent token IDs (last N tokens)
     * @param top_k - Number of candidates to generate
     * @return Vector of candidate token IDs ordered by confidence
     */
    std::vector<uint32_t> SpeculateCandidates(
        const std::vector<uint32_t>& context,
        size_t top_k = 5);

    /**
     * Validate a speculatively generated token.
     * Called after main generation to check if speculation was correct.
     *
     * @param predicted_token - The token we predicted
     * @param actual_token - The token actually generated
     * @return true if prediction matches
     */
    bool ValidatePrediction(uint32_t predicted_token, uint32_t actual_token);

    /**
     * Pre-execute possible continuations for speculation.
     * Can be called in parallel with main inference.
     *
     * @param candidate_tokens - Tokens to speculatively process
     * @param context - Input context
     * @return Vector of pre-computed logits for candidates
     */
    std::vector<std::vector<float>> SpeculativePrecompute(
        const std::vector<uint32_t>& candidate_tokens,
        const std::vector<uint32_t>& context);

    /**
     * Update model predictions based on correctness.
     * Trains the speculation model to improve accuracy.
     */
    void LearnFromPrediction(
        const std::vector<uint32_t>& context,
        uint32_t predicted_token,
        uint32_t actual_token,
        bool was_correct);

    /**
     * Get current performance statistics.
     */
    const PredictionStats& GetStats() const { return m_stats; }

    /**
     * Pre-execute possible continuations for speculation using batched operations.
     * 
     * @param candidate_tokens - Tokens to speculatively process
     * @param context - Input context
     * @return Vector of pre-computed logits for candidates
     */
    std::vector<std::vector<float>> SpeculativeBatchPrecompute(
        const std::vector<uint32_t>& candidate_tokens,
        const std::vector<uint32_t>& context);

    /**
     * Reset statistics.
     */
    void ResetStats();

    /**
     * Enable/disable speculative decoding.
     */
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    /**
     * Configure speculation parameters.
     */
    void SetSpeculationDepth(size_t depth);
    void SetConfidenceThreshold(float threshold);
    void SetTemperature(float temp);
    void UpdateNGramModel(const std::vector<uint32_t>& context, uint32_t actual_token, bool was_correct);
    void InitializeFromLoader(RawrXD::IGGUFLoader* loader);

private:
    bool m_enabled = true;
    size_t m_speculation_depth = 4;  // How many tokens ahead to predict
    float m_confidence_threshold = 0.6f;
    float m_temperature = 0.7f;
    uint32_t m_vocabSize = 32000;
    size_t m_embeddingDim = 4096;
    RawrXD::IGGUFLoader* m_loader = nullptr;

    // Token n-gram statistics for simple prediction
    std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, int>>> m_bigram_model;
    std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, int>>> m_trigram_model;

    mutable std::mutex m_mutex;
    PredictionStats m_stats;

    // Last prediction time for latency tracking
    std::chrono::high_resolution_clock::time_point m_last_prediction_start;
};

} // namespace RawrXD
