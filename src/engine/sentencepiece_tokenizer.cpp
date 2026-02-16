// ============================================================================
// sentencepiece_tokenizer.cpp — SentencePiece (Unigram) Tokenizer
// ============================================================================
// Pure C++20 port — zero Qt dependencies.
// Implements Viterbi algorithm over a trie-index vocabulary for subword
// tokenization used by LLaMA, Mistral, and other modern LLMs.
// ============================================================================

#include "sentencepiece_tokenizer.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <iostream>
#include <locale>
#include <codecvt>

// SCAFFOLD_108: SentencePiece tokenizer


// ============================================================================
// UTF-8 <-> UTF-32 helpers (self-contained, no ICU/Qt)
// ============================================================================

std::u32string SentencePieceTokenizer::utf8ToUtf32(const std::string& s)
{
    std::u32string result;
    result.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = 0;
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) {
            cp = c; i += 1;
        } else if ((c >> 5) == 0x06) {
            cp = (c & 0x1F) << 6;
            if (i + 1 < s.size()) cp |= (static_cast<uint8_t>(s[i+1]) & 0x3F);
            i += 2;
        } else if ((c >> 4) == 0x0E) {
            cp = (c & 0x0F) << 12;
            if (i + 1 < s.size()) cp |= (static_cast<uint8_t>(s[i+1]) & 0x3F) << 6;
            if (i + 2 < s.size()) cp |= (static_cast<uint8_t>(s[i+2]) & 0x3F);
            i += 3;
        } else if ((c >> 3) == 0x1E) {
            cp = (c & 0x07) << 18;
            if (i + 1 < s.size()) cp |= (static_cast<uint8_t>(s[i+1]) & 0x3F) << 12;
            if (i + 2 < s.size()) cp |= (static_cast<uint8_t>(s[i+2]) & 0x3F) << 6;
            if (i + 3 < s.size()) cp |= (static_cast<uint8_t>(s[i+3]) & 0x3F);
            i += 4;
        } else {
            ++i; // skip invalid
        }
        result.push_back(static_cast<char32_t>(cp));
    }
    return result;
}

