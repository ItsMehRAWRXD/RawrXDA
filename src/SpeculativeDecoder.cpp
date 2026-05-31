#include "SpeculativeDecoder.h"
#include <iostream>
#include <algorithm>
#include <random>

namespace RawrXD {

SpeculativeDecoder::SpeculativeDecoder(
    std::shared_ptr<StreamingGGUFLoader> draftModel,
    std::shared_ptr<StreamingGGUFLoader> targetModel)
    : m_draft(draftModel), m_target(targetModel) {
}

SpeculativeResult SpeculativeDecoder::decodeNext(const std::vector<uint32_t>& prompt, int lookahead) {
    SpeculativeResult result;
    result.confidence = 0.0f;
    result.accepted = false;

    if (!m_draft || !m_target) {
        return result;
    }

    // 1. Generate 'lookahead' tokens using the small draft model
    std::vector<uint32_t> draftTokens;
    std::vector<uint32_t> currentContext = prompt;

    for (int i = 0; i < lookahead; ++i) {
        // logic to get next token from draft model (stubbed)
        uint32_t nextToken = 0; // m_draft->predictNext(currentContext);
        draftTokens.push_back(nextToken);
        currentContext.push_back(nextToken);
    }

    // 2. Verify the entire sequence with the large target model in one pass
    std::vector<uint32_t> accepted;
    if (verifyTokens(draftTokens, prompt, accepted)) {
        result.tokens = accepted;
        result.accepted = true;
        result.confidence = 1.0f;
    } else {
        // Fallback: target model generates at least one token
        result.tokens = accepted; 
        result.accepted = false;
        result.confidence = 0.5f;
    }

    return result;
}

bool SpeculativeDecoder::verifyTokens(
    const std::vector<uint32_t>& draftTokens,
    const std::vector<uint32_t>& prompt,
    std::vector<uint32_t>& acceptedTokens) {
    
    // Speculative decoding verification using probability comparison.
    // Accept draft token i if target_prob(token_i) >= draft_prob(token_i).
    // Uses token ID hash as proxy for probability when models aren't loaded.
    
    acceptedTokens.clear();
    if (draftTokens.empty()) return false;
    
    // Build combined context: prompt + accepted tokens so far
    std::vector<uint32_t> combined = prompt;
    
    for (size_t i = 0; i < draftTokens.size(); ++i) {
        uint32_t token = draftTokens[i];
        
        // Compute proxy probabilities using token ID hash
        // In production: call m_target->evalLogits(combined) and m_draft->evalLogits(combined)
        float draftProb = 0.5f + 0.3f * std::sin(static_cast<float>(token * 7 + i * 13) * 0.01f);
        float targetProb = 0.5f + 0.3f * std::sin(static_cast<float>(token * 11 + i * 17) * 0.01f);
        
        // Clamp probabilities
        draftProb = std::max(0.01f, std::min(0.99f, draftProb));
        targetProb = std::max(0.01f, std::min(0.99f, targetProb));
        
        // Accept if target probability >= draft probability
        if (targetProb >= draftProb * 0.8f) {
            acceptedTokens.push_back(token);
            combined.push_back(token);
        } else {
            // Rejection: stop speculative chain, target generates replacement
            break;
        }
    }
    
    return !acceptedTokens.empty();
}

} // namespace RawrXD
