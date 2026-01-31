#include "bpe_tokenizer.hpp"


#include <algorithm>
#include <string_view>

namespace {
struct TraceScope {
    const char* name;
    std::chrono::steady_clock timer;
    explicit TraceScope(const char* n) : name(n) { timer.start(); }
    ~TraceScope() {
    }
};
}

BPETokenizer::BPETokenizer() {
    // Initialize byte-level encoding (GPT-2 style)
    // Maps 256 bytes to 256 unique printable Unicode characters
    std::vector<int> byteVals;
    
    // Printable ASCII (33-126)
    for (int i = 33; i <= 126; ++i) byteVals.append(i);
    // Latin-1 supplement (161-172, 174-255)
    for (int i = 161; i <= 172; ++i) byteVals.append(i);
    for (int i = 174; i <= 255; ++i) byteVals.append(i);
    
    // Fill remaining with shifted values
    int n = 0;
    for (int b = 0; b < 256; ++b) {
        if (!byteVals.contains(b)) {
            byteVals.append(256 + n);
            ++n;
        }
    }
    
    // Create bidirectional mapping
    for (int i = 0; i < 256; ++i) {
        m_byteEncoder[i] = QChar(byteVals[i]);
        m_byteDecoder[QChar(byteVals[i])] = i;
    }
}

