#include "vocabulary_loader.hpp"
#include "gguf_loader.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QDataStream>
#include <QVariant>
#include <QList>
#include <iostream>
#include <chrono>

VocabularyLoader::VocabularyLoader() {
}

bool VocabularyLoader::loadFromGGUF(const QString& ggufPath) {
    // Reset state before attempting load
    m_tokens.clear();
    m_textToId.clear();
    m_idToIndex.clear();
    m_special = {};
    m_vocabSize = 0;
    m_type = UNKNOWN;

    // --- Zero-day fast path: use GGUF metadata keys (tokenizer.ggml.token_N or tokenizer.ggml.tokens) ---
    auto loadFromMetadataKeys = [this](const QString& path) -> bool {
        GGUFLoaderQt loader(path);
        if (!loader.isOpen()) {
            qWarning() << "[Vocab] GGUFLoaderQt failed to open" << path;
            return false;
        }

        // Pull tokenizer metadata blob upfront to avoid repeated IO
        QHash<QString, QByteArray> tokenizerMetadata;
        try {
            tokenizerMetadata = loader.getTokenizerMetadata();
            qInfo() << "[Vocab] tokenizer metadata entries:" << tokenizerMetadata.size();
        } catch (const std::exception& e) {
            qWarning() << "[Vocab] Failed to read tokenizer metadata via GGUFLoaderQt:" << e.what();
        }

        // Detect vocab size if advertised; otherwise grow until a key is missing
        int advertisedSize = loader.getParam("tokenizer.ggml.vocab_size", -1).toInt();
        if (advertisedSize <= 0) {
            advertisedSize = loader.getParam("tokenizer.ggml.vocab.size", -1).toInt();
        }

        // Parse tokens from metadata map first (tokenizer.ggml.tokens)
        auto tryTokenArray = [&](const QByteArray& blob) -> bool {
            if (blob.isEmpty()) return false;
            QList<QByteArray> tokenList = blob.split('\0');
            for (int i = 0; i < tokenList.size(); ++i) {
                if (tokenList[i].isEmpty()) continue;
                Token token;
                token.id = i;
                token.text = QString::fromUtf8(tokenList[i]);
                token.score = 0.0f;
                token.isSpecial = false;
                m_tokens.append(token);
                m_textToId[token.text] = token.id;
                m_idToIndex[token.id] = m_tokens.size() - 1;
            }
            return !m_tokens.isEmpty();
        };

        // Parse per-index token entries (tokenizer.ggml.token_N)
        auto tryTokenEntries = [&](const QHash<QString, QByteArray>& meta) -> bool {
            int loadedCount = 0;
            const int kMaxScan = (advertisedSize > 0) ? advertisedSize : 200000; // hard safety cap
            for (int i = 0; i < kMaxScan; ++i) {
                const QString tokenKey = QStringLiteral("tokenizer.ggml.token_%1").arg(i);
                if (!meta.contains(tokenKey)) {
                    if (advertisedSize > 0 && i < advertisedSize) {
                        // hole inside advertised range: stop but consider success if >0 loaded
                        break;
                    }
                    if (i == 0) {
                        return false; // nothing found at all
                    }
                    break;
                }

                const QByteArray rawToken = meta.value(tokenKey);
                Token token;
                token.id = i;
                token.text = QString::fromUtf8(rawToken);
                token.score = loader.getParam(QStringLiteral("tokenizer.ggml.score_%1").arg(i), 0.0).toFloat();
                token.isSpecial = false;

                m_tokens.append(token);
                m_textToId[token.text] = token.id;
                m_idToIndex[token.id] = m_tokens.size() - 1;
                ++loadedCount;

                if (advertisedSize > 0 && loadedCount >= advertisedSize) {
                    break;
                }
            }
            return loadedCount > 0;
        };

        bool loaded = false;
        if (!tokenizerMetadata.isEmpty()) {
            // Prefer array form if present
            loaded = tryTokenArray(tokenizerMetadata.value(QStringLiteral("tokenizer.ggml.tokens")));

            // If no array, try per-token entries from metadata hash
            if (!loaded) {
                loaded = tryTokenEntries(tokenizerMetadata);
            }
        }

        // Fallback: query params directly if metadata map is empty or missing entries
        if (!loaded) {
            int loadedCount = 0;
            const int kMaxScan = (advertisedSize > 0) ? advertisedSize : 200000; // hard safety cap
            for (int i = 0; i < kMaxScan; ++i) {
                const QString tokenKey = QStringLiteral("tokenizer.ggml.token_%1").arg(i);
                QVariant tokenVar = loader.getParam(tokenKey, QVariant());
                if (!tokenVar.isValid()) {
                    if (advertisedSize > 0 && i < advertisedSize) {
                        break;
                    }
                    if (i == 0) {
                        return false; // nothing found
                    }
                    break;
                }

                QByteArray rawToken = tokenVar.canConvert<QByteArray>() ? tokenVar.toByteArray() : tokenVar.toString().toUtf8();

                Token token;
                token.id = i;
                token.text = QString::fromUtf8(rawToken);
                token.score = loader.getParam(QStringLiteral("tokenizer.ggml.score_%1").arg(i), 0.0).toFloat();
                token.isSpecial = false;

                m_tokens.append(token);
                m_textToId[token.text] = token.id;
                m_idToIndex[token.id] = m_tokens.size() - 1;
                ++loadedCount;

                if (advertisedSize > 0 && loadedCount >= advertisedSize) {
                    break;
                }
            }
            loaded = loadedCount > 0;
        }

        // Attach scores if provided as array
        if (loaded) {
            QByteArray scoreBlob;
            if (tokenizerMetadata.contains(QStringLiteral("tokenizer.ggml.scores"))) {
                scoreBlob = tokenizerMetadata.value(QStringLiteral("tokenizer.ggml.scores"));
            }

            if (!scoreBlob.isEmpty()) {
                QDataStream scoreStream(scoreBlob);
                scoreStream.setByteOrder(QDataStream::LittleEndian);
                for (int i = 0; i < m_tokens.size() && scoreStream.status() == QDataStream::Ok; ++i) {
                    float score = 0.0f;
                    scoreStream >> score;
                    m_tokens[i].score = score;
                }
            }
        }

        if (!loaded) {
            qWarning() << "[Vocab] No tokenizer.ggml token entries found in GGUF metadata";
            return false;
        }

        m_vocabSize = m_tokens.size();
        qInfo() << "[Vocab] Loaded" << m_tokens.size() << "tokens via GGUF metadata";
        return true;
    };

    // Prefer metadata-driven loader; fall back to raw GGUF parsing if needed
    bool success = false;
    try {
        success = loadFromMetadataKeys(ggufPath);
    } catch (const std::exception& e) {
        qWarning() << "[Vocab] Exception while loading metadata keys:" << e.what();
    } catch (...) {
        qWarning() << "[Vocab] Unknown exception while loading metadata keys";
    }

    if (!success) {
        QFile file(ggufPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open GGUF file:" << ggufPath;
            return false;
        }
        
        success = loadGGUFMetadata(file);
        file.close();
    }
    
    if (success) {
        m_type = detectType();
        detectSpecialTokens();
        qInfo() << "Loaded vocabulary from GGUF:" << m_tokens.size() << "tokens, type:" << m_type;
    }
    
    return success;
}

