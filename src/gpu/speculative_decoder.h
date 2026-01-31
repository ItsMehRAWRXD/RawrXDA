#ifndef SPECULATIVE_DECODER_H
#define SPECULATIVE_DECODER_H

class SpeculativeDecoder 
{public:
    explicit SpeculativeDecoder();
    ~SpeculativeDecoder();

    // Set draft model (smaller, faster model for speculation)
    void setDraftModel(const std::string &modelPath);
    
    // Set target model (larger, more accurate model for verification)
    void setTargetModel(const std::string &modelPath);
    
    // Generate tokens using speculative decoding
    // Returns verified tokens from target model
    std::vector<int> generateTokens(const std::string &prompt, int maxTokens);
\npublic:\n    void tokensGenerated(const std::vector<int> &tokens);
    void acceptanceRateChanged(float rate);

private:
    std::vector<int> generateDraftTokens(const std::string &prompt, int maxTokens);
    std::vector<int> verifyTokens(const std::string &prompt, const std::vector<int> &draftTokens);

    std::string m_draftModelPath;
    std::string m_targetModelPath;
    bool m_gpuAccelerated;
    bool m_draftModelLoaded;
    bool m_targetModelLoaded;
};

#endif // SPECULATIVE_DECODER_H

