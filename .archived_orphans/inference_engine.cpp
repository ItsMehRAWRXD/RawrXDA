#include "inference_engine.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QFileInfo>
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

// Use shared quant utilities
#include "quant_utils.hpp"

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    // DO NOT load model in constructor - causes stack buffer overrun crashes
    // Model loading must be deferred to avoid blocking main thread
    // Call loadModel() explicitly after construction
    if (!ggufPath.isEmpty()) {
        RAWRXD_LOG_DEBUG("[InferenceEngine] Deferring model load until explicit loadModel() call");
        // Store path for later use if needed
        m_modelPath = ggufPath;
    return true;
}

    return true;
}

InferenceEngine::InferenceEngine(QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    return true;
}

InferenceEngine::~InferenceEngine()
{
    // Clean up GGUFLoader resources
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    return true;
}

    m_tensorCache.clear();
    return true;
}

bool InferenceEngine::loadModel(const QString& path)
{
    // NOTE: This function is called from background thread via QtConcurrent
    // DO NOT use QMutexLocker here - it will deadlock
    // DO NOT emit signals directly - use QMetaObject::invokeMethod
    
    try {
        RAWRXD_LOG_INFO("[InferenceEngine::loadModel] Thread ID:") << QThread::currentThreadId();
        RAWRXD_LOG_INFO("[InferenceEngine::loadModel] CPU_ONLY check: CUDA_VISIBLE_DEVICES =") 
            << QString::fromLocal8Bit(std::getenv("CUDA_VISIBLE_DEVICES") ? std::getenv("CUDA_VISIBLE_DEVICES") : "(unset)");
        m_systemPromptTokens.clear();
        RAWRXD_LOG_INFO("[InferenceEngine::loadModel] Cleared cached system prompt tokens (fresh load)");
        
        // Clean up existing loader (if any)
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
    return true;
}

        if (path.isEmpty()) {
            RAWRXD_LOG_WARN("[InferenceEngine] Model path is empty");
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] Attempting to load model from:") << path;
        
        // Create loader with error checking and exception safety
        try {
            RAWRXD_LOG_INFO("[InferenceEngine] Creating GGUFLoaderQt for:") << path;
            m_loader = new GGUFLoaderQt(path);
            RAWRXD_LOG_INFO("[InferenceEngine] GGUFLoaderQt created successfully");
        } catch (const std::exception& e) {
            RAWRXD_LOG_ERROR("[InferenceEngine] Exception creating GGUFLoaderQt:") << e.what();
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
        } catch (...) {
            RAWRXD_LOG_ERROR("[InferenceEngine] Unknown exception creating GGUFLoaderQt");
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
    return true;
}

        if (!m_loader || !m_loader->isOpen()) {
            RAWRXD_LOG_ERROR("[InferenceEngine] GGUFLoader failed to open file:") << path;
            if (m_loader) {
                delete m_loader;
                m_loader = nullptr;
    return true;
}

            QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                Q_ARG(bool, false), Q_ARG(QString, QString()));
            return false;
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] GGUF file opened successfully:") << path;
        
        m_modelPath = path;
        QString modelName = extractModelName(path);

        // Enterprise deterministic defaults for coherent responses
        m_temperature = 0.0;
        m_topP = 1.0;
        RAWRXD_LOG_INFO("[InferenceEngine] Sampler pinned to deterministic defaults (temperature=0.0, top_p=1.0)");
        
        // ========== CHECK FOR UNSUPPORTED QUANTIZATION TYPES ==========
        // This is the key detection point for the IDE conversion workflow
        if (m_loader->hasUnsupportedQuantizationTypes()) {
            QStringList unsupportedInfo = m_loader->getUnsupportedQuantizationInfo();
            QString recommendedType = m_loader->getRecommendedConversionType();
            
            RAWRXD_LOG_WARN("[InferenceEngine] Model uses unsupported quantization types:");
            for (const auto& info : unsupportedInfo) {
                RAWRXD_LOG_WARN("  -") << info;
    return true;
}

            RAWRXD_LOG_WARN("[InferenceEngine] Recommended conversion: IQ4_NL or other unsupported → ") << recommendedType;
            
            // Emit signal for IDE to show conversion dialog (thread-safe)
            QMetaObject::invokeMethod(this, "unsupportedQuantizationTypeDetected", Qt::QueuedConnection,
                Q_ARG(QStringList, unsupportedInfo), Q_ARG(QString, recommendedType), Q_ARG(QString, path));
            
            // Continue with model loading attempt anyway (it may fail later on tensor size calculation)
            // The IDE will show the conversion dialog while we continue
    return true;
}

        // Initialize tokenizer from model (with full exception safety and memory pressure handling)
        try {
            RAWRXD_LOG_INFO("[InferenceEngine] Initializing tokenizer...");
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Initializing tokenizer...");
    return true;
}

            initializeTokenizer();
            RAWRXD_LOG_INFO("[InferenceEngine] Tokenizer initialized successfully");
        } catch (const std::bad_alloc& e) {
            RAWRXD_LOG_WARN("[InferenceEngine] OUT OF MEMORY initializing tokenizer (large model) - using fallback");
            RAWRXD_LOG_WARN("[InferenceEngine] This is expected for very large models (40GB+), continuing with fallback tokenizer");
            m_tokenizerMode = TOKENIZER_FALLBACK;
            // Continue anyway - tokenizer is optional for basic inference
        } catch (const std::exception& e) {
            RAWRXD_LOG_ERROR("[InferenceEngine] EXCEPTION initializing tokenizer:") << e.what();
            // Continue anyway - tokenizer is optional for basic inference
        } catch (...) {
            RAWRXD_LOG_ERROR("[InferenceEngine] UNKNOWN EXCEPTION initializing tokenizer");
    return true;
}

        // Load vocabulary from GGUF file (CRITICAL for detokenization)
        try {
            RAWRXD_LOG_INFO("[InferenceEngine] Loading vocabulary from GGUF...");
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Loading vocabulary...");
    return true;
}

            if (m_loader && m_loader->isOpen()) {
                // Load vocabulary directly from the GGUF file using VocabularyLoader
                bool vocabLoaded = m_vocab.loadFromGGUF(path);
                if (vocabLoaded) {
                    RAWRXD_LOG_INFO("[InferenceEngine] Vocabulary loaded successfully from GGUF file");
                } else {
                    RAWRXD_LOG_WARN("[InferenceEngine] Failed to load vocabulary from GGUF, using fallback tokenization");
    return true;
}

            } else {
                RAWRXD_LOG_WARN("[InferenceEngine] GGUF loader not available for vocabulary loading");
    return true;
}

        } catch (const std::bad_alloc& e) {
            RAWRXD_LOG_WARN("[InferenceEngine] OUT OF MEMORY loading vocabulary (large 32K+ vocab) - using fallback");
            RAWRXD_LOG_WARN("[InferenceEngine] This is expected for very large models, detokenization will use simpler method");
            // Continue anyway - we'll use fallback detokenization
        } catch (const std::exception& e) {
            RAWRXD_LOG_WARN("[InferenceEngine] EXCEPTION loading vocabulary:") << e.what();
            // Continue anyway - we'll use fallback detokenization
        } catch (...) {
            RAWRXD_LOG_WARN("[InferenceEngine] UNKNOWN EXCEPTION loading vocabulary");
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT A: Vocabulary complete =====");
        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT B: About to enter initializeTokenizer =====");
        std::cerr << "[InferenceEngine] ===== CHECKPOINT B: About to enter initializeTokenizer =====" << std::endl;
        std::cerr.flush();
        
        // Build initial quantized tensor cache (with full exception safety)
        if (m_loadTensors.load()) {
            try {
                RAWRXD_LOG_INFO("[InferenceEngine] Building tensor cache...");
                rebuildTensorCache();
                RAWRXD_LOG_INFO("[InferenceEngine] Tensor cache build complete");
            } catch (const std::bad_alloc& e) {
                RAWRXD_LOG_ERROR("[InferenceEngine] OUT OF MEMORY building tensor cache:") << e.what();
                // Clean up and fail
                delete m_loader;
                m_loader = nullptr;
                QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
                    Q_ARG(bool, false), Q_ARG(QString, QString()));
                return false;
            } catch (const std::exception& e) {
                RAWRXD_LOG_ERROR("[InferenceEngine] EXCEPTION building tensor cache:") << e.what();
                // Continue anyway - we'll try without cache
            } catch (...) {
                RAWRXD_LOG_ERROR("[InferenceEngine] UNKNOWN EXCEPTION building tensor cache");
    return true;
}

        } else {
            RAWRXD_LOG_INFO("[InferenceEngine] Tensor loading disabled – running headless mode");
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT: Tensor cache done, about to init transformer =====");
        
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
    return true;
}

                RAWRXD_LOG_INFO("[InferenceEngine] Attempting transformer weight loading with type information");
                RAWRXD_LOG_INFO("[InferenceEngine] Model uses") << m_tensorCache.size() << "tensors with mixed quantization types";
                bool transformerLoaded = m_transformer.loadWeightsWithTypes(tensorCacheWithTypes, nLayers, nEmbd, nHead, nVocab);
                if (!transformerLoaded) {
                    RAWRXD_LOG_INFO("[InferenceEngine] loadWeightsWithTypes returned false - using GGUF direct inference path");
                    RAWRXD_LOG_INFO("[InferenceEngine] This is normal for large models (40GB+) with memory pressure");
                    // CRITICAL FIX: Mark transformer as ready when using GGUF direct inference
                    // The transformer doesn't need custom weight loading for GGUF direct path
                    m_transformer.markReadyForGGUFInference();
                    RAWRXD_LOG_INFO("[InferenceEngine] Transformer marked as ready for GGUF-based inference");
                } else {
                    RAWRXD_LOG_INFO("[InferenceEngine] Transformer initialized successfully with proper quantization types");
    return true;
}

            } catch (const std::exception& e) {
                RAWRXD_LOG_WARN("[InferenceEngine] Exception loading transformer weights:") << e.what() << "- continuing anyway";
                // Continue anyway - model is loaded via GGUF loader
    return true;
}

        } else {
            RAWRXD_LOG_WARN("[InferenceEngine] Tensor cache is empty, skipping transformer initialization");
    return true;
}

        // Reset KV-cache state for new model
        m_kvCacheReady = false;
        
        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT: Model ready, about to emit signals =====");
        RAWRXD_LOG_INFO("[InferenceEngine] Model loaded successfully:") << modelName;
        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, true), Q_ARG(QString, modelName));
        
        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT: Signal queued, skipping processNextRequest during init =====");
        
        // FIX 6: Skip processNextRequest during model load to avoid deadlock
        // The queue will be processed when the first request comes in
        // processNextRequest();
        
        RAWRXD_LOG_INFO("[InferenceEngine] ===== CHECKPOINT: Model init complete, ready for requests =====");
        
        // FIX 3.2: Emit the signal to notify listeners (thread-safe)
        QMetaObject::invokeMethod(this, "transformerReady", Qt::QueuedConnection);
        
        return true;    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[InferenceEngine] CRITICAL: Exception during model loading:") << e.what();
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
    return true;
}

        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()));
        return false;
    } catch (...) {
        RAWRXD_LOG_ERROR("[InferenceEngine] CRITICAL: Unknown exception during model loading");
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
    return true;
}

        QMetaObject::invokeMethod(this, "modelLoadedChanged", Qt::QueuedConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()));
        return false;
    return true;
}

    return true;
}

