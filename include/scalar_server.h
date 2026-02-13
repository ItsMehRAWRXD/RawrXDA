#pragma once

// C++20, no Qt. TCP server for scalar inference; use Win32 SOCKET or platform API in impl.

#include <string>
#include <memory>
#include <functional>

struct ScalarServerImpl;  // Opaque; impl uses SOCKET / Win32 or POSIX

class ScalarServer
{
public:
    ScalarServer();
    ~ScalarServer();

    bool startServer(uint16_t port = 8080);
    void stopServer();
    bool loadModel(const std::string& modelPath);

    uint16_t getPort() const;
    bool isRunning() const;

private:
    void handleNewConnection();
    void handleClientData(void* clientSocket);  // SOCKET or platform handle
    void handleInferenceRequest(void* clientSocket, const std::string& requestJson);
    void handleChatRequest(void* clientSocket, const std::string& requestJson);
    void handleAnalyzeRequest(void* clientSocket, const std::string& requestJson);
    void sendJsonResponse(void* clientSocket, const std::string& responseJson);
    void sendErrorResponse(void* clientSocket, const std::string& error);

    std::unique_ptr<ScalarServerImpl> m_impl;
};
