#include "inference_engine.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QElapsedTimer>
#include "EnterpriseTelemetry.h"
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QPair>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <numeric>
#include <mutex>
#include <QtConcurrent/QtConcurrentRun>
#include <cstdlib>
#include <iostream>
#include "ollama_proxy.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>

// Agentic failure detection and correction
// #include "../agent/agentic_failure_detector.hpp"
// #include "../agent/agentic_puppeteer.hpp"

// Use shared quant utilities
#include "quant_utils.hpp"
#include "settings_manager.h"
#include <QProcessEnvironment>

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    m_failureDetector = new AgenticFailureDetector(this);
    m_puppeteer = new AgenticPuppeteer(this);
    m_ollamaProxy = new OllamaProxy(this);

    // Connect OllamaProxy signals to InferenceEngine signals
    connect(m_ollamaProxy, &OllamaProxy::tokenArrived, this, [this](const QString& token) {
        emit streamToken(m_currentRequestId, token);
    });
    
    connect(m_ollamaProxy, &OllamaProxy::generationComplete, this, [this]() {
        emit streamFinished(m_currentRequestId);
    });

    connect(m_ollamaProxy, &OllamaProxy::error, this, [this](const QString& msg) {
        qWarning() << "[InferenceEngine] Ollama error:" << msg;
        emit streamFinished(m_currentRequestId);
    });

    // Initialize with default Ollama model directory (configurable)
    setModelDirectory(defaultModelDirectory());

    // DO NOT load model in constructor - causes stack buffer overrun crashes
    // Model loading must be deferred to avoid blocking main thread
    // Call loadModel() explicitly after construction
    if (!ggufPath.isEmpty()) {
        qDebug() << "[InferenceEngine] Deferring model load until explicit loadModel() call";
        // Store path for later use if needed
        m_modelPath = ggufPath;
    }
}

void InferenceEngine::setOllamaModel(const QString& modelName)
{
    m_useOllama = true;
    m_modelPath.clear();
    if (m_ollamaProxy) {
        m_ollamaProxy->setModel(modelName);
    }
    QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
        Q_ARG(bool, true), Q_ARG(QString, modelName));
}

InferenceEngine::InferenceEngine(QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    m_failureDetector = new AgenticFailureDetector(this);
    m_puppeteer = new AgenticPuppeteer(this);
    m_ollamaProxy = new OllamaProxy(this);
    
    connect(m_ollamaProxy, &OllamaProxy::tokenArrived, this, [this](const QString& token) {
        emit streamToken(m_currentRequestId, token);
    });
    connect(m_ollamaProxy, &OllamaProxy::generationComplete, this, [this]() {
        emit streamFinished(m_currentRequestId);
    });
    connect(m_ollamaProxy, &OllamaProxy::error, this, [this](const QString& msg) {
        qWarning() << "[InferenceEngine] Ollama error:" << msg;
        emit streamFinished(m_currentRequestId);
    });

    // Initialize with default Ollama model directory (configurable)
    setModelDirectory(defaultModelDirectory());
}

QString InferenceEngine::generateSync(const QString& prompt, int maxTokens)
{
    QElapsedTimer timer;
    timer.start();
    qInfo() << "[InferenceEngine] Starting generateSync. Max tokens:" << maxTokens;

    QString response;
    if (m_useOllama && m_ollamaProxy) {
        response = m_ollamaProxy->generateResponseSync(prompt, m_temperature, maxTokens);
    } else {
        if (!isModelLoaded()) {
            qWarning() << "[InferenceEngine] generateSync failed: No model loaded (GGUF or Ollama)";
            return QString();
        }

        // Local model logic
        std::vector<int32_t> inputTokens = tokenize(prompt);
        std::vector<int32_t> outputTokens = generate(inputTokens, maxTokens);
        response = detokenize(outputTokens);
    }

    qInfo() << "[InferenceEngine] generateSync completed in" << timer.elapsed() << "ms. Result length:" << response.length();
    return response;
}

QString InferenceEngine::defaultModelDirectory()
{
    QString configuredPath = SettingsManager::instance().getValue("models/defaultPath", "").toString().trimmed();
    if (!configuredPath.isEmpty()) {
        qInfo() << "[InferenceEngine] Using configured Ollama model directory:" << configuredPath;
        return configuredPath;
    }

    QString envPath = qEnvironmentVariable("OLLAMA_MODELS").trimmed();
    if (!envPath.isEmpty()) {
        qInfo() << "[InferenceEngine] Using OLLAMA_MODELS environment directory:" << envPath;
        return envPath;
    }

    const QString fallback = "D:/OllamaModels";
    qInfo() << "[InferenceEngine] Falling back to default Ollama model directory:" << fallback;
    return fallback;
}