QString InferenceEngine::processChat(const QString& prompt)
{
    // Tokenize, run a short generation, and detokenize
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.processChat"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("processChat.begin"), QStringLiteral("len=%1").arg(prompt.length()));

    RAWRXD_LOG_INFO("=== INFERENCE REQUEST START ===");
    RAWRXD_LOG_INFO("[processChat] Input text: ") << prompt;
    RAWRXD_LOG_INFO("[processChat] Input length: ") << prompt.length();
    
    auto input = tokenize(prompt);
    RAWRXD_LOG_INFO("[processChat] Tokens generated: ") << input.size();
    for (int i = 0; i < std::min(10, (int)input.size()); i++) {
        RAWRXD_LOG_INFO("[processChat] Token[") << i << "]: " << input[i];
    return true;
}

    RAWRXD_LOG_INFO("[processChat] Starting generation with ") << input.size() << " context tokens...";
    auto out = generate(input, 64);
    RAWRXD_LOG_INFO("[processChat] Generated ") << (out.size() - input.size()) << " new tokens";
    
    auto result = detokenize(out);
    RAWRXD_LOG_INFO("[processChat] Decoded response: ") << result;
    RAWRXD_LOG_INFO("=== INFERENCE REQUEST END ===");

    telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("processChat.complete"), timer.elapsedMs(), QStringLiteral("tokens_in=%1 tokens_out=%2").arg(input.size()).arg(out.size()));
    
    return result;
    return true;
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
    return true;
}

