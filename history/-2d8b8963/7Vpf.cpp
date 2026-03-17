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
    // TODO: Complete ggml API integration
    /*
    int nLayers = 12;
    int nEmbd = 768;
    int nHead = 12;
    int nVocab = 50257;
    
    if (!m_tensorCache.isEmpty()) {
        m_transformer.loadWeights(m_tensorCache, nLayers, nEmbd, nHead, nVocab);
    }
    */
    
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
    
    // Transformer inference implementation in progress
    // Using fallback for now
    qInfo() << "Transformer inference requested for:" << prompt;
    
    qint64 totalSize = 0;
    for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
        totalSize += it.value().size();
    }
    
    response = QString("✓ Transformer inference ready!\n\n"
                      "Model: %1\n"
                      "Quantization: %2\n"
                      "Cached tensors: %3 (%4 KB)\n"
                      "Input: \"%5\"\n\n"
                      "[Full transformer inference with ggml backend - implementation in progress]")
                  .arg(extractModelName(m_modelPath))
                  .arg(m_quantMode)
                  .arg(m_tensorCache.size())
                  .arg(totalSize / 1024)
                  .arg(prompt);
    
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
    if (!m_tensorCache.isEmpty()) {
        // TODO: Reload transformer when ggml API integration is complete
        // m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text) constange
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
    
    // Delegate to TransformerInference for actual execution
    return m_transformer.generate(prompt);
}