InferenceEngine::~InferenceEngine()
{
    if (m_ollamaProxy) {
        m_ollamaProxy->stopGeneration();
    }
    
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
        qInfo() << "[InferenceEngine::loadModel] CPU_ONLY check: CUDA_VISIBLE_DEVICES =" 
            << QString::fromLocal8Bit(std::getenv("CUDA_VISIBLE_DEVICES") ? std::getenv("CUDA_VISIBLE_DEVICES") : "(unset)");
        m_systemPromptTokens.clear();
        qInfo() << "[InferenceEngine::loadModel] Cleared cached system prompt tokens (fresh load)";
        
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
        
        // Check if this is an Ollama blob or model
        if (m_ollamaProxy->isBlobPath(path)) {
            qInfo() << "[InferenceEngine] Detected Ollama blob path, switching to OllamaProxy";
            m_useOllama = true;
            m_modelPath = path;
            QString modelName = m_ollamaProxy->resolveBlobToModel(path);
            m_ollamaProxy->setModel(modelName);
            
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, true), Q_ARG(QString, path));
            return true;
        }

        // Reset Ollama flag if loading a regular GGUF
        m_useOllama = false;

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

        // Enterprise deterministic defaults for coherent responses
        m_temperature = 0.0;
        m_topP = 1.0;
        qInfo() << "[InferenceEngine] Sampler pinned to deterministic defaults (temperature=0.0, top_p=1.0)";
        
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
        
        // Initialize tokenizer from model (with full exception safety and memory pressure handling)
        try {
            qInfo() << "[InferenceEngine] Initializing tokenizer...";
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Initializing tokenizer...");
            }
            initializeTokenizer();
            qInfo() << "[InferenceEngine] Tokenizer initialized successfully";
        } catch (const std::bad_alloc& e) {
            qWarning() << "[InferenceEngine] OUT OF MEMORY initializing tokenizer (large model) - using fallback";
            qWarning() << "[InferenceEngine] This is expected for very large models (40GB+), continuing with fallback tokenizer";
            m_tokenizerMode = TOKENIZER_FALLBACK;
            // Continue anyway - tokenizer is optional for basic inference
        } catch (const std::exception& e) {
            qCritical() << "[InferenceEngine] EXCEPTION initializing tokenizer:" << e.what();
            // Continue anyway - tokenizer is optional for basic inference
        } catch (...) {
            qCritical() << "[InferenceEngine] UNKNOWN EXCEPTION initializing tokenizer";
        }
        
        // Load vocabulary from GGUF file (CRITICAL for detokenization)
        try {
            qInfo() << "[InferenceEngine] Loading vocabulary from GGUF...";
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Loading vocabulary...");
            }
            
            if (m_loader && m_loader->isOpen()) {
                // Load vocabulary directly from the GGUF file using VocabularyLoader
                bool vocabLoaded = m_vocab.loadFromGGUF(path);
                if (vocabLoaded) {
                    qInfo() << "[InferenceEngine] Vocabulary loaded successfully from GGUF file";
                } else {
                    qWarning() << "[InferenceEngine] Failed to load vocabulary from GGUF, using fallback tokenization";
                }
            } else {
                qWarning() << "[InferenceEngine] GGUF loader not available for vocabulary loading";
            }
        } catch (const std::bad_alloc& e) {
            qWarning() << "[InferenceEngine] OUT OF MEMORY loading vocabulary (large 32K+ vocab) - using fallback";
            qWarning() << "[InferenceEngine] This is expected for very large models, detokenization will use simpler method";
            // Continue anyway - we'll use fallback detokenization
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] EXCEPTION loading vocabulary:" << e.what();
            // Continue anyway - we'll use fallback detokenization
        } catch (...) {
            qWarning() << "[InferenceEngine] UNKNOWN EXCEPTION loading vocabulary";
        }
        
        qInfo() << "[InferenceEngine] ===== CHECKPOINT A: Vocabulary complete =====";
        qInfo() << "[InferenceEngine] ===== CHECKPOINT B: About to enter initializeTokenizer =====";
        std::cerr << "[InferenceEngine] ===== CHECKPOINT B: About to enter initializeTokenizer =====" << std::endl;
        std::cerr.flush();
        
        // Build initial quantized tensor cache (with full exception safety)
        if (m_loadTensors.load()) {
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
        } else {
            qInfo() << "[InferenceEngine] Tensor loading disabled – running headless mode";
        }
        
        qInfo() << "[InferenceEngine] ===== CHECKPOINT: Tensor cache done, about to init transformer =====";
        
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
                    qInfo() << "[InferenceEngine] loadWeightsWithTypes returned false - using GGUF direct inference path";
                    qInfo() << "[InferenceEngine] This is normal for large models (40GB+) with memory pressure";
                    // CRITICAL FIX: Mark transformer as ready when using GGUF direct inference
                    // The transformer doesn't need custom weight loading for GGUF direct path
                    m_transformer.markReadyForGGUFInference();
                    qInfo() << "[InferenceEngine] Transformer marked as ready for GGUF-based inference";
                } else {
                    qInfo() << "[InferenceEngine] Transformer initialized successfully with proper quantization types";
                    // Ensure direct mode is disabled if we successfully loaded weights
                    m_transformer.disableGGUFDirectMode();
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
        
        qInfo() << "[InferenceEngine] ===== CHECKPOINT: Model ready, about to emit signals =====";
        qInfo() << "[InferenceEngine] Model loaded successfully:" << modelName;
        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, true), Q_ARG(QString, modelName));
        
        qInfo() << "[InferenceEngine] ===== CHECKPOINT: Signal queued, skipping processNextRequest during init =====";
        
        // FIX 6: Skip processNextRequest during model load to avoid deadlock
        // The queue will be processed when the first request comes in
        // processNextRequest();
        
        qInfo() << "[InferenceEngine] ===== CHECKPOINT: Model init complete, ready for requests =====";
        
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
    // Real code analysis implementation
    // Provides structural analysis without requiring the full transformer
    
    QElapsedTimer timer;
    timer.start();
    
    // Basic metrics
    int charCount = code.size();
    int lineCount = code.count('\n') + 1;
    auto tokens = tokenize(code);
    int tokenCount = tokens.size();
    
    // Analyze code structure
    int functionCount = 0;
    int classCount = 0;
    int commentLines = 0;
    int emptyLines = 0;
    int importCount = 0;
    int maxLineLength = 0;
    int totalIndentation = 0;
    
    QStringList lines = code.split('\n');
    bool inMultiLineComment = false;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        
        // Track max line length
        if (line.length() > maxLineLength) {
            maxLineLength = line.length();
        }
        
        // Count leading whitespace for indentation analysis
        int indent = 0;
        for (QChar c : line) {
            if (c == ' ') indent++;
            else if (c == '\t') indent += 4;
            else break;
        }
        totalIndentation += indent;
        
        // Empty lines
        if (trimmed.isEmpty()) {
            emptyLines++;
            continue;
        }
        
        // Multi-line comments
        if (trimmed.startsWith("/*")) inMultiLineComment = true;
        if (inMultiLineComment) {
            commentLines++;
            if (trimmed.contains("*/")) inMultiLineComment = false;
            continue;
        }
        
        // Single-line comments
        if (trimmed.startsWith("//") || trimmed.startsWith("#") || trimmed.startsWith("--")) {
            commentLines++;
            continue;
        }
        
        // Function detection (multi-language heuristics)
        if (trimmed.contains("function ") || trimmed.contains("def ") ||
            trimmed.contains("fn ") || trimmed.contains("func ") ||
            (trimmed.contains("(") && trimmed.contains(")") && 
             (trimmed.endsWith("{") || trimmed.endsWith(":") || trimmed.contains("->")))) {
            // Exclude if it looks like a function call
            if (!trimmed.contains("=") || trimmed.contains("= function") ||
                trimmed.contains("= [") || trimmed.contains("->")) {
                functionCount++;
            }
        }
        
        // Class detection
        if (trimmed.startsWith("class ") || trimmed.startsWith("struct ") ||
            trimmed.startsWith("interface ") || trimmed.startsWith("enum ")) {
            classCount++;
        }
        
        // Import detection
        if (trimmed.startsWith("import ") || trimmed.startsWith("from ") ||
            trimmed.startsWith("#include") || trimmed.startsWith("using ") ||
            trimmed.startsWith("require(") || trimmed.startsWith("require '")) {
            importCount++;
        }
    }
    
    // Calculate metrics
    int codeLines = lineCount - commentLines - emptyLines;
    double avgLineLength = charCount / (double)lineCount;
    double avgIndentation = totalIndentation / (double)lineCount;
    double commentRatio = lineCount > 0 ? (commentLines * 100.0 / lineCount) : 0;
    double tokensPerLine = lineCount > 0 ? (tokenCount / (double)lineCount) : 0;
    
    // Build analysis report
    QString analysis = QString(
        "📊 **Code Analysis Report**\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
        "**Size Metrics:**\n"
        "• Characters: %1\n"
        "• Lines: %2 (code: %3, comments: %4, empty: %5)\n"
        "• Tokens: %6 (~%7/line)\n\n"
        "**Structure:**\n"
        "• Functions/Methods: %8\n"
        "• Classes/Structs: %9\n"
        "• Imports/Includes: %10\n\n"
        "**Style Metrics:**\n"
        "• Max line length: %11 chars\n"
        "• Avg line length: %12 chars\n"
        "• Avg indentation: %13 spaces\n"
        "• Comment ratio: %14%\n\n"
        "⏱ Analysis time: %15ms"
    ).arg(charCount)
     .arg(lineCount).arg(codeLines).arg(commentLines).arg(emptyLines)
     .arg(tokenCount).arg(QString::number(tokensPerLine, 'f', 1))
     .arg(functionCount)
     .arg(classCount)
     .arg(importCount)
     .arg(maxLineLength)
     .arg(QString::number(avgLineLength, 'f', 1))
     .arg(QString::number(avgIndentation, 'f', 1))
     .arg(QString::number(commentRatio, 'f', 1))
     .arg(timer.elapsed());
    
    return analysis;
}

