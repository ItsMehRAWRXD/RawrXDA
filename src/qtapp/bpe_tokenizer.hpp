#pragma once


#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>

struct MetricsTimer {
    QElapsedTimer timer;
    MetricsTimer() { timer.start(); }
    qint64 elapsedUs() const { return timer.nsecsElapsed() / 1000; }
};

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
    bool loadFromFiles(const std::string& vocabPath, const std::string& mergesPath);
    
    /**
     * @brief Load BPE vocabulary from GGUF metadata
     * @param metadata Key-value pairs from GGUF file
     * @return true if loaded successfully
     */
    bool loadFromGGUFMetadata(const std::unordered_map<std::string, std::vector<uint8_t>>& metadata);

    bool isReverseVocabReady() const noexcept { return !reverseVocab_.empty(); }
    
    /**
     * @brief Encode text to token IDs using BPE
     * @param text Input text string
     * @return Vector of token IDs
     */
    std::vector<int32_t> encode(const std::string& text);
    
    /**
     * @brief Decode token IDs back to text
     * @param tokens Vector of token IDs
     * @return Decoded text string
     */
    std::string decode(const std::vector<int32_t>& tokens);
    
    /**
     * @brief Get vocabulary size
     */
    int vocabSize() const { return m_vocab.size(); }
    
    /**
     * @brief Check if tokenizer is ready
     */
    bool isReady() const noexcept { return m_ready; }
    
    /**
     * @brief Get special token IDs
     */
    int32_t bosToken() const { return m_bosToken; }
    int32_t eosToken() const { return m_eosToken; }
    int32_t padToken() const { return m_padToken; }
    int32_t unkToken() const { return m_unkToken; }

private:
    // Core BPE algorithm
    std::vector<std::string> byteEncode(const std::string& text);
    std::vector<std::string> applyBPE(const std::vector<std::string>& tokens);
    std::pair<int, int> findBestMergePair(const std::vector<std::string>& tokens);
    
    // Byte-level encoding helpers
    std::string byteToUnicode(uint8_t byte);
    uint8_t unicodeToByte(QChar ch);
    
    // Vocabulary: token string -> token ID
    std::unordered_map<std::string, int32_t> m_vocab;
    
    // Reverse vocabulary: token ID -> token string
    std::unordered_map<int32_t, std::string> m_reverseVocab;

    // Enterprise-grade greedy longest-match vocab (piece -> id)
    std::unordered_map<std::string, uint32_t> reverseVocab_;
    
    // BPE merge rules: (token1, token2) -> priority
    std::unordered_map<std::pair<std::string, std::string>, int> m_merges;
    
    // Special tokens
    int32_t m_bosToken{1};      // Beginning of sequence
    int32_t m_eosToken{2};      // End of sequence
    int32_t m_padToken{0};      // Padding
    int32_t m_unkToken{3};      // Unknown token
    
    // Byte-level encoding map (256 bytes -> 256 unique unicode chars)
    std::unordered_map<uint8_t, QChar> m_byteEncoder;
    std::unordered_map<QChar, uint8_t> m_byteDecoder;

    // Byte-level cache for direct id → piece lookup during decode
    std::vector<std::string> m_bytePieces;

    bool m_ready{false};

    // ---------- enterprise fallback ----------
    // Greedy longest-match tokenizer (SentencePiece-compatible)
    // Returns true on success, fills ids vector with token IDs
    bool greedyLongestMatch(const std::string& text, std::vector<uint32_t>& ids, MetricsTimer& mt) const;
    bool greedyLongestMatch(const std::string& text, std::vector<int32_t>& ids) const;
    
    // Precompiled regex pattern cache for text splitting
    struct TextSplit {
        std::string text;
        bool isSpecial;
    };
    std::vector<TextSplit> splitText(const std::string& text);
};

