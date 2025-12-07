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
    
    if (metadata.contains("tokenizer.ggml.tokens")) {
        QByteArray tokensData = metadata["tokenizer.ggml.tokens"];
        QDataStream stream(tokensData);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        int32_t numTokens;
        stream >> numTokens;
        
        for (int32_t i = 0; i < numTokens; ++i) {
            quint32 len;
            stream >> len;
            QByteArray tokenBytes(len, Qt::Uninitialized);
            stream.readRawData(tokenBytes.data(), len);
            
            QString token = QString::fromUtf8(tokenBytes);
            m_vocab[token] = i;
            m_reverseVocab[i] = token;
        }
    }
    
    if (metadata.contains("tokenizer.ggml.merges")) {
        QByteArray mergesData = metadata["tokenizer.ggml.merges"];
        QTextStream stream(mergesData);
        
        int priority = 0;
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty()) continue;
            
            QStringList parts = line.split(' ');
            if (parts.size() >= 2) {
                QPair<QString, QString> pair(parts[0], parts[1]);
                m_merges[pair] = priority++;
            }
        }
    }
    
    qInfo() << "BPE loaded from GGUF:" << m_vocab.size() << "tokens";
    return !m_vocab.isEmpty();
}

QVector<QString> BPETokenizer::byteEncode(const QString& text) {
    QVector<QString> result;
    QByteArray utf8 = text.toUtf8();
    
    for (uint8_t byte : utf8) {
        result.append(QString(m_byteEncoder[byte]));
    }
    
    return result;
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
        qWarning() << "BPE tokenizer not initialized";
        return {};
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
    
    for (int32_t tokenId : tokens) {
        // Skip special tokens
        if (tokenId == m_bosToken || tokenId == m_eosToken || tokenId == m_padToken) {
            continue;
        }
        
        if (!m_reverseVocab.contains(tokenId)) {
            qWarning() << "Unknown token ID:" << tokenId;
            continue;
        }
        
        QString token = m_reverseVocab[tokenId];
        
        // Decode byte-level representation back to UTF-8
        for (QChar ch : token) {
            if (m_byteDecoder.contains(ch)) {
                utf8.append(m_byteDecoder[ch]);
            }
        }
    }
    
    return QString::fromUtf8(utf8);
}
