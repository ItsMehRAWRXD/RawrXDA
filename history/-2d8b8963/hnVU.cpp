#include "inference_engine.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
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

bool InferenceEngine::loadModel(const QString& path)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    m_loader = new GGUFLoader(path);
    
    if (!m_loader->isOpen()) {
        qWarning() << "Failed to load GGUF model:" << path;
        delete m_loader;
        m_loader = nullptr;
        emit modelLoadedChanged(false, QString());
        return false;
    }
    
    m_modelPath = path;
    QString modelName = extractModelName(path);
    qInfo() << "Model loaded successfully:" << modelName;
    
    // Build initial quantized tensor cache
    rebuildTensorCache();
    
    // Initialize transformer with cached weights
    int nLayers = 12;
    int nEmbd = 768;
    int nHead = 12;
    int nVocab = 50257;
    
    if (!m_tensorCache.isEmpty()) {
        m_transformer.loadWeights(m_tensorCache, nLayers, nEmbd, nHead, nVocab);
    }
    
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
    if (!m_loader || !m_loader->isOpen()) return {};
    return m_loader->tensorNames();
}

void InferenceEngine::request(const QString& prompt, qint64 reqId)
{
    QMutexLocker lock(&m_mutex);
    
    if (!m_loader || !m_loader->isOpen()) {
        emit error(reqId, "No model loaded");
        return;
    }
    
    QString response;
    
    // Try actual transformer inference
    if (m_transformer.isReady()) {
        qInfo() << "Running transformer inference";
        
        std::vector<int32_t> tokens = tokenize(prompt);
        if (tokens.empty()) {
            emit error(reqId, "Tokenization failed");
            return;
        }
        
        std::vector<int32_t> generated = m_transformer.generate(tokens, 100, 0.8f);
        response = detokenize(generated);
        
        if (response.isEmpty()) {
            response = "[Generated " + QString::number(generated.size()) + " tokens]";
        }
    } else {
        // Fallback
        qint64 totalSize = 0;
        for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
            totalSize += it.value().size();
        }
        
        response = QString("Transformer not ready. Model: %1, Quant: %2, Cached: %3 tensors (%4 KB)")
                      .arg(extractModelName(m_modelPath))
                      .arg(m_quantMode)
                      .arg(m_tensorCache.size())
                      .arg(totalSize / 1024);
    }
    
    for (int i = 0; i < response.length(); ++i) {
        emit streamToken(reqId, QString(response[i]));
        QThread::msleep(5);
    }
    emit streamFinished(reqId);
    emit resultReady(reqId, response);
}

void InferenceEngine::unloadModel()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
        m_modelPath.clear();
        emit modelLoadedChanged(false, QString());
        qInfo() << "Model unloaded";
    }
}

QString InferenceEngine::extractModelName(const QString& path) const
{
    QFileInfo info(path);
    return info.fileName();
}

void InferenceEngine::setQuantMode(const QString& mode)
{
    QMutexLocker lock(&m_mutex);
    
    if (mode == m_quantMode) return;
    
    qInfo() << "Changing quantization mode:" << m_quantMode << "->" << mode;
    m_quantMode = mode;
    rebuildTensorCache();
    emit quantChanged(mode);
}

void InferenceEngine::setLayerQuant(const QString& tensorName, const QString& quant)
{
    QMutexLocker lock(&m_mutex);
    
    qInfo() << "Setting layer quant:" << tensorName << "->" << quant;
    m_perLayerQuant[tensorName] = quant;
    
    if (m_loader && m_loader->isOpen()) {
        QByteArray raw = m_loader->inflateWeight(tensorName);
        if (!raw.isEmpty()) m_tensorCache[tensorName] = apply_quant(raw, quant);
    }
}

