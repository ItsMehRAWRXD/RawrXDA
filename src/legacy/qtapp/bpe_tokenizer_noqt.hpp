/**
 * bpe_tokenizer_noqt.hpp
 * Pure C++ BPE tokenizer without Qt dependencies
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>

class BPETokenizer {
public:
    BPETokenizer();
    explicit BPETokenizer(const std::string& vocabPath);
    
    ~BPETokenizer() = default;
    
    // Load vocabulary from file
    bool loadVocab(const std::string& vocabPath);
    
    // Tokenization
    std::vector<int32_t> tokenize(const std::string& text);
    std::vector<int32_t> tokenizeSpecial(const std::string& text);
    
    // Detokenization
    std::string detokenize(const std::vector<int32_t>& tokens);
    std::string detokenizeSpecial(const std::vector<int32_t>& tokens);
    
    // Token operations
    std::string decode(int32_t token);
    int32_t encode(const std::string& text);
    
    // Vocabulary access
    size_t vocabSize() const { return m_vocab.size(); }
    bool hasToken(int32_t id) const { return m_vocabReverse.find(id) != m_vocabReverse.end(); }
    std::string getToken(int32_t id) const;
    
    // Special tokens
    int32_t bosToken() const { return 1; }
    int32_t eosToken() const { return 2; }
    int32_t padToken() const { return 0; }
    int32_t unkToken() const { return 3; }
    
    // Parameters
    void setByteLevel(bool enabled) { m_byteLevel = enabled; }
    void setLowercase(bool enabled) { m_lowercase = enabled; }
    
private:
    struct TokenPair {
        int32_t first;
        int32_t second;
        uint64_t rank;
        
        bool operator<(const TokenPair& other) const {
            return rank < other.rank;
        }
    };
    
    // Vocabulary mapping: string -> token_id
    std::unordered_map<std::string, int32_t> m_vocab;
    
    // Reverse mapping: token_id -> string
    std::unordered_map<int32_t, std::string> m_vocabReverse;
    
    // Merge rules: pair -> rank (for BPE merging)
    std::map<std::pair<int32_t, int32_t>, uint64_t> m_mergeRules;
    
    bool m_byteLevel = true;
    bool m_lowercase = false;
    
    // Helper methods
    std::vector<int32_t> basicTokenize(const std::string& text);
    std::vector<int32_t> wordpieceTokenize(const std::vector<int32_t>& tokens);
    std::vector<int32_t> bpeMerge(std::vector<int32_t> tokens);
    
    std::string bytesToString(const std::vector<uint8_t>& bytes);
    std::vector<uint8_t> stringToBytes(const std::string& text);
};
