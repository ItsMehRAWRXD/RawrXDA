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

// Use shared quant utilities
#include "quant_utils.hpp"

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr)
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
    
    // Emit a log that we're starting to load
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"loading gguf\" path=\"%2\"")
                        .arg(nowIsoOffset(), path));

    m_loader = new GGUFLoader(path);
    
    if (!m_loader->isOpen()) {
        qWarning() << "Failed to load GGUF model:" << path;
        emit logMessage(QString("time=%1 level=ERROR source=inference_engine.cpp:loadModel msg=\"failed to open gguf\" path=\"%2\"")
                            .arg(nowIsoOffset(), path));
        delete m_loader;
        m_loader = nullptr;
        emit modelLoadedChanged(false, QString());
        return false;
    }
    
    m_modelPath = path;
    QString modelName = extractModelName(path);
    qInfo() << "Model loaded successfully:" << modelName;
    emit logMessage(QString("time=%1 level=INFO source=inference_engine.cpp:loadModel msg=\"model opened\" name=\"%2\"")
                        .arg(nowIsoOffset(), modelName));
    
    // Initialize tokenizer from model
    initializeTokenizer();
    
    // Build initial quantized tensor cache
    rebuildTensorCache();
    
    // Initialize transformer with model architecture
    // Using standard GPT-2 like architecture as default
    int nLayers = 12;
    int nEmbd = 768;
    int nHead = 12;
    int nVocab = 50257;
    
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
    
    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        // Use transformer for generation
        m_inferenceTimer.start();
        
        for (int i = 0; i < maxTokens; ++i) {
            // Get logits for next token
            std::vector<float> logits = m_transformer.forward(result);
            
            if (logits.empty()) {
                qWarning() << "Transformer forward pass returned no logits";
                break;
            }
            
            // Apply temperature
            if (m_temperature != 1.0) {
                for (float& logit : logits) {
                    logit /= m_temperature;
                }
            }
            
            // Sample next token (greedy for now, could add top-k/nucleus sampling)
            int32_t nextToken = 0;
            float maxLogit = logits[0];
            for (size_t j = 1; j < logits.size(); ++j) {
                if (logits[j] > maxLogit) {
                    maxLogit = logits[j];
                    nextToken = static_cast<int32_t>(j);
                }
            }
            
            // Check for EOS token (2 is common EOS)
            if (nextToken == 2 || nextToken == 0) {
                break;
            }
            
            result.push_back(nextToken);
        }
        
        // Update performance metrics
        qint64 elapsed = m_inferenceTimer.elapsed();
        int tokensGenerated = result.size() - inputTokens.size();
        if (elapsed > 0 && tokensGenerated > 0) {
            m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
        }
        
    } else {
        // Fallback: Simple echo with placeholder
        qWarning() << "Transformer not ready, using placeholder generation";
        
        // Just add a few placeholder tokens
        for (int i = 0; i < std::min(maxTokens, 10); ++i) {
            result.push_back(1000 + i);  // Placeholder tokens
        }
    }
    
    return result;
}