void InferenceEngine::rebuildTensorCache()
{
    if (!m_loader || !m_loader->isOpen()) return;

    qInfo() << "Rebuilding tensor cache:" << m_quantMode;
    m_tensorCache.clear();

    const QStringList names = m_loader->tensorNames();
    for (const QString& name : names) {
        const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
        QByteArray raw = m_loader->inflateWeight(name);
        if (raw.isEmpty()) continue;
        m_tensorCache.insert(name, apply_quant(raw, qmode));
    }
    
    if (!m_tensorCache.isEmpty()) {
        m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text)
{
    // Simple space-based tokenization for now
    // TODO: Replace with BPE or SentencePiece tokenizer
    std::vector<int32_t> tokens;
    QStringList words = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    for (const QString& word : words) {
        // Use hash of word as token ID (simple placeholder)
        int32_t token = qHash(word) & 0xFFFF;  // Limit to 16-bit range
        tokens.push_back(token);
    }
    
    return tokens;
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    // Simple placeholder: convert token IDs to hex strings
    // TODO: Replace with actual vocabulary lookup
    QStringList parts;
    for (int32_t token : tokens) {
        parts.append(QString("tok_%1").arg(token, 4, 16, QChar('0')));
    }
    
    return parts.join(" ");
}

bool InferenceEngine::initGGMLContext()
{
    if (m_ggmlCtx) return true;
    
    // Allocate ggml context with sufficient memory
    size_t mem_size = 512 * 1024 * 1024;  // 512 MB for context
    struct ggml_init_params params = {
        /*.mem_size   =*/ mem_size,
        /*.mem_buffer =*/ nullptr,
        /*.no_alloc   =*/ false,
    };
    
    m_ggmlCtx = ggml_init(params);
    if (!m_ggmlCtx) {
        qCritical() << "Failed to initialize GGML context";
        return false;
    }
    
    qInfo() << "Initialized GGML context with" << (mem_size / 1024 / 1024) << "MB";
    return true;
}

void InferenceEngine::freeGGMLContext()
{
    if (m_ggmlCtx) {
        ggml_free(m_ggmlCtx);
        m_ggmlCtx = nullptr;
        m_ggmlTensors.clear();
        qInfo() << "Freed GGML context";
    }
}

QString InferenceEngine::runTransformerInference(const QString& prompt, qint64 reqId)
{
    Q_UNUSED(reqId);
    
    if (!m_ggmlCtx) {
        qWarning() << "GGML context not initialized";
        return QString("Error: GGML context not ready");
    }
    
    // Start timing for performance metrics
    m_inferenceTimer.start();
    
    // Load embedding weights to demonstrate the pipeline
    QByteArray weights = m_loader->inflateWeight("token_embed.weight");
    
    if (weights.isEmpty()) {
        // Try alternative tensor names
        weights = m_loader->inflateWeight("model.embed_tokens.weight");
    }
    
    if (weights.isEmpty()) {
        // Try to get any tensor from cache to demonstrate quantization
        if (!m_tensorCache.isEmpty()) {
            auto it = m_tensorCache.constBegin();
            weights = it.value();
        }
    }
    
    // Generate response with actual quantization information
    QString response = QString("Transformer inference for: '%1'\n"
                              "Model: %2\n"
                              "Quantization: %3\n"
                              "Memory: %4 MB\n"
                              "Tensor count: %5\n"
                              "Weight data: %6 bytes\n"
                              "\n"
                              "Note: Using scalar quantization pipeline.\n"
                              "Full attention mechanism and KV-cache pending.")
                          .arg(prompt)
                          .arg(extractModelName(m_modelPath))
                          .arg(m_quantMode)
                          .arg(m_memoryUsageMB)
                          .arg(m_tensorCache.size())
                          .arg(weights.size());
    
    // Track token count and timing
    int tokenCount = prompt.split(' ').size() + 20;  // Approximate: input tokens + generated tokens
    
    // Calculate performance metrics
    qint64 elapsedMs = m_inferenceTimer.elapsed();
    if (elapsedMs > 0) {
        m_tokensPerSecond = (tokenCount * 1000.0) / elapsedMs;
    }
    
    return response;
}


