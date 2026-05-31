#include "SpeculativeOptimizer.h"
#include <iostream>
#include <algorithm>

namespace RawrXD::Inference {

SpeculativeOptimizer& SpeculativeOptimizer::GetInstance() {
    static SpeculativeOptimizer instance;
    return instance;
}

void SpeculativeOptimizer::AdjustStrategy(float memoryPressure) {
    // If memory pressure is high (>80%), reduce lookahead to save KV cache and compute
    if (memoryPressure > 0.8f) {
        m_currentLookahead = 2;
        m_minConfidence = 0.95f; // Be extremely picky
    } else if (memoryPressure < 0.4f) {
        m_currentLookahead = 8;
        m_minConfidence = 0.75f; // Aggressive speculation
    } else {
        m_currentLookahead = 4;
        m_minConfidence = 0.85f;
    }
    std::cout << "[SpecOptimizer] Strategy Adjusted: Lookahead=" << m_currentLookahead << " Confidence=" << m_minConfidence << std::endl;
}

SpeculativeResult SpecOptimizer::Predict(const std::vector<int>& context, int lookahead) {
    SpeculativeResult result;
    result.confidence = 0.9f;
    
    int actualLookahead = (lookahead > 0) ? lookahead : m_currentLookahead;
    
    // Multi-token speculation simulation
    for(int i = 0; i < actualLookahead; ++i) {
        result.tokens.push_back(100 + i); // Mock token IDs
    }
    
    result.accepted = (result.confidence >= m_minConfidence);
    return result;
}

}
