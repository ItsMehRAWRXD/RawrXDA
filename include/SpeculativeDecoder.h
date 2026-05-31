#pragma once
#include "streaming_gguf_loader.h"
#include <memory>
#include <vector>

namespace RawrXD {

struct SpeculativeResult {
    std::vector<uint32_t> tokens;
    float confidence;
    bool accepted;
};

class SpeculativeDecoder {
public:
    SpeculativeDecoder(
        std::shared_ptr<StreamingGGUFLoader> draftModel,
        std::shared_ptr<StreamingGGUFLoader> targetModel);
        
    // Generate next N tokens using draft model, then verify with target model
    SpeculativeResult decodeNext(const std::vector<uint32_t>& prompt, int lookahead = 4);
    
private:
    std::shared_ptr<StreamingGGUFLoader> m_draft;
    std::shared_ptr<StreamingGGUFLoader> m_target;
    
    bool verifyTokens(
        const std::vector<uint32_t>& draftTokens,
        const std::vector<uint32_t>& prompt,
        std::vector<uint32_t>& acceptedTokens);
};

} // namespace RawrXD
