#include "inference_engine.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

// Use shared quant utilities
#include "quant_utils.hpp"
#include "gguf_parser.hpp"

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr), m_parser(nullptr)
{
    if (!ggufPath.isEmpty()) {
        loadModel(ggufPath);
    }
}

// Helper to format local time with offset like 2025-12-02T22:54:07.082-05:00
static QString nowIsoOffset()
{
    const QDateTime dt = QDateTime::currentDateTime();
    int off = dt.offsetFromUtc();
    const QChar sign = off >= 0 ? '+' : '-';
    off = std::abs(off);
    const int hh = off / 3600;
    const int mm = (off % 3600) / 60;
    return dt.toString("yyyy-MM-dd'T'HH:mm:ss.zzz") + QString("%1%2:%3").arg(sign).arg(hh,2,10,QChar('0')).arg(mm,2,10,QChar('0'));
}

bool InferenceEngine::loadModel(const QString& path)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    if (m_parser) {
        delete m_parser;
        m_parser = nullptr;
    }
    
    // Emit a log that we're starting to load
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"loading gguf\" path=\"%2\"")
                        .arg(nowIsoOffset(), path));

    // Use new GGUFParser for proper GGUF v3 parsing
    m_parser = new GGUFParser(path);
    
    if (!m_parser->isValid()) {
        qWarning() << "Failed to parse GGUF model:" << path;
        emit logMessage(QString("time=%1 level=ERROR source=inference_engine.cpp:loadModel msg=\"failed to parse gguf\" path=\"%2\"")
                            .arg(nowIsoOffset(), path));
        delete m_parser;
        m_parser = nullptr;
        emit modelLoadedChanged(false, QString());
        return false;
    }
    
    // Also keep old loader for backward compatibility (if needed)
    m_loader = new GGUFLoader(path);
    
    m_modelPath = path;
    QString modelName = extractModelName(path);
    const GGUFMetadata& meta = m_parser->metadata();
    
    qInfo() << "Model loaded successfully:" << modelName;
    qInfo() << "  Architecture:" << meta.architecture;
    qInfo() << "  Layers:" << meta.n_layer;
    qInfo() << "  Embedding:" << meta.n_embd;
    qInfo() << "  Heads:" << meta.n_head;
    qInfo() << "  Vocab:" << meta.vocab_size;
    qInfo() << "  Tensors:" << m_parser->tensors().size();
    
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"model parsed\" name=\"%2\" arch=\"%3\" layers=%4 tensors=%5")
                        .arg(nowIsoOffset(), modelName, meta.architecture)
                        .arg(meta.n_layer)
                        .arg(m_parser->tensors().size()));
    
    // Detect quantization types in model
    detectQuantizationTypes();
    
    // Initialize tokenizer from model
    initializeTokenizer();
    
    // Build initial quantized tensor cache
    rebuildTensorCache();
    
    // Initialize transformer with model architecture from metadata
    int nLayers = meta.n_layer > 0 ? meta.n_layer : 12;
    int nEmbd = meta.n_embd > 0 ? meta.n_embd : 768;
    int nHead = meta.n_head > 0 ? meta.n_head : 12;
    int nVocab = meta.vocab_size > 0 ? meta.vocab_size : 50257;
    
    // For Q2_K models, transformer is built in buildTransformerFromQ2kCache()
    // For other formats, build standard transformer here
    if (m_detectedQuantFormat != "Q2_K" && !m_tensorCache.isEmpty()) {
        bool transformerLoaded = m_transformer.loadWeights(m_tensorCache, nLayers, nEmbd, nHead, nVocab);
        if (!transformerLoaded) {
            qWarning() << "Transformer weight loading failed, inference will be limited";
            emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:loadModel msg=\"transformer weights not ready\"")
                                .arg(nowIsoOffset()));
        } else {
            qInfo() << "Transformer initialized successfully";
            emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"transformer initialized\"")
                                .arg(nowIsoOffset()));
        }
    }
    
    // Log the detected quantization format
    QString quantFormatLog = m_detectedQuantFormat.isEmpty() ? m_quantMode : m_detectedQuantFormat;
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"gguf loaded\" tensors_cached=%2 quant=\"%3\"")
                        .arg(nowIsoOffset())
                        .arg(m_tensorCache.size())
                        .arg(quantFormatLog));
    emit modelLoadedChanged(true, modelName);
    return true;
}

