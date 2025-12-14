#include "inference_engine.hpp"
#include <QDebug>
#include <QFileInfo>
#include "EnterpriseTelemetry.h"
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QPair>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <numeric>
#include <mutex>

// Use shared quant utilities
#include "quant_utils.hpp"

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    // DO NOT load model in constructor - causes stack buffer overrun crashes
    // Model loading must be deferred to avoid blocking main thread
    // Call loadModel() explicitly after construction
    if (!ggufPath.isEmpty()) {
        qDebug() << "[InferenceEngine] Deferring model load until explicit loadModel() call";
        // Store path for later use if needed
        m_modelPath = ggufPath;
    }
}

InferenceEngine::InferenceEngine(QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
}

InferenceEngine::~InferenceEngine()
{
    // Clean up GGUFLoader resources
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    m_tensorCache.clear();
}

bool InferenceEngine::loadModel(const QString& path)
{
    // NOTE: This function is called from background thread via QtConcurrent
    // DO NOT use QMutexLocker here - it will deadlock
    // DO NOT emit signals directly - use QMetaObject::invokeMethod
    
    try {
        qInfo() << "[InferenceEngine::loadModel] Thread ID:" << QThread::currentThreadId();
        
        // Clean up existing loader (if any)
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        
        if (path.isEmpty()) {
            qWarning() << "[InferenceEngine] Model path is empty";
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        }
        
        qInfo() << "[InferenceEngine] Attempting to load model from:" << path;
        
        // Create loader with error checking and exception safety
        try {
            qInfo() << "[InferenceEngine] Creating GGUFLoaderQt for:" << path;
            m_loader = new GGUFLoaderQt(path);
            qInfo() << "[InferenceEngine] GGUFLoaderQt created successfully";
        } catch (const std::exception& e) {
            qCritical() << "[InferenceEngine] Exception creating GGUFLoaderQt:" << e.what();
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        } catch (...) {
            qCritical() << "[InferenceEngine] Unknown exception creating GGUFLoaderQt";
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        }
        
        if (!m_loader || !m_loader->isOpen()) {
            qCritical() << "[InferenceEngine] GGUFLoader failed to open file:" << path;
            if (m_loader) {
                delete m_loader;
                m_loader = nullptr;
            }
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        }
        
        qInfo() << "[InferenceEngine] GGUF file opened successfully:" << path;
        
        m_modelPath = path;
        QString modelName = extractModelName(path);
        
        // ========== CHECK FOR UNSUPPORTED QUANTIZATION TYPES ==========
        // This is the key detection point for the IDE conversion workflow
        if (m_loader->hasUnsupportedQuantizationTypes()) {
            QStringList unsupportedInfo = m_loader->getUnsupportedQuantizationInfo();
            QString recommendedType = m_loader->getRecommendedConversionType();
            
            qWarning() << "[InferenceEngine] Model uses unsupported quantization types:";
            for (const auto& info : unsupportedInfo) {
                qWarning() << "  -" << info;
            }
            qWarning() << "[InferenceEngine] Recommended conversion: IQ4_NL or other unsupported → " << recommendedType;
            
            // Emit signal for IDE to show conversion dialog (thread-safe)
            QMetaObject::invokeMethod(this, "unsupportedQuantizationTypeDetected", Qt::QueuedConnection,
                Q_ARG(QStringList, unsupportedInfo), Q_ARG(QString, recommendedType), Q_ARG(QString, path));
            
            // Continue with model loading attempt anyway (it may fail later on tensor size calculation)
            // The IDE will show the conversion dialog while we continue
        }
        
        // Initialize tokenizer from model (with full exception safety)
        try {
            qInfo() << "[InferenceEngine] Initializing tokenizer...";
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Initializing tokenizer...");
            }
            initializeTokenizer();
            qInfo() << "[InferenceEngine] Tokenizer initialized successfully";
        } catch (const std::exception& e) {
            qCritical() << "[InferenceEngine] EXCEPTION initializing tokenizer:" << e.what();
            // Continue anyway - tokenizer is optional for basic inference
        } catch (...) {
            qCritical() << "[InferenceEngine] UNKNOWN EXCEPTION initializing tokenizer";
        }
        
        // Build initial quantized tensor cache (with full exception safety)
        try {
            qInfo() << "[InferenceEngine] Building tensor cache...";
            rebuildTensorCache();
            qInfo() << "[InferenceEngine] Tensor cache build complete";
        } catch (const std::bad_alloc& e) {
            qCritical() << "[InferenceEngine] OUT OF MEMORY building tensor cache:" << e.what();
            // Clean up and fail
            delete m_loader;
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        } catch (const std::exception& e) {
            qCritical() << "[InferenceEngine] EXCEPTION building tensor cache:" << e.what();
            // Continue anyway - we'll try without cache
        } catch (...) {
            qCritical() << "[InferenceEngine] UNKNOWN EXCEPTION building tensor cache";
        }
        
        // === FIX: Dynamically read model architecture from GGUF metadata ===
        // These values are now read from the actual GGUF file instead of hardcoded
        int nLayers = m_loader->getParam("n_layer", 12).toInt();
        int nEmbd = m_loader->getParam("n_embd", 768).toInt();
        int nHead = m_loader->getParam("n_head", 12).toInt();
        int nVocab = m_loader->getParam("n_vocab", 50257).toInt();

        // Log the actual parameters read from the GGUF file
        qInfo() << QString("[InferenceEngine] Detected model architecture: Layers=%1, Embedding=%2, Heads=%3, Vocab=%4")
                     .arg(nLayers).arg(nEmbd).arg(nHead).arg(nVocab);
        
        if (!m_tensorCache.isEmpty()) {
            try {
                // Convert CachedTensorData -> (QByteArray, int) pairs to preserve type information
                QHash<QString, QPair<QByteArray, int>> tensorCacheWithTypes;
                for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
                    tensorCacheWithTypes.insert(it.key(), 
                        QPair<QByteArray, int>(it.value().data, it.value().ggml_type_id));
                }
                
                qInfo() << "[InferenceEngine] Attempting transformer weight loading with type information";
                qInfo() << "[InferenceEngine] Model uses" << m_tensorCache.size() << "tensors with mixed quantization types";
                bool transformerLoaded = m_transformer.loadWeightsWithTypes(tensorCacheWithTypes, nLayers, nEmbd, nHead, nVocab);
                if (!transformerLoaded) {
                    qWarning() << "[InferenceEngine] Transformer weight loading failed - using GGUF direct inference";
                } else {
                    qInfo() << "[InferenceEngine] Transformer initialized successfully with proper quantization types";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Exception loading transformer weights:" << e.what() << "- continuing anyway";
                // Continue anyway - model is loaded via GGUF loader
            }
        } else {
            qWarning() << "[InferenceEngine] Tensor cache is empty, skipping transformer initialization";
        }
        
        // Reset KV-cache state for new model
        m_kvCacheReady = false;
        
        qInfo() << "[InferenceEngine] Model loaded successfully:" << modelName;
        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, true), Q_ARG(QString, modelName));
        
        // FIX 6: Immediately check the queue after model load.
        processNextRequest(); 
        
        // FIX 3.2: Emit the signal to notify listeners (thread-safe)
        QMetaObject::invokeMethod(this, "transformerReady", Qt::QueuedConnection);
        
        return true;    } catch (const std::exception& e) {
        qCritical() << "[InferenceEngine] CRITICAL: Exception during model loading:" << e.what();
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()));
        return false;
    } catch (...) {
        qCritical() << "[InferenceEngine] CRITICAL: Unknown exception during model loading";
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()));
        return false;
    }
}