bool InferenceEngine::isModelLoaded() const
{
    QMutexLocker lock(&m_mutex);
    return (m_loader && m_loader->isOpen()) || m_useOllama;
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
    request(prompt, reqId, false);  // Default to non-streaming
}

void InferenceEngine::request(const QString& prompt, qint64 reqId, bool streaming)
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
    request.streaming = streaming;
    m_requestQueue.enqueue(request);

    qInfo() << QString("Request %1 enqueued (streaming: %2). Queue size: %3").arg(reqId).arg(streaming).arg(m_requestQueue.size());

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

void InferenceEngine::stopInference()
{
    if (m_useOllama && m_ollamaProxy) {
        m_ollamaProxy->stopGeneration();
    }
    // For local GGUF, we'd need a way to interrupt the loop in streamingGenerateWorker
}

void InferenceEngine::setModelDirectory(const QString& dir)
{
    if (m_ollamaProxy) {
        m_ollamaProxy->detectBlobs(dir);
    }
}

QStringList InferenceEngine::detectedOllamaModels() const
{
    if (m_ollamaProxy) {
        return m_ollamaProxy->detectedModels();
    }
    return QStringList();
}

bool InferenceEngine::isBlobPath(const QString& path) const
{
    if (m_ollamaProxy) {
        return m_ollamaProxy->isBlobPath(path);
    }
    return false;
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
            // ===== ENTERPRISE SAMPLING DEFAULTS =====
            qInfo() << "[InferenceEngine] Sampler configured: temperature=" << m_temperature 
                << ", top_p=" << m_topP << " (enterprise defaults for coherent output)";
        
        
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
    return tokenizeInternal(text, true, true);
}

std::vector<int32_t> InferenceEngine::tokenizeInternal(const QString& text, bool includeSystemPrompt, bool includeSpecialTokens)
{
    qDebug() << "=== TOKENIZE START ===";
    qDebug() << "[tokenize] Text length:" << text.length() << "mode:" << m_tokenizerMode;

    const bool prependSystem = includeSystemPrompt && !m_systemPromptTokens.empty();
    std::vector<int32_t> tokens;

    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                qDebug() << "[tokenize] Using BPE tokenizer";
                tokens = m_bpeTokenizer.encode(text);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                    qDebug() << "[tokenize] Prefixed system prompt tokens (" << m_systemPromptTokens.size() << ")";
                }
                qDebug() << "[tokenize] BPE produced" << tokens.size() << "tokens";
                qDebug() << "=== TOKENIZE END ===";
                return tokens;
            }
            qWarning() << "[tokenize] BPE tokenizer not ready";
            break;

        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                qDebug() << "[tokenize] Using SentencePiece tokenizer";
                tokens = m_spTokenizer.encode(text, includeSpecialTokens, false);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                    qDebug() << "[tokenize] Prefixed system prompt tokens (" << m_systemPromptTokens.size() << ")";
                }
                qDebug() << "[tokenize] SentencePiece produced" << tokens.size() << "tokens";
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
    if (includeSpecialTokens) {
        tokens.push_back(1);
        qDebug() << "[tokenize] Added BOS token (1)";
    }

    if (prependSystem) {
        tokens.insert(tokens.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
        qDebug() << "[tokenize] Prefixed system prompt tokens (" << m_systemPromptTokens.size() << ")";
    }

    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:]+"), Qt::SkipEmptyParts);
    qDebug() << "[tokenize] Split into" << words.size() << "words";

    for (const QString& word : words) {
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                uint32_t hash = qHash(word.toLower());
                tokens.push_back((hash % 50000) + 256);
            }
        } else {
            uint32_t hash = qHash(word.toLower());
            tokens.push_back((hash % 50000) + 256);
        }
    }

    if (includeSpecialTokens) {
        tokens.push_back(2);
    }

    qDebug() << "[tokenize] Final token count:" << tokens.size();
    qDebug() << "=== TOKENIZE END ===";
    return tokens;
}