bool InferenceEngine::isModelLoaded() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader && m_loader->isOpen();
}

QString InferenceEngine::modelPath() const
{
    QMutexLocker lock(&m_mutex);
    return m_modelPath;
}

QStringList InferenceEngine::tensorNames() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader ? m_loader->tensorNames() : QStringList();
}

qint64 InferenceEngine::memoryUsageMB() const
{
    QMutexLocker lock(&m_mutex);
    return m_memoryUsageMB;
}

double InferenceEngine::tokensPerSecond() const
{
    QMutexLocker lock(&m_mutex);
    return m_tokensPerSecond;
}

double InferenceEngine::temperature() const
{
    QMutexLocker lock(&m_mutex);
    return m_temperature;
}

void InferenceEngine::request(const QString& prompt, qint64 reqId)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        qWarning() << "No model loaded for inference request" << reqId;
        emit logMessage(QString("time=%1 level=ERROR source=inference_engine.cpp:request msg=\"no model loaded\" req_id=%2")
                            .arg(nowIsoOffset())
                            .arg(reqId));
        emit error(reqId, "Error: No model loaded");
        return;
    }
    
    m_inferenceTimer.start();
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:request msg=\"inference start\" req_id=%2 prompt_tokens=%3")
                        .arg(nowIsoOffset())
                        .arg(reqId)
                        .arg(tokenize(prompt).size()));
    
    // Use transformer if ready, otherwise fallback to placeholder
    if (m_transformer.isReady()) {
        // Tokenize using word-based approach (better than character-based)
        std::vector<int32_t> tokens = tokenize(prompt);
        
        qInfo() << "Running transformer inference with" << tokens.size() << "input tokens";
        
        // Generate response autoregressively (max 50 new tokens)
        std::vector<int32_t> generatedTokens = m_transformer.generate(tokens, 50, m_temperature);
        
        // Detokenize back to text
        QString response = detokenize(generatedTokens);
        
        qint64 elapsed = m_inferenceTimer.elapsed();
        int totalTokens = tokens.size() + generatedTokens.size();
        m_tokensPerSecond = (totalTokens * 1000.0) / std::max(elapsed, 1LL);
        
        qInfo() << "Inference completed:" << totalTokens << "tokens in" << elapsed 
                << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << "tok/s)";
        emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:request msg=\"inference done\" req_id=%2 tokens=%3 elapsed_ms=%4 tps=%5")
                    .arg(nowIsoOffset())
                    .arg(reqId)
                    .arg(totalTokens)
                    .arg(elapsed)
                    .arg(QString::number(m_tokensPerSecond, 'f', 1)));
        
        emit resultReady(reqId, response);
    } else {
        // Fallback: model not fully initialized
        QString response = QString("⚠ Model loaded but transformer not ready\n\n"
                                  "Model: %1\n"
                                  "Quantization: %2\n"
                                  "Cached tensors: %3\n\n"
                                  "Input: \"%4\"\n\n"
                                  "[Transformer weights still loading...]")
                              .arg(extractModelName(m_modelPath))
                              .arg(m_quantMode)
                              .arg(m_tensorCache.size())
                              .arg(prompt);
        
        qInfo() << "Transformer not ready, using fallback response";
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:request msg=\"fallback response (transformer not ready)\" req_id=%2")
                            .arg(nowIsoOffset())
                            .arg(reqId));
        emit resultReady(reqId, response);
    }
}

void InferenceEngine::unloadModel()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    if (m_parser) {
        delete m_parser;
        m_parser = nullptr;
    }
    
    m_modelPath.clear();
    m_tensorCache.clear();
    
    emit modelLoadedChanged(false, QString());
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:unloadModel msg=\"model unloaded\"")
                        .arg(nowIsoOffset()));
}

QString InferenceEngine::extractModelName(const QString& path) const
{
    QFileInfo modelInfo(path);
    return modelInfo.fileName();
}

void InferenceEngine::setQuantMode(const QString& mode)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_quantMode == mode) return;
    
    m_quantMode = mode;
    rebuildTensorCache();
    
    emit quantChanged(mode);
}