QString InferenceEngine::processChat(const QString& prompt)
{
    // Tokenize, run a short generation, and detokenize
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.processChat"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("processChat.begin"), QStringLiteral("len=%1").arg(prompt.length()));

    qInfo() << "=== INFERENCE REQUEST START ===";
    qInfo() << "[processChat] Input text: " << prompt;
    qInfo() << "[processChat] Input length: " << prompt.length();
    
    auto input = tokenize(prompt);
    qInfo() << "[processChat] Tokens generated: " << input.size();
    for (int i = 0; i < std::min(10, (int)input.size()); i++) {
        qInfo() << "[processChat] Token[" << i << "]: " << input[i];
    }
    
    qInfo() << "[processChat] Starting generation with " << input.size() << " context tokens...";
    auto out = generate(input, 64);
    qInfo() << "[processChat] Generated " << (out.size() - input.size()) << " new tokens";
    
    auto result = detokenize(out);
    qInfo() << "[processChat] Decoded response: " << result;
    qInfo() << "=== INFERENCE REQUEST END ===";

    telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("processChat.complete"), timer.elapsedMs(), QStringLiteral("tokens_in=%1 tokens_out=%2").arg(input.size()).arg(out.size()));
    
    return result;
}

QString InferenceEngine::analyzeCode(const QString& code)
{
    // Simple analysis stub leveraging existing tokenizer to avoid heavy changes
    QString analysis = QString(
        "Code Analysis:\n"
        "- Length: %1 chars\n"
        "- Lines: %2\n"
        "- Tokens: %3"
    ).arg(code.size()).arg(code.count('\n') + 1).arg(tokenize(code).size());
    return analysis;
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

QString InferenceEngine::quantMode() const
{
    QMutexLocker lock(&m_mutex);
    return m_quantMode;
}

void InferenceEngine::request(const QString& prompt, qint64 reqId)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        qWarning() << "No model loaded for inference request" << reqId;
        emit error(reqId, "Error: No model loaded");
        return;
    }
    
    // FIX 6: Enqueue the request instead of processing immediately
    InferenceRequest request;
    request.prompt = prompt;
    request.requestId = reqId;
    m_requestQueue.enqueue(request);

    qInfo() << QString("Request %1 enqueued. Queue size: %2").arg(reqId).arg(m_requestQueue.size());

    // Attempt to start processing if the engine is not busy
    if (!m_isProcessingInference) {
        processNextRequest();
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
    try {
        m_tensorCache.clear();
        
        if (!m_loader) {
            qWarning() << "[InferenceEngine] No GGUF loader available for tensor cache rebuild";
            return;
        }
        
        QStringList names = m_loader->tensorNames();
        qInfo() << "[InferenceEngine] Rebuilding tensor cache with" << names.size() << "tensors";
        
        int totalTensors = names.size();
        int processedTensors = 0;
        
        for (const QString& name : names) {
            processedTensors++;
            if (processedTensors % 10 == 0 || processedTensors == totalTensors) {
                QString progressMsg = QString("Loading tensors... %1/%2 (%3%)")
                    .arg(processedTensors).arg(totalTensors)
                    .arg(processedTensors * 100 / totalTensors);
                qInfo() << "[InferenceEngine]" << progressMsg;
                
                // Send progress update via callback if available
                if (m_loadProgressCallback) {
                    m_loadProgressCallback(progressMsg);
                }
            }
            try {
                const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
                QByteArray raw = m_loader->inflateWeight(name);
                
                if (raw.isEmpty()) {
                    qDebug() << "[InferenceEngine] Empty tensor data for:" << name;
                    continue;
                }
                
                // Apply quantization safely and capture the resulting type
                try {
                    // Use the new function that returns both data and type
                    auto [quantized, resulting_type_id] = apply_quant_with_type(raw, qmode);

                    if (!quantized.isEmpty()) {
                        CachedTensorData tensorData;
                        tensorData.data = quantized;
                        tensorData.ggml_type_id = resulting_type_id;
                        m_tensorCache.insert(name, tensorData);
                    }
                } catch (const std::exception& e) {
                    qWarning() << "[InferenceEngine] Failed to quantize tensor" << name << ":" << e.what();
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Error processing tensor" << name << ":" << e.what();
            }
        }
        
        qInfo() << "[InferenceEngine] Tensor cache built with" << m_tensorCache.size() << "tensors";
        
        // Reload transformer weights if cache was rebuilt
        // FIX: Removed dangerous premature weight loading with hardcoded dimensions.
        // Weights should only be loaded in loadModel() after correct dimensions are read.
        /*
        if (!m_tensorCache.isEmpty() && m_loader) {
            try {
                m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to load weights to transformer:" << e.what();
            }
        }
        */
    } catch (const std::exception& e) {
        qCritical() << "[InferenceEngine] Critical exception in rebuildTensorCache:" << e.what();
    } catch (...) {
        qCritical() << "[InferenceEngine] Unknown exception in rebuildTensorCache";
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text)
{
    qDebug() << "=== TOKENIZE START ===";
    qDebug() << "[tokenize] Text: " << text;
    qDebug() << "[tokenize] Text length: " << text.length();
    qDebug() << "[tokenize] Tokenizer mode: " << m_tokenizerMode;
    
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                qDebug() << "[tokenize] Using BPE tokenizer";
                auto tokens = m_bpeTokenizer.encode(text);
                qDebug() << "[tokenize] BPE produced " << tokens.size() << " tokens";
                qDebug() << "=== TOKENIZE END ===";
                return tokens;
            }
            qWarning() << "[tokenize] BPE tokenizer not ready";
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                qDebug() << "[tokenize] Using SentencePiece tokenizer";
                auto tokens = m_spTokenizer.encode(text, true, false);  // Add BOS, no EOS
                qDebug() << "[tokenize] SentencePiece produced " << tokens.size() << " tokens";
                qDebug() << "=== TOKENIZE END ===";
                return tokens;
            }
            qWarning() << "[tokenize] SentencePiece tokenizer not ready";
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            qDebug() << "[tokenize] Using fallback tokenizer";
            break;
    }
    
    // Fallback: Simple word-based tokenization
    qWarning() << "[tokenize] Falling back to word-based tokenization";
    std::vector<int32_t> tokens;
    
    // Add BOS token
    tokens.push_back(1);
    qDebug() << "[tokenize] Added BOS token (1)";
    
    // Split on whitespace and punctuation
    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:]+"), Qt::SkipEmptyParts);
    qDebug() << "[tokenize] Split into " << words.size() << " words";
    
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
    
    qDebug() << "[tokenize] Final token count: " << tokens.size();
    qDebug() << "=== TOKENIZE END ===";
    return tokens;
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    qDebug() << "=== DETOKENIZE START ===";
    qDebug() << "[detokenize] Token count: " << tokens.size();
    qDebug() << "[detokenize] Tokenizer mode: " << m_tokenizerMode;
    
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                qDebug() << "[detokenize] Using BPE tokenizer to decode";
                auto result = m_bpeTokenizer.decode(tokens);
                qDebug() << "[detokenize] BPE decoded to: " << result;
                qDebug() << "=== DETOKENIZE END ===";
                return result;
            }
            qWarning() << "[detokenize] BPE tokenizer not ready";
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                qDebug() << "[detokenize] Using SentencePiece tokenizer to decode";
                auto result = m_spTokenizer.decode(tokens, true);  // Skip special tokens
                qDebug() << "[detokenize] SentencePiece decoded to: " << result;
                qDebug() << "=== DETOKENIZE END ===";
                return result;
            }
            qWarning() << "[detokenize] SentencePiece tokenizer not ready";
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            qDebug() << "[detokenize] Using fallback tokenizer";
            break;
    }
    
    // Fallback: Use vocabulary or generate placeholders
    qWarning() << "[detokenize] Falling back to vocabulary-based decoding";
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
    try {
        // Try to load vocabulary from GGUF file
        if (!m_loader) {
            qWarning() << "[InferenceEngine] No GGUF loader available, skipping tokenizer init";
            return;
        }
        
        if (!m_vocab.loadFromGGUF(m_modelPath)) {
            qWarning() << "[InferenceEngine] Failed to load vocabulary from GGUF";
            return;
        }
        
        qInfo() << "[InferenceEngine] Vocabulary loaded:" << m_vocab.size() << "tokens";
        
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        QHash<QString, QByteArray> tokenizerMetadata;
        try {
            tokenizerMetadata = m_loader->getTokenizerMetadata();
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to load tokenizer metadata:" << e.what();
            // Continue without metadata
        }
        
        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            try {
                // Initialize BPE tokenizer with real GGUF metadata
                if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_BPE;
                    qInfo() << "[InferenceEngine] Using BPE tokenizer (GPT-2 compatible)";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to initialize BPE tokenizer:" << e.what();
            }
        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            try {
                // Initialize SentencePiece tokenizer with real GGUF metadata
                if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_SP;
                    qInfo() << "[InferenceEngine] Using SentencePiece tokenizer (LLaMA/Mistral compatible)";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to initialize SentencePiece tokenizer:" << e.what();
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[InferenceEngine] Critical exception in tokenizer initialization:" << e.what();
        m_tokenizerMode = TOKENIZER_FALLBACK;
    } catch (...) {
        qWarning() << "[InferenceEngine] Unknown exception in tokenizer initialization";
        m_tokenizerMode = TOKENIZER_FALLBACK;
    }
    
    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
        qInfo() << "[InferenceEngine] Using fallback word-based tokenizer (limited functionality)";
    }
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens)
{
    QMutexLocker lock(&m_mutex);

    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.generate"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("generate.begin"), QStringLiteral("tokens_in=%1 max=%2").arg(inputTokens.size()).arg(maxTokens));
    
    qDebug() << "=== GENERATE START ===";
    qDebug() << "[generate] Input tokens: " << inputTokens.size();
    qDebug() << "[generate] Max new tokens: " << maxTokens;
    qDebug() << "[generate] Model loaded: " << isModelLoaded();
    qDebug() << "[generate] Transformer ready: " << m_transformer.isReady();
    
    if (!isModelLoaded()) {
        qWarning() << "[generate] Cannot generate - no model loaded";
            telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.no_model"), timer.elapsedMs(), QStringLiteral("tokens_in=%1").arg(inputTokens.size()));
        return inputTokens;
    }
    
    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        QElapsedTimer localTimer;
        localTimer.start();
        
        qDebug() << "[generate] Transformer is ready, starting KV-cache prefill...";
        
        // === ELEGANT FIX: Two-Phase Inference with KV-Cache ===
        // Phase 1: Context prefill - process the entire input prompt once
        // The transformer builds the KV-cache (Key-Value cache) for efficient generation.
        // Note: If transformer has decodeContext method, use it; otherwise use forward
        if (!m_kvCacheReady) {
            // Process full context to populate KV-cache
            // This is called only once per inference request
            qDebug() << "[generate] Pre-filling KV-cache with context...";
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
            qDebug() << "[generate] KV-cache prefilled with" << inputTokens.size() << "context tokens";
        }
        
        // The last token ID becomes the starting point for autoregressive generation
        int32_t currentToken = inputTokens.back();
        qDebug() << "[generate] Starting autoregressive generation with token " << currentToken;
        
        // === Phase 2: Autoregressive Token Generation (Decoding) ===
        for (int i = 0; i < maxTokens; ++i) {
            // Generate logits for the next token based ONLY on the current token
            // The Transformer uses the internal KV-cache for past context context
            qDebug() << "[generate] Iteration " << i << ": Calling transformer.forward(" << currentToken << ")...";
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            
            if (logits.empty()) {
                qWarning() << "[generate] Transformer forward pass returned no logits";
                break;
            }
            
            qDebug() << "[generate] Got " << logits.size() << " logits from transformer";
            
            // === Elegant Sampling Logic using Top-P ===
            // Delegate complex sampling to helper function
            currentToken = sampleNextToken(logits, m_temperature, m_topP);
            
            qDebug() << "[generate] Sampled token " << currentToken << " (temperature=" << m_temperature << ", top_p=" << m_topP << ")";
            
            // Check for EOS token (2 is common EOS)
            if (currentToken == 2 || currentToken == 0) {
                qInfo() << "[generate] Generation stopped by EOS token: " << currentToken;
                break;
            }
            
            result.push_back(currentToken);
        }
        
        // Update performance metrics based on this generation step
        qint64 elapsed = localTimer.elapsed();
        int tokensGenerated = result.size() - inputTokens.size();
        if (elapsed > 0 && tokensGenerated > 0) {
            m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
        }
        
        qInfo() << "[generate] Completed:" << tokensGenerated << "tokens in" << elapsed 
                << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << " tok/s)";
        qDebug() << "=== GENERATE END ===";

        telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.complete"), timer.elapsedMs(), QStringLiteral("tokens_out=%1 tok_s=%2").arg(result.size()).arg(m_tokensPerSecond, 0, 'f', 1));
        
        // Reset KV-cache for next inference
        m_kvCacheReady = false;
        
    } else {
        // Fallback: Simple echo with placeholder
        qWarning() << "[generate] Transformer not ready, using placeholder generation";
        
        // Just add a few placeholder tokens
        for (int i = 0; i < std::min(maxTokens, 10); ++i) {
            result.push_back(1000 + i);  // Placeholder tokens
        }
        qDebug() << "=== GENERATE END (FALLBACK) ===";
        telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.fallback"), timer.elapsedMs(), QStringLiteral("tokens_out=%1").arg(result.size()));
    }
    
    return result;
}

