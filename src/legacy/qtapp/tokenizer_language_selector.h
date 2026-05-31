#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QMap>
#include <vector>

class TokenizerLanguageSelector : public QObject {
    Q_OBJECT

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

    explicit TokenizerLanguageSelector(QObject* parent = nullptr);
    ~TokenizerLanguageSelector();

    // Language configuration
    void setLanguage(Language language, const QJsonObject& config = QJsonObject());
    Language getCurrentLanguage() const;
    QJsonObject getLanguageConfig(Language language) const;

    // Tokenizer selection
    void setTokenizer(TokenizerType type, const QJsonObject& params = QJsonObject());
    TokenizerType getCurrentTokenizer() const;
    QJsonObject getTokenizerParams(TokenizerType type) const;

    // Vocabulary management
    bool loadVocabulary(const QString& filePath);
    bool saveVocabulary(const QString& filePath);
    int getVocabularySize() const;

    // Special tokens
    void setSpecialTokens(const QMap<QString, QString>& tokens);
    QMap<QString, QString> getSpecialTokens() const;

    // Performance optimization
    void setOptimizationLevel(int level);
    int getOptimizationLevel() const;

    // Statistics
    QJsonObject getStatistics() const;

signals:
    void languageChanged(Language newLanguage);
    void tokenizerChanged(TokenizerType newTokenizer);
    void vocabularyLoaded(bool success);

private:
    Language m_currentLanguage;
    TokenizerType m_currentTokenizer;
    QMap<QString, QString> m_specialTokens;
    std::map<Language, QJsonObject> m_languageConfigs;
    std::map<TokenizerType, QJsonObject> m_tokenizerParams;
    std::vector<QString> m_vocabulary;
    int m_vocabularySize;
    int m_optimizationLevel;
};