bool VocabularyLoader::loadGGUFMetadata(QFile& file) {
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read GGUF header
    char magic[4];
    stream.readRawData(magic, 4);
    if (memcmp(magic, "GGUF", 4) != 0) {
        qWarning() << "Invalid GGUF magic";
        return false;
    }
    
    quint32 version;
    quint64 tensorCount, kvCount;
    stream >> version >> tensorCount >> kvCount;
    
    qDebug() << "GGUF version:" << version << "kv_count:" << kvCount;
    
    // Read key-value metadata
    QHash<QString, QByteArray> metadata;
    
    for (quint64 i = 0; i < kvCount; ++i) {
        // Read key
        quint64 keyLen;
        stream >> keyLen;
        QByteArray keyBytes(keyLen, Qt::Uninitialized);
        stream.readRawData(keyBytes.data(), keyLen);
        QString key = QString::fromUtf8(keyBytes);
        
        // Read value type
        quint32 valueType;
        stream >> valueType;
        
        QByteArray value;
        
        // Type 8 = string, 9 = array
        if (valueType == 8) {
            quint64 strLen;
            stream >> strLen;
            value.resize(strLen);
            stream.readRawData(value.data(), strLen);
        } else if (valueType == 9) {
            // Array - read element type and count
            quint32 elemType;
            quint64 arrayLen;
            stream >> elemType >> arrayLen;
            
            // For string arrays (common for tokens)
            if (elemType == 8) {
                for (quint64 j = 0; j < arrayLen; ++j) {
                    quint64 strLen;
                    stream >> strLen;
                    QByteArray str(strLen, Qt::Uninitialized);
                    stream.readRawData(str.data(), strLen);
                    value.append(str);
                    value.append('\0');  // Delimiter
                }
            }
        } else {
            // Skip other types
            stream.skipRawData(16);  // Approximate
        }
        
        metadata[key] = value;
    }
    
    // Extract tokens from metadata
    if (metadata.contains("tokenizer.ggml.tokens")) {
        QByteArray tokensData = metadata["tokenizer.ggml.tokens"];
        QList<QByteArray> tokenList = tokensData.split('\0');
        
        m_tokens.reserve(tokenList.size());
        
        for (int i = 0; i < tokenList.size(); ++i) {
            if (tokenList[i].isEmpty()) continue;
            
            Token token;
            token.text = QString::fromUtf8(tokenList[i]);
            token.id = i;
            token.score = 0.0f;
            token.isSpecial = false;
            
            m_tokens.append(token);
            m_textToId[token.text] = token.id;
            m_idToIndex[token.id] = m_tokens.size() - 1;
        }
    }
    
    // Load scores if available
    if (metadata.contains("tokenizer.ggml.scores")) {
        QByteArray scoresData = metadata["tokenizer.ggml.scores"];
        QDataStream scoreStream(scoresData);
        scoreStream.setByteOrder(QDataStream::LittleEndian);
        
        for (int i = 0; i < qMin(m_tokens.size(), scoresData.size() / 4); ++i) {
            float score;
            scoreStream >> score;
            if (i < m_tokens.size()) {
                m_tokens[i].score = score;
            }
        }
    }
    
    // Get model name
    if (metadata.contains("general.name")) {
        m_modelName = QString::fromUtf8(metadata["general.name"]);
    }
    
    m_vocabSize = m_tokens.size();
    return !m_tokens.isEmpty();
}

