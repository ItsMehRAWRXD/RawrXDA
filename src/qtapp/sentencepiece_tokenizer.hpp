#pragma once


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
    
    void initialize();
    
    /**
     * @brief Load SentencePiece model from file
     * @param modelPath Path to .model file (protobuf format)
     * @return true if loaded successfully
     */
    bool loadFromFile(const std::string& modelPath);
    
    /**
     * @brief Load from GGUF metadata
     * @param metadata GGUF key-value metadata
     * @return true if loaded successfully
     */
    bool loadFromGGUFMetadata(const std::unordered_map<std::string, std::vector<uint8_t>>& metadata);
    
    /**
     * @brief Encode text to token IDs
     * @param text Input text
     * @param addBos Prepend BOS token
     * @param addEos Append EOS token
     * @return Token IDs
     */
    std::vector<int32_t> encode(const std::string& text, bool addBos = false, bool addEos = false);
    
    /**
     * @brief Decode token IDs to text
     * @param tokens Token IDs
     * @param skipSpecial Skip special tokens (BOS/EOS/PAD)
     * @return Decoded text
     */
    std::string decode(const std::vector<int32_t>& tokens, bool skipSpecial = true);
    
    /**
     * @brief Get vocabulary size
     */
    int vocabSize() const { return m_pieces.size(); }
    
    /**
     * @brief Check if ready
     */
    bool isReady() const { return !m_pieces.empty(); }
    
    /**
     * @brief Get special token IDs
     */
    int32_t bosToken() const { return m_bosId; }
    int32_t eosToken() const { return m_eosId; }
    int32_t unkToken() const { return m_unkId; }
    int32_t padToken() const { return m_padId; }

private:
    struct SentencePiece {
        std::string piece;          // Token string (may include ▁ for space)
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
    std::vector<int32_t> encodeUnigram(const std::string& text);
    Lattice* buildLattice(const std::string& text);
    std::vector<int32_t> viterbi(Lattice* lattice);
    
    // Normalization
    std::string normalize(const std::string& text);
    std::string replaceSP(const std::string& text);  // Replace spaces with ▁
    std::string unreplaceSP(const std::string& text); // Replace ▁ with spaces
    
    // Vocabulary
    std::vector<SentencePiece> m_pieces;
    std::unordered_map<std::string, int32_t> m_pieceToId;
    
    // Trie for efficient prefix matching
    struct TrieNode {
        std::unordered_map<QChar, TrieNode*> children;
        int32_t tokenId{-1};
        ~TrieNode() { qDeleteAll(children); }
    };
    TrieNode* m_trie{nullptr};
    void buildTrie();
    void insertTrie(const std::string& piece, int32_t id);
    std::vector<int32_t> findMatchingPieces(const std::string& text, int pos);
    
    // Special tokens
    int32_t m_bosId{1};
    int32_t m_eosId{2};
    int32_t m_unkId{0};
    int32_t m_padId{-1};
    
    // Byte fallback for unknown characters
    bool m_byteFallback{true};
    std::unordered_map<uint8_t, int32_t> m_byteTokens;
};


