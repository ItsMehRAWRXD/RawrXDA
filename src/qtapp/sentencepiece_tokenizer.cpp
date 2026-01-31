#include "sentencepiece_tokenizer.hpp"


#include <algorithm>
#include <cmath>
#include <limits>

// Production-ready SentencePiece with proper library
#ifdef USE_SENTENCEPIECE
#include <sentencepiece_processor.h>
#include <sentencepiece_trainer.h>
#endif

// Simple lattice structure for Viterbi algorithm
struct SentencePieceTokenizer::Lattice {
    struct Node {
        int pos;                              // Position in text
        float score;                          // Cumulative score
        int backPointer;                      // Previous node
        int32_t tokenId;                      // Token at this position
    };
    
    std::vector<std::vector<Node>> nodes;  // nodes[pos] = possible tokens starting at pos
    std::string text;
    
    Lattice(const std::string& t) : text(t) {
        nodes.resize(t.length() + 1);
        // Initialize start node
        Node start{0, 0.0f, -1, -1};
        nodes[0].append(start);
    }
};

SentencePieceTokenizer::SentencePieceTokenizer() {
    m_trie = nullptr;
}

void SentencePieceTokenizer::initialize() {
    if (m_trie) return;  // Already initialized
    m_trie = new TrieNode();
}

SentencePieceTokenizer::~SentencePieceTokenizer() {
    if (m_trie) {
        delete m_trie;
        m_trie = nullptr;
    }
}