void InferenceEngine::buildSystemPromptTokens()
{
    static const QString kEnterpriseSystemPrompt = QStringLiteral(
        "You are the Zero-Day enterprise mission agent. Respond in concise, clear English with actionable steps and no emojis.");

    try {
        m_systemPromptTokens = tokenizeInternal(kEnterpriseSystemPrompt, false, false);
        qInfo() << "[InferenceEngine] Cached system prompt tokens:" << m_systemPromptTokens.size();
    } catch (const std::exception& e) {
        qWarning() << "[InferenceEngine] Failed to cache system prompt tokens:" << e.what();
    }
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    qDebug() << "=== DETOKENIZE START ===";
    qDebug() << "[detokenize] Token count: " << tokens.size();
    qDebug() << "[detokenize] Tokenizer mode: " << m_tokenizerMode;
    qDebug() << "[detokenize] Vocab loaded: " << m_vocab.isLoaded();
    
    QString result;
    
    // First try: Use actual tokenizers if available
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                qDebug() << "[detokenize] Using BPE tokenizer to decode";
                auto bpeResult = m_bpeTokenizer.decode(tokens);
                qDebug() << "[detokenize] BPE decoded to: " << bpeResult;
                qDebug() << "=== DETOKENIZE END ===";
                return bpeResult;
            }
            qWarning() << "[detokenize] BPE tokenizer not ready, trying fallback";
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                qDebug() << "[detokenize] Using SentencePiece tokenizer to decode";
                auto spResult = m_spTokenizer.decode(tokens, true);
                qDebug() << "[detokenize] SentencePiece decoded to: " << spResult;
                qDebug() << "=== DETOKENIZE END ===";
                return spResult;
            }
            qWarning() << "[detokenize] SentencePiece tokenizer not ready, trying fallback";
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            qDebug() << "[detokenize] Using fallback/vocabulary-based decoder";
            break;
    }
    
    // Fallback: Decode using vocabulary
    if (m_vocab.isLoaded()) {
        qDebug() << "[detokenize] Vocabulary is loaded, using it to decode tokens";

        // Some tokenizers use byte-fallback markers like `[control_69]` to represent raw bytes.
        // If we emit those literally, chat output becomes unreadable and can destabilize rendering.
        static const QRegularExpression kControlTokenRe(
            QStringLiteral(R"(^\[control_(\d{1,3})\]$)")
        );

        QByteArray pendingBytes;
        auto flushPendingBytes = [&]() {
            if (pendingBytes.isEmpty()) {
                return;
            }
            result += QString::fromUtf8(pendingBytes);
            pendingBytes.clear();
        };

        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t tokenId = tokens[i];

            // Skip special tokens (BOS=1, EOS=2, PAD=0)
            if (tokenId == 0 || tokenId == 1 || tokenId == 2) {
                qDebug() << "[detokenize] Token" << tokenId << "-> skipped (special)";
                continue;
            }

            // Look up in vocabulary
            VocabularyLoader::Token vocabToken = m_vocab.getToken(tokenId);
            if (vocabToken.id >= 0 && !vocabToken.text.isEmpty()) {
                const QString tokenText = vocabToken.text;

                const QRegularExpressionMatch m = kControlTokenRe.match(tokenText);
                if (m.hasMatch()) {
                    bool ok = false;
                    const int byteValue = m.captured(1).toInt(&ok);
                    if (ok && byteValue >= 0 && byteValue <= 255) {
                        pendingBytes.append(static_cast<char>(byteValue));
                        continue;
                    }
                }

                flushPendingBytes();
                result += tokenText;
                qDebug() << "[detokenize] Token" << tokenId << "-> vocab:" << tokenText;
            } else {
                flushPendingBytes();
                qDebug() << "[detokenize] Token" << tokenId << "-> unknown";
            }
        }

        flushPendingBytes();

        // SentencePiece uses U+2581 '▁' as a word-boundary marker.
        result.replace(QChar(0x2581), QChar(' '));
    } else {
        // No vocabulary loaded - complete fallback
        qWarning() << "[detokenize] No vocabulary loaded, using character fallback";
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t token = tokens[i];
            
            // Skip special tokens
            if (token == 0 || token == 1 || token == 2) continue;
            
            // Try ASCII range first
            if (token >= 32 && token < 127) {
                result += QChar(token);
            } else if (token < 256) {
                result += QChar(token);
            } else {
                // Unknown - show as space
                result += " ";
            }
        }
    }
    
    qDebug() << "[detokenize] Final result: " << result;
    qDebug() << "=== DETOKENIZE END ===";
    return result.trimmed();
}

