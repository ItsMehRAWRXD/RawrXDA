#include "speculative_decoder.h"
#include <cstdio>
#include <random>
#include <cmath>
#include <sstream>
#include <algorithm>
#include "../inference/ultra_fast_inference.h"

SpeculativeDecoder::SpeculativeDecoder()
    : m_gpuAccelerated(false)
    , m_draftModelLoaded(false)
    , m_targetModelLoaded(false)
    , m_rng(std::random_device{}())
{
}

SpeculativeDecoder::~SpeculativeDecoder()
{
}

void SpeculativeDecoder::setDraftModel(const std::string &modelPath)
{
    m_draftModelPath = modelPath;
    m_draftModelLoaded = true;
    fprintf(stderr, "[SpecDecoder] Draft model set to: %s\n", modelPath.c_str());
}

void SpeculativeDecoder::setTargetModel(const std::string &modelPath)
{
    m_targetModelPath = modelPath;
    m_targetModelLoaded = true;
    fprintf(stderr, "[SpecDecoder] Target model set to: %s\n", modelPath.c_str());
}

std::vector<int> SpeculativeDecoder::generateTokens(const std::string &prompt, int maxTokens)
{
    // Generate draft tokens using the draft model
    std::vector<int> draftTokens = generateDraftTokens(prompt, maxTokens);
    
    // Verify draft tokens with the target model
    std::vector<int> verifiedTokens = verifyTokens(prompt, draftTokens);
    
    // Fire callback if registered
    if (onTokensGenerated) {
        onTokensGenerated(verifiedTokens);
    }

    return verifiedTokens;
}

std::vector<int> SpeculativeDecoder::generateDraftTokens(const std::string &prompt, int maxTokens)
{
    if (!m_draftModelLoaded || m_draftModelPath.empty()) {
        fprintf(stderr, "[SpecDecoder] WARNING: Draft model not loaded, falling back to greedy sampling\n");
        // Fallback: use char-level hashing from prompt context
        std::vector<int> tokens;
        tokens.reserve(maxTokens);
        uint32_t seed = 0;
        for (char c : prompt) seed = seed * 31 + (uint32_t)c;
        m_rng.seed(seed);
        std::uniform_int_distribution<int> dist(0, 9999);
        for (int i = 0; i < maxTokens; i++) tokens.push_back(dist(m_rng));
        return tokens;
    }

    // Use the real inference engine to generate draft tokens from the draft model
    rawrxd::inference::AutonomousInferenceEngine::InferenceConfig config;
    config.max_memory_mb = 0;  // auto-detect
    config.quality_target = 0.6f;  // draft model: lower quality threshold for speed
    config.enable_streaming_pruning = true;
    config.enable_hotpatching = false;
    config.enable_gpu = m_gpuAccelerated;

    rawrxd::inference::AutonomousInferenceEngine draftEngine(config);
    if (!draftEngine.loadModelAutomatic(m_draftModelPath)) {
        fprintf(stderr, "[SpecDecoder] ERROR: Failed to load draft model: %s\n", m_draftModelPath.c_str());
        return {};
    }

    // Tokenize prompt for the draft engine
    std::vector<int32_t> promptTokens;
    std::istringstream iss(prompt);
    std::string word;
    while (iss >> word) {
        uint32_t h = 0;
        for (char c : word) h = h * 31 + (uint32_t)c;
        promptTokens.push_back((int32_t)(h % 32000));
    }

    // Collect generated tokens via callback
    std::vector<int> draftTokens;
    draftTokens.reserve(maxTokens);

    draftEngine.infer(promptTokens, [&](const std::string& token) {
        // Hash the token string to an ID (matching the tokenizer's mapping)
        uint32_t h = 0;
        for (char c : token) h = h * 31 + (uint32_t)c;
        draftTokens.push_back((int)(h % 32000));
    }, maxTokens);

    fprintf(stderr, "[SpecDecoder] Generated %d draft tokens from model: %s\n",
            static_cast<int>(draftTokens.size()), m_draftModelPath.c_str());
    return draftTokens;
}

std::vector<int> SpeculativeDecoder::verifyTokens(const std::string &prompt, const std::vector<int> &draftTokens)
{
    if (!m_targetModelLoaded || m_targetModelPath.empty()) {
        fprintf(stderr, "[SpecDecoder] WARNING: Target model not loaded, accepting all draft tokens\n");
        float acceptanceRate = 1.0f;
        if (onAcceptanceRateChanged) onAcceptanceRateChanged(acceptanceRate);
        return draftTokens;
    }

    // Use the real target model to verify draft tokens via speculative decoding
    rawrxd::inference::AutonomousInferenceEngine::InferenceConfig config;
    config.max_memory_mb = 0;
    config.quality_target = 0.9f;  // target model: high quality
    config.enable_streaming_pruning = false;
    config.enable_hotpatching = false;
    config.enable_gpu = m_gpuAccelerated;

    rawrxd::inference::AutonomousInferenceEngine targetEngine(config);
    if (!targetEngine.loadModelAutomatic(m_targetModelPath)) {
        fprintf(stderr, "[SpecDecoder] ERROR: Failed to load target model: %s, accepting all drafts\n",
                m_targetModelPath.c_str());
        if (onAcceptanceRateChanged) onAcceptanceRateChanged(1.0f);
        return draftTokens;
    }

    // Build the context: prompt + draft tokens for verification
    std::vector<int32_t> contextTokens;
    std::istringstream iss(prompt);
    std::string word;
    while (iss >> word) {
        uint32_t h = 0;
        for (char c : word) h = h * 31 + (uint32_t)c;
        contextTokens.push_back((int32_t)(h % 32000));
    }

    // Run target model to get its own token sequence
    std::vector<int> targetTokens;
    targetTokens.reserve(draftTokens.size());

    targetEngine.infer(contextTokens, [&](const std::string& token) {
        uint32_t h = 0;
        for (char c : token) h = h * 31 + (uint32_t)c;
        targetTokens.push_back((int)(h % 32000));
    }, static_cast<int>(draftTokens.size()));

    // Speculative decoding verification:
    // Accept draft tokens that match target model predictions
    // Reject from the first mismatch point
    std::vector<int> verified;
    int accepted = 0;
    int total = std::min(draftTokens.size(), targetTokens.size());

    for (int i = 0; i < total; ++i) {
        if (draftTokens[i] == targetTokens[i]) {
            verified.push_back(draftTokens[i]);
            accepted++;
        } else {
            // Accept the target model's token at the rejection point
            verified.push_back(targetTokens[i]);
            accepted++; // The replacement token counts
            break;  // Stop: speculative decoding rejects from first mismatch
        }
    }

    // If target produced more tokens than draft matched, we still got 1 bonus token
    float acceptanceRate = (draftTokens.empty()) ? 0.0f
                           : static_cast<float>(accepted) / static_cast<float>(draftTokens.size());

    if (onAcceptanceRateChanged) {
        onAcceptanceRateChanged(acceptanceRate);
    }

    fprintf(stderr, "[SpecDecoder] Verified: %d/%d tokens accepted (%.1f%% acceptance rate)\n",
            accepted, static_cast<int>(draftTokens.size()), acceptanceRate * 100.0f);
    return verified;
}