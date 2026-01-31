#pragma once


#include <vector>

class TokenizerLanguageSelector : public void {

public:
    enum class Language {
        English,
        Chinese,
        Japanese,
        Multilingual,
        Custom
    };

    enum class TokenizerType {
        WordPiece,
        BPE,
        SentencePiece,
        CharacterBased,
        MeCab,
        Janome
    };

    explicit TokenizerLanguageSelector(void* parent = nullptr);
    ~TokenizerLanguageSelector();

    // Language configuration
    void setLanguage(Language language, const void*& config = void*());
    Language getCurrentLanguage() const;
    void* getLanguageConfig(Language language) const;

    // Tokenizer selection
    void setTokenizer(TokenizerType type, const void*& params = void*());
    TokenizerType getCurrentTokenizer() const;
    void* getTokenizerParams(TokenizerType type) const;

    // Vocabulary management
    bool loadVocabulary(const std::string& filePath);
    bool saveVocabulary(const std::string& filePath);
    int getVocabularySize() const;

    // Special tokens
    void setSpecialTokens(const std::map<std::string, std::string>& tokens);
    std::map<std::string, std::string> getSpecialTokens() const;

    // Performance optimization
    void setOptimizationLevel(int level);
    int getOptimizationLevel() const;

    // Statistics
    void* getStatistics() const;


    void languageChanged(Language newLanguage);
    void tokenizerChanged(TokenizerType newTokenizer);
    void vocabularyLoaded(bool success);

private:
    Language m_currentLanguage;
    TokenizerType m_currentTokenizer;
    std::map<std::string, std::string> m_specialTokens;
    std::map<Language, void*> m_languageConfigs;
    std::map<TokenizerType, void*> m_tokenizerParams;
    std::vector<std::string> m_vocabulary;
    int m_vocabularySize;
    int m_optimizationLevel;
};