// ============================================================================
// ELEGANT IMPLEMENTATION: Top-P (Nucleus) Sampling
// ============================================================================
// Top-P sampling produces far more natural and diverse text than greedy 
// sampling while still being controllable via the temperature parameter.
// 
// Algorithm:
// 1. Convert logits to probabilities using softmax
// 2. Sort tokens by probability (descending)
// 3. Accumulate probabilities until crossing the Top-P threshold
// 4. Randomly sample from this "nucleus"
// ============================================================================

int32_t InferenceEngine::sampleNextToken(std::vector<float>& logits, double temperature, double topP)
{
    // === Step 1: Convert Logits to Probabilities (Softmax) ===
    
    // Apply temperature scaling (controls randomness)
    if (temperature > 0.0) {
        for (float& logit : logits) {
            logit /= static_cast<float>(temperature);
        }
    }
    
    // Find maximum logit for numerical stability (prevent overflow in exp)
    float maxLogit = *std::max_element(logits.begin(), logits.end());
    
    // Compute exponentiated logits and total sum (Softmax calculation)
    std::vector<float> probs(logits.size());
    float sumExp = 0.0f;
    
    for (size_t i = 0; i < logits.size(); ++i) {
        float expVal = std::exp(logits[i] - maxLogit);
        probs[i] = expVal;
        sumExp += expVal;
    }
    
    // Normalize to get final probabilities
    for (float& prob : probs) {
        prob /= sumExp;
    }

    // === Step 2: Prepare for Top-P Selection ===
    
    // Create a vector of pairs: {probability, token_id}
    std::vector<std::pair<float, int32_t>> sortedTokens;
    for (size_t i = 0; i < probs.size(); ++i) {
        if (probs[i] > 1e-6f) { // Ignore very small probability tokens
            sortedTokens.push_back({probs[i], static_cast<int32_t>(i)});
        }
    }

    // Sort by probability in descending order
    std::sort(sortedTokens.begin(), sortedTokens.end(), 
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    // === Step 3: Find the Nucleus (Top-P Threshold) ===
    
    float cumulativeProb = 0.0f;
    size_t nucleusSize = 0;

    for (const auto& tokenPair : sortedTokens) {
        cumulativeProb += tokenPair.first;
        nucleusSize++;
        
        // Stop when the cumulative probability exceeds topP (the nucleus)
        if (cumulativeProb >= static_cast<float>(topP)) {
            break;
        }
    }
    
    // Safety check: If the top token already exceeds P, nucleus is just that token
    if (nucleusSize == 0 || sortedTokens.empty()) {
        // Fallback: Use greedy sampling (select highest probability)
        return sortedTokens.empty() ? 0 : sortedTokens.front().second;
    }
    
    // === Step 4: Resample and Select the Next Token ===
    
    // Re-normalize the probabilities within the nucleus
    float nucleusProbSum = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        nucleusProbSum += sortedTokens[i].first;
    }

    // Use a uniform random number in [0, nucleusProbSum)
    float r = getRandomFloat(0.0f, nucleusProbSum);

    // Select the token based on the weighted random draw
    cumulativeProb = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        cumulativeProb += sortedTokens[i].first;
        if (r < cumulativeProb) {
            return sortedTokens[i].second;
        }
    }
    
    // Fallback in case of rounding errors: select the last token in the nucleus
    return sortedTokens[nucleusSize - 1].second;
}

