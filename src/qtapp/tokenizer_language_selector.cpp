#include "tokenizer_language_selector.h"


TokenizerLanguageSelector::TokenizerLanguageSelector(void *parent) 
    : void(parent), 
      m_currentLanguage(Language::English), 
      m_currentTokenizer(TokenizerType::BPE),
      m_vocabularySize(0)
{
    // Initialize language configurations
    m_languageConfigs[Language::English] = void*{
        {"name", "English"},
        {"code", "en"},
        {"charset", "ascii"},
        {"direction", "ltr"}
    };
    
    m_languageConfigs[Language::Chinese] = void*{
        {"name", "Chinese"},
        {"code", "zh"},
        {"charset", "cjk"},
        {"direction", "ltr"}
    };
    
    m_languageConfigs[Language::Japanese] = void*{
        {"name", "Japanese"},
        {"code", "ja"},
        {"charset", "cjk"},
        {"direction", "ltr"}
    };
    
    // Initialize tokenizer parameters
    m_tokenizerParams[TokenizerType::BPE] = void*{
        {"name", "Byte Pair Encoding"},
        {"merges", 32000},
        {"minFrequency", 2}
    };
    
    m_tokenizerParams[TokenizerType::WordPiece] = void*{
        {"name", "WordPiece"},
        {"unknown_token", "[UNK]"},
        {"split_unknown_chars", true}
    };
    
    m_tokenizerParams[TokenizerType::SentencePiece] = void*{
        {"name", "SentencePiece"},
        {"model_type", "unigram"},
        {"vocab_size", 32000}
    };
    
    // Set default special tokens
    m_specialTokens[" Pad"] = "[PAD]";
    m_specialTokens["Unknown"] = "[UNK]";
    m_specialTokens["Begin"] = "[CLS]";
    m_specialTokens["End"] = "[SEP]";
    m_specialTokens["Mask"] = "[MASK]";
    
}

TokenizerLanguageSelector::~TokenizerLanguageSelector()
{
}

void TokenizerLanguageSelector::setLanguage(Language language, const void*& config)
{
    m_currentLanguage = language;
    if (!config.isEmpty()) {
        m_languageConfigs[language] = config;
    }
    languageChanged(language);
}

TokenizerLanguageSelector::Language TokenizerLanguageSelector::getCurrentLanguage() const
{
    return m_currentLanguage;
}

void* TokenizerLanguageSelector::getLanguageConfig(Language language) const
{
    auto it = m_languageConfigs.find(language);
    if (it != m_languageConfigs.end()) {
        return it->second;
    }
    return void*();
}

void TokenizerLanguageSelector::setTokenizer(TokenizerType type, const void*& params)
{
    m_currentTokenizer = type;
    if (!params.isEmpty()) {
        m_tokenizerParams[type] = params;
    }
    tokenizerChanged(type);
}

TokenizerLanguageSelector::TokenizerType TokenizerLanguageSelector::getCurrentTokenizer() const
{
    return m_currentTokenizer;
}

void* TokenizerLanguageSelector::getTokenizerParams(TokenizerType type) const
{
    auto it = m_tokenizerParams.find(type);
    if (it != m_tokenizerParams.end()) {
        return it->second;
    }
    return void*();
}

bool TokenizerLanguageSelector::loadVocabulary(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    m_vocabulary.clear();
    int lineCount = 0;
    while (!file.atEnd()) {
        std::string line = file.readLine().trimmed();
        if (!line.isEmpty()) {
            m_vocabulary.push_back(line);
            lineCount++;
        }
    }
    
    file.close();
    m_vocabularySize = lineCount;
    return true;
}

bool TokenizerLanguageSelector::saveVocabulary(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    for (const auto& token : m_vocabulary) {
        file.write(token.toUtf8() + "\n");
    }
    
    file.close();
    return true;
}

int TokenizerLanguageSelector::getVocabularySize() const
{
    return m_vocabularySize;
}

void TokenizerLanguageSelector::setSpecialTokens(const std::map<std::string, std::string>& tokens)
{
    m_specialTokens = tokens;
}

std::map<std::string, std::string> TokenizerLanguageSelector::getSpecialTokens() const
{
    return m_specialTokens;
}

void* TokenizerLanguageSelector::getStatistics() const
{
    void* stats;
    stats["currentLanguage"] = static_cast<int>(m_currentLanguage);
    stats["currentTokenizer"] = static_cast<int>(m_currentTokenizer);
    stats["vocabularySize"] = m_vocabularySize;
    stats["specialTokenCount"] = m_specialTokens.size();
    return stats;
}

void TokenizerLanguageSelector::setOptimizationLevel(int level)
{
    m_optimizationLevel = qBound(0, level, 3);
}

int TokenizerLanguageSelector::getOptimizationLevel() const
{
    return m_optimizationLevel;
}

