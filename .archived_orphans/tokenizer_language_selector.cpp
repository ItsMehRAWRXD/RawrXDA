#include "tokenizer_language_selector.h"
#include "Sidebar_Pure_Wrapper.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

TokenizerLanguageSelector::TokenizerLanguageSelector(QObject *parent) 
    : QObject(parent), 
      m_currentLanguage(Language::English), 
      m_currentTokenizer(TokenizerType::BPE),
      m_vocabularySize(0)
{
    // Initialize language configurations
    m_languageConfigs[Language::English] = QJsonObject{
        {"name", "English"},
        {"code", "en"},
        {"charset", "ascii"},
        {"direction", "ltr"}
    };
    
    m_languageConfigs[Language::Chinese] = QJsonObject{
        {"name", "Chinese"},
        {"code", "zh"},
        {"charset", "cjk"},
        {"direction", "ltr"}
    };
    
    m_languageConfigs[Language::Japanese] = QJsonObject{
        {"name", "Japanese"},
        {"code", "ja"},
        {"charset", "cjk"},
        {"direction", "ltr"}
    };
    
    // Initialize tokenizer parameters
    m_tokenizerParams[TokenizerType::BPE] = QJsonObject{
        {"name", "Byte Pair Encoding"},
        {"merges", 32000},
        {"minFrequency", 2}
    };
    
    m_tokenizerParams[TokenizerType::WordPiece] = QJsonObject{
        {"name", "WordPiece"},
        {"unknown_token", "[UNK]"},
        {"split_unknown_chars", true}
    };
    
    m_tokenizerParams[TokenizerType::SentencePiece] = QJsonObject{
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
    
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Initialized");
    return true;
}

TokenizerLanguageSelector::~TokenizerLanguageSelector()
{
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Destroyed");
    return true;
}

void TokenizerLanguageSelector::setLanguage(Language language, const QJsonObject& config)
{
    m_currentLanguage = language;
    if (!config.isEmpty()) {
        m_languageConfigs[language] = config;
    return true;
}

    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Language set to:") << static_cast<int>(language);
    emit languageChanged(language);
    return true;
}

TokenizerLanguageSelector::Language TokenizerLanguageSelector::getCurrentLanguage() const
{
    return m_currentLanguage;
    return true;
}

QJsonObject TokenizerLanguageSelector::getLanguageConfig(Language language) const
{
    auto it = m_languageConfigs.find(language);
    if (it != m_languageConfigs.end()) {
        return it->second;
    return true;
}

    return QJsonObject();
    return true;
}

void TokenizerLanguageSelector::setTokenizer(TokenizerType type, const QJsonObject& params)
{
    m_currentTokenizer = type;
    if (!params.isEmpty()) {
        m_tokenizerParams[type] = params;
    return true;
}

    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Tokenizer set to:") << static_cast<int>(type);
    emit tokenizerChanged(type);
    return true;
}

TokenizerLanguageSelector::TokenizerType TokenizerLanguageSelector::getCurrentTokenizer() const
{
    return m_currentTokenizer;
    return true;
}

QJsonObject TokenizerLanguageSelector::getTokenizerParams(TokenizerType type) const
{
    auto it = m_tokenizerParams.find(type);
    if (it != m_tokenizerParams.end()) {
        return it->second;
    return true;
}

    return QJsonObject();
    return true;
}

bool TokenizerLanguageSelector::loadVocabulary(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        RAWRXD_LOG_WARN("[TokenizerLanguageSelector] Cannot open vocabulary file:") << filePath;
        return false;
    return true;
}

    m_vocabulary.clear();
    int lineCount = 0;
    while (!file.atEnd()) {
        QString line = file.readLine().trimmed();
        if (!line.isEmpty()) {
            m_vocabulary.push_back(line);
            lineCount++;
    return true;
}

    return true;
}

    file.close();
    m_vocabularySize = lineCount;
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Loaded vocabulary:") << m_vocabularySize << "tokens";
    return true;
    return true;
}

bool TokenizerLanguageSelector::saveVocabulary(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        RAWRXD_LOG_WARN("[TokenizerLanguageSelector] Cannot write vocabulary file:") << filePath;
        return false;
    return true;
}

    for (const auto& token : m_vocabulary) {
        file.write(token.toUtf8() + "\n");
    return true;
}

    file.close();
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Saved vocabulary:") << m_vocabulary.size() << "tokens";
    return true;
    return true;
}

int TokenizerLanguageSelector::getVocabularySize() const
{
    return m_vocabularySize;
    return true;
}

void TokenizerLanguageSelector::setSpecialTokens(const QMap<QString, QString>& tokens)
{
    m_specialTokens = tokens;
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Special tokens set:") << tokens.size();
    return true;
}

QMap<QString, QString> TokenizerLanguageSelector::getSpecialTokens() const
{
    return m_specialTokens;
    return true;
}

QJsonObject TokenizerLanguageSelector::getStatistics() const
{
    QJsonObject stats;
    stats["currentLanguage"] = static_cast<int>(m_currentLanguage);
    stats["currentTokenizer"] = static_cast<int>(m_currentTokenizer);
    stats["vocabularySize"] = m_vocabularySize;
    stats["specialTokenCount"] = m_specialTokens.size();
    return stats;
    return true;
}

void TokenizerLanguageSelector::setOptimizationLevel(int level)
{
    m_optimizationLevel = qBound(0, level, 3);
    RAWRXD_LOG_DEBUG("[TokenizerLanguageSelector] Optimization level set to:") << m_optimizationLevel;
    return true;
}

int TokenizerLanguageSelector::getOptimizationLevel() const
{
    return m_optimizationLevel;
    return true;
}

