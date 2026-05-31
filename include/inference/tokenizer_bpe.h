#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

namespace RawrXD::Inference {

struct BPEMergeRule {
    std::string first;
    std::string second;
    std::string merged;
    int priority = 0;
};

class TokenizerBPE {
public:
    TokenizerBPE();
    ~TokenizerBPE() = default;
    
    // Vocabulary loading
    bool loadVocabulary(const std::string& path);
    bool loadFromJSON(const std::string& jsonPath);
    
    // Encoding/Decoding
    std::vector<uint32_t> encode(const std::string& text) const;
    std::string decode(const std::vector<uint32_t>& tokens) const;
    
    // Token operations
    std::string getTokenText(uint32_t tokenId) const;
    uint32_t getTokenId(const std::string& text) const;
    bool hasToken(const std::string& text) const;
    
    // Special tokens
    uint32_t getBOSToken() const { return m_bosTokenId; }
    uint32_t getEOSToken() const { return m_eosTokenId; }
    uint32_t getUNKToken() const { return m_unkTokenId; }
    uint32_t getPADToken() const { return m_padTokenId; }
    
    // Vocabulary info
    size_t vocabSize() const { return m_idToToken.size(); }
    bool isInitialized() const { return !m_idToToken.empty(); }

private:
    std::unordered_map<std::string, uint32_t> m_tokenToId;
    std::vector<std::string> m_idToToken;
    std::vector<BPEMergeRule> m_mergeRules;
    
    // Special token IDs
    uint32_t m_bosTokenId = 1;
    uint32_t m_eosTokenId = 2;
    uint32_t m_unkTokenId = 0;
    uint32_t m_padTokenId = 0;
    
    // Character-level fallback
    std::map<char, uint32_t> m_charToToken;
    
    void applyBPERules(std::vector<std::string>& pieces) const;
    std::vector<std::string> preTokenize(const std::string& text) const;
};

} // namespace RawrXD::Inference