void InferenceEngine::setLayerQuant(const QString& tensorName, const QString& quant)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_perLayerQuant.value(tensorName) == quant) return;
    
    m_perLayerQuant.insert(tensorName, quant);
    rebuildTensorCache();
    
    emit quantChanged(QString("%1->%2").arg(tensorName, quant));
}

void InferenceEngine::rebuildTensorCache()
{
    m_tensorCache.clear();
    
    if (!m_loader) return;
    
    // Detect quantization format first
    QString quantFormat = detectQuantizationFormat();
    
    // If Q2_K format detected, use specialized loading
    if (quantFormat == "Q2_K") {
        qInfo() << "Using Q2_K tensor loading pipeline";
        emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:rebuildTensorCache msg=\"Q2_K pipeline\"")
                            .arg(nowIsoOffset()));
        loadQ2kTensors();
    } else {
        // Standard tensor cache building for other formats
        QStringList names = m_loader->tensorNames();
        for (const QString& name : names) {
            const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
            QByteArray raw = m_loader->inflateWeight(name);
            if (raw.isEmpty()) continue;
            m_tensorCache.insert(name, apply_quant(raw, qmode));
        }
    }
    
    // Reload transformer weights if cache was rebuilt
    if (!m_tensorCache.isEmpty() && m_loader) {
        if (quantFormat == "Q2_K") {
            // Use Q2_K-specific transformer building
            buildTransformerFromQ2kCache();
        } else {
            // Standard transformer loading
            m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
        }
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text)
{
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                return m_bpeTokenizer.encode(text);
            }
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                return m_spTokenizer.encode(text, true, false);  // Add BOS, no EOS
            }
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            break;
    }
    
    // PRODUCTION-GRADE Fallback: Robust word-based tokenization with vocabulary and statistics
    std::vector<int32_t> tokens;
    
    // Input validation
    if (text.isEmpty()) {
        qWarning() << "Empty text provided to tokenize()";
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:tokenize msg=empty_input_text")
                           .arg(nowIsoOffset()));
        return tokens;  // Return empty
    }
    
    tokens.reserve(text.split(QRegularExpression("\\s+")).size() + 2);
    
    // Add BOS token if vocabulary supports it
    if (m_vocab.isLoaded() && m_vocab.size() > 0) {
        tokens.push_back(1);  // Standard BOS token
    }
    
    // Split on comprehensive set of delimiters
    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:\\-()\"'`]+"), Qt::SkipEmptyParts);
    
    if (words.isEmpty()) {
        qWarning() << "No tokens extracted from text:" << text;
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:tokenize msg=no_tokens_extracted")
                           .arg(nowIsoOffset()));
        return tokens;  // Return BOS only
    }
    
    int unknownWordCount = 0;
    for (const QString& word : words) {
        if (word.isEmpty()) continue;
        
        // Use vocabulary if available
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                // Subword fallback: character-level tokenization for unknown words
                for (const QChar& ch : word) {
                    uint32_t hash = qHash(ch.unicode());
                    int32_t charToken = static_cast<int32_t>((hash % 256) + 256);
                    tokens.push_back(charToken);
                }
                unknownWordCount++;
            }
        } else {
            // No vocabulary: use stable hash-based tokenization
            uint32_t hash = qHash(word.toLower());
            int32_t tokenId = static_cast<int32_t>((hash % 50000) + 256);
            tokens.push_back(tokenId);
        }
    }
    
    // Add EOS token
    tokens.push_back(2);
    
    // Log comprehensive statistics
    double unknownRate = words.isEmpty() ? 0.0 : (100.0 * unknownWordCount / words.size());
    qInfo() << "Fallback tokenization: input=" << text.length() << "chars, words=" << words.size()
            << "tokens=" << tokens.size() << "unknown=" << unknownWordCount 
            << QString("(%1%)").arg(unknownRate, 0, 'f', 1);
    
    // Warn if unknown word rate is high
    if (unknownRate > 50.0) {
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:tokenize msg=high_unknown_rate unknown_rate=%2")
                           .arg(nowIsoOffset())
                           .arg(unknownRate, 0, 'f', 1));
    }
    
    return tokens;
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                return m_bpeTokenizer.decode(tokens);
            }
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                return m_spTokenizer.decode(tokens, true);  // Skip special tokens
            }
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            break;
    }
    
    // Fallback: Use vocabulary or generate placeholders
    QString result;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        int32_t token = tokens[i];
        
        // Skip special tokens
        if (token == 1 || token == 2) continue;  // BOS/EOS
        
        // Use vocabulary if available
        if (m_vocab.isLoaded()) {
            VocabularyLoader::Token vocabToken = m_vocab.getToken(token);
            if (vocabToken.id >= 0) {
                result += vocabToken.text + " ";
                continue;
            }
        }
        
        // Pure fallback: placeholder
        if (token >= 256 && token < 50256) {
            result += QString("tok_%1 ").arg(token);
        } else if (token < 256) {
            result += QChar(token);
        }
    }
    
    return result.trimmed();
}