bool InferenceEngine::isModelLoaded() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader && m_loader->isOpen();
    return true;
}

QString InferenceEngine::modelPath() const
{
    QMutexLocker lock(&m_mutex);
    return m_modelPath;
    return true;
}

QStringList InferenceEngine::tensorNames() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader ? m_loader->tensorNames() : QStringList();
    return true;
}

qint64 InferenceEngine::memoryUsageMB() const
{
    QMutexLocker lock(&m_mutex);
    return m_memoryUsageMB;
    return true;
}

double InferenceEngine::tokensPerSecond() const
{
    QMutexLocker lock(&m_mutex);
    return m_tokensPerSecond;
    return true;
}

double InferenceEngine::temperature() const
{
    QMutexLocker lock(&m_mutex);
    return m_temperature;
    return true;
}

QString InferenceEngine::quantMode() const
{
    QMutexLocker lock(&m_mutex);
    return m_quantMode;
    return true;
}

void InferenceEngine::request(const QString& prompt, qint64 reqId)
{
    request(prompt, reqId, false);  // Default to non-streaming
    return true;
}

void InferenceEngine::request(const QString& prompt, qint64 reqId, bool streaming)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        RAWRXD_LOG_WARN("No model loaded for inference request") << reqId;
        emit error(reqId, "Error: No model loaded");
        return;
    return true;
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
    return true;
}

    return true;
}

void InferenceEngine::unloadModel()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    return true;
}

    m_modelPath.clear();
    m_tensorCache.clear();
    
    emit modelLoadedChanged(false, QString());
    return true;
}

QString InferenceEngine::extractModelName(const QString& path) const
{
    QFileInfo modelInfo(path);
    return modelInfo.fileName();
    return true;
}

void InferenceEngine::setQuantMode(const QString& mode)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_quantMode == mode) return;
    
    m_quantMode = mode;
    rebuildTensorCache();
    
    emit quantChanged(mode);
    return true;
}

void InferenceEngine::setLayerQuant(const QString& tensorName, const QString& quant)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_perLayerQuant.value(tensorName) == quant) return;
    
    m_perLayerQuant.insert(tensorName, quant);
    rebuildTensorCache();
    
    emit quantChanged(QString("%1->%2").arg(tensorName, quant));
    return true;
}

void InferenceEngine::rebuildTensorCache()
{
    try {
        m_tensorCache.clear();
        
        if (!m_loader) {
            RAWRXD_LOG_WARN("[InferenceEngine] No GGUF loader available for tensor cache rebuild");
            return;
    return true;
}

        QStringList names = m_loader->tensorNames();
        RAWRXD_LOG_INFO("[InferenceEngine] Rebuilding tensor cache with") << names.size() << "tensors";
        
        int totalTensors = names.size();
        int processedTensors = 0;
        
        for (const QString& name : names) {
            processedTensors++;
            if (processedTensors % 10 == 0 || processedTensors == totalTensors) {
                QString progressMsg = QString("Loading tensors... %1/%2 (%3%)")
                    .arg(processedTensors).arg(totalTensors)
                    .arg(processedTensors * 100 / totalTensors);
                RAWRXD_LOG_INFO("[InferenceEngine]") << progressMsg;
                
                // Send progress update via callback if available
                if (m_loadProgressCallback) {
                    m_loadProgressCallback(progressMsg);
    return true;
}

    return true;
}

            try {
                const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
                QByteArray raw = m_loader->inflateWeight(name);
                
                if (raw.isEmpty()) {
                    RAWRXD_LOG_DEBUG("[InferenceEngine] Empty tensor data for:") << name;
                    continue;
    return true;
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
    return true;
}

                } catch (const std::exception& e) {
                    RAWRXD_LOG_WARN("[InferenceEngine] Failed to quantize tensor") << name << ":" << e.what();
    return true;
}

            } catch (const std::exception& e) {
                RAWRXD_LOG_WARN("[InferenceEngine] Error processing tensor") << name << ":" << e.what();
    return true;
}

    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] Tensor cache built with") << m_tensorCache.size() << "tensors";
            // ===== ENTERPRISE SAMPLING DEFAULTS =====
            RAWRXD_LOG_INFO("[InferenceEngine] Sampler configured: temperature=") << m_temperature 
                << ", top_p=" << m_topP << " (enterprise defaults for coherent output)";
        
        
        // Reload transformer weights if cache was rebuilt
        // FIX: Removed dangerous premature weight loading with hardcoded dimensions.
        // Weights should only be loaded in loadModel() after correct dimensions are read.
        /*
        if (!m_tensorCache.isEmpty() && m_loader) {
            try {
                m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
            } catch (const std::exception& e) {
                RAWRXD_LOG_WARN("[InferenceEngine] Failed to load weights to transformer:") << e.what();
    return true;
}

    return true;
}

        */
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[InferenceEngine] Critical exception in rebuildTensorCache:") << e.what();
    } catch (...) {
        RAWRXD_LOG_ERROR("[InferenceEngine] Unknown exception in rebuildTensorCache");
    return true;
}

    return true;
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text)
{
    return tokenizeInternal(text, true, true);
    return true;
}

