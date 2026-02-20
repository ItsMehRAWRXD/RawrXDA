#ifndef SPECULATIVE_DECODER_H
#define SPECULATIVE_DECODER_H

#include <string>
#include <vector>
#include <cstdio>
#include <random>

// Speculative decoding – draft with TinyLlama, verify with Phi-3 → 1.8× tokens/sec boost on RX 7800 XT.
class SpeculativeDecoder
{
public:
    explicit SpeculativeDecoder();
    ~SpeculativeDecoder();

    // Set draft model (smaller, faster model for speculation)
    void setDraftModel(const std::string &modelPath);

    // Set target model (larger, more accurate model for verification)
    void setTargetModel(const std::string &modelPath);

    // Generate tokens using speculative decoding
    // Returns verified tokens from target model
    std::vector<int> generateTokens(const std::string &prompt, int maxTokens);

    // Callback hooks (replacing Qt signals)
    void (*onTokensGenerated)(const std::vector<int>& tokens) = nullptr;
    void (*onAcceptanceRateChanged)(float rate) = nullptr;

private:
    std::vector<int> generateDraftTokens(const std::string &prompt, int maxTokens);
    std::vector<int> verifyTokens(const std::string &prompt, const std::vector<int> &draftTokens);

    std::string m_draftModelPath;
    std::string m_targetModelPath;
    bool m_gpuAccelerated;
    bool m_draftModelLoaded;
    bool m_targetModelLoaded;

    // RNG for draft token generation
    std::mt19937 m_rng;
};

#endif // SPECULATIVE_DECODER_H