void InferenceEngine::initializeTokenizer()
{
    try {
        qInfo() << "[InferenceEngine::initializeTokenizer] ENTRY";
        std::cerr << "[InferenceEngine::initializeTokenizer] ENTRY" << std::endl;
        std::cerr.flush();
        
        // Try to load vocabulary from GGUF file
        if (!m_loader) {
            qWarning() << "[InferenceEngine] No GGUF loader available, skipping tokenizer init";
            return;
        }
        
        qInfo() << "[InferenceEngine::initializeTokenizer] About to call m_vocab.loadFromGGUF";
        std::cerr << "[InferenceEngine::initializeTokenizer] About to call m_vocab.loadFromGGUF" << std::endl;
        
        if (!m_vocab.loadFromGGUF(m_modelPath)) {
            qWarning() << "[InferenceEngine] Failed to load vocabulary from GGUF";
            return;
        }
        
        qInfo() << "[InferenceEngine] Vocabulary loaded:" << m_vocab.size() << "tokens";
        
        qInfo() << "[InferenceEngine::initializeTokenizer] Attempting tokenizer init with 5s timeout";
        std::cerr << "[InferenceEngine::initializeTokenizer] Attempting tokenizer init with 5s timeout" << std::endl;
        std::cerr.flush();
        
        // Use timeout-protected initialization
        if (!initializeTokenizerWithTimeout(5000)) {
            qWarning() << "[InferenceEngine] Tokenizer initialization failed or timed out, using fallback";
            loadFallbackTokenizer();
            return;
        }
        buildSystemPromptTokens();
        
        // Populate transformer vocabulary for internal detokenization
        if (m_vocab.isLoaded()) {
            QHash<int32_t, QString> vocabMap;
            TransformerInference::TokenizerData tokenizerData;
            
            int vocabSize = m_vocab.size();
            for (int i = 0; i < vocabSize; ++i) {
                VocabularyLoader::Token t = m_vocab.getToken(i);
                if (t.id != -1) {
                    vocabMap.insert(t.id, t.text);
                    tokenizerData.idToToken.insert(t.id, t.text);
                    tokenizerData.tokenToId.insert(t.text, t.id);
                    
                    // Store special tokens
                    if (t.text == "<unk>") tokenizerData.unkToken = t.text;
                    else if (t.text == "<s>") tokenizerData.bosToken = t.text;
                    else if (t.text == "</s>") tokenizerData.eosToken = t.text;
                    else if (t.text == "<pad>") tokenizerData.padToken = t.text;
                }
            }
            m_transformer.setVocabulary(vocabMap);
            m_transformer.setTokenizerData(tokenizerData);
            
            qInfo() << "[InferenceEngine] Populated transformer with" << vocabMap.size() << "tokens";
        }

        qInfo() << "[InferenceEngine] Tokenizer initialized successfully";
        return;
        
        // Original code below is now handled by initializeTokenizerWithTimeout
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        QHash<QString, QByteArray> tokenizerMetadata;
        try {
            tokenizerMetadata = m_loader->getTokenizerMetadata();
        } catch (const std::bad_alloc&) {
            qWarning() << "[InferenceEngine] Memory allocation failed loading tokenizer metadata (file may be too large)";
            qWarning() << "[InferenceEngine] Falling back to simple word tokenizer";
            m_tokenizerMode = TOKENIZER_FALLBACK;
            return;
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
            } catch (const std::bad_alloc&) {
                qWarning() << "[InferenceEngine] Memory allocation failed in BPE tokenizer, using fallback";
                m_tokenizerMode = TOKENIZER_FALLBACK;
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
            } catch (const std::bad_alloc&) {
                qWarning() << "[InferenceEngine] Memory allocation failed in SentencePiece tokenizer, using fallback";
                m_tokenizerMode = TOKENIZER_FALLBACK;
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
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.generate"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("generate.begin"), QStringLiteral("tokens_in=%1 max=%2").arg(inputTokens.size()).arg(maxTokens));
    
    qDebug() << "=== GENERATE START ===";
    qDebug() << "[generate] Input tokens: " << inputTokens.size();
    qDebug() << "[generate] Max new tokens: " << maxTokens;
    
    // Check model loaded (with brief lock for safety)
    {
        QMutexLocker lock(&m_mutex);
        if (!m_loader || !m_loader->isOpen()) {
            qWarning() << "[generate] Cannot generate - no model loaded";
            telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.no_model"), timer.elapsedMs(), QStringLiteral("tokens_in=%1").arg(inputTokens.size()));
            return inputTokens;
        }
    }
    
    qDebug() << "[generate] Model loaded: true";
    qDebug() << "[generate] Transformer ready: " << m_transformer.isReady();
    
    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        QElapsedTimer localTimer;
        localTimer.start();
        
        qDebug() << "[generate] Transformer is ready, starting KV-cache prefill...";
        
        // === FIXED: Process entire prompt to get proper starting context ===
        // Phase 1: Context prefill - process the entire input prompt once
        // The transformer builds the KV-cache (Key-Value cache) for efficient generation.
        qDebug() << "[generate] Pre-filling KV-cache with" << inputTokens.size() << "prompt tokens...";
        std::vector<float> contextLogits = m_transformer.forward(inputTokens);
        qDebug() << "[generate] KV-cache prefilled, got" << contextLogits.size() << "logits from last token";
        
        // Sample the FIRST generated token from the prompt's final logits
        // This ensures the model's response is conditioned on the full prompt
        int32_t currentToken = sampleNextToken(contextLogits, m_temperature, m_topP);
        result.push_back(currentToken);
        qDebug() << "[generate] First generated token after prompt:" << currentToken;
        
        // === Phase 2: Autoregressive Token Generation (Decoding) ===
        // Generate remaining tokens (we already have the first one from prompt context)
        for (int i = 1; i < maxTokens; ++i) {
            // Generate logits for the next token based ONLY on the current token
            // The Transformer uses the internal KV-cache for past context
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
        
    } else {
        // Fallback generation when transformer is not ready
        // Generate meaningful fallback instead of placeholder tokens
        qWarning() << "[generate] Transformer not ready, using intelligent fallback generation";
        
        // Attempt to use OllamaProxy if configured
        if (m_useOllama && m_ollamaProxy) {
            qInfo() << "[generate] Attempting Ollama fallback generation via proxy";
            
            // Build prompt from input tokens using detokenize
            QString prompt = detokenize(inputTokens);
            
            // Use synchronous generation via OllamaProxy 
            // Note: This is a simplified fallback - full async would use signals/slots
            QString generatedText = m_ollamaProxy->generateResponseSync(prompt, 0.7f, maxTokens);
            
            if (!generatedText.isEmpty()) {
                // Tokenize the generated response
                auto generatedTokens = tokenize(generatedText);
                result.insert(result.end(), generatedTokens.begin(), generatedTokens.end());
                
                qInfo() << "[generate] Ollama fallback generated" << generatedTokens.size() << "tokens";
                telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.ollama_fallback"), timer.elapsedMs(), 
                                      QStringLiteral("tokens_out=%1").arg(result.size()));
                return result;
            }
            qWarning() << "[generate] Ollama fallback returned empty, using echo mode";
        }
        
        // Ultimate fallback: Echo input with acknowledgment tokens
        // This provides meaningful output even without a model
        QString echoResponse = QString("[No model loaded] Input received with %1 tokens").arg(inputTokens.size());
        auto echoTokens = tokenize(echoResponse);
        result.insert(result.end(), echoTokens.begin(), echoTokens.end());
        
        qDebug() << "=== GENERATE END (FALLBACK) ===";
        telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.fallback"), timer.elapsedMs(), QStringLiteral("tokens_out=%1").arg(result.size()));
    }
    
    return result;
}

void InferenceEngine::generateStreaming(const QString& prompt,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete)
{
    // Tokenize then call vector variant
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(onToken), std::move(onComplete));
}