void InferenceEngine::initializeTokenizer()
{
    // Try to load vocabulary from GGUF file
    if (m_vocab.loadFromGGUF(m_modelPath)) {
        qInfo() << "Vocabulary loaded:" << m_vocab.size() << "tokens";
        
        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            // Initialize BPE tokenizer
            QHash<QString, QByteArray> dummyMetadata;  // GGUF metadata would go here
            if (m_bpeTokenizer.loadFromGGUFMetadata(dummyMetadata)) {
                m_tokenizerMode = TOKENIZER_BPE;
                qInfo() << "Using BPE tokenizer";
            }
        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            // Initialize SentencePiece tokenizer
            QHash<QString, QByteArray> dummyMetadata;
            if (m_spTokenizer.loadFromGGUFMetadata(dummyMetadata)) {
                m_tokenizerMode = TOKENIZER_SP;
                qInfo() << "Using SentencePiece tokenizer";
            }
        }
    }
    
    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
        qInfo() << "Using fallback word-based tokenizer (limited functionality)";
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:initializeTokenizer msg=\"fallback tokenizer in use\"")
                            .arg(nowIsoOffset()));
    }
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        qWarning() << "Cannot generate - no model loaded";
        return inputTokens;
    }
    
    if (inputTokens.empty()) {
        qWarning() << "Cannot generate from empty input";
        return inputTokens;
    }
    
    std::vector<int32_t> result = inputTokens;
    result.reserve(inputTokens.size() + maxTokens);
    
    // Check if transformer is ready
    if (!m_transformer.isReady()) {
        qCritical() << "CRITICAL: Transformer not ready for inference";
        qCritical() << "Model path:" << m_modelPath;
        qCritical() << "Quantization mode:" << m_quantMode;
        qCritical() << "Tensor cache size:" << m_tensorCache.size();
        
        // Production-grade error response with diagnostic data
        QString diagnostics = QString("\u26a0\ufe0f  INFERENCE INITIALIZATION ERROR\n\n"
                                      "Status: Transformer not fully initialized\n"
                                      "Model: %1\n"
                                      "Quantization: %2\n"
                                      "Cached Tensors: %3\n\n"
                                      "Diagnostics:\n"
                                      "- Parser valid: %4\n"
                                      "- Model loaded: %5\n"
                                      "- Tokenizer mode: %6\n\n"
                                      "This typically occurs if:\n"
                                      "1. Model weights failed to load\n"
                                      "2. Insufficient memory for model\n"
                                      "3. Incompatible tensor formats\n"
                                      "4. Hardware limitations\n\n"
                                      "Recommended action: Check logs and retry")
                                .arg(extractModelName(m_modelPath))
                                .arg(m_quantMode)
                                .arg(m_tensorCache.size())
                                .arg(m_parser && m_parser->isValid() ? "yes" : "no")
                                .arg(isModelLoaded() ? "yes" : "no")
                                .arg(m_tokenizerMode);
        
        emit logMessage(QString("time=%1 level=CRITICAL source=inference_engine.cpp:generate "
                               "msg=\"transformer initialization failed\" "
                               "model=%2 quantMode=%3 tensorCache=%4")
                            .arg(nowIsoOffset())
                            .arg(extractModelName(m_modelPath))
                            .arg(m_quantMode)
                            .arg(m_tensorCache.size()));
        
        emit error(-1, diagnostics);
        return result;  // Return only input tokens (no generation)
    }
    
    // Start inference timer
    m_inferenceTimer.start();
    
    qInfo() << "Starting token generation: max_tokens=" << maxTokens 
            << "temperature=" << m_temperature 
            << "input_len=" << inputTokens.size();
    
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:generate msg=\"generation start\" max_tokens=%2 temperature=%3 input_len=%4")
                        .arg(nowIsoOffset())
                        .arg(maxTokens)
                        .arg(QString::number(m_temperature, 'f', 2))
                        .arg(inputTokens.size()));
    
    // Token generation loop
    int tokensGenerated = 0;
    bool eosEncountered = false;
    
    for (int step = 0; step < maxTokens; ++step) {
        // Get logits for next token prediction
        std::vector<float> logits = m_transformer.forward(result);
        
        if (logits.empty()) {
            qWarning() << "Transformer forward pass failed at step" << step;
            emit logMessage(QString("time=%1 level=ERROR source=inference_engine.cpp:generate msg=\"forward pass failed\" step=%2")
                                .arg(nowIsoOffset())
                                .arg(step));
            break;
        }
        
        // Log logits info for debugging (vocab size varies by model)
        if (step == 0) {
            qDebug() << "Logits size:" << logits.size();
        }
        
        // Apply temperature scaling for sampling diversity
        if (m_temperature > 0.0 && m_temperature != 1.0) {
            for (float& logit : logits) {
                logit /= m_temperature;
            }
        }
        
        // Sample next token
        int32_t nextToken = -1;
        
        if (m_temperature <= 0.0) {
            // Greedy sampling (deterministic)
            float maxLogit = logits[0];
            nextToken = 0;
            for (size_t j = 1; j < logits.size(); ++j) {
                if (logits[j] > maxLogit) {
                    maxLogit = logits[j];
                    nextToken = static_cast<int32_t>(j);
                }
            }
        } else {
            // Nucleus (top-p) sampling with temperature
            // 1. Convert logits to probabilities using softmax
            std::vector<std::pair<float, int32_t>> probIndex;
            probIndex.reserve(logits.size());
            
            float maxLogit = *std::max_element(logits.begin(), logits.end());
            float sumExp = 0.0f;
            
            for (size_t j = 0; j < logits.size(); ++j) {
                float prob = std::exp(logits[j] - maxLogit);  // Numerically stable
                probIndex.push_back({prob, static_cast<int32_t>(j)});
                sumExp += prob;
            }
            
            // Normalize to probabilities
            for (auto& pi : probIndex) {
                pi.first /= sumExp;
            }
            
            // Sort by probability (descending)
            std::sort(probIndex.begin(), probIndex.end(), 
                     [](const auto& a, const auto& b) { return a.first > b.first; });
            
            // Nucleus sampling (top-p = 0.9)
            const float topP = 0.9f;
            float cumulativeProb = 0.0f;
            size_t nucleusSize = 0;
            
            for (size_t j = 0; j < probIndex.size(); ++j) {
                cumulativeProb += probIndex[j].first;
                nucleusSize = j + 1;
                if (cumulativeProb >= topP) break;
            }
            
            // Ensure at least top-k candidates (k=40)
            nucleusSize = std::max(nucleusSize, std::min<size_t>(40, probIndex.size()));
            
            // Sample from the nucleus
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float r = dist(rng);
            
            cumulativeProb = 0.0f;
            for (size_t j = 0; j < nucleusSize; ++j) {
                cumulativeProb += probIndex[j].first;
                if (r < cumulativeProb) {
                    nextToken = probIndex[j].second;
                    break;
                }
            }
            
            // Fallback to top token if sampling failed
            if (nextToken < 0) {
                nextToken = probIndex[0].second;
            }
        }
        
        // Validate token
        if (nextToken < 0 || nextToken >= static_cast<int32_t>(logits.size())) {
            qWarning() << "Invalid token sampled:" << nextToken;
            break;
        }
        
        // Check for end-of-sequence tokens (comprehensive list)
        // Model-specific EOS detection from metadata when available
        bool isEosToken = false;
        
        // Standard EOS tokens across models
        if (nextToken == 2 || nextToken == 50256) {
            isEosToken = true;  // GPT-2 EOS
        }
        // LLaMA/Mistral end-of-turn markers
        else if (nextToken == 32000 || nextToken == 32001 || nextToken == 32002) {
            isEosToken = true;
        }
        // Anthropic Claude model markers
        else if (nextToken == 100277 || nextToken == 100278) {
            isEosToken = true;
        }
        // Falcon model markers
        else if (nextToken == 11) {
            isEosToken = true;
        }
        // Model metadata EOS (if available)
        else if (m_parser && m_parser->isValid()) {
            const GGUFMetadata& meta = m_parser->metadata();
            if (!meta.eos_tokens.isEmpty()) {
                for (int eosId : meta.eos_tokens) {
                    if (nextToken == eosId) {
                        isEosToken = true;
                        break;
                    }
                }
            }
        }
        
        if (isEosToken) {
            qInfo() << "EOS token detected:" << nextToken << "at step" << step;
            eosEncountered = true;
            // Include EOS token in result for proper detokenization context
            result.push_back(nextToken);
            tokensGenerated++;
            
            emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:generate "
                                   "msg=\"end-of-sequence detected\" token=%2 step=%3")
                                .arg(nowIsoOffset())
                                .arg(nextToken)
                                .arg(step));
            break;
        }
        
        // Add token to result
        result.push_back(nextToken);
        tokensGenerated++;
        
        // Progress logging every 10 tokens
        if (step % 10 == 0 && step > 0) {
            qDebug() << "Generated" << step << "tokens...";
        }
    }
    
    // Update performance metrics
    qint64 elapsed = m_inferenceTimer.elapsed();
    if (elapsed > 0 && tokensGenerated > 0) {
        m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
    }
    
    qInfo() << "Generation complete:" 
            << "tokens_generated=" << tokensGenerated
            << "elapsed_ms=" << elapsed
            << "tps=" << QString::number(m_tokensPerSecond, 'f', 2)
            << "eos=" << (eosEncountered ? "yes" : "no");
    
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:generate msg=\"generation complete\" "
                           "tokens_generated=%2 elapsed_ms=%3 tps=%4 eos=%5 total_len=%6")
                        .arg(nowIsoOffset())
                        .arg(tokensGenerated)
                        .arg(elapsed)
                        .arg(QString::number(m_tokensPerSecond, 'f', 2))
                        .arg(eosEncountered ? "true" : "false")
                        .arg(result.size()));
    
    return result;
}

