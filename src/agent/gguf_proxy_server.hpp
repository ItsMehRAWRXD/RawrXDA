#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link against ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class AgentHotPatcher;

struct ClientConnection {
    SOCKET clientSocket = INVALID_SOCKET;
    SOCKET ggufSocket = INVALID_SOCKET;
    std::vector<uint8_t> requestBuffer;
    std::vector<uint8_t> responseBuffer;
};

class GGUFProxyServer {
public:
    GGUFProxyServer();
    ~GGUFProxyServer();

    // No copy
    GGUFProxyServer(const GGUFProxyServer&) = delete;
    GGUFProxyServer& operator=(const GGUFProxyServer&) = delete;

    void initialize(int listenPort, AgentHotPatcher* hotPatcher, const std::string& ggufEndpoint);
    
    // Returns true if started
    bool startServer();
    void stopServer();

    bool isListening() const { return m_isListening; }

    // Statistics json string
    std::string getServerStatistics() const;

private:
    void acceptLoop();
    void handleClient(SOCKET clientSocket);
    SOCKET connectToBackend();

    int m_listenPort = 0;
    std::string m_ggufEndpoint;
    AgentHotPatcher* m_hotPatcher = nullptr;
    
    SOCKET m_listenSocket = INVALID_SOCKET;
    std::atomic<bool> m_isListening{false};
    std::thread m_acceptThread;
    
    mutable std::mutex m_mutex;
    std::vector<std::thread> m_clientThreads; // Or detach?
    
    std::atomic<long long> m_requestsProcessed{0};
    std::atomic<long long> m_bytesTransferred{0};
};
