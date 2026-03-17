#pragma once
#include <windows.h>
#include <wininet.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>
#include <expected>
#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include "utils/Expected.h"

using json = nlohmann::json;

namespace RawrXD::Net {

enum class NetError {
    Success = 0,
    InitializationFailed,
    ConnectionFailed,
    Timeout,
    SSLFailed,
    AuthenticationFailed,
    BufferOverflow,
    InvalidResponse,
    ResourceExhausted,
    Cancelled,
    WriteFailed,
    ReadFailed
};

struct NetworkConfig {
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{30000};
    std::chrono::milliseconds writeTimeout{30000};
    bool enableSSL = true;
    bool verifySSL = true;
    std::string userAgent = "RawrXD/3.0";
    size_t maxConnections = 10;
    size_t maxRetries = 3;
    bool enableCompression = true;
    std::optional<std::string> apiKey;
    std::optional<std::string> proxy;
};

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::chrono::milliseconds duration;
    size_t bytesRead = 0;
};

struct WebSocketFrame {
    enum class Opcode : uint8_t {
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA
    };
    
    Opcode opcode;
    bool isFinal;
    std::vector<uint8_t> payload;
};

class ConnectionPool {
public:
    explicit ConnectionPool(const NetworkConfig& config);
    ~ConnectionPool();
    
    // Non-copyable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    // Real connection management
    RawrXD::Expected<void, NetError> initialize();
    void shutdown();
    
    RawrXD::Expected<void*, NetError> acquireConnection(
        const std::string& host,
        int port,
        bool useSSL
    );
    
    void releaseConnection(void* handle);
    void invalidateConnection(void* handle);
    
    // Status
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;
    json getStatus() const;
    
private:
    NetworkConfig m_config;
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    
    struct ConnectionInfo {
        void* handle;
        std::string host;
        int port;
        bool useSSL;
        std::chrono::steady_clock::time_point lastUsed;
        std::atomic<bool> isValid{true};
        std::atomic<bool> isInUse{false};
    };
    
    std::vector<std::unique_ptr<ConnectionInfo>> m_connections;
    std::unordered_map<std::string, size_t> m_connectionCounts;
    
    // Real implementation
    RawrXD::Expected<void*, NetError> createConnection(
        const std::string& host,
        int port,
        bool useSSL
    );
    
    void cleanupInvalidConnections();
    std::string makeConnectionKey(
        const std::string& host,
        int port,
        bool useSSL
    );
};

class HttpClient {
public:
    explicit HttpClient(const NetworkConfig& config);
    ~HttpClient();
    
    // Non-copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    
    // Real HTTP operations
    std::expected<HttpResponse, NetError> get(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {}
    );
    
    std::expected<HttpResponse, NetError> post(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers = {}
    );
    
    std::expected<HttpResponse, NetError> postJson(
        const std::string& url,
        const nlohmann::json& json,
        const std::map<std::string, std::string>& headers = {}
    );
    
    // Real streaming
    std::expected<void, NetError> streamGet(
        const std::string& url,
        std::function<void(const std::vector<char>&)> callback,
        const std::map<std::string, std::string>& headers = {}
    );
    
    // Real authentication
    void setApiKey(const std::string& apiKey);
    void setBearerToken(const std::string& token);
    void setBasicAuth(const std::string& username, const std::string& password);
    
    // Status
    json getMetrics() const;
    void resetMetrics();
    
private:
    NetworkConfig m_config;
    std::unique_ptr<ConnectionPool> m_connectionPool;
    std::atomic<size_t> m_requestCount{0};
    std::atomic<size_t> m_bytesTransferred{0};
    std::atomic<size_t> m_errorCount{0};
    mutable std::mutex m_authMutex;
    
    std::string m_apiKey;
    std::string m_bearerToken;
    std::pair<std::string, std::string> m_basicAuth;
    
    // Real implementation
    std::expected<HttpResponse, NetError> executeRequest(
        const std::string& method,
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers
    );
    
    std::expected<void, NetError> addAuthHeaders(
        std::map<std::string, std::string>& headers
    );
    
    std::expected<void, NetError> handleCompression(
        HttpResponse& response
    );
    
    std::expected<void, NetError> handleRedirects(
        HttpResponse& response
    );
    
    // Retry logic
    std::expected<HttpResponse, NetError> executeWithRetry(
        std::function<RawrXD::Expected<HttpResponse, NetError>()> func,
        int retries
    );

    std::string base64Encode(const std::string& input);
};

class WebSocketClient {
public:
    WebSocketClient(const NetworkConfig& config);
    ~WebSocketClient();
    
    // Non-copyable
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    
    // Real WebSocket operations
    std::expected<void, NetError> connect(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {}
    );
    
    void disconnect();
    bool isConnected() const { return m_connected.load(); }
    
    // Send messages
    std::expected<void, NetError> sendText(const std::string& message);
    std::expected<void, NetError> sendBinary(const std::vector<uint8_t>& data);
    
    // Receive message
    std::expected<WebSocketFrame, NetError> receiveFrame(
        int timeoutMs = 5000
    );

    // State checking
    bool isConnected() const;
    
    // Ping/Pong
    std::expected<void, NetError> ping();
    std::expected<void, NetError> pong();
    
private:
    NetworkConfig m_config;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    void* m_socket = nullptr;
    std::thread m_receiveThread;
    std::function<void(const WebSocketFrame&)> m_messageHandler;
    mutable std::mutex m_handlerMutex;
    
    // Real implementation
    std::expected<void, NetError> performHandshake(
        HINTERNET hRequest
    );
    
    std::expected<void, NetError> sendFrame(
        WebSocketOpcode opcode,
        const uint8_t* data,
        size_t length
    );
    
    std::expected<WebSocketFrame, NetError> parseFrame(
        const std::vector<uint8_t>& buffer
    );
    
    void receiveLoop();
    
    // Frame construction
    std::vector<uint8_t> constructFrame(
        WebSocketFrame::Opcode opcode,
        const std::vector<uint8_t>& payload,
        bool isFinal
    );

    std::string generateWebSocketKey();
};

// Global network manager
class NetworkManager {
public:
    static NetworkManager& instance();
    
    // Initialize network stack
    std::expected<void, NetError> initialize(const NetworkConfig& config);
    void shutdown();
    
    // Get clients
    HttpClient& getHttpClient();
    WebSocketClient& getWebSocketClient();
    ConnectionPool& getConnectionPool();
    
    // Status
    bool isInitialized() const { return m_initialized.load(); }
    json getNetworkStatus() const;
    
private:
    NetworkManager() = default;
    ~NetworkManager() = default;
    
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    std::atomic<bool> m_initialized{false};
    NetworkConfig m_config;
    std::unique_ptr<HttpClient> m_httpClient;
    std::unique_ptr<WebSocketClient> m_websocketClient;
    std::unique_ptr<ConnectionPool> m_connectionPool;
};

} // namespace RawrXD::Net