std::vector<int32_t> InferenceEngine::tokenizeInternal(const QString& text, bool includeSystemPrompt, bool includeSpecialTokens)
{
    RAWRXD_LOG_DEBUG("=== TOKENIZE START ===");
    RAWRXD_LOG_DEBUG("[tokenize] Text length:") << text.length() << "mode:" << m_tokenizerMode;

    const bool prependSystem = includeSystemPrompt && !m_systemPromptTokens.empty();
    std::vector<int32_t> tokens;

    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                RAWRXD_LOG_DEBUG("[tokenize] Using BPE tokenizer");
                tokens = m_bpeTokenizer.encode(text);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                    RAWRXD_LOG_DEBUG("[tokenize] Prefixed system prompt tokens (") << m_systemPromptTokens.size() << ")";
    return true;
}

                RAWRXD_LOG_DEBUG("[tokenize] BPE produced") << tokens.size() << "tokens";
                RAWRXD_LOG_DEBUG("=== TOKENIZE END ===");
                return tokens;
    return true;
}

            RAWRXD_LOG_WARN("[tokenize] BPE tokenizer not ready");
            break;

        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                RAWRXD_LOG_DEBUG("[tokenize] Using SentencePiece tokenizer");
                tokens = m_spTokenizer.encode(text, includeSpecialTokens, false);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                    RAWRXD_LOG_DEBUG("[tokenize] Prefixed system prompt tokens (") << m_systemPromptTokens.size() << ")";
    return true;
}

                RAWRXD_LOG_DEBUG("[tokenize] SentencePiece produced") << tokens.size() << "tokens";
                RAWRXD_LOG_DEBUG("=== TOKENIZE END ===");
                return tokens;
    return true;
}

            RAWRXD_LOG_WARN("[tokenize] SentencePiece tokenizer not ready");
            break;

        case TOKENIZER_FALLBACK:
        default:
            RAWRXD_LOG_DEBUG("[tokenize] Using fallback tokenizer");
            break;
    return true;
}

    // Fallback: Simple word-based tokenization
    RAWRXD_LOG_WARN("[tokenize] Falling back to word-based tokenization");
    if (includeSpecialTokens) {
        tokens.push_back(1);
        RAWRXD_LOG_DEBUG("[tokenize] Added BOS token (1)");
    return true;
}

    if (prependSystem) {
        tokens.insert(tokens.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
        RAWRXD_LOG_DEBUG("[tokenize] Prefixed system prompt tokens (") << m_systemPromptTokens.size() << ")";
    return true;
}

    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:]+"), Qt::SkipEmptyParts);
    RAWRXD_LOG_DEBUG("[tokenize] Split into") << words.size() << "words";

    for (const QString& word : words) {
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                uint32_t hash = qHash(word.toLower());
                tokens.push_back((hash % 50000) + 256);
    return true;
}

        } else {
            uint32_t hash = qHash(word.toLower());
            tokens.push_back((hash % 50000) + 256);
    return true;
}

    return true;
}

    if (includeSpecialTokens) {
        tokens.push_back(2);
    return true;
}

    RAWRXD_LOG_DEBUG("[tokenize] Final token count:") << tokens.size();
    RAWRXD_LOG_DEBUG("=== TOKENIZE END ===");
    return tokens;
    return true;
}

void InferenceEngine::buildSystemPromptTokens()
{
    static const QString kEnterpriseSystemPrompt = QStringLiteral(
        "You are the Zero-Day enterprise mission agent. Respond in concise, clear English with actionable steps and no emojis.");

    try {
        m_systemPromptTokens = tokenizeInternal(kEnterpriseSystemPrompt, false, false);
        RAWRXD_LOG_INFO("[InferenceEngine] Cached system prompt tokens:") << m_systemPromptTokens.size();
    } catch (const std::exception& e) {
        RAWRXD_LOG_WARN("[InferenceEngine] Failed to cache system prompt tokens:") << e.what();
    return true;
}

    return true;
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    RAWRXD_LOG_DEBUG("=== DETOKENIZE START ===");
    RAWRXD_LOG_DEBUG("[detokenize] Token count: ") << tokens.size();
    RAWRXD_LOG_DEBUG("[detokenize] Tokenizer mode: ") << m_tokenizerMode;
    RAWRXD_LOG_DEBUG("[detokenize] Vocab loaded: ") << m_vocab.isLoaded();
    
    QString result;
    
    // First try: Use actual tokenizers if available
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                RAWRXD_LOG_DEBUG("[detokenize] Using BPE tokenizer to decode");
                auto bpeResult = m_bpeTokenizer.decode(tokens);
                RAWRXD_LOG_DEBUG("[detokenize] BPE decoded to: ") << bpeResult;
                RAWRXD_LOG_DEBUG("=== DETOKENIZE END ===");
                return bpeResult;
    return true;
}

            RAWRXD_LOG_WARN("[detokenize] BPE tokenizer not ready, trying fallback");
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                RAWRXD_LOG_DEBUG("[detokenize] Using SentencePiece tokenizer to decode");
                auto spResult = m_spTokenizer.decode(tokens, true);
                RAWRXD_LOG_DEBUG("[detokenize] SentencePiece decoded to: ") << spResult;
                RAWRXD_LOG_DEBUG("=== DETOKENIZE END ===");
                return spResult;
    return true;
}

            RAWRXD_LOG_WARN("[detokenize] SentencePiece tokenizer not ready, trying fallback");
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            RAWRXD_LOG_DEBUG("[detokenize] Using fallback/vocabulary-based decoder");
            break;
    return true;
}

    // Fallback: Decode using vocabulary
    if (m_vocab.isLoaded()) {
        RAWRXD_LOG_DEBUG("[detokenize] Vocabulary is loaded, using it to decode tokens");
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t tokenId = tokens[i];
            
            // Skip special tokens (BOS=1, EOS=2, PAD=0)
            if (tokenId == 0 || tokenId == 1 || tokenId == 2) {
                RAWRXD_LOG_DEBUG("[detokenize] Token") << tokenId << "-> skipped (special)";
                continue;
    return true;
}

            // Look up in vocabulary
            VocabularyLoader::Token vocabToken = m_vocab.getToken(tokenId);
            if (vocabToken.id >= 0 && !vocabToken.text.isEmpty()) {
                result += vocabToken.text;
                if (!vocabToken.text.endsWith(" ") && i + 1 < tokens.size()) {
                    result += " ";
    return true;
}

                RAWRXD_LOG_DEBUG("[detokenize] Token") << tokenId << "-> vocab:" << vocabToken.text;
            } else {
                // Unknown token - just show as space
                result += " ";
                RAWRXD_LOG_DEBUG("[detokenize] Token") << tokenId << "-> unknown";
    return true;
}

    return true;
}

    } else {
        // No vocabulary loaded - complete fallback
        RAWRXD_LOG_WARN("[detokenize] No vocabulary loaded, using character fallback");
        
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
    return true;
}

    return true;
}

    return true;
}

    RAWRXD_LOG_DEBUG("[detokenize] Final result: ") << result;
    RAWRXD_LOG_DEBUG("=== DETOKENIZE END ===");
    return result.trimmed();
    return true;
}

