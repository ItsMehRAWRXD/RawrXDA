#pragma once


#include <memory>

class InferenceEngine;

/**
 * @brief HTTP server for GGUF model inference with Ollama-compatible API
 * 
 * Features:
 * - Auto-starts if not already running
 * - Port conflict detection and auto-recovery
 * - Ollama-compatible endpoints (/api/generate, /api/tags, etc.)
 * - OpenAI-compatible endpoints (/v1/chat/completions)
 * - Health monitoring and graceful shutdown
 * - Streaming response support
 */
class GGUFServer : public void {

public:
    explicit GGUFServer(InferenceEngine* engine, void* parent = nullptr);
    ~GGUFServer();

    /**
     * @brief Start the server (auto-starts if not already running)
     * @param port Port to listen on (default: 11434 for Ollama compatibility)
     * @return true if server started or already running
     */
    bool start(quint16 port = 11434);

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const;

    /**
     * @brief Get current server port
     */
    quint16 port() const;

    /**
     * @brief Check if a server is already running on the specified port
     * @param port Port to check
     * @return true if server detected on port
     */
    static bool isServerRunningOnPort(quint16 port);

    /**
     * @brief Get server statistics
     */
    struct ServerStats {
        quint64 totalRequests = 0;
        quint64 successfulRequests = 0;
        quint64 failedRequests = 0;
        quint64 totalTokensGenerated = 0;
        qint64 uptimeSeconds = 0;
        std::string startTime;
    };
    ServerStats getStats() const;

    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const std::string& endpoint, const std::string& method);
    void requestCompleted(const std::string& endpoint, bool success, qint64 durationMs);
    void error(const std::string& errorMessage);

private:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onHealthCheck();

private:
    // HTTP request parsing
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string httpVersion;
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        std::unordered_map<std::string, std::string> queryParams;
    };

    struct HttpResponse {
        int statusCode = 200;
        std::string statusText = "OK";
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
    };

    // Request handlers
    HttpRequest parseHttpRequest(const std::vector<uint8_t>& rawData);
    void handleRequest(void** socket, const HttpRequest& request);
    void sendResponse(void** socket, const HttpResponse& response);

    // API endpoint handlers
    void handleGenerateRequest(const HttpRequest& request, HttpResponse& response);
    void handleChatCompletionsRequest(const HttpRequest& request, HttpResponse& response);
    void handleTagsRequest(HttpResponse& response);
    void handlePullRequest(const HttpRequest& request, HttpResponse& response);
    void handlePushRequest(const HttpRequest& request, HttpResponse& response);
    void handleShowRequest(const HttpRequest& request, HttpResponse& response);
    void handleDeleteRequest(const HttpRequest& request, HttpResponse& response);
    void handleHealthRequest(HttpResponse& response);
    void handleNotFound(HttpResponse& response);
    void handleCorsPreflightRequest(HttpResponse& response);

    // Auto-start functionality
    bool tryBindPort(quint16 port);
    bool waitForServerShutdown(quint16 port, int maxWaitMs = 5000);
    
    // Utilities
    std::string getCurrentTimestamp() const;
    void* parseJsonBody(const std::vector<uint8_t>& body);
    void logRequest(const std::string& method, const std::string& path, int statusCode);
    
    // BOTTLENECK #3 FIX: Lightweight JSON field extraction (streaming-style, no DOM)
    std::string extractJsonField(const std::vector<uint8_t>& json, const std::string& fieldName);
    void* extractJsonArray(const std::vector<uint8_t>& json, const std::string& fieldName);

private:
    InferenceEngine* m_engine;          ///< Inference engine for model operations
    void** m_server;               ///< TCP server instance
    std::unordered_map<void**, std::vector<uint8_t>> m_pendingRequests; ///< Buffer for incomplete requests
    std::mutex m_mutex;                     ///< Thread safety
    
    // Server state
    bool m_isRunning;
    quint16 m_port;
    std::chrono::system_clock::time_point m_startTime;
    
    // Statistics
    ServerStats m_stats;
    
    // Health monitoring
    void** m_healthTimer;
    
    // Configuration
    static constexpr int MAX_REQUEST_SIZE = 100 * 1024 * 1024; // 100MB max request
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 30000;     // 30 seconds
    static constexpr int DEFAULT_TIMEOUT_MS = 120000;          // 2 minutes
};

