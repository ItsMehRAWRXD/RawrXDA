#include "bpe_tokenizer.hpp"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

BPETokenizer::BPETokenizer() {
    // Initialize byte-level encoding (GPT-2 style)
    // Maps 256 bytes to 256 unique printable Unicode characters
    QVector<int> byteVals;
    
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

bool BPETokenizer::loadFromFiles(const QString& vocabPath, const QString& mergesPath) {
    // Load vocabulary file (format: "token\tid" per line)
    QFile vocabFile(vocabPath);
    if (!vocabFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open vocab file:" << vocabPath;
        return false;
    }
    
    QTextStream vocabStream(&vocabFile);
    vocabStream.setEncoding(QStringConverter::Utf8);
    
    while (!vocabStream.atEnd()) {
        QString line = vocabStream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QStringList parts = line.split('\t');
        if (parts.size() >= 2) {
            QString token = parts[0];
            int32_t id = parts[1].toInt();
            m_vocab[token] = id;
            m_reverseVocab[id] = token;
        }
    }
    vocabFile.close();
    
    // Load merges file (format: "token1 token2" per line, ordered by priority)
    QFile mergesFile(mergesPath);
    if (!mergesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open merges file:" << mergesPath;
        return false;
    }
    
    QTextStream mergesStream(&mergesFile);
    mergesStream.setEncoding(QStringConverter::Utf8);
    
    int priority = 0;
    while (!mergesStream.atEnd()) {
        QString line = mergesStream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        QStringList parts = line.split(' ');
        if (parts.size() >= 2) {
            QPair<QString, QString> pair(parts[0], parts[1]);
            m_merges[pair] = priority++;
        }
    }
    mergesFile.close();
    
    qInfo() << "BPE tokenizer loaded:" << m_vocab.size() << "tokens," << m_merges.size() << "merges";
    return true;
}

bool BPETokenizer::loadFromGGUFMetadata(const QHash<QString, QByteArray>& metadata) {
    // Extract vocabulary from GGUF metadata
    // Common keys: "tokenizer.ggml.tokens", "tokenizer.ggml.merges"

    // Tokens are stored as a \0-delimited blob; split and index by position
    m_ready = false;
    m_vocab.clear();
    m_reverseVocab.clear();
    m_bytePieces.clear();

    // Extract vocabulary from GGUF metadata (null-delimited blob)
    if (metadata.contains("tokenizer.ggml.tokens")) {
        const QByteArray tokensData = metadata.value("tokenizer.ggml.tokens");
        const QList<QByteArray> tokenList = tokensData.split('\0');
        int32_t idx = 0;
        m_bytePieces.reserve(tokenList.size());
        for (const QByteArray& raw : tokenList) {
            if (raw.isEmpty()) {
                continue;
            }
            const QString token = QString::fromUtf8(raw);
            m_vocab[token] = idx;
            m_reverseVocab[idx] = token;
            m_bytePieces.append(token);
            ++idx;
        }
    }

    // Build byte fallback cache (GPT-2 style control bytes)
    for (int i = 0; i < m_bytePieces.size(); ++i) {
        const QString& piece = m_bytePieces[i];
        if (piece.size() == 1) {
            const uint8_t b = static_cast<uint8_t>(piece.at(0).toLatin1());
            if (b < 0x20 || b == 0x7f) {
                QByteArray ba;
                ba.append(static_cast<char>(b));
                m_bytePieces[i] = QString::fromUtf8(ba);
            }
        }
    }

    // Merges are optional for decoding; parse if present
    if (metadata.contains("tokenizer.ggml.merges")) {
        QByteArray mergesData = metadata.value("tokenizer.ggml.merges");
        QTextStream stream(&mergesData, QIODevice::ReadOnly);

        int priority = 0;
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;

            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                m_merges[qMakePair(parts[0], parts[1])] = priority++;
            }
        }
    }

    m_ready = !m_vocab.isEmpty();
    qInfo() << "BPE loaded from GGUF:" << m_vocab.size() << "tokens" << ", merges:" << m_merges.size();
    return m_ready;
}