void InferenceEngine::generateStreaming(const std::vector<int32_t>& inputTokens,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete)
{
    if (m_threadingEnabled.load()) {
        // Run worker in background to avoid blocking UI thread
        auto future = QtConcurrent::run([this, inputTokens, maxTokens, onToken = std::move(onToken), onComplete = std::move(onComplete)]() mutable {
            streamingGenerateWorker(inputTokens, maxTokens, onToken, onComplete);
        });
    } else {
        streamingGenerateWorker(inputTokens, maxTokens, std::move(onToken), std::move(onComplete));
    }
}

void InferenceEngine::streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              TokenCallback onToken,
                                              CompleteCallback onComplete)
{
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.generate.streaming"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("stream.begin"), QStringLiteral("tokens_in=%1 max=%2").arg(inputTokens.size()).arg(maxTokens));

    // Ensure model loaded
    {
        QMutexLocker lock(&m_mutex);
        if (!m_loader || !m_loader->isOpen()) {
            qWarning() << "[stream] Cannot generate - no model loaded";
            telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("stream.no_model"), timer.elapsedMs(), QStringLiteral("tokens_in=%1").arg(inputTokens.size()));
            if (onComplete) onComplete();
            return;
        }
    }

    if (!m_transformer.isReady()) {
        qWarning() << "[stream] Transformer not ready";
        if (onComplete) onComplete();
        return;
    }

    // Prefill KV-cache if not ready
    if (!m_kvCacheReady && !inputTokens.empty()) {
        qDebug() << "[stream] Prefilling KV-cache with" << inputTokens.size() << "tokens";
        m_transformer.forward(inputTokens);
        m_kvCacheReady = true;
    }

    int32_t currentToken = inputTokens.empty() ? 1 : inputTokens.back();
    std::vector<int32_t> emitted;

    for (int i = 0; i < maxTokens; ++i) {
        auto logits = m_transformer.forward(std::vector<int32_t>{currentToken});
        if (logits.empty()) {
            qWarning() << "[stream] Empty logits";
            break;
        }

        currentToken = sampleNextToken(logits, m_temperature, m_topP);
        emitted.push_back(currentToken);

        // Detokenize incrementally; emit token fragment
        QString frag = detokenize(std::vector<int32_t>{ currentToken });
        if (!frag.isEmpty() && onToken) {
            // Marshal back to QObject thread if needed
            QMetaObject::invokeMethod(this, [onToken, frag]() {
                onToken(frag);
            }, Qt::QueuedConnection);
        }

        // Simple EOS check (common 2/0, configurable in future)
        if (currentToken == 2 || currentToken == 0) {
            qDebug() << "[stream] EOS reached at" << i;
            break;
        }

        // Pace a bit to keep UI responsive
        QThread::msleep(10);
    }

    // Emit completion
    if (onComplete) {
        QMetaObject::invokeMethod(this, [onComplete]() { onComplete(); }, Qt::QueuedConnection);
    }

    // Reset KV-cache for next request
    m_kvCacheReady = false;
    telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("stream.end"), timer.elapsedMs(), QStringLiteral("tokens_out=%1").arg(emitted.size()));
}

void InferenceEngine::generateStreaming(const QString& prompt,
                                        int maxTokens,
                                        std::function<void(const std::string&)> tokenCallback,
                                        std::function<void()> completeCallback)
{
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(tokenCallback), std::move(completeCallback));
}

void InferenceEngine::generateStreaming(const std::vector<int32_t>& inputTokens,
                                        int maxTokens,
                                        std::function<void(const std::string&)> tokenCallback,
                                        std::function<void()> completeCallback)
{
    if (m_threadingEnabled.load()) {
        // Run in background thread to avoid blocking UI thread
        auto future = QtConcurrent::run([this, inputTokens, maxTokens, tokenCallback, completeCallback]() {
            streamingGenerateWorker(inputTokens, maxTokens, tokenCallback, completeCallback);
        });
    } else {
        streamingGenerateWorker(inputTokens, maxTokens, tokenCallback, completeCallback);
    }
}