bool SentencePieceTokenizer::loadFromFile(const std::string& modelPath) {
#ifdef USE_SENTENCEPIECE
    // Production-ready implementation using SentencePiece library
    m_spProcessor = std::make_unique<sentencepiece::SentencePieceProcessor>();
    
    const auto status = m_spProcessor->Load(modelPath.toStdString());
    if (!status.ok()) {
                   << "Error:" << std::string::fromStdString(status.ToString());
        m_spProcessor.reset();
        return false;
    }
    
    // Load vocabulary into our internal structures for compatibility
    const int vocabSize = m_spProcessor->GetPieceSize();
    m_pieces.reserve(vocabSize);
    m_tokenIdToString.clear();
    m_stringToTokenId.clear();
    
    for (int i = 0; i < vocabSize; ++i) {
        const std::string piece = m_spProcessor->IdToPiece(i);
        const float score = m_spProcessor->GetScore(i);
        const bool isControl = m_spProcessor->IsControl(i);
        const bool isUnused = m_spProcessor->IsUnused(i);
        const bool isByte = m_spProcessor->IsByte(i);
        
        TokenPiece tp;
        tp.piece = std::string::fromStdString(piece);
        tp.score = score;
        tp.type = isControl ? TokenType::Control :
                  isUnused ? TokenType::Unused :
                  isByte ? TokenType::Byte :
                  TokenType::Normal;
        
        m_pieces.append(tp);
        m_tokenIdToString[i] = tp.piece;
        m_stringToTokenId[tp.piece] = i;
    }
    
    buildTrie();
    return true;
    
#else
    // Fallback: simplified protobuf parser (not production-ready)
    std::fstream file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    // Simplified parser - reads basic model structure
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Skip header, read pieces count
    file.seek(16);
    int32_t numPieces;
    stream >> numPieces;
    
    m_pieces.reserve(numPieces);
    
    for (int32_t i = 0; i < numPieces; ++i) {
        quint32 pieceLen;
        stream >> pieceLen;
        
        std::vector<uint8_t> pieceBytes(pieceLen, //Uninitialized);
        stream.readRawData(pieceBytes.data(), pieceLen);
        
        float score;
        stream >> score;
        
        SentencePiece piece;
        piece.piece = std::string::fromUtf8(pieceBytes);
        piece.score = score;
        piece.id = i;
        piece.type = SentencePiece::NORMAL;
        
        // Detect special tokens
        if (piece.piece == "<s>") m_bosId = i;
        else if (piece.piece == "</s>") m_eosId = i;
        else if (piece.piece == "<unk>") m_unkId = i;
        else if (piece.piece == "<pad>") m_padId = i;
        
        m_pieces.append(piece);
        m_pieceToId[piece.piece] = i;
    }
    
    buildTrie();
    
    return true;
#endif
}

bool SentencePieceTokenizer::loadFromGGUFMetadata(const std::unordered_map<std::string, std::vector<uint8_t>>& metadata) {
    // Load from GGUF tokenizer metadata
    if (metadata.contains("tokenizer.ggml.tokens")) {
        std::vector<uint8_t> tokensData = metadata["tokenizer.ggml.tokens"];
        QDataStream stream(tokensData);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        int32_t numTokens;
        stream >> numTokens;
        
        m_pieces.reserve(numTokens);
        
        for (int32_t i = 0; i < numTokens; ++i) {
            quint32 len;
            stream >> len;
            
            std::vector<uint8_t> tokenBytes(len, //Uninitialized);
            stream.readRawData(tokenBytes.data(), len);
            
            SentencePiece piece;
            piece.piece = std::string::fromUtf8(tokenBytes);
            piece.score = 0.0f;  // Default score if not provided
            piece.id = i;
            piece.type = SentencePiece::NORMAL;
            
            // Check for special tokens
            if (piece.piece == "<s>" || piece.piece == "<|begin_of_text|>") m_bosId = i;
            else if (piece.piece == "</s>" || piece.piece == "<|end_of_text|>") m_eosId = i;
            else if (piece.piece == "<unk>") m_unkId = i;
            
            m_pieces.append(piece);
            m_pieceToId[piece.piece] = i;
        }
        
        // Load scores if available
        if (metadata.contains("tokenizer.ggml.scores")) {
            std::vector<uint8_t> scoresData = metadata["tokenizer.ggml.scores"];
            QDataStream scoreStream(scoresData);
            scoreStream.setByteOrder(QDataStream::LittleEndian);
            
            for (int i = 0; i < m_pieces.size(); ++i) {
                float score;
                scoreStream >> score;
                m_pieces[i].score = score;
            }
        }
        
        buildTrie();
        return true;
    }
    
    return false;
}

void SentencePieceTokenizer::buildTrie() {
    initialize();  // Ensure trie is allocated
    for (const SentencePiece& piece : m_pieces) {
        insertTrie(piece.piece, piece.id);
    }
}

void SentencePieceTokenizer::insertTrie(const std::string& piece, int32_t id) {
    TrieNode* node = m_trie;
    for (QChar ch : piece) {
        if (!node->children.contains(ch)) {
            node->children[ch] = new TrieNode();
        }
        node = node->children[ch];
    }
    node->tokenId = id;
}

std::vector<int32_t> SentencePieceTokenizer::findMatchingPieces(const std::string& text, int pos) {
    std::vector<int32_t> matches;
    TrieNode* node = m_trie;
    
    for (int i = pos; i < text.length(); ++i) {
        QChar ch = text[i];
        if (!node->children.contains(ch)) break;
        
        node = node->children[ch];
        if (node->tokenId >= 0) {
            matches.append(node->tokenId);
        }
    }
    
    return matches;
}

std::string SentencePieceTokenizer::normalize(const std::string& text) {
    // Basic NFKC normalization (simplified)
    std::string result = text;
    
    // Replace various whitespace with standard space
    result.replace(std::regex("[\\t\\n\\r]+"), " ");
    
    // Trim
    result = result.trimmed();
    
    return result;
}

std::string SentencePieceTokenizer::replaceSP(const std::string& text) {
    // Replace space with ▁ (U+2581)
    std::string result = text;
    result.replace(' ', QChar(0x2581));
    return result;
}

std::string SentencePieceTokenizer::unreplaceSP(const std::string& text) {
    // Replace ▁ with space
    std::string result = text;
    result.replace(QChar(0x2581), ' ');
    return result;
}

SentencePieceTokenizer::Lattice* SentencePieceTokenizer::buildLattice(const std::string& text) {
    Lattice* lattice = new Lattice(text);
    
    for (int pos = 0; pos < text.length(); ++pos) {
        if (lattice->nodes[pos].isEmpty()) continue;
        
        // Find all pieces that can start at this position
        std::vector<int32_t> matches = findMatchingPieces(text, pos);
        
        for (int32_t tokenId : matches) {
            const SentencePiece& piece = m_pieces[tokenId];
            int endPos = pos + piece.piece.length();
            
            if (endPos > text.length()) continue;
            
            // Add node to lattice
            for (const Lattice::Node& prevNode : lattice->nodes[pos]) {
                Lattice::Node newNode;
                newNode.pos = endPos;
                newNode.score = prevNode.score + piece.score;
                newNode.backPointer = pos;
                newNode.tokenId = tokenId;
                
                lattice->nodes[endPos].append(newNode);
            }
        }
        
        // Byte fallback for unknown characters
        if (m_byteFallback && lattice->nodes[pos + 1].isEmpty() && pos + 1 <= text.length()) {
            uint8_t byte = text[pos].toLatin1();
            
            Lattice::Node byteNode;
            byteNode.pos = pos + 1;
            byteNode.score = lattice->nodes[pos].first().score - 10.0f;  // Penalty
            byteNode.backPointer = pos;
            byteNode.tokenId = m_unkId;
            
            lattice->nodes[pos + 1].append(byteNode);
        }
    }
    
    return lattice;
}

std::vector<int32_t> SentencePieceTokenizer::viterbi(Lattice* lattice) {
    std::vector<int32_t> result;
    
    int endPos = lattice->text.length();
    if (lattice->nodes[endPos].isEmpty()) {
        delete lattice;
        return result;
    }
    
    // Find best path (highest score)
    const Lattice::Node* bestEnd = &lattice->nodes[endPos].first();
    for (const Lattice::Node& node : lattice->nodes[endPos]) {
        if (node.score > bestEnd->score) {
            bestEnd = &node;
        }
    }
    
    // Backtrack to collect tokens
    std::vector<int32_t> reversed;
    int pos = endPos;
    
    while (pos > 0) {
        const Lattice::Node* current = nullptr;
        
        for (const Lattice::Node& node : lattice->nodes[pos]) {
            if (&node == bestEnd || node.backPointer == bestEnd->backPointer) {
                current = &node;
                break;
            }
        }
        
        if (!current) break;
        
        if (current->tokenId >= 0) {
            reversed.push_back(current->tokenId);
        }
        
        pos = current->backPointer;
        if (pos >= 0 && !lattice->nodes[pos].isEmpty()) {
            bestEnd = &lattice->nodes[pos].first();
        }
    }
    
    // Reverse to get correct order
    result.assign(reversed.rbegin(), reversed.rend());
    
    delete lattice;
    return result;
}

std::vector<int32_t> SentencePieceTokenizer::encode(const std::string& text, bool addBos, bool addEos) {
    if (!isReady()) {
        return {};
    }
    
    std::vector<int32_t> result;
    
    if (addBos) {
        result.push_back(m_bosId);
    }
    
    // Normalize and preprocess
    std::string normalized = normalize(text);
    std::string withSP = replaceSP(" " + normalized);  // Add leading space
    
    // Build lattice and find best tokenization
    Lattice* lattice = buildLattice(withSP);
    std::vector<int32_t> tokens = viterbi(lattice);
    
    result.insert(result.end(), tokens.begin(), tokens.end());
    
    if (addEos) {
        result.push_back(m_eosId);
    }
    
    return result;
}

std::string SentencePieceTokenizer::decode(const std::vector<int32_t>& tokens, bool skipSpecial) {
    if (!isReady()) return std::string();
    
    std::string result;
    
    for (int32_t tokenId : tokens) {
        if (tokenId < 0 || tokenId >= m_pieces.size()) {
            continue;
        }
        
        const SentencePiece& piece = m_pieces[tokenId];
        
        // Skip special tokens if requested
        if (skipSpecial) {
            if (tokenId == m_bosId || tokenId == m_eosId || 
                tokenId == m_padId || tokenId == m_unkId) {
                continue;
            }
        }
        
        result += piece.piece;
    }
    
    // Replace ▁ with spaces
    result = unreplaceSP(result);
    
    return result.trimmed();
}

