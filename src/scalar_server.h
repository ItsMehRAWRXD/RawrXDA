#pragma once


// Forward declarations


class TransformerBlockScalar;
class InferenceEngine;

class ScalarServer : public void
{

public:
    explicit ScalarServer(void *parent = nullptr);
    ~ScalarServer();
    
    bool startServer(quint16 port = 8080);
    void stopServer();
    bool loadModel(const std::string &modelPath);
    
    quint16 getPort() const;
    bool isRunning() const;

private:
    void handleNewConnection();
    void handleClientData(void* *clientSocket);

private:
    void handleInferenceRequest(void* *clientSocket, const void* &request);
    void handleChatRequest(void* *clientSocket, const void* &request);
    void handleAnalyzeRequest(void* *clientSocket, const void* &request);
    
    void sendJsonResponse(void* *clientSocket, const void* &response);
    void sendErrorResponse(void* *clientSocket, const std::string &error);
    
    void* *m_server;
    TransformerBlockScalar *m_transformerBlock;
    InferenceEngine *m_inferenceEngine;
};