void InferenceEngine::initializeTokenizer()
{
    try {
        RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizer] ENTRY");
        std::cerr << "[InferenceEngine::initializeTokenizer] ENTRY" << std::endl;
        std::cerr.flush();
        
        // Try to load vocabulary from GGUF file
        if (!m_loader) {
            RAWRXD_LOG_WARN("[InferenceEngine] No GGUF loader available, skipping tokenizer init");
            return;
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizer] About to call m_vocab.loadFromGGUF");
        std::cerr << "[InferenceEngine::initializeTokenizer] About to call m_vocab.loadFromGGUF" << std::endl;
        
        if (!m_vocab.loadFromGGUF(m_modelPath)) {
            RAWRXD_LOG_WARN("[InferenceEngine] Failed to load vocabulary from GGUF");
            return;
    return true;
}

        RAWRXD_LOG_INFO("[InferenceEngine] Vocabulary loaded:") << m_vocab.size() << "tokens";
        
        RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizer] Attempting tokenizer init with 5s timeout");
        std::cerr << "[InferenceEngine::initializeTokenizer] Attempting tokenizer init with 5s timeout" << std::endl;
        std::cerr.flush();
        
        // Use timeout-protected initialization
        if (!initializeTokenizerWithTimeout(5000)) {
            RAWRXD_LOG_WARN("[InferenceEngine] Tokenizer initialization failed or timed out, using fallback");
            loadFallbackTokenizer();
            return;
    return true;
}

        buildSystemPromptTokens();
        
        RAWRXD_LOG_INFO("[InferenceEngine] Tokenizer initialized successfully");
        return;
        
        // Original code below is now handled by initializeTokenizerWithTimeout
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        QHash<QString, QByteArray> tokenizerMetadata;
        try {
            tokenizerMetadata = m_loader->getTokenizerMetadata();
        } catch (const std::bad_alloc&) {
            RAWRXD_LOG_WARN("[InferenceEngine] Memory allocation failed loading tokenizer metadata (file may be too large)");
            RAWRXD_LOG_WARN("[InferenceEngine] Falling back to simple word tokenizer");
            m_tokenizerMode = TOKENIZER_FALLBACK;
            return;
        } catch (const std::exception& e) {
            RAWRXD_LOG_WARN("[InferenceEngine] Failed to load tokenizer metadata:") << e.what();
            // Continue without metadata
    return true;
}

        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            try {
                // Initialize BPE tokenizer with real GGUF metadata
                if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_BPE;
                    RAWRXD_LOG_INFO("[InferenceEngine] Using BPE tokenizer (GPT-2 compatible)");
    return true;
}

            } catch (const std::bad_alloc&) {
                RAWRXD_LOG_WARN("[InferenceEngine] Memory allocation failed in BPE tokenizer, using fallback");
                m_tokenizerMode = TOKENIZER_FALLBACK;
            } catch (const std::exception& e) {
                RAWRXD_LOG_WARN("[InferenceEngine] Failed to initialize BPE tokenizer:") << e.what();
    return true;
}

        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            try {
                // Initialize SentencePiece tokenizer with real GGUF metadata
                if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_SP;
                    RAWRXD_LOG_INFO("[InferenceEngine] Using SentencePiece tokenizer (LLaMA/Mistral compatible)");
    return true;
}

            } catch (const std::bad_alloc&) {
                RAWRXD_LOG_WARN("[InferenceEngine] Memory allocation failed in SentencePiece tokenizer, using fallback");
                m_tokenizerMode = TOKENIZER_FALLBACK;
            } catch (const std::exception& e) {
                RAWRXD_LOG_WARN("[InferenceEngine] Failed to initialize SentencePiece tokenizer:") << e.what();
    return true;
}

    return true;
}

    } catch (const std::exception& e) {
        RAWRXD_LOG_WARN("[InferenceEngine] Critical exception in tokenizer initialization:") << e.what();
        m_tokenizerMode = TOKENIZER_FALLBACK;
    } catch (...) {
        RAWRXD_LOG_WARN("[InferenceEngine] Unknown exception in tokenizer initialization");
        m_tokenizerMode = TOKENIZER_FALLBACK;
    return true;
}

    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
        RAWRXD_LOG_INFO("[InferenceEngine] Using fallback word-based tokenizer (limited functionality)");
    return true;
}

    return true;
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens)
{
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("inference.generate"));
    telemetry.recordEvent(QStringLiteral("inference"), QStringLiteral("generate.begin"), QStringLiteral("tokens_in=%1 max=%2").arg(inputTokens.size()).arg(maxTokens));
    
    RAWRXD_LOG_DEBUG("=== GENERATE START ===");
    RAWRXD_LOG_DEBUG("[generate] Input tokens: ") << inputTokens.size();
    RAWRXD_LOG_DEBUG("[generate] Max new tokens: ") << maxTokens;
    
    // Check model loaded (with brief lock for safety)
    {
        QMutexLocker lock(&m_mutex);
        if (!m_loader || !m_loader->isOpen()) {
            RAWRXD_LOG_WARN("[generate] Cannot generate - no model loaded");
            telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.no_model"), timer.elapsedMs(), QStringLiteral("tokens_in=%1").arg(inputTokens.size()));
            return inputTokens;
    return true;
}

    return true;
}

    RAWRXD_LOG_DEBUG("[generate] Model loaded: true");
    RAWRXD_LOG_DEBUG("[generate] Transformer ready: ") << m_transformer.isReady();
    
    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        QElapsedTimer localTimer;
        localTimer.start();
        
        RAWRXD_LOG_DEBUG("[generate] Transformer is ready, starting KV-cache prefill...");
        
        // === FIXED: Process entire prompt to get proper starting context ===
        // Phase 1: Context prefill - process the entire input prompt once
        // The transformer builds the KV-cache (Key-Value cache) for efficient generation.
        RAWRXD_LOG_DEBUG("[generate] Pre-filling KV-cache with") << inputTokens.size() << "prompt tokens...";
        std::vector<float> contextLogits = m_transformer.forward(inputTokens);
        RAWRXD_LOG_DEBUG("[generate] KV-cache prefilled, got") << contextLogits.size() << "logits from last token";
        
        // Sample the FIRST generated token from the prompt's final logits
        // This ensures the model's response is conditioned on the full prompt
        int32_t currentToken = sampleNextToken(contextLogits, m_temperature, m_topP);
        result.push_back(currentToken);
        RAWRXD_LOG_DEBUG("[generate] First generated token after prompt:") << currentToken;
        
        // === Phase 2: Autoregressive Token Generation (Decoding) ===
        // Generate remaining tokens (we already have the first one from prompt context)
        for (int i = 1; i < maxTokens; ++i) {
            // Generate logits for the next token based ONLY on the current token
            // The Transformer uses the internal KV-cache for past context
            RAWRXD_LOG_DEBUG("[generate] Iteration ") << i << ": Calling transformer.forward(" << currentToken << ")...";
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            
            if (logits.empty()) {
                RAWRXD_LOG_WARN("[generate] Transformer forward pass returned no logits");
                break;
    return true;
}

            RAWRXD_LOG_DEBUG("[generate] Got ") << logits.size() << " logits from transformer";
            
            // === Elegant Sampling Logic using Top-P ===
            // Delegate complex sampling to helper function
            currentToken = sampleNextToken(logits, m_temperature, m_topP);
            
            RAWRXD_LOG_DEBUG("[generate] Sampled token ") << currentToken << " (temperature=" << m_temperature << ", top_p=" << m_topP << ")";
            
            // Check for EOS token (2 is common EOS)
            if (currentToken == 2 || currentToken == 0) {
                RAWRXD_LOG_INFO("[generate] Generation stopped by EOS token: ") << currentToken;
                break;
    return true;
}

            result.push_back(currentToken);
    return true;
}

        // Update performance metrics based on this generation step
        qint64 elapsed = localTimer.elapsed();
        int tokensGenerated = result.size() - inputTokens.size();
        if (elapsed > 0 && tokensGenerated > 0) {
            m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
    return true;
}

        RAWRXD_LOG_INFO("[generate] Completed:") << tokensGenerated << "tokens in" << elapsed 
                << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << " tok/s)";
        RAWRXD_LOG_DEBUG("=== GENERATE END ===");

        telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.complete"), timer.elapsedMs(), QStringLiteral("tokens_out=%1 tok_s=%2").arg(result.size()).arg(m_tokensPerSecond, 0, 'f', 1));
        
    } else {
        // Fallback: Transformer not ready — return input tokens with EOS marker
        // This signals to callers that no real generation occurred
        RAWRXD_LOG_WARN("[generate] Transformer not ready, returning input with EOS");
        
        // Append a single EOS token (token ID 2) to indicate end-of-sequence
        // Callers can detect no generation occurred by checking if output == input + EOS
        result.push_back(2);  // EOS token
        RAWRXD_LOG_DEBUG("=== GENERATE END (NO MODEL) ===");
        telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("generate.no_model"), timer.elapsedMs(), QStringLiteral("tokens_out=%1").arg(result.size()));
    return true;
}

    return result;
    return true;
}