std::string SentencePieceTokenizer::utf32ToUtf8(const std::u32string& s)
{
    std::string result;
    result.reserve(s.size() * 4);
    for (char32_t cp : s) {
        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return result;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

SentencePieceTokenizer::SentencePieceTokenizer()
    : m_trie(std::make_unique<SPTrieNode>())
{
}

SentencePieceTokenizer::~SentencePieceTokenizer() = default;

// ============================================================================
// loadFromFile — Simplified protobuf SentencePiece .model reader
// ============================================================================

bool SentencePieceTokenizer::loadFromFile(const std::string& modelPath)
{
    std::ifstream file(modelPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SentencePiece] Failed to open model: " << modelPath << "\n";
        return false;
    }

    // Skip 16-byte header
    file.seekg(16, std::ios::beg);
    if (!file.good()) {
        std::cerr << "[SentencePiece] Invalid model header\n";
        return false;
    }

    // Read piece count
    int32_t numPieces = 0;
    file.read(reinterpret_cast<char*>(&numPieces), 4);
    if (numPieces <= 0 || numPieces > 500000) {
        std::cerr << "[SentencePiece] Invalid piece count: " << numPieces << "\n";
        return false;
    }

    m_pieces.reserve(numPieces);

    for (int32_t i = 0; i < numPieces; ++i) {
        uint32_t pieceLen = 0;
        file.read(reinterpret_cast<char*>(&pieceLen), 4);
        if (!file.good() || pieceLen > 4096) break;

        std::string pieceStr(pieceLen, '\0');
        file.read(pieceStr.data(), pieceLen);

        float score = 0.0f;
        file.read(reinterpret_cast<char*>(&score), 4);

        SentencePieceToken piece;
        piece.piece = std::move(pieceStr);
        piece.score = score;
        piece.id    = i;
        piece.type  = SentencePieceToken::NORMAL;

        // Detect special tokens
        if (piece.piece == "<s>")    m_bosId = i;
        else if (piece.piece == "</s>")  m_eosId = i;
        else if (piece.piece == "<unk>") m_unkId = i;
        else if (piece.piece == "<pad>") m_padId = i;

        m_pieceToId[piece.piece] = i;
        m_pieces.push_back(std::move(piece));
    }

    buildTrie();

    std::cout << "[SentencePiece] Loaded " << m_pieces.size() << " pieces from file\n";
    return true;
}

// ============================================================================
// loadFromGGUFMetadata — Load from GGUF tokenizer.ggml.tokens
// ============================================================================

bool SentencePieceTokenizer::loadFromGGUFMetadata(
    const std::unordered_map<std::string, std::vector<uint8_t>>& metadata)
{
    auto tokensIt = metadata.find("tokenizer.ggml.tokens");
    if (tokensIt == metadata.end()) {
        return false;
    }

    const auto& tokensData = tokensIt->second;
    if (tokensData.size() < 4) return false;

    // Parse token count
    int32_t numTokens = 0;
    std::memcpy(&numTokens, tokensData.data(), 4);
    if (numTokens <= 0 || numTokens > 500000) return false;

    m_pieces.reserve(numTokens);

    size_t offset = 4;
    for (int32_t i = 0; i < numTokens; ++i) {
        if (offset + 4 > tokensData.size()) break;

        uint32_t len = 0;
        std::memcpy(&len, tokensData.data() + offset, 4);
        offset += 4;

        if (offset + len > tokensData.size()) break;

        std::string tokenStr(reinterpret_cast<const char*>(tokensData.data() + offset), len);
        offset += len;

        SentencePieceToken piece;
        piece.piece = std::move(tokenStr);
        piece.score = 0.0f;
        piece.id    = i;
        piece.type  = SentencePieceToken::NORMAL;

        // Special token detection
        if (piece.piece == "<s>"   || piece.piece == "<|begin_of_text|>") m_bosId = i;
        else if (piece.piece == "</s>"  || piece.piece == "<|end_of_text|>")  m_eosId = i;
        else if (piece.piece == "<unk>") m_unkId = i;
        else if (piece.piece == "<pad>") m_padId = i;

        m_pieceToId[piece.piece] = i;
        m_pieces.push_back(std::move(piece));
    }

    // Load scores if available
    auto scoresIt = metadata.find("tokenizer.ggml.scores");
    if (scoresIt != metadata.end()) {
        const auto& scoresData = scoresIt->second;
        size_t maxScores = std::min(scoresData.size() / 4, m_pieces.size());
        for (size_t i = 0; i < maxScores; ++i) {
            float score = 0.0f;
            std::memcpy(&score, scoresData.data() + i * 4, 4);
            m_pieces[i].score = score;
        }
    }

    buildTrie();
    std::cout << "[SentencePiece] Loaded " << m_pieces.size() << " pieces from GGUF metadata\n";
    return true;
}

// ============================================================================
// Trie Construction & Prefix Matching
// ============================================================================

void SentencePieceTokenizer::buildTrie()
{
    m_trie = std::make_unique<SPTrieNode>();  // Reset
    for (const auto& piece : m_pieces) {
        std::u32string u32 = utf8ToUtf32(piece.piece);
        insertTrie(u32, piece.id);
    }
}

void SentencePieceTokenizer::insertTrie(const std::u32string& piece, int32_t id)
{
    SPTrieNode* node = m_trie.get();
    for (char32_t ch : piece) {
        auto it = node->children.find(ch);
        if (it == node->children.end()) {
            node->children[ch] = std::make_unique<SPTrieNode>();
            it = node->children.find(ch);
        }
        node = it->second.get();
    }
    node->tokenId = id;
}

std::vector<int32_t> SentencePieceTokenizer::findMatchingPieces(
    const std::u32string& text, int pos)
{
    std::vector<int32_t> matches;
    SPTrieNode* node = m_trie.get();

    for (size_t i = static_cast<size_t>(pos); i < text.length(); ++i) {
        char32_t ch = text[i];
        auto it = node->children.find(ch);
        if (it == node->children.end()) break;

        node = it->second.get();
        if (node->tokenId >= 0) {
            matches.push_back(node->tokenId);
        }
    }

    return matches;
}

// ============================================================================
// Normalization
// ============================================================================

std::string SentencePieceTokenizer::normalize(const std::string& text)
{
    std::string result;
    result.reserve(text.size());

    bool lastWasSpace = false;
    for (char c : text) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        if (c == ' ' && lastWasSpace) continue;
        result.push_back(c);
        lastWasSpace = (c == ' ');
    }

    // Trim leading/trailing spaces
    size_t start = result.find_first_not_of(' ');
    size_t end   = result.find_last_not_of(' ');
    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

// Replace space with U+2581 (Lower One Eighth Block = SentencePiece space marker)
std::u32string SentencePieceTokenizer::replaceSP(const std::u32string& text)
{
    std::u32string result;
    result.reserve(text.size());
    for (char32_t c : text) {
        result.push_back(c == U' ' ? U'\u2581' : c);
    }
    return result;
}

std::u32string SentencePieceTokenizer::unreplaceSP(const std::u32string& text)
{
    std::u32string result;
    result.reserve(text.size());
    for (char32_t c : text) {
        result.push_back(c == U'\u2581' ? U' ' : c);
    }
    return result;
}

// ============================================================================
// Lattice Construction (Viterbi preparation)
// ============================================================================

std::unique_ptr<SPLattice> SentencePieceTokenizer::buildLattice(const std::u32string& text)
{
    auto lattice = std::make_unique<SPLattice>(text);

    for (size_t pos = 0; pos < text.length(); ++pos) {
        if (lattice->nodes[pos].empty()) continue;

        // Find all pieces that can start at this position
        std::vector<int32_t> matches = findMatchingPieces(text, static_cast<int>(pos));

        for (int32_t tokenId : matches) {
            const SentencePieceToken& piece = m_pieces[static_cast<size_t>(tokenId)];
            std::u32string u32piece = utf8ToUtf32(piece.piece);
            size_t endPos = pos + u32piece.length();

            if (endPos > text.length()) continue;

            for (const SPLatticeNode& prevNode : lattice->nodes[pos]) {
                SPLatticeNode newNode;
                newNode.pos         = static_cast<int>(endPos);
                newNode.score       = prevNode.score + piece.score;
                newNode.backPointer = static_cast<int>(pos);
                newNode.tokenId     = tokenId;

                lattice->nodes[endPos].push_back(newNode);
            }
        }

        // Byte fallback for unknown characters
        if (m_byteFallback && pos + 1 <= text.length() &&
            lattice->nodes[pos + 1].empty()) {
            SPLatticeNode byteNode;
            byteNode.pos         = static_cast<int>(pos + 1);
            byteNode.score       = lattice->nodes[pos].front().score - 10.0f;
            byteNode.backPointer = static_cast<int>(pos);
            byteNode.tokenId     = m_unkId;
            lattice->nodes[pos + 1].push_back(byteNode);
        }
    }

    return lattice;
}

// ============================================================================
// Viterbi Decoding — Find optimal tokenization path
// ============================================================================

std::vector<int32_t> SentencePieceTokenizer::viterbi(std::unique_ptr<SPLattice> lattice)
{
    std::vector<int32_t> result;
    size_t endPos = lattice->text.length();

    if (lattice->nodes[endPos].empty()) {
        std::cerr << "[SentencePiece] No valid tokenization found\n";
        return result;
    }

    // Find best ending node (highest cumulative score)
    const SPLatticeNode* bestEnd = &lattice->nodes[endPos].front();
    for (const auto& node : lattice->nodes[endPos]) {
        if (node.score > bestEnd->score) {
            bestEnd = &node;
        }
    }

    // Backtrack through lattice to collect token IDs
    std::vector<int32_t> reversed;
    int pos = static_cast<int>(endPos);

    // Simple greedy backtrack using backPointer chain
    while (pos > 0) {
        // Find the best node at this position
        const SPLatticeNode* current = nullptr;
        float bestScore = -std::numeric_limits<float>::infinity();

        for (const auto& node : lattice->nodes[static_cast<size_t>(pos)]) {
            if (node.score > bestScore) {
                bestScore = node.score;
                current = &node;
            }
        }

        if (!current || current->backPointer < 0) break;

        if (current->tokenId >= 0) {
            reversed.push_back(current->tokenId);
        }

        pos = current->backPointer;
    }

    // Reverse to get correct order
    result.assign(reversed.rbegin(), reversed.rend());
    return result;
}

// ============================================================================
// encode — Full tokenization pipeline
// ============================================================================

std::vector<int32_t> SentencePieceTokenizer::encode(
    const std::string& text, bool addBos, bool addEos)
{
    if (!isReady()) {
        std::cerr << "[SentencePiece] Not initialized\n";
        return {};
    }

    std::vector<int32_t> result;

    if (addBos) {
        result.push_back(m_bosId);
    }

    // Normalize and preprocess
    std::string normalized = normalize(text);
    std::u32string u32text = utf8ToUtf32(" " + normalized);  // Add leading space
    std::u32string withSP  = replaceSP(u32text);

    // Build lattice and run Viterbi
    auto lattice = buildLattice(withSP);
    std::vector<int32_t> tokens = viterbi(std::move(lattice));

    result.insert(result.end(), tokens.begin(), tokens.end());

    if (addEos) {
        result.push_back(m_eosId);
    }

    return result;
}

// ============================================================================
// decode — Token IDs back to string
// ============================================================================

std::string SentencePieceTokenizer::decode(
    const std::vector<int32_t>& tokens, bool skipSpecial)
{
    if (!isReady()) return "";

    std::u32string result32;

    for (int32_t tokenId : tokens) {
        if (tokenId < 0 || static_cast<size_t>(tokenId) >= m_pieces.size()) {
            std::cerr << "[SentencePiece] Invalid token ID: " << tokenId << "\n";
            continue;
        }

        // Skip special tokens if requested
        if (skipSpecial) {
            if (tokenId == m_bosId || tokenId == m_eosId ||
                tokenId == m_padId || tokenId == m_unkId) {
                continue;
            }
        }

        std::u32string pieceU32 = utf8ToUtf32(m_pieces[static_cast<size_t>(tokenId)].piece);
        result32 += pieceU32;
    }

    // Replace U+2581 with spaces
    std::u32string decoded = unreplaceSP(result32);

    // Convert back to UTF-8 and trim
    std::string result = utf32ToUtf8(decoded);

    // Trim leading/trailing spaces
    size_t start = result.find_first_not_of(' ');
    size_t end   = result.find_last_not_of(' ');
    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}
