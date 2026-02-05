#pragma once
#include <QString>
#include <QHash>
#include <QVector>
#include <QFile>
#include <vector>
#include <cstdint>

/**
 * @brief Universal vocabulary loader for GGUF models
 * 
 * Loads vocabulary from various sources:
 * - GGUF metadata (embedded in model file)
 * - External vocab.json/tokenizer.json files
 * - Direct vocab.txt files
 * 
 * Supports multiple tokenizer types:
 * - BPE (GPT-2/GPT-3 style)
 * - SentencePiece (LLaMA/Mistral style)
 * - WordPiece (BERT style)
 */
class VocabularyLoader {
public:
    enum TokenizerType {
        UNKNOWN = 0,
        BPE = 1,          // Byte Pair Encoding (GPT-2, GPT-3)
        SENTENCEPIECE = 2, // Unigram/SentencePiece (LLaMA, Mistral)
        WORDPIECE = 3      // WordPiece (BERT)
    };
    
    struct Token {
        QString text;
        int32_t id;
        float score{0.0f};
        bool isSpecial{false};
    };
    
    VocabularyLoader();
    ~VocabularyLoader() = default;
    
    /**
     * @brief Load vocabulary from GGUF file metadata
     * @param ggufPath Path to GGUF model file
     * @return true if loaded successfully
     */
    bool loadFromGGUF(const QString& ggufPath);
    
    /**
     * @brief Load vocabulary from JSON file (HuggingFace format)
     * @param jsonPath Path to vocab.json or tokenizer.json
     * @return true if loaded successfully
     */
    bool loadFromJSON(const QString& jsonPath);
    
    /**
     * @brief Load vocabulary from text file (one token per line)
     * @param txtPath Path to vocab.txt
     * @return true if loaded successfully
     */
    bool loadFromText(const QString& txtPath);
    
    /**
     * @brief Get token by ID
     */
    Token getToken(int32_t id) const;
    
    /**
     * @brief Get token ID by text
     */
    int32_t getTokenId(const QString& text) const;
    
    /**
     * @brief Get all tokens
     */
    const QVector<Token>& getAllTokens() const { return m_tokens; }
    
    /**
     * @brief Get vocabulary size
     */
    int size() const { return m_tokens.size(); }
    
    /**
     * @brief Get tokenizer type detected
     */
    TokenizerType getType() const { return m_type; }
    
    /**
     * @brief Check if loaded
     */
    bool isLoaded() const { return !m_tokens.isEmpty(); }
    
    /**
     * @brief Get special token IDs
     */
    struct SpecialTokens {
        int32_t bos{-1};   // Beginning of sequence
        int32_t eos{-1};   // End of sequence
        int32_t unk{-1};   // Unknown
        int32_t pad{-1};   // Padding
        int32_t cls{-1};   // Classification (BERT)
        int32_t sep{-1};   // Separator (BERT)
        int32_t mask{-1};  // Mask token (BERT)
    };
    const SpecialTokens& getSpecialTokens() const { return m_special; }
    
    /**
     * @brief Export vocabulary to files for external tokenizers
     * @param outputDir Directory to save vocab files
     * @return true if exported successfully
     */
    bool exportToFiles(const QString& outputDir);

private:
    // Internal loaders
    bool loadGGUFMetadata(QFile& file);
    bool parseTokenizerJSON(const QByteArray& jsonData);
    bool parseVocabJSON(const QByteArray& jsonData);
    
    // Type detection
    TokenizerType detectType();
    void detectSpecialTokens();
    
    // Vocabulary storage
    QVector<Token> m_tokens;
    QHash<QString, int32_t> m_textToId;
    QHash<int32_t, int> m_idToIndex;  // id -> index in m_tokens
    
    SpecialTokens m_special;
    TokenizerType m_type{UNKNOWN};
    
    // Metadata
    QString m_modelName;
    int m_vocabSize{0};
};