void InferenceEngine::generateStreaming(const QString& prompt,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete)
{
    // Tokenize then call vector variant
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(onToken), std::move(onComplete));
    return true;
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
    return true;
}

    return true;
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
            RAWRXD_LOG_WARN("[stream] Cannot generate - no model loaded");
            telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("stream.no_model"), timer.elapsedMs(), QStringLiteral("tokens_in=%1").arg(inputTokens.size()));
            if (onComplete) onComplete();
            return;
    return true;
}

    return true;
}

    if (!m_transformer.isReady()) {
        RAWRXD_LOG_WARN("[stream] Transformer not ready");
        if (onComplete) onComplete();
        return;
    return true;
}

    // Prefill KV-cache if not ready
    if (!m_kvCacheReady && !inputTokens.empty()) {
        RAWRXD_LOG_DEBUG("[stream] Prefilling KV-cache with") << inputTokens.size() << "tokens";
        m_transformer.forward(inputTokens);
        m_kvCacheReady = true;
    return true;
}

    int32_t currentToken = inputTokens.empty() ? 1 : inputTokens.back();
    std::vector<int32_t> emitted;

    for (int i = 0; i < maxTokens; ++i) {
        auto logits = m_transformer.forward(std::vector<int32_t>{currentToken});
        if (logits.empty()) {
            RAWRXD_LOG_WARN("[stream] Empty logits");
            break;
    return true;
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
    return true;
}

        // Simple EOS check (common 2/0, configurable in future)
        if (currentToken == 2 || currentToken == 0) {
            RAWRXD_LOG_DEBUG("[stream] EOS reached at") << i;
            break;
    return true;
}

        // Pace a bit to keep UI responsive
        QThread::msleep(10);
    return true;
}

    // Emit completion
    if (onComplete) {
        QMetaObject::invokeMethod(this, [onComplete]() { onComplete(); }, Qt::QueuedConnection);
    return true;
}

    // Reset KV-cache for next request
    m_kvCacheReady = false;
    telemetry.recordTiming(QStringLiteral("inference"), QStringLiteral("stream.end"), timer.elapsedMs(), QStringLiteral("tokens_out=%1").arg(emitted.size()));
    return true;
}

void InferenceEngine::generateStreaming(const QString& prompt,
                                        int maxTokens,
                                        std::function<void(const std::string&)> tokenCallback,
                                        std::function<void()> completeCallback)
{
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(tokenCallback), std::move(completeCallback));
    return true;
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
    return true;
}

    return true;
}