void InferenceEngine::detectQuantizationTypes()
{
    if (!m_parser || !m_parser->isValid()) {
        return;
    }
    
    // Analyze tensor quantization types to determine primary quantization mode
    QHash<QString, int> typeCount;
    
    for (const GGUFTensorInfo& tensor : m_parser->tensors()) {
        QString typeName = GGUFParser::typeName(tensor.type);
        typeCount[typeName]++;
    }
    
    // Find most common quantization type
    QString primaryQuant;
    int maxCount = 0;
    
    for (auto it = typeCount.constBegin(); it != typeCount.constEnd(); ++it) {
        qDebug() << "  " << it.key() << ":" << it.value() << "tensors";
        if (it.value() > maxCount) {
            maxCount = it.value();
            primaryQuant = it.key();
        }
    }
    
    if (!primaryQuant.isEmpty()) {
        m_quantMode = primaryQuant;
        qInfo() << "Detected primary quantization:" << m_quantMode << "(" << maxCount << "tensors)";
        emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:detectQuantizationTypes "
                               "msg=\"quantization detected\" type=\"%2\" count=%3")
                           .arg(nowIsoOffset(), m_quantMode)
                           .arg(maxCount));
    }
}

// ========================= Q2_K Tensor Support =========================

QString InferenceEngine::detectQuantizationFormat()
{
    if (m_detectedQuantFormat.isEmpty() && m_loader) {
        // Check if this is a Q2_K model by examining tensor names or first block
        QStringList names = m_loader->tensorNames();
        
        // Q2_K models typically have weight.n tensors that are Q2_K quantized
        // Check if any tensor mentions "q2k" in metadata or try to load a weight tensor
        for (const QString& name : names) {
            if (name.contains("q2k", Qt::CaseInsensitive) || 
                name.contains("Q2_K", Qt::CaseInsensitive)) {
                m_detectedQuantFormat = "Q2_K";
                qInfo() << "Detected Q2_K quantization from tensor:" << name;
                emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:detectQuantizationFormat "
                                       "msg=\"Q2_K detected\" tensor=\"%2\"")
                                   .arg(nowIsoOffset(), name));
                return "Q2_K";
            }
        }
        
        // Try to infer from tensor block size
        // Q2_K blocks are 84 bytes (16 scales + 64 qs + 2 d + 2 dmin)
        if (!names.isEmpty()) {
            QByteArray firstWeight = m_loader->inflateWeight(names.first());
            if (!firstWeight.isEmpty()) {
                int blockSize = firstWeight.size() / 256;  // Assuming QK_K=256
                if (blockSize == 84) {
                    m_detectedQuantFormat = "Q2_K";
                    qInfo() << "Detected Q2_K from tensor block size (84 bytes per block)";
                    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:detectQuantizationFormat "
                                           "msg=\"Q2_K detected from block size\"")
                                       .arg(nowIsoOffset()));
                    return "Q2_K";
                }
            }
        }
        
        // Default detection if nothing matched
        m_detectedQuantFormat = m_quantMode;
    }
    
    return m_detectedQuantFormat;
}

