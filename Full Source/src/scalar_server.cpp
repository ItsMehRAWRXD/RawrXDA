// Scalar Server - Lightweight inference server for scalar operations

#include "scalar_server.h"
#include "qtapp/inference_engine.hpp"
#include "transformer_block_scalar.h"


ScalarServer::ScalarServer(void *parent)
    : void(parent)
    , m_server(new void*(this))
    , m_transformerBlock(new TransformerBlockScalar(this))
    , m_inferenceEngine(new InferenceEngine(this))
{
// Qt connect removed
}

ScalarServer::~ScalarServer()
{
    stopServer();
}

bool ScalarServer::startServer(quint16 port)
{
    if (m_server->isListening()) {
        return true;
    }
    
    if (!m_server->listen(std::string::Any, port)) {
        return false;
    }
    
    return true;
}

void ScalarServer::stopServer()
{
    if (m_server->isListening()) {
        m_server->close();
    }
}

void ScalarServer::handleNewConnection()
{
    void* *clientSocket = m_server->nextPendingConnection();
// Qt connect removed
    });
// Qt connect removed
}

void ScalarServer::handleClientData(void* *clientSocket)
{
    std::vector<uint8_t> data = clientSocket->readAll();
    
    // Parse JSON request
    void* doc = void*::fromJson(data);
    if (doc.isNull()) {
        sendErrorResponse(clientSocket, "Invalid JSON");
        return;
    }
    
    void* request = doc.object();
    std::string method = request.value("method").toString();
    
    if (method == "inference") {
        handleInferenceRequest(clientSocket, request);
    } else if (method == "chat") {
        handleChatRequest(clientSocket, request);
    } else if (method == "analyze") {
        handleAnalyzeRequest(clientSocket, request);
    } else {
        sendErrorResponse(clientSocket, "Unknown method: " + method);
    }
}

void ScalarServer::handleInferenceRequest(void* *clientSocket, const void* &request)
{
    void* inputArray = request.value("input").toArray();
    uint32_t layerIdx = request.value("layer").toInt();
    uint32_t seqLen = request.value("seq_len").toInt();
    
    // Convert input to float array
    std::vector<float> input(inputArray.size());
    for (int i = 0; i < inputArray.size(); ++i) {
        input[i] = inputArray[i].toDouble();
    }
    
    // Perform inference
    std::vector<float> output(input.size());
    bool success = m_transformerBlock->forwardPass(input.data(), output.data(), layerIdx, seqLen);
    
    // Prepare response
    void* response;
    response["success"] = success;
    
    if (success) {
        void* outputArray;
        for (float val : output) {
            outputArray.append(val);
        }
        response["output"] = outputArray;
    } else {
        response["error"] = "Inference failed";
    }
    
    sendJsonResponse(clientSocket, response);
}

void ScalarServer::handleChatRequest(void* *clientSocket, const void* &request)
{
    std::string message = request.value("message").toString();
    
    // Process chat message through inference engine
    std::string response = m_inferenceEngine->processChat(message);
    
    void* jsonResponse;
    jsonResponse["success"] = true;
    jsonResponse["response"] = response;
    
    sendJsonResponse(clientSocket, jsonResponse);
}

void ScalarServer::handleAnalyzeRequest(void* *clientSocket, const void* &request)
{
    std::string code = request.value("code").toString();
    
    // Analyze code through inference engine
    std::string analysis = m_inferenceEngine->analyzeCode(code);
    
    void* jsonResponse;
    jsonResponse["success"] = true;
    jsonResponse["analysis"] = analysis;
    
    sendJsonResponse(clientSocket, jsonResponse);
}

void ScalarServer::sendJsonResponse(void* *clientSocket, const void* &response)
{
    void* doc(response);
    std::vector<uint8_t> data = doc.toJson(void*::Compact);
    
    clientSocket->write(data);
    clientSocket->flush();
}

void ScalarServer::sendErrorResponse(void* *clientSocket, const std::string &error)
{
    void* response;
    response["success"] = false;
    response["error"] = error;
    
    sendJsonResponse(clientSocket, response);
}

bool ScalarServer::loadModel(const std::string &modelPath)
{
    // Load model weights into transformer block
    // This would typically involve GGUF loader integration
    
    // For now, initialize with default parameters
    return m_transformerBlock->initialize(32, 32, 128, 4096);
}

quint16 ScalarServer::getPort() const
{
    return m_server->serverPort();
}

bool ScalarServer::isRunning() const
{
    return m_server->isListening();
}
