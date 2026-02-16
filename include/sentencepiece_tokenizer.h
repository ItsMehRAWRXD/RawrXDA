// sentencepiece_tokenizer.h — SentencePiece (Unigram) Tokenizer
// Pure C++20 port — no Qt dependencies.
// Required for LLaMA / Mistral model family tokenization.
// Uses Viterbi algorithm with trie-based prefix matching.
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

// ============================================================================
// SentencePiece token descriptor
// ============================================================================
struct SentencePieceToken {
    std::string piece;          // Token string (may include U+2581 for space)
    float       score{0.0f};    // Log probability score
    int32_t     id{-1};         // Token ID

    enum Type : uint8_t {
        NORMAL       = 0,
        UNKNOWN      = 1,
        CONTROL      = 2,
        USER_DEFINED = 3,
        UNUSED       = 4,
        BYTE_TOKEN   = 5
    } type{NORMAL};
};

// ============================================================================
// Trie node for efficient prefix matching
// ============================================================================
struct SPTrieNode {
    std::unordered_map<char32_t, std::unique_ptr<SPTrieNode>> children;
    int32_t tokenId{-1};
};

// ============================================================================
// Lattice node for Viterbi decoding
// ============================================================================
struct SPLatticeNode {
    int     pos{0};             // Position in text
    float   score{0.0f};       // Cumulative score
    int     backPointer{-1};   // Previous node index
    int32_t tokenId{-1};       // Token at this position
};

struct SPLattice {
    std::vector<std::vector<SPLatticeNode>> nodes;
    std::u32string text;

    explicit SPLattice(const std::u32string& t) : text(t) {
        nodes.resize(t.length() + 1);
        SPLatticeNode start;
        start.pos = 0;
        start.score = 0.0f;
        start.backPointer = -1;
        start.tokenId = -1;
        nodes[0].push_back(start);
    }
};

// ============================================================================
// SentencePieceTokenizer — Unigram Language Model Tokenizer
// ============================================================================
class SentencePieceTokenizer {
public:
    SentencePieceTokenizer();
    ~SentencePieceTokenizer();

    // Non-copyable
    SentencePieceTokenizer(const SentencePieceTokenizer&) = delete;
    SentencePieceTokenizer& operator=(const SentencePieceTokenizer&) = delete;

    /// Load SentencePiece model from .model file (simplified protobuf)
    bool loadFromFile(const std::string& modelPath);

    /// Load from GGUF metadata key/value pairs
    bool loadFromGGUFMetadata(
        const std::unordered_map<std::string, std::vector<uint8_t>>& metadata);

    /// Encode text to token IDs using Viterbi decoding
    std::vector<int32_t> encode(const std::string& text,
                                 bool addBos = false,
                                 bool addEos = false);

    /// Decode token IDs back to text
    std::string decode(const std::vector<int32_t>& tokens,
                       bool skipSpecial = true);

    // Accessors
    int      vocabSize()  const { return static_cast<int>(m_pieces.size()); }
    bool     isReady()    const { return !m_pieces.empty(); }
    int32_t  bosToken()   const { return m_bosId; }
    int32_t  eosToken()   const { return m_eosId; }
    int32_t  unkToken()   const { return m_unkId; }
    int32_t  padToken()   const { return m_padId; }

private:
    // Viterbi tokenization pipeline
    std::unique_ptr<SPLattice> buildLattice(const std::u32string& text);
    std::vector<int32_t>       viterbi(std::unique_ptr<SPLattice> lattice);

    // Normalization helpers
    std::string    normalize(const std::string& text);
    std::u32string replaceSP(const std::u32string& text);
    std::u32string unreplaceSP(const std::u32string& text);

    // UTF-8 <-> UTF-32 helpers
    static std::u32string utf8ToUtf32(const std::string& s);
    static std::string    utf32ToUtf8(const std::u32string& s);

    // Trie operations
    void buildTrie();
    void insertTrie(const std::u32string& piece, int32_t id);
    std::vector<int32_t> findMatchingPieces(const std::u32string& text, int pos);

    // Vocabulary
    std::vector<SentencePieceToken>               m_pieces;
    std::unordered_map<std::string, int32_t>      m_pieceToId;

    // Trie root
    std::unique_ptr<SPTrieNode> m_trie;

    // Special tokens
    int32_t m_bosId{1};
    int32_t m_eosId{2};
    int32_t m_unkId{0};
    int32_t m_padId{-1};

    // Byte fallback for unknown characters
    bool m_byteFallback{true};
    std::unordered_map<uint8_t, int32_t> m_byteTokens;
};
