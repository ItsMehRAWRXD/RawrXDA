#pragma once
#include <QString>
#include <QHash>
#include <QVector>
#include <vector>
#include <cstdint>

/**
 * @brief SentencePiece tokenizer (Google's subword tokenizer)
 * 
 * Implements unigram language model tokenization used by many modern LLMs
 * including LLaMA, Mistral, and others. Supports both encoding and decoding.
 */
class SentencePieceTokenizer {
public:
    SentencePieceTokenizer();
    ~SentencePieceTokenizer();
    
    /**
     * @brief Load SentencePiece model from file
     * @param modelPath Path to .model file (protobuf format)
     * @return true if loaded successfully
     */
    bool loadFromFile(const QString& modelPath);
    
    /**
     * @brief Load from GGUF metadata
     * @param metadata GGUF key-value metadata
     * @return true if loaded successfully
     */
    bool loadFromGGUFMetadata(const QHash<QString, QByteArray>& metadata);
    
    /**
     * @brief Encode text to token IDs
     * @param text Input text
     * @param addBos Prepend BOS token
     * @param addEos Append EOS token
     * @return Token IDs
     */
    std::vector<int32_t> encode(const QString& text, bool addBos = false, bool addEos = false);
    
    /**
     * @brief Decode token IDs to text
     * @param tokens Token IDs
     * @param skipSpecial Skip special tokens (BOS/EOS/PAD)
     * @return Decoded text
     */
    QString decode(const std::vector<int32_t>& tokens, bool skipSpecial = true);
    
    /**
     * @brief Get vocabulary size
     */
    int vocabSize() const { return m_pieces.size(); }
    
    /**
     * @brief Check if ready
     */
    bool isReady() const { return !m_pieces.isEmpty(); }
    
    /**
     * @brief Get special token IDs
     */
    int32_t bosToken() const { return m_bosId; }
    int32_t eosToken() const { return m_eosId; }
    int32_t unkToken() const { return m_unkId; }
    int32_t padToken() const { return m_padId; }

private:
    struct SentencePiece {
        QString piece;          // Token string (may include ▁ for space)
        float score;            // Log probability score
        int32_t id;             // Token ID
        enum Type {
            NORMAL = 0,
            UNKNOWN = 1,
            CONTROL = 2,
            USER_DEFINED = 3,
            UNUSED = 4,
            BYTE = 5
        } type;
    };
    
    // Core tokenization algorithm
    struct Lattice;
    std::vector<int32_t> encodeUnigram(const QString& text);
    Lattice* buildLattice(const QString& text);
    std::vector<int32_t> viterbi(Lattice* lattice);
    
    // Normalization
    QString normalize(const QString& text);
    QString replaceSP(const QString& text);  // Replace spaces with ▁
    QString unreplaceSP(const QString& text); // Replace ▁ with spaces
    
    // Vocabulary
    QVector<SentencePiece> m_pieces;
    QHash<QString, int32_t> m_pieceToId;
    
    // Trie for efficient prefix matching
    struct TrieNode {
        QHash<QChar, TrieNode*> children;
        int32_t tokenId{-1};
        ~TrieNode() { qDeleteAll(children); }
    };
    TrieNode* m_trie{nullptr};
    void buildTrie();
    void insertTrie(const QString& piece, int32_t id);
    QVector<int32_t> findMatchingPieces(const QString& text, int pos);
    
    // Special tokens
    int32_t m_bosId{1};
    int32_t m_eosId{2};
    int32_t m_unkId{0};
    int32_t m_padId{-1};
    
    // Byte fallback for unknown characters
    bool m_byteFallback{true};
    QHash<uint8_t, int32_t> m_byteTokens;
};
