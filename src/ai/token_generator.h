#pragma once
#include <string>
#include <vector>
#include <expected>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

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
    TokenGenerator();
    ~TokenGenerator() = default;
    
    // Real tokenization
    std::expected<std::vector<int>, TokenError> encode(const std::string& text);
    std::expected<std::string, TokenError> decode(const std::vector<int>& tokens);
    
    // Real vocabulary management
    std::expected<void, TokenError> loadVocabulary(const std::string& vocabPath);
    
private:
    std::unordered_map<std::string, int> m_vocab;
    std::unordered_map<int, std::string> m_idToToken;
    std::unordered_map<std::string, int> m_bpeMerges; // Simplified key for map
    
    int m_bosToken = 1;
    int m_eosToken = 2;
    int m_padToken = 0;
    int m_unkToken = 3;
    mutable std::mutex m_mutex;
    
    std::expected<std::vector<int>, TokenError> bpeEncode(const std::string& text);
    std::expected<std::string, TokenError> bpeDecode(const std::vector<int>& tokens);
    
    std::expected<int, TokenError> findToken(const std::string& token);
    std::expected<std::string, TokenError> findTokenString(int tokenId);
};

} // namespace RawrXD