QVector<QString> BPETokenizer::byteEncode(const QString& text) {
    QVector<QString> result;
    QByteArray utf8 = text.toUtf8();
    
    for (uint8_t byte : utf8) {
        result.append(QString(m_byteEncoder[byte]));
    }
    
    return result;

// ============================================================================
// ENTERPRISE FALLBACK: Greedy Longest-Match Tokenizer
// ============================================================================
// SentencePiece-compatible greedy algorithm that handles unknown bytes gracefully.
// No complex BPE merge table required - uses direct vocabulary lookup.
// ============================================================================
bool BPETokenizer::greedyLongestMatch(const QString& text, std::vector<int32_t>& ids) const
{
    ids.clear();
    if (text.isEmpty()) {
        return true;
    }
    
    // Reserve space for efficiency
    ids.reserve(text.length());
    
    // Convert to UTF-8 for byte-level processing
    QByteArray utf8Data = text.toUtf8();
    std::string remaining(utf8Data.constData(), utf8Data.size());
    
    int processedBytes = 0;
    int unknownBytes = 0;
    
    while (!remaining.empty()) {
        bool found = false;
        // Try longest possible slice first (up to 32 bytes for efficiency)
        size_t maxLen = std::min(remaining.size(), size_t(32));
        
        for (size_t len = maxLen; len > 0; --len) {
            QString piece = QString::fromUtf8(remaining.data(), static_cast<int>(len));
            
            // Check if this piece exists in vocabulary
            auto it = m_vocab.find(piece);
            if (it != m_vocab.end()) {
                ids.push_back(it.value());
                remaining.erase(0, len);
                processedBytes += len;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Unknown byte → emit <unk> token and skip one byte
            ids.push_back(m_unkToken);
            remaining.erase(0, 1);
            unknownBytes++;
        }
    }
    
    // Enterprise logging
    qInfo() << "[BPE::greedyLongestMatch] text_len=" << text.length() 
            << " tokens=" << ids.size() 
            << " processed_bytes=" << processedBytes
            << " unknown_bytes=" << unknownBytes;
    
    return true;
}
}

QPair<int, int> BPETokenizer::findBestMergePair(const QVector<QString>& tokens) {
    int bestIdx = -1;
    int bestPriority = INT_MAX;
    
    for (int i = 0; i < tokens.size() - 1; ++i) {
        QPair<QString, QString> pair(tokens[i], tokens[i + 1]);
        
        if (m_merges.contains(pair)) {
            int priority = m_merges[pair];
            if (priority < bestPriority) {
                bestPriority = priority;
                bestIdx = i;
            }
        }
    }
    
    return QPair<int, int>(bestIdx, bestPriority);
}

QVector<QString> BPETokenizer::applyBPE(const QVector<QString>& tokens) {
    if (tokens.size() <= 1) return tokens;
    
    QVector<QString> result = tokens;
    
    while (true) {
        QPair<int, int> best = findBestMergePair(result);
        if (best.first == -1) break;  // No more merges possible
        
        int idx = best.first;
        QString merged = result[idx] + result[idx + 1];
        
        QVector<QString> newResult;
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

QVector<BPETokenizer::TextSplit> BPETokenizer::splitText(const QString& text) {
    QVector<TextSplit> splits;
    
    // GPT-2 style pattern: splits on whitespace, punctuation, contractions
    QRegularExpression pattern(
        "'s|'t|'re|'ve|'m|'ll|'d| ?\\p{L}+| ?\\p{N}+| ?[^\\s\\p{L}\\p{N}]+|\\s+(?!\\S)|\\s+"
    );
    
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        TextSplit split;
        split.text = match.captured(0);
        split.isSpecial = false;
        splits.append(split);
    }
    
    return splits;
}

std::vector<int32_t> BPETokenizer::encode(const QString& text) {
    if (!isReady()) {
        qWarning() << "[BPE::encode] Tokenizer not ready";
        return {};
        // ---------- ENTERPRISE FALLBACK: Use greedy longest-match if no merges ----------
        std::vector<int32_t> ids;
        if (m_merges.isEmpty()) {
            qInfo() << "[BPE::encode] Using greedy longest-match fallback (no merges available)";
            greedyLongestMatch(text, ids);
            return ids;
        }
    
    }
    
    std::vector<int32_t> result;
    QVector<TextSplit> splits = splitText(text);
    
    for (const TextSplit& split : splits) {
        // Byte-level encode the text chunk
        QVector<QString> byteTokens = byteEncode(split.text);
        
        // Apply BPE merges
        QVector<QString> bpeTokens = applyBPE(byteTokens);
        
        // Convert to token IDs
        for (const QString& token : bpeTokens) {
            if (m_vocab.contains(token)) {
                result.push_back(m_vocab[token]);
            } else {
                result.push_back(m_unkToken);  // Unknown token
            }
        }
    }
    
    return result;
}

QString BPETokenizer::decode(const std::vector<int32_t>& tokens) {
    if (!isReady()) return QString();
    QByteArray utf8;
    utf8.reserve(static_cast<int>(tokens.size() * 4));

    for (int32_t tokenId : tokens) {
        if (tokenId < 0) continue;
        if (tokenId == m_bosToken || tokenId == m_eosToken || tokenId == m_padToken) {
            continue;
        }

        if (tokenId >= m_bytePieces.size()) {
            tokenId = 0; // clamp to safe id
        }

        QString token = m_bytePieces.value(tokenId);
        if (token.isEmpty() && m_reverseVocab.contains(tokenId)) {
            token = m_reverseVocab.value(tokenId);
        }
        if (token.isEmpty()) {
            continue;
        }

        for (QChar ch : token) {
            if (m_byteDecoder.contains(ch)) {
                utf8.append(static_cast<char>(m_byteDecoder.value(ch)));
            } else {
                const QByteArray tmp = QString(ch).toUtf8();
                utf8.append(tmp);
            }
        }
    }

    return QString::fromUtf8(utf8);
}
