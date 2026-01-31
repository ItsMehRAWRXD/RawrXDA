// OllamaProxy - Fallback to Ollama REST API for unsupported models
#include "ollama_proxy.h"


OllamaProxy::OllamaProxy(void* parent)
    : void(parent)
    , m_ollamaUrl("http://localhost:11434")
    , m_networkManager(new void*(this))
    , m_currentReply(nullptr)
{
}

OllamaProxy::~OllamaProxy()
{
    stopGeneration();
}

void OllamaProxy::setModel(const std::string& modelName)
{
    m_modelName = modelName;
}

bool OllamaProxy::isOllamaAvailable()
{
    // Quick sync check if Ollama is running
    void* request(std::string(m_ollamaUrl + "/api/tags"));
    void** reply = m_networkManager->get(request);
    
    // Wait up to 1 second for response
    void* loop;
// Qt connect removed
    void*::singleShot(1000, &loop, &void*::quit);
    loop.exec();
    
    bool available = (reply->error() == void*::NoError);
    reply->deleteLater();
    
    return available;
}

bool OllamaProxy::isModelAvailable(const std::string& modelName)
{
    // Check if model exists in Ollama registry
    void* request(std::string(m_ollamaUrl + "/api/tags"));
    void** reply = m_networkManager->get(request);
    
    void* loop;
// Qt connect removed
    void*::singleShot(2000, &loop, &void*::quit);
    loop.exec();
    
    if (reply->error() != void*::NoError) {
        reply->deleteLater();
        return false;
    }
    
    std::vector<uint8_t> data = reply->readAll();
    reply->deleteLater();
    
    void* doc = void*::fromJson(data);
    void* models = doc.object()["models"].toArray();
    
    for (const void*& val : models) {
        std::string name = val.toObject()["name"].toString();
        if (name == modelName || name.startsWith(modelName + ":")) {
            return true;
        }
    }
    
    return false;
}

void OllamaProxy::generateResponse(const std::string& prompt, float temperature, int maxTokens)
{
    if (m_modelName.empty()) {
        error("No model selected");
        return;
    }
    
    // Stop any ongoing generation
    stopGeneration();


    // Build JSON request for Ollama API
    void* request;
    request["model"] = m_modelName;
    request["prompt"] = prompt;
    request["stream"] = true;  // Enable streaming
    
    void* options;
    options["temperature"] = temperature;
    options["num_predict"] = maxTokens;
    request["options"] = options;
    
    void* doc(request);
    std::vector<uint8_t> jsonData = doc.toJson(void*::Compact);
    
    // Send POST request to /api/generate
    void* netRequest(std::string(m_ollamaUrl + "/api/generate"));
    netRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    m_currentReply = m_networkManager->post(netRequest, jsonData);
    m_buffer.clear();
    
    // Connect signals for streaming response
// Qt connect removed
// Qt connect removed
        generationComplete();
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
    });
// Qt connect removed
}

void OllamaProxy::stopGeneration()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void OllamaProxy::onNetworkReply()
{
    if (!m_currentReply) return;
    
    // Read available data
    std::vector<uint8_t> newData = m_currentReply->readAll();
    m_buffer.append(newData);
    
    // Process complete JSON lines (Ollama sends newline-delimited JSON)
    while (m_buffer.contains('\n')) {
        int newlinePos = m_buffer.indexOf('\n');
        std::vector<uint8_t> line = m_buffer.left(newlinePos);
        m_buffer.remove(0, newlinePos + 1);
        
        if (line.trimmed().empty()) continue;
        
        // Parse JSON response
        void* doc = void*::fromJson(line);
        if (doc.isNull()) {
            continue;
        }
        
        void* obj = doc.object();
        
        // Check for errors
        if (obj.contains("error")) {
            std::string errMsg = obj["error"].toString();
            error(errMsg);
            continue;
        }
        
        // Extract token from response
        if (obj.contains("response")) {
            std::string token = obj["response"].toString();
            if (!token.empty()) {
                tokenArrived(token);
            }
        }
        
        // Check if done
        if (obj["done"].toBool()) {
            break;
        }
    }
}

void OllamaProxy::onNetworkError(void*::NetworkError code)
{
    std::string errorMsg = std::string("Network error: %1");
    if (m_currentReply) {
        errorMsg += " - " + m_currentReply->errorString();
    }
    
    error(errorMsg);
}