void InferenceEngine::loadQ2kTensors()
{
    if (!m_loader) return;
    
    QMutexLocker lock(&m_mutex);
    
    qInfo() << "Loading Q2_K quantized tensors...";
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadQ2kTensors msg=\"loading Q2_K tensors\"")
                        .arg(nowIsoOffset()));
    
    QStringList tensorNames = m_loader->tensorNames();
    int q2kTensorsLoaded = 0;
    
    for (const QString& name : tensorNames) {
        // Skip non-weight tensors (attention masks, embeddings, etc.)
        if (name.contains("bias") || name.contains("mask") || 
            name.contains("position", Qt::CaseInsensitive) ||
            name.contains("token_emb", Qt::CaseInsensitive)) {
            continue;
        }
        
        // Load the raw quantized tensor
        QByteArray rawQuantized = m_loader->inflateWeight(name);
        if (rawQuantized.isEmpty()) {
            qWarning() << "Failed to load tensor:" << name;
            continue;
        }
        
        // Dequantize Q2_K tensor to float32
        QByteArray dequantized = dequantizeQ2kTensor(rawQuantized);
        if (!dequantized.isEmpty()) {
            m_tensorCache.insert(name, dequantized);
            q2kTensorsLoaded++;
        }
    }
    
    qInfo() << "Q2_K tensors loaded:" << q2kTensorsLoaded << "/" << tensorNames.size();
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadQ2kTensors "
                           "msg=\"Q2_K tensors loaded\" count=%2 total=%3")
                        .arg(nowIsoOffset())
                        .arg(q2kTensorsLoaded)
                        .arg(tensorNames.size()));
}

