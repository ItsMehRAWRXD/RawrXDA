#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace Prometheus {

// =============================================================================
// SPECULATIVE DECODER — Minimal stub for compilation
// =============================================================================
struct SpeculativeDecoder {
    uint32_t draftTokens = 8;
    uint32_t acceptedTokens = 0;
    uint32_t rejectedTokens = 0;

    void reset() {
        acceptedTokens = 0;
        rejectedTokens = 0;
    }

    struct DraftResult {
        std::vector<uint32_t> tokens;
        std::vector<float> logits;
    };

    DraftResult draft(const std::vector<uint32_t>& prefix, uint32_t count) {
        // Stub: return empty draft
        (void)prefix;
        (void)count;
        return {};
    }

    bool verify(uint32_t targetToken, uint32_t draftToken, float draftProb) {
        (void)targetToken;
        (void)draftToken;
        (void)draftProb;
        return false;
    }
};

} // namespace Prometheus
