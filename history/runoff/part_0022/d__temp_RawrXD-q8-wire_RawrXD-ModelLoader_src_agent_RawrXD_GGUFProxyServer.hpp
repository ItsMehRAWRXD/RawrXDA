/**
 * @file RawrXD_GGUFProxyServer.hpp
 * @brief Win32/Winsock2 GGUF Model Proxy Server (replaces Qt TCP server)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wsock32.lib")

namespace RawrXD::Agent {

struct GGUFClientConnection {
    SOCKET client_socket = INVALID_SOCKET;
    SOCKET gguf_socket = INVALID_SOCKET;
    std::vector<uint8_t> request_buffer;
    std::vector<uint8_t> response_buffer;
    std::string client_id;
    uint64_t created_time = 0;
};

class GGUFProxyServer {
public:
    explicit GGUFProxyServer();
    ~GGUFProxyServer();

    /**
     * @brief Initialize the proxy server
     */
    void Initialize(int listen_port, const std::string& gguf_endpoint);

    /**
     * @brief Start listening for connections
     */
    bool StartServer();

    /**
     * @brief Stop the server and close all sockets
     */
    void StopServer();

    /**
     * @brief Check if server is running
     */
    bool IsListening() const { return listening_; }

    /**
     * @brief Get server statistics
     */
    std::string GetServerStatistics() const;

    /**
     * @brief Set connection pool size
     */
    void SetConnectionPoolSize(int size) { max_connections_ = size; }

    /**
     * @brief Set socket timeout in milliseconds
     */
    void SetConnectionTimeout(int ms) { socket_timeout_ms_ = ms; }

    /**
     * @brief Register callback for connection events
     */
    void SetOnConnectionCallback(std::function<void(const std::string&)> callback) {
        on_connection_cb_ = callback;
    }

private:
    void AcceptConnections();
    void HandleClientConnection(GGUFClientConnection* conn);
    void ForwardToGGUF(GGUFClientConnection* conn);
    void SendResponseToClient(GGUFClientConnection* conn);

    SOCKET listen_socket_ = INVALID_SOCKET;
    int listen_port_ = 0;
    std::string gguf_endpoint_;
    std::atomic<bool> listening_{false};
    std::atomic<bool> shutdown_{false};

    std::map<std::string, std::unique_ptr<GGUFClientConnection>> connections_;
    mutable std::mutex connections_mutex_;

    std::unique_ptr<std::thread> accept_thread_;
    int max_connections_ = 64;
    int socket_timeout_ms_ = 30000;

    uint64_t total_connections_ = 0;
    uint64_t total_requests_ = 0;
    uint64_t total_errors_ = 0;

    std::function<void(const std::string&)> on_connection_cb_;
};

} // namespace RawrXD::Agent
