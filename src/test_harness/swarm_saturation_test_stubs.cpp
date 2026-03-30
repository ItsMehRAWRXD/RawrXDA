// ============================================================================
// swarm_saturation_test_stubs.cpp — Minimal Stubs for Saturation Test Linking
// ============================================================================
//
// Provides minimal implementations of external dependencies needed by
// swarm_orchestrator.cpp when linked into a test context.
//
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <expected>

namespace rawrxd::orchestration {

// Minimal InferencePacer stub interface
class InferencePacer {
public:
    virtual ~InferencePacer() = default;
    virtual void OnLanePrefillStart(uint64_t requestId, uint8_t laneId) {}
    virtual void OnLanePrefillDone(uint64_t requestId, uint8_t laneId) {}
    virtual void OnSwarmDivergence(uint64_t requestId, uint32_t tokenIndex, double sigma) {}
};

} // namespace rawrxd::orchestration

namespace RawrXD {

// Minimal TokenGenerator stub matching the real interface
enum class TokenError {
    Success = 0,
    EncodingFailed,
    DecodingFailed,
    VocabularyNotLoaded,
    TokenNotFound,
    InvalidTokenId
};

class TokenGenerator {
public:
    TokenGenerator() {}
    ~TokenGenerator() = default;
    
    // Real tokenization interface (minimal stubs)
    std::expected<std::vector<int>, TokenError> encode(const std::string& text) {
        // Stub: return empty
        return std::vector<int>{};
    }
    
    std::expected<std::string, TokenError> decode(const std::vector<int>& tokens) {
        // Stub: return concatenated token ID strings
        std::string result;
        for (int id : tokens) {
            if (!result.empty()) result += " ";
            result += "token_" + std::to_string(id);
        }
        return result;
    }
    
    // Real vocabulary management (minimal stubs)
    std::expected<void, TokenError> loadVocabulary(const std::string& vocabPath) {
        return {};
    }
};

} // namespace RawrXD

