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
        } else {
            qInfo() << "Transformer initialized successfully";
        }
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
        emit error(reqId, "Error: No model loaded");
        return;
    }
    
    m_inferenceTimer.start();
    
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
}