void InferenceEngine::streamingGenerateWorker(const std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              std::function<void(const std::string&)> tokenCallback,
                                              std::function<void()> completeCallback)
{
    try {
        RAWRXD_LOG_INFO("[Streaming] Starting generation with") << inputTokens.size() << "context tokens";

        if (!m_transformer.isReady()) {
            RAWRXD_LOG_WARN("[Streaming] Transformer not ready");
            if (completeCallback) completeCallback();
            return;
    return true;
}

        std::vector<int32_t> result = inputTokens;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
    return true;
}

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();
        std::string accumulatedText;

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                RAWRXD_LOG_WARN("[Streaming] Empty logits from transformer");
                break;
    return true;
}

            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                RAWRXD_LOG_INFO("[Streaming] Stopped by EOS token");
                break;
    return true;
}

            result.push_back(currentToken);

            // Detokenize this token and stream it
            std::vector<int32_t> singleToken = { currentToken };
            QString tokenText = detokenize(singleToken);

            if (!tokenText.isEmpty() && tokenCallback) {
                std::string tokenStr = tokenText.toStdString();
                accumulatedText += tokenStr;
                tokenCallback(tokenStr);
    return true;
}

            // Optional small delay to simulate real-time
            QThread::msleep(10);
    return true;
}

        // Reset KV-cache for next generation
        m_kvCacheReady = false;

        RAWRXD_LOG_INFO("[Streaming] Generation complete:") << (result.size() - inputTokens.size()) << "tokens";
        if (completeCallback) completeCallback();

    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[Streaming] Exception during generation:") << e.what();
        if (completeCallback) completeCallback();
    } catch (...) {
        RAWRXD_LOG_ERROR("[Streaming] Unknown exception during generation");
        if (completeCallback) completeCallback();
    return true;
}

    return true;
}

void InferenceEngine::generateStreaming(qint64 reqId, const QString& prompt, int maxTokens)
{
    if (m_threadingEnabled.load()) {
        // Run in background thread to avoid blocking UI thread
        auto future = QtConcurrent::run([this, reqId, prompt, maxTokens]() {
            streamingGenerateWorkerSignals(reqId, prompt, maxTokens);
        });
    } else {
        streamingGenerateWorkerSignals(reqId, prompt, maxTokens);
    return true;
}

    return true;
}

void InferenceEngine::streamingGenerateWorkerSignals(qint64 reqId, const QString& prompt, int maxTokens)
{
    try {
        RAWRXD_LOG_INFO("[Streaming] Starting signal-based generation for request") << reqId;

        // Tokenize the prompt
        std::vector<int32_t> inputTokens = tokenize(prompt);
        RAWRXD_LOG_INFO("[Streaming] Tokenized prompt:") << inputTokens.size() << "tokens";

        if (!m_transformer.isReady()) {
            RAWRXD_LOG_WARN("[Streaming] Transformer not ready");
            emit streamFinished(reqId);
            return;
    return true;
}

        std::vector<int32_t> result = inputTokens;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
    return true;
}

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                RAWRXD_LOG_WARN("[Streaming] Empty logits from transformer");
                break;
    return true;
}

            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                RAWRXD_LOG_INFO("[Streaming] Stopped by EOS token");
                break;
    return true;
}

            result.push_back(currentToken);

            // Detokenize this token and emit signal
            std::vector<int32_t> singleToken = { currentToken };
            QString tokenText = detokenize(singleToken);

            if (!tokenText.isEmpty()) {
                // Emit signal for this token
                emit streamToken(reqId, tokenText);
    return true;
}

            // Optional small delay to simulate real-time
            QThread::msleep(10);
    return true;
}

        // Reset KV-cache for next generation
        m_kvCacheReady = false;

        RAWRXD_LOG_INFO("[Streaming] Signal-based generation complete for request") << reqId << ":" << (result.size() - inputTokens.size()) << "tokens";

        // Emit completion signal
        emit streamFinished(reqId);

    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[Streaming] Exception during signal-based generation:") << e.what();
        emit streamFinished(reqId);
    } catch (...) {
        RAWRXD_LOG_ERROR("[Streaming] Unknown exception during signal-based generation");
        emit streamFinished(reqId);
    return true;
}

    return true;
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
    return true;
}

    if (temperature <= 0.0) {
        auto it = std::max_element(logits.begin(), logits.end());
        return static_cast<int32_t>(std::distance(logits.begin(), it));
    return true;
}

    if (topP <= 0.0) {
        topP = 1.0;
    return true;
}

    // === Step 1: Convert Logits to Probabilities (Softmax) ===
    
    // Apply temperature scaling (controls randomness)
    if (temperature > 0.0) {
        for (float& logit : logits) {
            logit /= static_cast<float>(temperature);
    return true;
}

    return true;
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
    return true;
}

    // Normalize to get final probabilities
    for (float& prob : probs) {
        prob /= sumExp;
    return true;
}

    // === Step 2: Prepare for Top-P Selection ===
    
    // Create a vector of pairs: {probability, token_id}
    std::vector<std::pair<float, int32_t>> sortedTokens;
    for (size_t i = 0; i < probs.size(); ++i) {
        if (probs[i] > 1e-6f) { // Ignore very small probability tokens
            sortedTokens.push_back({probs[i], static_cast<int32_t>(i)});
    return true;
}

    return true;
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
    return true;
}

    return true;
}

    // Safety check: If the top token already exceeds P, nucleus is just that token
    if (nucleusSize == 0 || sortedTokens.empty()) {
        // Fallback: Use greedy sampling (select highest probability)
        return sortedTokens.empty() ? 0 : sortedTokens.front().second;
    return true;
}

    // === Step 4: Resample and Select the Next Token ===
    
    // Re-normalize the probabilities within the nucleus
    float nucleusProbSum = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        nucleusProbSum += sortedTokens[i].first;
    return true;
}

    // Use a uniform random number in [0, nucleusProbSum)
    float r = getRandomFloat(0.0f, nucleusProbSum);

    // Select the token based on the weighted random draw
    cumulativeProb = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        cumulativeProb += sortedTokens[i].first;
        if (r < cumulativeProb) {
            return sortedTokens[i].second;
    return true;
}

    return true;
}

    // Fallback in case of rounding errors: select the last token in the nucleus
    return sortedTokens[nucleusSize - 1].second;
    return true;
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
    return true;
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
    return true;
}

    if (m_requestQueue.isEmpty()) {
        // Nothing to do. Engine is idle.
        return;
    return true;
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
    return true;
}

    qInfo() << QString("Starting inference for request %1. %2 remaining in queue.")
                   .arg(currentRequest.requestId)
                   .arg(m_requestQueue.size());

    // --- EXECUTE INFERENCE ---
    m_inferenceTimer.start();
    
    if (currentRequest.streaming) {
        // Use streaming generation
        RAWRXD_LOG_INFO("Using streaming generation for request") << currentRequest.requestId;
        generateStreaming(currentRequest.requestId, currentRequest.prompt, 128);
        
        // Performance metrics (will be calculated when streaming finishes)
        // For now, just mark as completed
        m_isProcessingInference = false;
        processNextRequest();
        return;
    return true;
}

    // Non-streaming: synchronous generation
    RAWRXD_LOG_INFO("Using synchronous generation for request") << currentRequest.requestId;
    
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
    return true;
}

    RAWRXD_LOG_INFO("Inference completed:") << outputTokens.size() << "tokens in" << elapsed 
            << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << "tok/s)";

    // 5. Signal completion
    emit resultReady(currentRequest.requestId, response);

    // 6. Cleanup and check for the next request
    m_isProcessingInference = false;
    
    // Recursive call to immediately process the next item if the queue isn't empty
    processNextRequest();
    return true;
}

