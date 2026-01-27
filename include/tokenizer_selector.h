#pragma once

#include <QString>
#include <QDialog>
#include <QJsonObject>
#include <vector>
#include <map>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QTextEdit;
class QVBoxLayout;

/**
 * @class TokenizerSelector
 * @brief Select and configure tokenizers for multiple languages
 *
 * Supported languages:
 * - English (WordPiece, BPE, SentencePiece)
 * - Chinese (Character-based, BPE)
 * - Japanese (MeCab, Janome)
 * - Multilingual (SentencePiece, mBERT)
 * - Custom (user-defined regex or code)
 *
 * Features:
 * - Vocabulary size configuration
 * - Special tokens management (CLS, SEP, PAD, UNK)
 * - Character coverage for multilingual
 * - Subword regularization
 * - Byte pair encoding parameters
 */
class TokenizerSelector : public QDialog
{
    Q_OBJECT

public:
    enum class TokenizerType {
        WordPiece,      // BERT-style
        BPE,            // Byte Pair Encoding
        SentencePiece,  // Universal
        CharacterBased, // Character-level
        Janome,         // Japanese tokenizer
        MeCab,          // Japanese tokenizer
        Custom          // User-defined
    };

    enum class Language {
        English,
        Chinese,
        Japanese,
        Multilingual,
        Custom
    };

    struct TokenizerConfig {
        Language language;
        TokenizerType tokenizerType;
        QString name;
        int vocabSize;
        int minFrequency;
        float characterCoverage;    // 0.0 to 1.0 (default 0.9995)
        bool lowercaseTokens;
        bool addSpecialTokens;
        QString specialTokens;      // JSON format: {"cls": "[CLS]", "sep": "[SEP]", ...}
        int maxTokenLength;
        bool enableSubwordRegularization;
        float subwordRegularizationAlpha;  // Regularization parameter
    };

    struct TokenizerMetrics {
        int vocabularySize;
        int uniqueTokens;
        float averageTokensPerSentence;
        float oovRate;              // Out-of-vocabulary rate
        QString encoding;           // Encoding used (utf-8, etc.)
    };

    // Constructor
    explicit TokenizerSelector(QWidget* parent = nullptr);
    ~TokenizerSelector() override;
    
    void initialize();

    /**
     * @brief Set current tokenizer configuration
     * @param config Configuration to set
     */
    void setConfiguration(const TokenizerConfig& config);

    /**
     * @brief Get current tokenizer configuration
     * @return Selected configuration
     */
    TokenizerConfig getConfiguration() const;

    /**
     * @brief Load tokenizer from file
     * @param filePath Path to tokenizer file
     * @return true if successful
     */
    bool loadTokenizer(const QString& filePath);

    /**
     * @brief Save tokenizer configuration
     * @param filePath Path to save
     * @return true if successful
     */
    bool saveTokenizer(const QString& filePath) const;

    /**
     * @brief Get metrics for current tokenizer
     * @return Tokenizer metrics
     */
    TokenizerMetrics getTokenizerMetrics() const;

    /**
     * @brief Preview tokenization of text
     * @param text Text to tokenize
     * @return List of tokens
     */
    std::vector<QString> previewTokenization(const QString& text) const;

    /**
     * @brief Get configuration as JSON
     * @return JSON object with all settings
     */
    QJsonObject toJson() const;

    /**
     * @brief Load configuration from JSON
     * @param config JSON configuration
     * @return true if successful
     */
    bool fromJson(const QJsonObject& config);

signals:
    /// Emitted when tokenizer is selected and confirmed
    void tokenizerSelected(const TokenizerConfig& config);

    /// Emitted when configuration changes
    void configurationChanged(const TokenizerConfig& config);

    /// Emitted on tokenizer load error
    void tokenizeError(const QString& error);

protected:
    void accept() override;

private:
    void setupUI();
    void setupConnections();
    void updateAvailableTokenizers();
    void updateMetricsDisplay();
    void onLanguageChanged(int index);
    void onTokenizerTypeChanged(int index);

    // UI Components
    QComboBox* m_languageCombo;
    QComboBox* m_tokenizerTypeCombo;
    QSpinBox* m_vocabSizeSpinBox;
    QSpinBox* m_minFrequencySpinBox;
    QLabel* m_characterCoverageLabel;
    QCheckBox* m_lowercaseCheckBox;
    QCheckBox* m_addSpecialTokensCheckBox;
    QTextEdit* m_specialTokensEdit;
    QSpinBox* m_maxTokenLengthSpinBox;
    QCheckBox* m_subwordRegularizationCheckBox;
    QLabel* m_metricsLabel;
    QTextEdit* m_previewEdit;
    QTextEdit* m_tokensEdit;

    // Current configuration
    TokenizerConfig m_config;

    // Available tokenizers per language
    std::map<Language, std::vector<TokenizerType>> m_availableTokenizers;

    void initializeTokenizerMap();
};
