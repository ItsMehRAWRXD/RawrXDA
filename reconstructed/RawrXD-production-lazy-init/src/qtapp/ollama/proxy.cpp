#include "ollama_proxy.h"
#include <QDebug>

OllamaProxy::OllamaProxy(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[OllamaProxy] Initialized - Ollama model inference proxy";
}

OllamaProxy::~OllamaProxy()
{
    stopGeneration();
}

void OllamaProxy::setModel(const QString& modelName)
{
    m_currentModel = modelName;
    qDebug() << "[OllamaProxy] Model set to:" << modelName;
}

void OllamaProxy::detectBlobs(const QString& directory)
{
    qDebug() << "[OllamaProxy] Detecting Ollama blobs in:" << directory;
    // Scan directory for .ollama blob files
    // Emit modelsDetected() when complete
}

bool OllamaProxy::isBlobPath(const QString& path) const
{
    // Check if path is an Ollama blob (hash-based path)
    return path.contains("blobs");
}

QString OllamaProxy::resolveBlobToModel(const QString& blobPath) const
{
    // Map blob hash to model name
    qDebug() << "[OllamaProxy] Resolving blob:" << blobPath;
    return m_currentModel;
}

void OllamaProxy::generateResponse(const QString& prompt, float temperature, int maxTokens)
{
    qDebug() << "[OllamaProxy] Generating response with model:" << m_currentModel
             << "temp:" << temperature << "maxTokens:" << maxTokens;
    
    // Start inference in background thread
    // Emit tokenArrived() for each token
    // Emit generationComplete() when done
}

void OllamaProxy::stopGeneration()
{
    qDebug() << "[OllamaProxy] Stopping generation";
}