bool VocabularyLoader::loadFromJSON(const QString& jsonPath) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open JSON file:" << jsonPath;
        return false;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    // Try as tokenizer.json (HuggingFace format)
    if (parseTokenizerJSON(jsonData)) {
        m_type = detectType();
        detectSpecialTokens();
        qInfo() << "Loaded vocabulary from tokenizer.json:" << m_tokens.size() << "tokens";
        return true;
    }
    
    // Try as vocab.json (simple format)
    if (parseVocabJSON(jsonData)) {
        m_type = detectType();
        detectSpecialTokens();
        qInfo() << "Loaded vocabulary from vocab.json:" << m_tokens.size() << "tokens";
        return true;
    }
    
    return false;
}

bool VocabularyLoader::parseTokenizerJSON(const QByteArray& jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) return false;
    
    QJsonObject root = doc.object();
    
    // HuggingFace tokenizer.json format
    if (root.contains("model") && root["model"].isObject()) {
        QJsonObject model = root["model"].toObject();
        
        if (model.contains("vocab") && model["vocab"].isObject()) {
            QJsonObject vocab = model["vocab"].toObject();
            
            m_tokens.reserve(vocab.size());
            
            for (auto it = vocab.begin(); it != vocab.end(); ++it) {
                Token token;
                token.text = it.key();
                token.id = it.value().toInt();
                token.score = 0.0f;
                token.isSpecial = false;
                
                m_tokens.append(token);
                m_textToId[token.text] = token.id;
                m_idToIndex[token.id] = m_tokens.size() - 1;
            }
            
            // Sort by ID
            std::sort(m_tokens.begin(), m_tokens.end(), 
                     [](const Token& a, const Token& b) { return a.id < b.id; });
            
            m_vocabSize = m_tokens.size();
            return true;
        }
    }
    
    return false;
}

bool VocabularyLoader::parseVocabJSON(const QByteArray& jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) return false;
    
    // Simple vocab.json: {"token": id, ...}
    if (doc.isObject()) {
        QJsonObject vocab = doc.object();
        
        m_tokens.reserve(vocab.size());
        
        for (auto it = vocab.begin(); it != vocab.end(); ++it) {
            Token token;
            token.text = it.key();
            token.id = it.value().toInt();
            token.score = 0.0f;
            
            m_tokens.append(token);
            m_textToId[token.text] = token.id;
            m_idToIndex[token.id] = m_tokens.size() - 1;
        }
        
        std::sort(m_tokens.begin(), m_tokens.end(),
                 [](const Token& a, const Token& b) { return a.id < b.id; });
        
        m_vocabSize = m_tokens.size();
        return true;
    }
    
    return false;
}

bool VocabularyLoader::loadFromText(const QString& txtPath) {
    QFile file(txtPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open text file:" << txtPath;
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    int32_t id = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        Token token;
        token.text = line;
        token.id = id++;
        token.score = 0.0f;
        
        m_tokens.append(token);
        m_textToId[token.text] = token.id;
        m_idToIndex[token.id] = m_tokens.size() - 1;
    }
    
    file.close();
    
    m_type = detectType();
    detectSpecialTokens();
    m_vocabSize = m_tokens.size();
    
    qInfo() << "Loaded vocabulary from text:" << m_tokens.size() << "tokens";
    return true;
}