QByteArray InferenceEngine::dequantizeQ2kTensor(const QByteArray& quantizedData)
{
    // Calculate expected number of Q2_K blocks (QK_K = 256 bytes per block, block is 84 bytes)
    const int BLOCK_SIZE = sizeof(block_q2_K);  // Should be 88 bytes (16+64+2+2+4 padding)
    
    if (quantizedData.size() < BLOCK_SIZE) {
        qWarning() << "Q2_K tensor too small:" << quantizedData.size() << "bytes";
        return QByteArray();
    }
    
    int numBlocks = quantizedData.size() / BLOCK_SIZE;
    if (numBlocks == 0) {
        qWarning() << "Invalid Q2_K tensor block count";
        return QByteArray();
    }
    
    int totalElements = numBlocks * QK_K;
    
    // Allocate output buffer for float32 data
    QByteArray output;
    output.resize(totalElements * sizeof(float));
    
    float* outputData = reinterpret_cast<float*>(output.data());
    const block_q2_K* blocks = reinterpret_cast<const block_q2_K*>(quantizedData.constData());
    
    // Use quant_utils dequantization function
    dequantize_row_q2_K(blocks, outputData, totalElements);
    
    qDebug() << "Dequantized Q2_K tensor:" << numBlocks << "blocks ->" 
             << totalElements << "float32 elements";
    
    return output;
}

