#pragma once
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "CommonTypes.h"
#include "vulkan_compute.h"
#include "utils/Expected.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace RawrXD {

enum class TokenError {
    Success = 0,
    EncodingFailed,
    DecodingFailed,
    VocabularyNotLoaded,
    TokenNotFound,
    InvalidTokenId,
    MergeRulesNotLoaded,
    TokenizationFailed,
    UnicodeConversionFailed,
    FileReadFailed,
    InvalidFormat,
    OutOfMemory,
    UnsupportedFeature,
    GpuExecutionFailed,
    NotImplemented
};

enum class TokenizationStrategy {
    BPE,
    WordPiece,
    SentencePiece,
    Unigram,
    ByteLevel
};

struct TokenizationConfig {
    std::string vocabularyPath;
    std::string mergesPath;
    std::string specialTokensPath;
    TokenizationStrategy strategy = TokenizationStrategy::BPE;
    bool lowerCase = false;
    bool stripAccents = false;
    bool addPrefixSpace = false;
    bool trimOffsets = true;
    size_t maxTokenLength = 0;
    std::string unkToken = "<unk>";
    std::string bosToken = "<s>";
    std::string eosToken = "</s>";
    std::string maskToken = "<mask>";
    std::string padToken = "<pad>";
    
    // Performance settings
    bool useGpu = false;
    bool enableGpu = false; // Alias for cpp compatibility
    bool enableCache = true;
    bool addSpecialTokens = true;
    bool handleUnicode = true;

    bool useSimd = true;
    size_t threads = 4;
    size_t cacheSize = 10000;
};

struct TokenInfo {
    int id;
    std::string text;
    float score;
    bool isSpecial;
    std::vector<int> originalBytes;
    std::string type;
};

class TokenGenerator {
public:
    TokenGenerator();
    explicit TokenGenerator(const TokenizationConfig& config);
    ~TokenGenerator();
    
    TokenGenerator(TokenGenerator&& other) noexcept;
    TokenGenerator& operator=(TokenGenerator&& other) noexcept;

    RawrXD::Expected<void, TokenError> initialize();

    // Core encoding/decoding
    RawrXD::Expected<std::vector<int>, TokenError> encode(const std::string& text);
    RawrXD::Expected<std::string, TokenError> decode(const std::vector<int>& tokens);
    
    // Batch processing
    RawrXD::Expected<std::vector<std::vector<int>>, TokenError> encodeBatch(
        const std::vector<std::string>& texts
    );
    
    RawrXD::Expected<std::vector<std::string>, TokenError> decodeBatch(
        const std::vector<std::vector<int>>& tokens
    );

    // Vocabulary management
    RawrXD::Expected<void, TokenError> loadVocabulary(const std::string& vocabPath);
    RawrXD::Expected<void, TokenError> loadVocabularyFromModel(const std::string& modelPath);
    RawrXD::Expected<void, TokenError> loadVocabularyFromMemory(
        const std::vector<std::string>& tokens, 
        const std::vector<float>& scores,
        const std::vector<uint32_t>& types
    );
    RawrXD::Expected<void, TokenError> loadMergeRules(const std::string& mergesPath);

    void setVulkanCompute(std::shared_ptr<VulkanCompute> vulkan);

    // Token info
    bool isValidTokenId(int id) const; // Added to match cpp
    RawrXD::Expected<TokenInfo, TokenError> getTokenInfo(int tokenId);
    RawrXD::Expected<TokenInfo, TokenError> getTokenInfo(const std::string& token);

private:
    TokenizationConfig m_config;
    std::atomic<bool> m_initialized{false};
    std::mutex m_mutex;
    
    // Vocabulary
    std::unordered_map<std::string, int> m_vocab;
    std::unordered_map<int, std::string> m_idToToken;
    std::unordered_map<int, TokenInfo> m_tokenInfo;
    std::vector<std::pair<std::string, std::string>> m_mergeRules;
    std::unordered_map<std::string, int> m_mergeRank;

    // Special Tokens
    int m_bosToken = 0;
    int m_eosToken = 1;
    int m_padToken = 2;
    int m_unkToken = 3;
    int m_maskToken = 4;

    // Cache
    mutable std::mutex m_cacheMutex;
    std::unordered_map<std::string, std::vector<int>> m_encodeCache;
    std::unordered_map<std::string, std::string> m_decodeCache;
    
    // Stats
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    std::atomic<size_t> m_cacheSize{0};
    std::atomic<size_t> m_totalEncodings{0};
    std::atomic<size_t> m_totalDecodings{0};
    std::atomic<long long> m_totalEncodingTime{0};
    std::atomic<long long> m_totalDecodingTime{0};
    
    std::shared_ptr<VulkanCompute> m_vulkan;

    // Helpers
    void createMinimalVocabulary();
    RawrXD::Expected<void, TokenError> loadVocabularyFromHuggingFace(const std::string& path);
    
    RawrXD::Expected<std::vector<int>, TokenError> bpeEncode(const std::string& text);
    RawrXD::Expected<std::string, TokenError> bpeDecode(const std::vector<int>& tokens);
    RawrXD::Expected<std::vector<std::string>, TokenError> applyBPERules(const std::vector<std::string>& tokens);
    
    RawrXD::Expected<std::vector<int>, TokenError> wordpieceEncode(const std::string& text);
    RawrXD::Expected<std::string, TokenError> wordpieceDecode(const std::vector<int>& tokens);
    RawrXD::Expected<std::vector<int>, TokenError> sentencepieceEncode(const std::string& text);
    RawrXD::Expected<std::string, TokenError> sentencepieceDecode(const std::vector<int>& tokens);

    RawrXD::Expected<int, TokenError> findToken(const std::string& token);
    RawrXD::Expected<std::string, TokenError> findTokenString(int tokenId);

    RawrXD::Expected<std::vector<int>, TokenError> getFromCache(const std::string& key, bool encoding);
    void addToCache(const std::string& key, const std::vector<int>& tokens, bool encoding);
    void clearCache();
    
    void logTokenization(const std::string& text, const std::vector<int>& tokens);
    
    RawrXD::Expected<std::vector<std::vector<int>>, TokenError> encodeBatchGPU(const std::vector<std::string>& texts);
    
    // Stubs needed by cpp
    void loadConfigFromJSON(const std::string&);
    void loadTokenizerConfigFromJSON(const std::string&);
    RawrXD::Expected<void, TokenError> loadVocabularyFromSentencePiece(const std::string&);
    RawrXD::Expected<void, TokenError> loadVocabularyFromJSON(const std::string&);
    bool isValidToken(const std::string& t) const;
    size_t getCacheSize() const;
    void evictCacheIfNeeded();
    std::string detectTokenType(const std::string&) const;
    void logError(const std::string&, TokenError);
    RawrXD::Expected<std::wstring, TokenError> utf8ToWide(const std::string&);
    RawrXD::Expected<std::string, TokenError> wideToUtf8(const std::wstring&);
    json getStatus() const;
    void setConfig(const TokenizationConfig& c);
};

} // namespace RawrXD
