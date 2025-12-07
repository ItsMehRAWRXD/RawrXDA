#pragma once
#include <QString>
#include <QHash>
#include <QVector>
#include <QPair>
#include <vector>
#include <cstdint>

/**
 * @brief Byte Pair Encoding (BPE) tokenizer compatible with tiktoken/OpenAI
 * 
 * Implements the BPE algorithm used by GPT-2, GPT-3, and GPT-4 models.
 * Supports both text encoding (str -> tokens) and decoding (tokens -> str).
 */
class BPETokenizer {
public:
    BPETokenizer();
    ~BPETokenizer() = default;
    
    /**
     * @brief Load BPE vocabulary and merge rules from files
     * @param vocabPath Path to vocabulary file (token -> id mapping)
     * @param mergesPath Path to merges file (BPE merge rules)
     * @return true if loaded successfully
     */
    bool loadFromFiles(const QString& vocabPath, const QString& mergesPath);
    
    /**
     * @brief Load BPE vocabulary from GGUF metadata
     * @param metadata Key-value pairs from GGUF file
     * @return true if loaded successfully
     */
    bool loadFromGGUFMetadata(const QHash<QString, QByteArray>& metadata);
    
    /**
     * @brief Encode text to token IDs using BPE
     * @param text Input text string
     * @return Vector of token IDs
     */
    std::vector<int32_t> encode(const QString& text);
    
    /**
     * @brief Decode token IDs back to text
     * @param tokens Vector of token IDs
     * @return Decoded text string
     */
    QString decode(const std::vector<int32_t>& tokens);
    
    /**
     * @brief Get vocabulary size
     */
    int vocabSize() const { return m_vocab.size(); }
    
    /**
     * @brief Check if tokenizer is ready
     */
    bool isReady() const { return !m_vocab.isEmpty() && !m_merges.isEmpty(); }
    
    /**
     * @brief Get special token IDs
     */
    int32_t bosToken() const { return m_bosToken; }
    int32_t eosToken() const { return m_eosToken; }
    int32_t padToken() const { return m_padToken; }
    int32_t unkToken() const { return m_unkToken; }

private:
    // Core BPE algorithm
    QVector<QString> byteEncode(const QString& text);
    QVector<QString> applyBPE(const QVector<QString>& tokens);
    QPair<int, int> findBestMergePair(const QVector<QString>& tokens);
    
    // Byte-level encoding helpers
    QString byteToUnicode(uint8_t byte);
    uint8_t unicodeToByte(QChar ch);
    
    // Vocabulary: token string -> token ID
    QHash<QString, int32_t> m_vocab;
    
    // Reverse vocabulary: token ID -> token string
    QHash<int32_t, QString> m_reverseVocab;
    
    // BPE merge rules: (token1, token2) -> priority
    QHash<QPair<QString, QString>, int> m_merges;
    
    // Special tokens
    int32_t m_bosToken{1};      // Beginning of sequence
    int32_t m_eosToken{2};      // End of sequence
    int32_t m_padToken{0};      // Padding
    int32_t m_unkToken{3};      // Unknown token
    
    // Byte-level encoding map (256 bytes -> 256 unique unicode chars)
    QHash<uint8_t, QChar> m_byteEncoder;
    QHash<QChar, uint8_t> m_byteDecoder;
    
    // Precompiled regex pattern cache for text splitting
    struct TextSplit {
        QString text;
        bool isSpecial;
    };
    QVector<TextSplit> splitText(const QString& text);
};
