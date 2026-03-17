#include "ollama_proxy.h"

OllamaProxy::OllamaProxy(void* parent) : m_isRunning(false) {}
OllamaProxy::~OllamaProxy() {}

void OllamaProxy::setModel(const String& modelName) {
    m_modelName = modelName;
}

bool OllamaProxy::isOllamaAvailable() {
    return false; // Stub
}

bool OllamaProxy::isModelAvailable(const String& modelName) {
    return false; // Stub
}

void OllamaProxy::generateResponse(const String& prompt, float temperature, int maxTokens) {
    // Stub
}

void OllamaProxy::stopGeneration() {
    m_isRunning = false;
}
