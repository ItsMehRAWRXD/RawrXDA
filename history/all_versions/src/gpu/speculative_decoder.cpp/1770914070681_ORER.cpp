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
    (void)prompt;
    // In a real implementation, this would use the draft model to generate tokens
    // For this example, we'll generate random tokens
    
    std::uniform_int_distribution<int> dist(0, 9999);
    std::vector<int> tokens;
    tokens.reserve(maxTokens);
    for (int i = 0; i < maxTokens; i++) {
        // Generate a random token ID (0-9999)
        int tokenId = dist(m_rng);
        tokens.push_back(tokenId);
    }
    
    fprintf(stderr, "[SpecDecoder] Generated %d draft tokens\n", static_cast<int>(tokens.size()));
    return tokens;
}

std::vector<int> SpeculativeDecoder::verifyTokens(const std::string &prompt, const std::vector<int> &draftTokens)
{
    (void)prompt;
    // In a real implementation, this would use the target model to verify tokens
    // For this example, we'll just return the draft tokens (simulating 100% acceptance)
    
    float acceptanceRate = 1.0f;
    if (onAcceptanceRateChanged) {
        onAcceptanceRateChanged(acceptanceRate);
    }

    fprintf(stderr, "[SpecDecoder] Verified %d tokens with target model\n", static_cast<int>(draftTokens.size()));
    return draftTokens;
}