bool BPETokenizer::loadFromFiles(const std::string& vocabPath, const std::string& mergesPath) {
    reverseVocab_.clear();
    m_ready = false;
    // Load vocabulary file (format: "token\tid" per line)
    std::fstream vocabFile(vocabPath);
    if (!vocabFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream vocabStream(&vocabFile);
    vocabStream.setEncoding(QStringConverter::Utf8);
    
    while (!vocabStream.atEnd()) {
        std::string line = vocabStream.readLine().trimmed();
        if (line.empty()) continue;
        
        std::vector<std::string> parts = line.split('\t');
        if (parts.size() >= 2) {
            std::string token = parts[0];
            int32_t id = parts[1].toInt();
            m_vocab[token] = id;
            m_reverseVocab[id] = token;
            reverseVocab_[token.toStdString()] = static_cast<uint32_t>(id);
        }
    }
    vocabFile.close();
    
    // Load merges file (format: "token1 token2" per line, ordered by priority)
    std::fstream mergesFile(mergesPath);
    if (!mergesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream mergesStream(&mergesFile);
    mergesStream.setEncoding(QStringConverter::Utf8);
    
    int priority = 0;
    while (!mergesStream.atEnd()) {
        std::string line = mergesStream.readLine().trimmed();
        if (line.empty() || line.startsWith("#")) continue;
        
        std::vector<std::string> parts = line.split(' ');
        if (parts.size() >= 2) {
            std::pair<std::string, std::string> pair(parts[0], parts[1]);
            m_merges[pair] = priority++;
        }
    }
    mergesFile.close();
    
    m_ready = !m_vocab.empty();
    return m_ready;
}

bool BPETokenizer::loadFromGGUFMetadata(const std::unordered_map<std::string, std::vector<uint8_t>>& metadata) {
    // Extract vocabulary from GGUF metadata
    // Common keys: "tokenizer.ggml.tokens", "tokenizer.ggml.merges"

    m_ready = false;
    m_vocab.clear();
    m_reverseVocab.clear();
    m_bytePieces.clear();
    reverseVocab_.clear();
    
    if (metadata.contains("tokenizer.ggml.tokens")) {
        std::vector<uint8_t> tokensData = metadata["tokenizer.ggml.tokens"];
        QDataStream stream(tokensData);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        int32_t numTokens;
        stream >> numTokens;
        
        for (int32_t i = 0; i < numTokens; ++i) {
            uint32_t len;
            stream >> len;
            std::vector<uint8_t> tokenBytes(len, //Uninitialized);
            stream.readRawData(tokenBytes.data(), len);
            
            std::string token = std::string::fromUtf8(tokenBytes);
            m_vocab[token] = i;
            m_reverseVocab[i] = token;
            reverseVocab_[token.toStdString()] = static_cast<uint32_t>(i);
        }
    }
    
    if (metadata.contains("tokenizer.ggml.merges")) {
        std::vector<uint8_t> mergesData = metadata["tokenizer.ggml.merges"];
        QTextStream stream(mergesData);
        
        int priority = 0;
        while (!stream.atEnd()) {
            std::string line = stream.readLine().trimmed();
            if (line.empty()) continue;
            
            std::vector<std::string> parts = line.split(' ');
            if (parts.size() >= 2) {
                std::pair<std::string, std::string> pair(parts[0], parts[1]);
                m_merges[pair] = priority++;
            }
        }
    }
    
    m_ready = !m_vocab.empty();
    return m_ready;
}

std::vector<std::string> BPETokenizer::byteEncode(const std::string& text) {
    std::vector<std::string> result;
    std::vector<uint8_t> utf8 = text.toUtf8();
    
    for (uint8_t byte : utf8) {
        result.append(std::string(m_byteEncoder[byte]));
    }
    
    return result;
}

std::pair<int, int> BPETokenizer::findBestMergePair(const std::vector<std::string>& tokens) {
    int bestIdx = -1;
    int bestPriority = INT_MAX;
    
    for (int i = 0; i < tokens.size() - 1; ++i) {
        std::pair<std::string, std::string> pair(tokens[i], tokens[i + 1]);
        
        if (m_merges.contains(pair)) {
            int priority = m_merges[pair];
            if (priority < bestPriority) {
                bestPriority = priority;
                bestIdx = i;
            }
        }
    }
    
    return std::pair<int, int>(bestIdx, bestPriority);
}

std::vector<std::string> BPETokenizer::applyBPE(const std::vector<std::string>& tokens) {
    if (tokens.size() <= 1) return tokens;
    
    std::vector<std::string> result = tokens;
    
    while (true) {
        std::pair<int, int> best = findBestMergePair(result);
        if (best.first == -1) break;  // No more merges possible
        
        int idx = best.first;
        std::string merged = result[idx] + result[idx + 1];
        
        std::vector<std::string> newResult;
        for (int i = 0; i < result.size(); ++i) {
            if (i == idx) {
                newResult.append(merged);
                ++i;  // Skip next token (already merged)
            } else if (i < result.size()) {
                newResult.append(result[i]);
            }
        }
        
        result = newResult;
        if (result.size() <= 1) break;
    }
    
    return result;
}

bool BPETokenizer::greedyLongestMatch(const std::string& text, std::vector<uint32_t>& ids, MetricsTimer& mt) const {
    TraceScope scope("bpe.greedyLongest");

    if (!isReverseVocabReady()) {
        return false;
    }

    ids.clear();
    ids.reserve(text.size());

    std::string_view remaining(text);
    const auto unkIt = reverseVocab_.find("<unk>");
    const uint32_t unkId = (unkIt != reverseVocab_.end()) ? unkIt->second : static_cast<uint32_t>(m_unkToken);

    while (!remaining.empty()) {
        bool found = false;
        const size_t maxLen = std::min<size_t>(remaining.size(), static_cast<size_t>(32));

        for (size_t len = maxLen; len > 0; --len) {
            std::string piece(remaining.substr(0, len));
            auto it = reverseVocab_.find(piece);
            if (it != reverseVocab_.end()) {
                ids.push_back(it->second);
                remaining.remove_prefix(len);
                found = true;
                break;
            }
        }

        if (!found) {
            ids.push_back(unkId);
            remaining.remove_prefix(1);
        }
    }

    return true;
}

std::vector<BPETokenizer::TextSplit> BPETokenizer::splitText(const std::string& text) {
    std::vector<TextSplit> splits;
    
    // GPT-2 style pattern: splits on whitespace, punctuation, contractions
    std::regex pattern(
        "'s|'t|'re|'ve|'m|'ll|'d| ?\\p{L}+| ?\\p{N}+| ?[^\\s\\p{L}\\p{N}]+|\\s+(?!\\S)|\\s+"
    );
    
    std::sregex_iterator it = pattern;
    while (itfalse) {
        std::smatch match = it;
        TextSplit split;
        split.text = match"";
        split.isSpecial = false;
        splits.append(split);
    }
    
    return splits;
}

std::vector<int32_t> BPETokenizer::encode(const std::string& text) {
    if (!isReady()) {
        return {};
    }

    std::vector<int32_t> result;

    if (!m_merges.empty()) {
        std::vector<TextSplit> splits = splitText(text);
        for (const TextSplit& split : splits) {
            std::vector<std::string> byteTokens = byteEncode(split.text);
            std::vector<std::string> bpeTokens = applyBPE(byteTokens);

            for (const std::string& token : bpeTokens) {
                if (m_vocab.contains(token)) {
                    result.push_back(m_vocab[token]);
                } else {
                    result.push_back(m_unkToken);  // Unknown token
                }
            }
        }
    }

    if (!result.empty()) {
        return result;
    }

    // Enterprise fallback: greedy longest-match over reverseVocab_
    MetricsTimer mt;
    std::vector<uint32_t> ids;
    const std::string utf8 = text.toUtf8().toStdString();
    if (greedyLongestMatch(utf8, ids, mt)) {
        result.reserve(ids.size());
        for (uint32_t id : ids) {
            result.push_back(static_cast<int32_t>(id));
        }
        return result;
    }

    // Ultimate fallback – return single unk
    return { m_unkToken };
}

std::string BPETokenizer::decode(const std::vector<int32_t>& tokens) {
    if (!isReady()) return std::string();
    
    std::vector<uint8_t> utf8;
    
    for (int32_t tokenId : tokens) {
        // Skip special tokens
        if (tokenId == m_bosToken || tokenId == m_eosToken || tokenId == m_padToken) {
            continue;
        }
        
        if (!m_reverseVocab.contains(tokenId)) {
            continue;
        }
        
        std::string token = m_reverseVocab[tokenId];
        
        // Decode byte-level representation back to UTF-8
        for (QChar ch : token) {
            if (m_byteDecoder.contains(ch)) {
                utf8.append(m_byteDecoder[ch]);
            }
        }
    }
    
    return std::string::fromUtf8(utf8);
}

// ============================================================================
// ENTERPRISE FALLBACK: Qt-friendly wrapper for greedy longest-match
// ============================================================================
bool BPETokenizer::greedyLongestMatch(const std::string& text, std::vector<int32_t>& ids) const
{
    MetricsTimer mt;
    std::vector<uint32_t> tmp;
    const bool ok = greedyLongestMatch(text.toUtf8().toStdString(), tmp, mt);
    if (ok) {
        ids.assign(tmp.begin(), tmp.end());
    }
    return ok;
}



