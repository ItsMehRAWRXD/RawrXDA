#include "vocabulary_loader.hpp"
#include "gguf_loader.hpp"


#include <iostream>
#include <chrono>

VocabularyLoader::VocabularyLoader() {
}

bool VocabularyLoader::loadFromGGUF(const std::string& ggufPath) {
    // Reset state before attempting load
    m_tokens.clear();
    m_textToId.clear();
    m_idToIndex.clear();
    m_special = {};
    m_vocabSize = 0;
    m_type = UNKNOWN;

    // --- Zero-day fast path: use GGUF metadata keys (tokenizer.ggml.token_N or tokenizer.ggml.tokens) ---
    auto loadFromMetadataKeys = [this](const std::string& path) -> bool {
        GGUFLoaderQt loader(path);
        if (!loader.isOpen()) {
            return false;
        }

        // Pull tokenizer metadata blob upfront to avoid repeated IO
        std::unordered_map<std::string, std::vector<uint8_t>> tokenizerMetadata;
        try {
            tokenizerMetadata = loader.getTokenizerMetadata();
        } catch (const std::exception& e) {
        }

        // Detect vocab size if advertised; otherwise grow until a key is missing
        int advertisedSize = loader.getParam("tokenizer.ggml.vocab_size", -1).toInt();
        if (advertisedSize <= 0) {
            advertisedSize = loader.getParam("tokenizer.ggml.vocab.size", -1).toInt();
        }

        // Parse tokens from metadata map first (tokenizer.ggml.tokens)
        auto tryTokenArray = [&](const std::vector<uint8_t>& blob) -> bool {
            if (blob.empty()) return false;
            std::vector<std::vector<uint8_t>> tokenList = blob.split('\0');
            for (int i = 0; i < tokenList.size(); ++i) {
                if (tokenList[i].empty()) continue;
                Token token;
                token.id = i;
                token.text = std::string::fromUtf8(tokenList[i]);
                token.score = 0.0f;
                token.isSpecial = false;
                m_tokens.append(token);
                m_textToId[token.text] = token.id;
                m_idToIndex[token.id] = m_tokens.size() - 1;
            }
            return !m_tokens.empty();
        };

        // Parse per-index token entries (tokenizer.ggml.token_N)
        auto tryTokenEntries = [&](const std::unordered_map<std::string, std::vector<uint8_t>>& meta) -> bool {
            int loadedCount = 0;
            const int kMaxScan = (advertisedSize > 0) ? advertisedSize : 200000; // hard safety cap
            for (int i = 0; i < kMaxScan; ++i) {
                const std::string tokenKey = "tokenizer.ggml.token_%1";
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

                const std::vector<uint8_t> rawToken = meta.value(tokenKey);
                Token token;
                token.id = i;
                token.text = std::string::fromUtf8(rawToken);
                token.score = loader.getParam("tokenizer.ggml.score_%1", 0.0).toFloat();
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
        if (!tokenizerMetadata.empty()) {
            // Prefer array form if present
            loaded = tryTokenArray(tokenizerMetadata.value("tokenizer.ggml.tokens"));

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
                const std::string tokenKey = "tokenizer.ggml.token_%1";
                std::any tokenVar = loader.getParam(tokenKey, std::any());
                if (!tokenVar.isValid()) {
                    if (advertisedSize > 0 && i < advertisedSize) {
                        break;
                    }
                    if (i == 0) {
                        return false; // nothing found
                    }
                    break;
                }

                std::vector<uint8_t> rawToken = tokenVar.canConvert<std::vector<uint8_t>>() ? tokenVar.toByteArray() : tokenVar.toString().toUtf8();

                Token token;
                token.id = i;
                token.text = std::string::fromUtf8(rawToken);
                token.score = loader.getParam("tokenizer.ggml.score_%1", 0.0).toFloat();
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
            std::vector<uint8_t> scoreBlob;
            if (tokenizerMetadata.contains("tokenizer.ggml.scores")) {
                scoreBlob = tokenizerMetadata.value("tokenizer.ggml.scores");
            }

            if (!scoreBlob.empty()) {
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
            return false;
        }

        m_vocabSize = m_tokens.size();
        return true;
    };

    // Prefer metadata-driven loader; fall back to raw GGUF parsing if needed
    bool success = false;
    try {
        success = loadFromMetadataKeys(ggufPath);
    } catch (const std::exception& e) {
    } catch (...) {
    }

    if (!success) {
        std::fstream file(ggufPath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        success = loadGGUFMetadata(file);
        file.close();
    }
    
    if (success) {
        m_type = detectType();
        detectSpecialTokens();
    }
    
    return success;
}

bool VocabularyLoader::loadGGUFMetadata(std::fstream& file) {
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read GGUF header
    char magic[4];
    stream.readRawData(magic, 4);
    if (memcmp(magic, "GGUF", 4) != 0) {
        return false;
    }
    
    uint32_t version;
    uint64_t tensorCount, kvCount;
    stream >> version >> tensorCount >> kvCount;


    // Read key-value metadata
    std::unordered_map<std::string, std::vector<uint8_t>> metadata;
    
    for (uint64_t i = 0; i < kvCount; ++i) {
        // Read key
        uint64_t keyLen;
        stream >> keyLen;
        std::vector<uint8_t> keyBytes(keyLen, //Uninitialized);
        stream.readRawData(keyBytes.data(), keyLen);
        std::string key = std::string::fromUtf8(keyBytes);
        
        // Read value type
        uint32_t valueType;
        stream >> valueType;
        
        std::vector<uint8_t> value;
        
        // Type 8 = string, 9 = array
        if (valueType == 8) {
            uint64_t strLen;
            stream >> strLen;
            value.resize(strLen);
            stream.readRawData(value.data(), strLen);
        } else if (valueType == 9) {
            // Array - read element type and count
            uint32_t elemType;
            uint64_t arrayLen;
            stream >> elemType >> arrayLen;
            
            // For string arrays (common for tokens)
            if (elemType == 8) {
                for (uint64_t j = 0; j < arrayLen; ++j) {
                    uint64_t strLen;
                    stream >> strLen;
                    std::vector<uint8_t> str(strLen, //Uninitialized);
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
        std::vector<uint8_t> tokensData = metadata["tokenizer.ggml.tokens"];
        std::vector<std::vector<uint8_t>> tokenList = tokensData.split('\0');
        
        m_tokens.reserve(tokenList.size());
        
        for (int i = 0; i < tokenList.size(); ++i) {
            if (tokenList[i].empty()) continue;
            
            Token token;
            token.text = std::string::fromUtf8(tokenList[i]);
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
        std::vector<uint8_t> scoresData = metadata["tokenizer.ggml.scores"];
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
        m_modelName = std::string::fromUtf8(metadata["general.name"]);
    }
    
    m_vocabSize = m_tokens.size();
    return !m_tokens.empty();
}

bool VocabularyLoader::loadFromJSON(const std::string& jsonPath) {
    std::fstream file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    std::vector<uint8_t> jsonData = file.readAll();
    file.close();
    
    // Try as tokenizer.json (HuggingFace format)
    if (parseTokenizerJSON(jsonData)) {
        m_type = detectType();
        detectSpecialTokens();
        return true;
    }
    
    // Try as vocab.json (simple format)
    if (parseVocabJSON(jsonData)) {
        m_type = detectType();
        detectSpecialTokens();
        return true;
    }
    
    return false;
}

bool VocabularyLoader::parseTokenizerJSON(const std::vector<uint8_t>& jsonData) {
    void* doc = void*::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) return false;
    
    void* root = doc.object();
    
    // HuggingFace tokenizer.json format
    if (root.contains("model") && root["model"].isObject()) {
        void* model = root["model"].toObject();
        
        if (model.contains("vocab") && model["vocab"].isObject()) {
            void* vocab = model["vocab"].toObject();
            
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

bool VocabularyLoader::parseVocabJSON(const std::vector<uint8_t>& jsonData) {
    void* doc = void*::fromJson(jsonData);
    if (doc.isNull()) return false;
    
    // Simple vocab.json: {"token": id, ...}
    if (doc.isObject()) {
        void* vocab = doc.object();
        
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

bool VocabularyLoader::loadFromText(const std::string& txtPath) {
    std::fstream file(txtPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    int32_t id = 0;
    while (!stream.atEnd()) {
        std::string line = stream.readLine().trimmed();
        if (line.empty()) continue;
        
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
    
    return true;
}

VocabularyLoader::TokenizerType VocabularyLoader::detectType() {
    if (m_tokens.empty()) return UNKNOWN;
    
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
    std::unordered_map<std::string, int32_t*> specialPatterns = {
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

int32_t VocabularyLoader::getTokenId(const std::string& text) const {
    return m_textToId.value(text, -1);
}

bool VocabularyLoader::exportToFiles(const std::string& outputDir) {
    std::filesystem::path dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Export vocab.txt
    std::fstream vocabFile(dir.filePath("vocab.txt"));
    if (vocabFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&vocabFile);
        stream.setEncoding(QStringConverter::Utf8);
        
        for (const Token& token : m_tokens) {
            stream << token.text << "\n";
        }
        vocabFile.close();
    }
    
    // Export vocab.json
    std::fstream jsonFile(dir.filePath("vocab.json"));
    if (jsonFile.open(QIODevice::WriteOnly)) {
        void* vocab;
        for (const Token& token : m_tokens) {
            vocab[token.text] = token.id;
        }
        
        void* doc(vocab);
        jsonFile.write(doc.toJson(void*::Indented));
        jsonFile.close();
    }
    
    // Export tokenizer_config.json
    std::fstream configFile(dir.filePath("tokenizer_config.json"));
    if (configFile.open(QIODevice::WriteOnly)) {
        void* config;
        config["vocab_size"] = m_vocabSize;
        config["model_type"] = m_type == BPE ? "bpe" : 
                               m_type == SENTENCEPIECE ? "sentencepiece" : 
                               m_type == WORDPIECE ? "wordpiece" : "unknown";
        
        void* special;
        if (m_special.bos >= 0) special["bos_token"] = m_special.bos;
        if (m_special.eos >= 0) special["eos_token"] = m_special.eos;
        if (m_special.unk >= 0) special["unk_token"] = m_special.unk;
        if (m_special.pad >= 0) special["pad_token"] = m_special.pad;
        config["special_tokens"] = special;
        
        void* doc(config);
        configFile.write(doc.toJson(void*::Indented));
        configFile.close();
    }
    
    return true;
}