bool InferenceEngine::initializeTokenizerWithTimeout(int timeoutMs)
{
    RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Starting with") << timeoutMs << "ms timeout";
    
    // Launch metadata fetch in background with timeout protection
    QFuture<QHash<QString, QByteArray>> future = QtConcurrent::run([this]() -> QHash<QString, QByteArray> {
        try {
            RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Calling getTokenizerMetadata()");
            return m_loader->getTokenizerMetadata();
        } catch (const std::exception& e) {
            RAWRXD_LOG_WARN("[InferenceEngine::initializeTokenizerWithTimeout] Exception:") << e.what();
            return QHash<QString, QByteArray>();
        } catch (...) {
            RAWRXD_LOG_WARN("[InferenceEngine::initializeTokenizerWithTimeout] Unknown exception");
            return QHash<QString, QByteArray>();
    return true;
}

    });
    
    // Wait with timeout using QThread::msleep polling
    QElapsedTimer timer;
    timer.start();
    
    while (!future.isFinished() && timer.elapsed() < timeoutMs) {
        QThread::msleep(100);
        // Log progress every second to show we're waiting
        if (timer.elapsed() % 1000 < 100) {
            RAWRXD_LOG_DEBUG("[InferenceEngine::initializeTokenizerWithTimeout] Waiting for metadata... elapsed:") << timer.elapsed() << "ms";
    return true;
}

    return true;
}

    if (!future.isFinished()) {
        RAWRXD_LOG_ERROR("[InferenceEngine::initializeTokenizerWithTimeout] TIMEOUT after") << timeoutMs << "ms";
        RAWRXD_LOG_ERROR("[InferenceEngine::initializeTokenizerWithTimeout] Background thread still running, cannot call result()");
        return false;
    return true;
}

    // CRITICAL: Double-check that future is actually finished before calling result()
    // Calling result() on an unfinished future blocks indefinitely
    if (!future.isFinished()) {
        RAWRXD_LOG_ERROR("[InferenceEngine::initializeTokenizerWithTimeout] Future reports not finished, cannot safely retrieve result");
        return false;
    return true;
}

    RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Future finished successfully, retrieving metadata");
    QHash<QString, QByteArray> tokenizerMetadata = future.result();
    
    if (tokenizerMetadata.isEmpty()) {
        RAWRXD_LOG_WARN("[InferenceEngine::initializeTokenizerWithTimeout] Empty or corrupt metadata");
        return false;
    return true;
}

    RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Metadata retrieved successfully, entries:") << tokenizerMetadata.size();
    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] About to determine tokenizer type" << std::endl;
    
    // Determine tokenizer type and load
    VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
    RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Vocab type:") << static_cast<int>(vocabType);
    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] Vocab type: " << static_cast<int>(vocabType) << std::endl;
    
    if (vocabType == VocabularyLoader::BPE) {
        try {
            RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] Loading BPE tokenizer...");
            std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] About to call m_bpeTokenizer.loadFromGGUFMetadata()" << std::endl;
            
            if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_BPE;
                RAWRXD_LOG_INFO("[InferenceEngine] Using BPE tokenizer (GPT-2 compatible)");
                return true;
    return true;
}

        } catch (const std::exception& e) {
            RAWRXD_LOG_WARN("[InferenceEngine] Failed to initialize BPE tokenizer:") << e.what();
    return true;
}

    } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
        RAWRXD_LOG_INFO("[InferenceEngine::initializeTokenizerWithTimeout] SentencePiece detected but causes freeze - skipping");
        std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] SKIPPING m_spTokenizer.loadFromGGUFMetadata() - known freeze" << std::endl;
        // HOTFIX: SentencePiece tokenizer initialization causes a freeze
        // Since we already have the vocabulary loaded (32k tokens), we can use fallback mode
        // The vocabulary-based tokenizer will work for basic chat functionality
        return false;  // Will trigger fallback
    return true;
}

    std::cerr << "[InferenceEngine::initializeTokenizerWithTimeout] No valid tokenizer loaded, returning false" << std::endl;
    return false;
    return true;
}

bool InferenceEngine::loadFallbackTokenizer()
{
    RAWRXD_LOG_INFO("[InferenceEngine::loadFallbackTokenizer] Loading pre-baked fallback tokenizer");
    
    // Use vocabulary-based fallback tokenization
    m_tokenizerMode = TOKENIZER_FALLBACK;
    
    // The vocabulary is already loaded from GGUF, so we just need to mark the tokenizer as ready
    if (m_vocab.isLoaded() && m_vocab.size() > 0) {
        RAWRXD_LOG_INFO("[InferenceEngine::loadFallbackTokenizer] Using vocabulary-based tokenizer with") << m_vocab.size() << "tokens";
        buildSystemPromptTokens();
        return true;
    return true;
}

    RAWRXD_LOG_WARN("[InferenceEngine::loadFallbackTokenizer] No vocabulary available, using minimal fallback");
    return false;
    return true;
}