VocabularyLoader::TokenizerType VocabularyLoader::detectType() {
    if (m_tokens.isEmpty()) return UNKNOWN;
    
    // Check for SentencePiece markers (▁ character)
    int spaceMarkers = 0;
    for (const Token& token : m_tokens) {
        if (token.text.contains(QChar(0x2581))) {  // ▁
            ++spaceMarkers;
        }
    }
    
    if (spaceMarkers > m_tokens.size() / 10) {
        return SENTENCEPIECE;
    }
    
    // Check for WordPiece markers (##)
    int wpMarkers = 0;
    for (const Token& token : m_tokens) {
        if (token.text.startsWith("##")) {
            ++wpMarkers;
        }
    }
    
    if (wpMarkers > m_tokens.size() / 10) {
        return WORDPIECE;
    }
    
    // Check for byte-level BPE (Ġ character or similar)
    int bpeMarkers = 0;
    for (const Token& token : m_tokens) {
        if (token.text.contains(QChar(0x0120))) {  // Ġ
            ++bpeMarkers;
        }
    }
    
    if (bpeMarkers > m_tokens.size() / 10) {
        return BPE;
    }
    
    // Default to BPE if uncertain
    return BPE;
}

void VocabularyLoader::detectSpecialTokens() {
    // Common special token patterns
    QHash<QString, int32_t*> specialPatterns = {
        {"<s>", &m_special.bos},
        {"<|begin_of_text|>", &m_special.bos},
        {"<|startoftext|>", &m_special.bos},
        {"[CLS]", &m_special.cls},
        
        {"</s>", &m_special.eos},
        {"<|end_of_text|>", &m_special.eos},
        {"<|endoftext|>", &m_special.eos},
        {"[SEP]", &m_special.sep},
        
        {"<unk>", &m_special.unk},
        {"[UNK]", &m_special.unk},
        
        {"<pad>", &m_special.pad},
        {"[PAD]", &m_special.pad},
        
        {"[MASK]", &m_special.mask}
    };
    
    for (const Token& token : m_tokens) {
        for (auto it = specialPatterns.begin(); it != specialPatterns.end(); ++it) {
            if (token.text == it.key()) {
                *(it.value()) = token.id;
                const_cast<Token&>(token).isSpecial = true;
            }
        }
    }
}

VocabularyLoader::Token VocabularyLoader::getToken(int32_t id) const {
    if (m_idToIndex.contains(id)) {
        return m_tokens[m_idToIndex[id]];
    }
    return Token{"", -1, 0.0f, false};
}

int32_t VocabularyLoader::getTokenId(const QString& text) const {
    return m_textToId.value(text, -1);
}

bool VocabularyLoader::exportToFiles(const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Export vocab.txt
    QFile vocabFile(dir.filePath("vocab.txt"));
    if (vocabFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&vocabFile);
        stream.setEncoding(QStringConverter::Utf8);
        
        for (const Token& token : m_tokens) {
            stream << token.text << "\n";
        }
        vocabFile.close();
        qInfo() << "Exported vocab.txt";
    }
    
    // Export vocab.json
    QFile jsonFile(dir.filePath("vocab.json"));
    if (jsonFile.open(QIODevice::WriteOnly)) {
        QJsonObject vocab;
        for (const Token& token : m_tokens) {
            vocab[token.text] = token.id;
        }
        
        QJsonDocument doc(vocab);
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
        qInfo() << "Exported vocab.json";
    }
    
    // Export tokenizer_config.json
    QFile configFile(dir.filePath("tokenizer_config.json"));
    if (configFile.open(QIODevice::WriteOnly)) {
        QJsonObject config;
        config["vocab_size"] = m_vocabSize;
        config["model_type"] = m_type == BPE ? "bpe" : 
                               m_type == SENTENCEPIECE ? "sentencepiece" : 
                               m_type == WORDPIECE ? "wordpiece" : "unknown";
        
        QJsonObject special;
        if (m_special.bos >= 0) special["bos_token"] = m_special.bos;
        if (m_special.eos >= 0) special["eos_token"] = m_special.eos;
        if (m_special.unk >= 0) special["unk_token"] = m_special.unk;
        if (m_special.pad >= 0) special["pad_token"] = m_special.pad;
        config["special_tokens"] = special;
        
        QJsonDocument doc(config);
        configFile.write(doc.toJson(QJsonDocument::Indented));
        configFile.close();
        qInfo() << "Exported tokenizer_config.json";
    }
    
    return true;
}
