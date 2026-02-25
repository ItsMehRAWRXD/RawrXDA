#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

/**
 * @class TokenizerSelector
 * @brief Select and configure tokenizers for multiple languages without Qt.
 */
class TokenizerSelector {
public:
    enum class TokenizerType {
        WordPiece,
        BPE,
        SentencePiece,
        CharacterBased,
        Janome,
        MeCab,
        Custom
    };

    enum class Language {
        English,
        Chinese,
        Japanese,
        Multilingual,
        Custom
    };

    struct TokenizerConfig {
        Language language = Language::English;
        TokenizerType tokenizerType = TokenizerType::BPE;
        std::string name = "default";
        int vocabSize = 32000;
        int minFrequency = 2;
        float characterCoverage = 0.9995f;
        bool lowercaseTokens = true;
        bool addSpecialTokens = true;
        std::string specialTokensJson = "{\"cls\":\"[CLS]\",\"sep\":\"[SEP]\",\"pad\":\"[PAD]\",\"unk\":\"[UNK]\"}";
        int maxTokenLength = 200;
        bool enableSubwordRegularization = false;
        float subwordRegularizationAlpha = 0.1f;
        std::string vocabPath;
        std::string mergesPath;
    };

    struct TokenizerMetrics {
        int vocabularySize = 0;
        int uniqueTokens = 0;
        float averageTokensPerSentence = 0.0f;
        float oovRate = 0.0f;
        std::string encoding = "utf-8";
    };

    TokenizerSelector();

    void initialize();
    void setConfiguration(const TokenizerConfig& config);
    TokenizerConfig getConfiguration() const;

    bool loadTokenizer(const std::string& filePath);
    bool saveTokenizer(const std::string& filePath) const;
    TokenizerMetrics getTokenizerMetrics() const;
    std::vector<std::string> previewTokenization(const std::string& text) const;

private:
    TokenizerConfig m_config;
    std::vector<std::string> m_vocab;
    std::unordered_map<std::string, int> m_vocabIndex;
    std::map<Language, std::vector<TokenizerType>> m_availableTokenizers;

    void initializeTokenizerMap();
    void loadVocabIfAvailable();
    std::string getJsonValue(const std::string& json, const std::string& key) const;
    static std::string trim(const std::string& s);
    static std::vector<std::string> split(const std::string& s, char delim);
};
