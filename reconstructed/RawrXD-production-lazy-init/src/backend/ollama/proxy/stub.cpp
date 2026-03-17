// Minimal OllamaProxy implementation stub for production build
// Full implementation deferred to separate compilation unit due to Qt include conflicts
#include "../include/ollama_proxy.h"
#include <QDebug>

OllamaProxy::OllamaProxy(QObject* parent)
    : QObject(parent)
    , m_ollamaUrl("http://localhost:11434")
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
    qDebug() << "[OllamaProxy] Initialized (stub implementation)";
}

OllamaProxy::~OllamaProxy()
{
    stopGeneration();
}

void OllamaProxy::setModel(const QString& modelName)
{
    m_modelName = modelName;
    qDebug() << "[OllamaProxy] Model set to:" << modelName;
}

bool OllamaProxy::isOllamaAvailable()
{
    qDebug() << "[OllamaProxy] Checking Ollama availability (stub)";
    return false;
}

bool OllamaProxy::isModelAvailable(const QString& modelName)
{
    qDebug() << "[OllamaProxy] Checking model:" << modelName << "(stub)";
    return false;
}

void OllamaProxy::generateResponse(const QString& prompt, float temperature, int maxTokens)
{
    qDebug() << "[OllamaProxy] Generate response (stub) - prompt length:" << prompt.length();
    // Stub implementation - emit completion immediately
    emit generationComplete();
}

void OllamaProxy::stopGeneration()
{
    qDebug() << "[OllamaProxy] Stop generation";
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
}
