/**
 * @file gguf_proxy_server.hpp
 * @brief TCP proxy between IDE-agent and GGUF model server (Qt-free, WinSock)
 */
#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <cstdint>

class AgentHotPatcher;

struct ClientConnection {
    uintptr_t clientSocket = 0;
    uintptr_t ggufSocket = 0;
    std::vector<uint8_t> requestBuffer;
    std::vector<uint8_t> responseBuffer;
};

class GGUFProxyServer {
public:
    GGUFProxyServer() = default;
    ~GGUFProxyServer();
    GGUFProxyServer(const GGUFProxyServer&) = delete;
    GGUFProxyServer& operator=(const GGUFProxyServer&) = delete;

    void initialize(int listenPort, AgentHotPatcher* hotPatcher, const std::string& ggufEndpoint);
    bool startServer();
    void stopServer();
    bool isListening() const;
    int getPort() const { return m_listenPort; }
    int getActiveConnections() const { return m_activeConnections; }
    nlohmann::json getServerStatistics() const;
    void setConnectionPoolSize(int size);
    void setConnectionTimeout(int ms);
    std::string parseIncomingRequest(const std::vector<uint8_t>& data);

    // Callbacks (replace Qt signals)
    std::function<void(int)> onServerStarted;
    std::function<void()> onServerStopped;

private:
    void handleIncomingConnection(uintptr_t socketDescriptor);
    void forwardToGGUF(uintptr_t socketDescriptor);
    void processGGUFResponse(uintptr_t socketDescriptor);
    void sendResponseToClient(uintptr_t socketDescriptor, const std::string& response);

    int m_listenPort = 0;
    std::string m_ggufEndpoint;
    AgentHotPatcher* m_hotPatcher = nullptr;
    uintptr_t m_listenSocket = 0;
    bool m_listening = false;
    std::map<uintptr_t, std::unique_ptr<ClientConnection>> m_connections;
    int m_connectionPoolSize = 10;
    int m_connectionTimeout = 5000;
    mutable std::mutex m_statsMutex;
    int64_t m_requestsProcessed = 0;
    int64_t m_hallucinationsCorrected = 0;
    int64_t m_navigationErrorsFixed = 0;
    int m_activeConnections = 0;
};