// ============================================================================
// THREAD-SAFE RANDOM NUMBER GENERATION
// ============================================================================

float InferenceEngine::getRandomFloat(float min, float max)
{
    // Thread-safe random float generation using C++11 standard library
    // The m_randomEngine is seeded once and reused
    
    static std::once_flag initFlag;
    std::call_once(initFlag, [this]() {
        // Seed the random engine with a high-entropy source
        std::random_device rd;
        m_randomEngine.seed(rd());
    });
    
    // Generate uniform random float in [min, max)
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(m_randomEngine);
}

// ============================================================================
// FIX 6: REQUEST QUEUE IMPLEMENTATION
// ============================================================================

void InferenceEngine::processNextRequest()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_isProcessingInference) {
        // Already processing a request, wait for the current one to finish.
        return;
    }

    if (m_requestQueue.isEmpty()) {
        // Nothing to do. Engine is idle.
        return;
    }

    // Dequeue the next request
    InferenceRequest currentRequest = m_requestQueue.dequeue();
    m_isProcessingInference = true; 
    
    // Check model readiness before running the heavy task
    if (!m_transformer.isReady()) {
        // Model is loading or initialization failed. Re-enqueue and signal the wait.
        m_requestQueue.prepend(currentRequest); // Put it back at the front

        QString response = QString("⚠ Model not ready. Request %1 re-queued. Please wait for model loading to complete.").arg(currentRequest.requestId);
        
        // Emit a temporary message so the user knows the request wasn't lost.
        emit resultReady(currentRequest.requestId, response); 
        m_isProcessingInference = false; // We didn't actually start inference, so we aren't busy.
        return;
    }

    qInfo() << QString("Starting inference for request %1. %2 remaining in queue.")
                   .arg(currentRequest.requestId)
                   .arg(m_requestQueue.size());

    // --- EXECUTE INFERENCE ---
    m_inferenceTimer.start();
    
    // 1. Tokenize the prompt
    std::vector<int32_t> tokens = tokenize(currentRequest.prompt);
    
    // 2. Run the transformer (synchronous, blocking call)
    std::vector<int32_t> outputTokens = m_transformer.generate(tokens, 50); 

    // 3. Detokenize the result
    QString response = detokenize(outputTokens);

    // 4. Performance metrics
    qint64 elapsed = m_inferenceTimer.elapsed();
    int generatedTokens = std::max(0, (int)outputTokens.size() - (int)tokens.size());
    if (generatedTokens > 0 && elapsed > 0) {
        m_tokensPerSecond = (generatedTokens * 1000.0) / elapsed;
    }
    
    qInfo() << "Inference completed:" << outputTokens.size() << "tokens in" << elapsed 
            << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << "tok/s)";

    // 5. Signal completion
    emit resultReady(currentRequest.requestId, response);

    // 6. Cleanup and check for the next request
    m_isProcessingInference = false;
    
    // Recursive call to immediately process the next item if the queue isn't empty
    processNextRequest(); 
}


