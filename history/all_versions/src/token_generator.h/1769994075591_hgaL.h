#pragma once
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
    GpuExecutionFailed
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
};

class TokenGenerator {
public:
    explicit TokenGenerator(const TokenizationConfig& config);
    ~TokenGenerator();

    RawrXD::Expected<void, TokenError> initialize();

    // Core encoding/decoding
    RawrXD::Expected<std::vector<int>, TokenError> encode(const std::string& text);
    RawrXD::Expected<std::string, TokenError> decode(const std::vector<int>& tokens);
    
    // Batch processing
    RawrXD::Expected<std::vector<std::vector<int>>, TokenError> encodeBatch(
        const std::vector<std::string>& texts,
        bool parallel = true
    );
    
    RawrXD::Expected<std::vector<std::string>, TokenError> decodeBatch(
        const std::vector<std::vector<int>>& tokens,
        bool parallel = true
    );

    // Vocabulary management
    RawrXD::Expected<void, TokenError> loadVocabulary(const std::string& vocabPath);
    RawrXD::Expected<void, TokenError> loadVocabularyFromModel(const std::string& modelPath);
    RawrXD::Expected<void, TokenError> loadVocabularyFromMemory(
        const std::string& vocabData, 
        const std::string& type,
        const std::vector<float>& scores = {}
    );
    RawrXD::Expected<void, TokenError> loadMergeRules(const std::string& mergesPath);

    // Token lookup
    RawrXD::Expected<int, TokenError> getTokenId(const std::string& token);
    RawrXD::Expected<std::string, TokenError> getTokenString(int tokenId);
    
    // Token info
    bool isSpecialToken(int tokenId) const;
    size_t getVocabularySize() const;
    RawrXD::Expected<TokenInfo, TokenError> getTokenInfo(int tokenId);

private:
    TokenizationConfig m_config;
};

} // namespace RawrXD