void InferenceEngine::buildTransformerFromQ2kCache()
{
    if (m_tensorCache.isEmpty()) {
        qWarning() << "Cannot build transformer - no tensors cached";
        return;
    }
    
    QMutexLocker lock(&m_mutex);
    
    qInfo() << "Building transformer from Q2_K cache:" << m_tensorCache.size() << "tensors";
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:buildTransformerFromQ2kCache "
                           "msg=\"building transformer\" tensor_count=%2")
                        .arg(nowIsoOffset())
                        .arg(m_tensorCache.size()));
    
    // Extract model architecture from tensor names
    // Q2_K models typically have: model.layers.N.attn.c_attn.weight, model.layers.N.mlp.c_fc.weight, etc.
    int maxLayer = -1;
    int nEmbd = 768;  // Default embedding dimension
    int nHead = 12;   // Default number of heads
    int nVocab = 50257;  // Default vocabulary size
    
    // Infer dimensions from tensor shapes
    // Most Q2_K models follow standard transformer architecture
    // This is a simplification - production code should parse metadata
    
    // Try to infer from embedding or token tensor
    for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
        const QString& name = it.key();
        const QByteArray& data = it.value();
        
        // Estimate embedding dimension from token_emb or similar
        if (name.contains("token_emb", Qt::CaseInsensitive) || 
            name.contains("wte", Qt::CaseInsensitive)) {
            // Each vocab entry is nEmbd floats
            nVocab = data.size() / (nEmbd * sizeof(float));
            qDebug() << "Inferred vocab size:" << nVocab;
        }
        
        // Extract max layer number
        QRegularExpression layerRe(R"(layers\.(\d+))");
        QRegularExpressionMatch match = layerRe.match(name);
        if (match.hasMatch()) {
            int layer = match.captured(1).toInt();
            if (layer > maxLayer) maxLayer = layer;
        }
    }
    
    int nLayers = std::max(0, maxLayer + 1);
    if (nLayers == 0) {
        // Use metadata if available, otherwise safe default
        if (m_parser && m_parser->isValid()) {
            const GGUFMetadata& meta = m_parser->metadata();
            nLayers = meta.n_layer > 0 ? meta.n_layer : 12;
            nEmbd = meta.n_embd > 0 ? meta.n_embd : 768;
            nHead = meta.n_head > 0 ? meta.n_head : 12;
            nVocab = meta.vocab_size > 0 ? meta.vocab_size : 50257;
            
            qInfo() << "Using configuration from GGUF metadata";
        } else {
            nLayers = 12;  // Safe default
            qWarning() << "No parser available, using default configuration";
        }
    }
    
    // Validate inferred dimensions
    if (nLayers < 1 || nLayers > 200) {
        qWarning() << "Invalid layer count:" << nLayers << ", using default 12";
        nLayers = 12;
    }
    if (nEmbd < 256 || nEmbd > 16384) {
        qWarning() << "Invalid embedding dimension:" << nEmbd << ", using default 768";
        nEmbd = 768;
    }
    if (nHead < 1 || nHead > 256) {
        qWarning() << "Invalid head count:" << nHead << ", using default 12";
        nHead = 12;
    }
    if (nVocab < 1000 || nVocab > 500000) {
        qWarning() << "Invalid vocab size:" << nVocab << ", using default 50257";
        nVocab = 50257;
    }
    
    // Ensure proper attention dimension divisibility
    int headDim = nEmbd / nHead;
    if (nEmbd % nHead != 0) {
        qWarning() << "Embedding dimension not divisible by head count, adjusting nEmbd to" << nHead * headDim;
        nEmbd = nHead * headDim;
    }
    
    qInfo() << "Transformer configuration (validated): nLayers=" << nLayers
            << "nEmbd=" << nEmbd << "nHead=" << nHead 
            << "headDim=" << headDim << "nVocab=" << nVocab;
    
    // Load weights into transformer
    bool success = m_transformer.loadWeights(m_tensorCache, nLayers, nEmbd, nHead, nVocab);
    
    if (success) {
        qInfo() << "Transformer successfully built from Q2_K tensors";
        emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:buildTransformerFromQ2kCache "
                               "msg=\"transformer built\" layers=%2 embd=%3 heads=%4")
                            .arg(nowIsoOffset())
                            .arg(nLayers)
                            .arg(nEmbd)
                            .arg(nHead));
    } else {
        qWarning() << "Failed to build transformer from Q2_K cache";
        emit logMessage(QString("time=%1 level=ERROR source=inference_engine.cpp:buildTransformerFromQ2kCache "
                               "msg=\"transformer build failed\"")
                            .arg(nowIsoOffset()));
    }
}