void InferenceEngine::streamingGenerateWorker(const std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              std::function<void(const std::string&)> tokenCallback,
                                              std::function<void()> completeCallback)
{
    try {
        qInfo() << "[Streaming] Starting generation with" << inputTokens.size() << "context tokens";

        if (!m_transformer.isReady()) {
            qWarning() << "[Streaming] Transformer not ready";
            if (completeCallback) completeCallback();
            return;
        }

        std::vector<int32_t> result = inputTokens;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
        }

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();
        std::string accumulatedText;

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                qWarning() << "[Streaming] Empty logits from transformer";
                break;
            }
            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                qInfo() << "[Streaming] Stopped by EOS token";
                break;
            }

            result.push_back(currentToken);

            // Detokenize this token and stream it
            std::vector<int32_t> singleToken = { currentToken };
            QString tokenText = detokenize(singleToken);

            if (!tokenText.isEmpty() && tokenCallback) {
                std::string tokenStr = tokenText.toStdString();
                accumulatedText += tokenStr;
                tokenCallback(tokenStr);
            }

            // Optional small delay to simulate real-time
            QThread::msleep(10);
        }

        // Reset KV-cache for next generation
        m_kvCacheReady = false;

        qInfo() << "[Streaming] Generation complete:" << (result.size() - inputTokens.size()) << "tokens";
        if (completeCallback) completeCallback();

    } catch (const std::exception& e) {
        qCritical() << "[Streaming] Exception during generation:" << e.what();
        if (completeCallback) completeCallback();
    } catch (...) {
        qCritical() << "[Streaming] Unknown exception during generation";
        if (completeCallback) completeCallback();
    }
}

void InferenceEngine::generateStreaming(qint64 reqId, const QString& prompt, int maxTokens)
{
    // Always use threading for concurrency support unless specifically disabled
    // This allows multiple generation requests (e.g. chat + autocomplete) to be handled
    if (!m_threadingEnabled.load()) {
         m_threadingEnabled.store(true);
    }
    
    // QtConcurrent::run spawns a new thread from the global thread pool
    // Note: We use the global thread pool which has been scaled up in main_v5.cpp
    // to support at least 20+ concurrent operations as requested.
    auto future = QtConcurrent::run([this, reqId, prompt, maxTokens]() {
        streamingGenerateWorkerSignals(reqId, prompt, maxTokens);
    });
}

void InferenceEngine::streamingGenerateWorkerSignals(qint64 reqId, const QString& prompt, int maxTokens)
{
    // High-performance concurrency gate
    // Cloud requests (Ollama) are I/O bound and thread-safe via network manager, so they 
    // bypass the heavy locking we use for local CPU/GPU/NPU inference.
    // For local models, we still serialize to prevent memory corruption.
    
    std::unique_ptr<QMutexLocker<QMutex>> mutexLocker;
    if (!m_useOllama) {
        // Local inference needs strict serialization to protect the model memory
        mutexLocker = std::make_unique<QMutexLocker<QMutex>>(&m_generationMutex);
    }
    
    m_currentRequestId = reqId;
    if (m_useOllama) {
        qInfo() << "[Streaming] Routing request" << reqId << "to OllamaProxy (Parallel Cloud Mode)";
        // Always invoke via the object's event loop to ensure network objects are accessed
        // from the thread they were created in (prevents cross-thread QObject parent errors)
        QMetaObject::invokeMethod(m_ollamaProxy, "generateResponse", Qt::QueuedConnection,
            Q_ARG(QString, prompt), Q_ARG(float, m_temperature), Q_ARG(int, maxTokens));
        return;
    }

    try {
        qInfo() << "[Streaming] Starting signal-based generation for request" << reqId;

        // Tokenize the prompt
        std::vector<int32_t> inputTokens = tokenize(prompt);
        qInfo() << "[Streaming] Tokenized prompt:" << inputTokens.size() << "tokens";

        if (!m_transformer.isReady()) {
            qWarning() << "[Streaming] Transformer not ready";
            emit streamFinished(reqId);
            return;
        }

        std::vector<int32_t> result = inputTokens;
        QString accumulatedResponse;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
        }

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                qWarning() << "[Streaming] Empty logits from transformer";
                break;
            }

            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                qInfo() << "[Streaming] Stopped by EOS token";
                break;
            }

            result.push_back(currentToken);

            // Detokenize this token and emit signal
            std::vector<int32_t> singleToken = { currentToken };
            QString tokenText = detokenize(singleToken);

            if (!tokenText.isEmpty()) {
                accumulatedResponse += tokenText;
                // Emit signal for this token
                emit streamToken(reqId, tokenText);
            }

            // Optional small delay to simulate real-time
            QThread::msleep(10);
        }

        // Agentic Failure Detection & Correction
        if (m_failureDetector && !accumulatedResponse.isEmpty()) {
            FailureDetection failure = m_failureDetector->detectFailure(accumulatedResponse, prompt);
            if (failure.type != DetectorFailure::None) {
                qWarning() << "[Agentic] Failure detected:" << failure.reason;
                
                if (m_puppeteer) {
                    // Attempt auto-correction
                    CorrectionResult correction = m_puppeteer->correctResponse(accumulatedResponse, prompt);
                    if (correction.success) {
                        qInfo() << "[Agentic] Correction successful";
                        emit streamToken(reqId, "\n\n[Agentic Correction]:\n" + correction.correctedOutput);
                    }
                }
            }
        }

        // Reset KV-cache for next generation
        m_kvCacheReady = false;

        qInfo() << "[Streaming] Signal-based generation complete for request" << reqId << ":" << (result.size() - inputTokens.size()) << "tokens";

        // Emit completion signal
        emit streamFinished(reqId);

    } catch (const std::exception& e) {
        qCritical() << "[Streaming] Exception during signal-based generation:" << e.what();
        emit streamFinished(reqId);
    } catch (...) {
        qCritical() << "[Streaming] Unknown exception during signal-based generation";
        emit streamFinished(reqId);
    }
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
    if (logits.empty()) {
        return 0;
    }

    if (temperature <= 0.0) {
        auto it = std::max_element(logits.begin(), logits.end());
        return static_cast<int32_t>(std::distance(logits.begin(), it));
    }

    if (topP <= 0.0) {
        topP = 1.0;
    }

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
    
    if (currentRequest.streaming) {
        // Use streaming generation
        qInfo() << "Using streaming generation for request" << currentRequest.requestId;
        generateStreaming(currentRequest.requestId, currentRequest.prompt, 128);
        
        // Performance metrics (will be calculated when streaming finishes)
        // For now, just mark as completed
        m_isProcessingInference = false;
        processNextRequest();
        return;
    }
    
    // Non-streaming: synchronous generation
    qInfo() << "Using synchronous generation for request" << currentRequest.requestId;
    
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

