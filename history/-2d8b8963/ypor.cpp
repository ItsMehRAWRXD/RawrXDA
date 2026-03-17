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
    
    if (!m_tensorCache.isEmpty()) {
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
    
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"gguf loaded\" tensors_cached=%2 quant=\"%3\"")
                        .arg(nowIsoOffset())
                        .arg(m_tensorCache.size())
                        .arg(m_quantMode));
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
    
    QStringList names = m_loader->tensorNames();
    for (const QString& name : names) {
        const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
        QByteArray raw = m_loader->inflateWeight(name);
        if (raw.isEmpty()) continue;
        m_tensorCache.insert(name, apply_quant(raw, qmode));
    }
    
    // Reload transformer weights if cache was rebuilt
    if (!m_tensorCache.isEmpty() && m_loader) {
        m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
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
    
    // Fallback: Simple word-based tokenization
    // TODO: This is a placeholder - production should always use proper tokenizer
    std::vector<int32_t> tokens;
    
    // Add BOS token
    tokens.push_back(1);
    
    // Split on whitespace and punctuation
    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:]+"), Qt::SkipEmptyParts);
    
    for (const QString& word : words) {
        // Use vocabulary if available
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                // Hash unknown words
                uint32_t hash = qHash(word.toLower());
                tokens.push_back((hash % 50000) + 256);
            }
        } else {
            // Pure fallback: hash-based
            uint32_t hash = qHash(word.toLower());
            tokens.push_back((hash % 50000) + 256);
        }
    }
    
    // Add EOS token
    tokens.push_back(2);
    
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
        qWarning() << "Transformer not ready, using placeholder generation";
        emit logMessage(QString("time=%1 level=WARN source=inference_engine.cpp:generate msg=\"transformer not ready\"")
                            .arg(nowIsoOffset()));
        
        // Fallback: Simple echo with placeholder
        for (int i = 0; i < std::min(maxTokens, 10); ++i) {
            result.push_back(1000 + i);  // Placeholder tokens
        }
        return result;
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
        
        // Check for end-of-sequence tokens
        // Common EOS tokens: 2 (GPT-2), 50256 (GPT-2 EOT), 1 (some models), 0 (padding/invalid)
        if (nextToken == 0 || nextToken == 2 || nextToken == 1 || nextToken == 50256) {
            qInfo() << "EOS token encountered:" << nextToken << "at step" << step;
            eosEncountered = true;
            // Optionally add EOS to result
            result.push_back(nextToken);
            tokensGenerated++;
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




