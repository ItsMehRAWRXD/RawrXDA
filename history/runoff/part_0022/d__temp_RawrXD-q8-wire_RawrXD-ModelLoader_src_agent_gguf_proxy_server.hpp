#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <nlohmann/json.hpp>

class AgentHotPatcher;

/**
 * @struct ClientConnection
 * @brief Represents a single client connection and its backend connection
 */
struct ClientConnection {
    uintptr_t clientSocket = 0;
    uintptr_t ggufSocket = 0;
    std::string requestBuffer;
    std::string responseBuffer;
    bool active = false;
};

/**
 * @class GGUFProxyServer
 * @brief A thin TCP proxy using Win32 sockets
 */
class GGUFProxyServer {
public:
    explicit GGUFProxyServer();
    ~GGUFProxyServer();

    /** Configuration */
    void initialize(int listenPort,
                    AgentHotPatcher* hotPatcher,
                    const std::string& ggufEndpoint);

    /** Lifecycle */
    bool startServer();
    void stopServer();
    bool isListening() const { return m_running.load(); }

    /** Statistics */
    nlohmann::json getServerStatistics() const;

    /** Callbacks */
    using ServerStartedCallback = std::function<void(int port)>;
    using ServerStoppedCallback = std::function<void()>;

    void setServerStartedCallback(ServerStartedCallback cb) { m_onServerStarted = cb; }
    void setServerStoppedCallback(ServerStoppedCallback cb) { m_onServerStopped = cb; }

private:
    void acceptLoop();
    void handleClient(uintptr_t clientSocket);
    bool connectToBackend(uintptr_t& socketOut, const std::string& endpoint);

    int m_listenPort = 0;
    AgentHotPatcher* m_hotPatcher = nullptr;
    std::string m_ggufEndpoint;
    
    std::atomic<bool> m_running{false};
    std::thread m_acceptThread;
    uintptr_t m_listenSocket = 0;

    mutable std::mutex m_connectionsMutex;
    std::vector<std::unique_ptr<ClientConnection>> m_connections;

    ServerStartedCallback m_onServerStarted;
    ServerStoppedCallback m_onServerStopped;

    // Statistics
    std::atomic<uint64_t> m_totalRequests{0};
    std::atomic<uint64_t> m_totalPatched{0};
};

#endif // GGUF_PROXY_SERVER_HPP