bool InferenceEngine::initializeTokenizerWithTimeout(int timeoutMs)
{
    qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Starting with" << timeoutMs << "ms timeout";
    
    // Launch metadata fetch in background with timeout protection
    QFuture<QHash<QString, QByteArray>> future = QtConcurrent::run([this]() -> QHash<QString, QByteArray> {
        try {
            qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Calling getTokenizerMetadata()";
            return m_loader->getTokenizerMetadata();
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine::initializeTokenizerWithTimeout] Exception:" << e.what();
            return QHash<QString, QByteArray>();
        } catch (...) {
            qWarning() << "[InferenceEngine::initializeTokenizerWithTimeout] Unknown exception";
            return QHash<QString, QByteArray>();
        }
    });
    
    // Wait with timeout using QThread::msleep polling
    QElapsedTimer timer;
    timer.start();
    
    while (!future.isFinished() && timer.elapsed() < timeoutMs) {
        QThread::msleep(100);
        // Log progress every second to show we're waiting
        if (timer.elapsed() % 1000 < 100) {
            qDebug() << "[InferenceEngine::initializeTokenizerWithTimeout] Waiting for metadata... elapsed:" << timer.elapsed() << "ms";
        }
    }
    
    if (!future.isFinished()) {
        qCritical() << "[InferenceEngine::initializeTokenizerWithTimeout] TIMEOUT after" << timeoutMs << "ms";
        qCritical() << "[InferenceEngine::initializeTokenizerWithTimeout] Background thread still running, cannot call result()";
        return false;
    }
    
    // CRITICAL: Double-check that future is actually finished before calling result()
    // Calling result() on an unfinished future blocks indefinitely
    if (!future.isFinished()) {
        qCritical() << "[InferenceEngine::initializeTokenizerWithTimeout] Future reports not finished, cannot safely retrieve result";
        return false;
    }
    
    qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Future finished successfully, retrieving metadata";
    QHash<QString, QByteArray> tokenizerMetadata = future.result();
    
    if (tokenizerMetadata.isEmpty()) {
        qWarning() << "[InferenceEngine::initializeTokenizerWithTimeout] Empty or corrupt metadata";
        return false;
    }
    
    qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Metadata retrieved successfully, entries:" << tokenizerMetadata.size();
    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] About to determine tokenizer type" << std::endl;
    
    // Determine tokenizer type and load
    VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
    qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Vocab type:" << static_cast<int>(vocabType);
    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] Vocab type: " << static_cast<int>(vocabType) << std::endl;
    
    if (vocabType == VocabularyLoader::BPE) {
        try {
            qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Loading BPE tokenizer...";
            std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] About to call m_bpeTokenizer.loadFromGGUFMetadata()" << std::endl;
            
            if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_BPE;
                qInfo() << "[InferenceEngine] Using BPE tokenizer (GPT-2 compatible)";
                return true;
            }
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to initialize BPE tokenizer:" << e.what();
        }
    } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
        try {
            qInfo() << "[InferenceEngine::initializeTokenizerWithTimeout] Loading SentencePiece tokenizer...";
            std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] About to call m_spTokenizer.loadFromGGUFMetadata()" << std::endl;

            if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_SP;
                qInfo() << "[InferenceEngine] Using SentencePiece tokenizer (LLaMA/Mistral compatible)";
                return true;
            }
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to initialize SentencePiece tokenizer:" << e.what();
        } catch (...) {
            qWarning() << "[InferenceEngine] Failed to initialize SentencePiece tokenizer: unknown exception";
        }
    }
    
    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] No valid tokenizer loaded, returning false" << std::endl;
    return false;
}

bool InferenceEngine::loadFallbackTokenizer()
{
    qInfo() << "[InferenceEngine::loadFallbackTokenizer] Loading pre-baked fallback tokenizer";
    
    // Use vocabulary-based fallback tokenization
    m_tokenizerMode = TOKENIZER_FALLBACK;
    
    // The vocabulary is already loaded from GGUF, so we just need to mark the tokenizer as ready
    if (m_vocab.isLoaded() && m_vocab.size() > 0) {
        qInfo() << "[InferenceEngine::loadFallbackTokenizer] Using vocabulary-based tokenizer with" << m_vocab.size() << "tokens";
        buildSystemPromptTokens();
        return true;
    }
    
    qWarning() << "[InferenceEngine::loadFallbackTokenizer] No vocabulary available, using minimal fallback";
    return false;
}

InferenceEngine::HealthStatus InferenceEngine::getHealthStatus() const
{
    QMutexLocker lock(&m_mutex);
    
    HealthStatus status;
    status.model_loaded = (m_loader != nullptr && m_loader->isOpen()) || m_useOllama;
    status.inference_ready = status.model_loaded;
    status.model_name = extractModelName(m_modelPath);
    status.quantization = m_quantMode;
    status.backend = "Vulkan";
    
    // GPU metrics (real VRAM tracking would require Vulkan API integration)
    status.gpu_available = true;
    status.total_vram_mb = 16384.0; // AMD RX 7800 XT - would query from actual GPU
    status.memory_usage_mb = static_cast<double>(m_memoryUsageMB);
    status.used_vram_mb = status.memory_usage_mb;
    
    // Performance metrics
    status.tokens_per_second = m_realtimeTokensPerSecond.load();
    status.avg_latency_ms = m_avgLatencyMs.load();
    status.p95_latency_ms = m_avgLatencyMs.load() * 1.5;  // Rough approximation
    status.p99_latency_ms = m_avgLatencyMs.load() * 2.0;  // Rough approximation
    status.active_requests = m_activeRequests.load();
    status.total_requests = m_totalRequests.load();
    status.total_requests_processed = m_totalRequests.load();
    status.pending_requests = m_requestQueue.size();
    
    return status;
}

double InferenceEngine::getTokensPerSecond() const
{
    // Return real-time TPS from atomic counter
    return m_realtimeTokensPerSecond.load();
}


