#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../memory/memory_oracle.h"

namespace RawrXD::Inference {

struct SpeculativeResult {
    std::vector<int> tokens;
    float confidence;
    bool accepted;
};

class SpeculativeOptimizer {
public:
    static SpeculativeOptimizer& GetInstance();

    // Speculative decoding with hardware awareness
    SpeculativeResult Predict(const std::vector<int>& context, int lookahead = 4);
    
    // Calibrate lookahead based on MemoryMorphController pressure
    void AdjustStrategy(float memoryPressure);

private:
    SpeculativeOptimizer() = default;
    
    int m_maxLookahead = 8;
    int m_currentLookahead = 4;
    float m_minConfidence = 0.85f;
};

}
