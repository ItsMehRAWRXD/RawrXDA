#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <mutex>
#include <queue>
#include <chrono>

using json = nlohmann::json;

/**
 * LLMHttpClient: Production-ready HTTP client for LLM APIs
 * 
 * Features:
 * - Real API calls to Ollama, OpenAI, Anthropic, HuggingFace
 * - Proper authentication (API keys, Bearer tokens, Basic auth)
 * - Request/response validation and error handling
 * - Streaming support with callback-based parsing
 * - Exponential backoff retry logic with jitter
 * - Connection pooling and timeout management
 * - Structured logging and metrics collection
 * - Rate limiting with token bucket algorithm
 */

enum class LLMBackend {
    OLLAMA,
    OPENAI,
    ANTHROPIC,
    HUGGINGFACE,
    AZURE_OPENAI,
    GOOGLE_PALM,
    LOCAL_GGUF
};

enum class AuthType {
    NONE,
    BEARER_TOKEN,
    API_KEY,
    BASIC_AUTH,
    OAUTH2
};

struct AuthCredentials {
    AuthType type = AuthType::NONE;
    std::string apiKey;
    std::string token;
    std::string username;
    std::string password;
    std::string customHeader;      // For custom auth like "X-API-Key"
    std::string oauthTokenUrl;     // For OAuth2 refresh
    std::string clientId;
    std::string clientSecret;
    int64_t tokenExpiresAt = 0;    // Timestamp for token refresh
};

struct HTTPConfig {
    std::string baseUrl;
    int timeoutMs = 30000;
    int maxRetries = 3;
    int retryDelayMs = 500;
    bool enableCompression = true;
    int connectionPoolSize = 10;
    int maxConcurrentRequests = 5;
    bool validateSSL = true;
    std::string userAgent = "RawrXD-LLMClient/1.0";
};

struct APIRequest {
    LLMBackend backend;
    std::string endpoint;          // e.g., "/api/generate", "/v1/chat/completions"
    std::string method = "POST";
    json body;
    std::map<std::string, std::string> headers;
    bool stream = false;
    int64_t createdAt = 0;
    std::string requestId;
};

struct APIResponse {
    int statusCode = 0;
    std::string statusMessage;
    json body;
    std::string rawBody;
    std::map<std::string, std::string> headers;
    int64_t responseTimeMs = 0;
    bool success = false;
    std::string error;
    int retryCount = 0;
    int64_t receivedAt = 0;
};

struct StreamChunk {
    std::string content;
    int tokenCount = 0;
    bool isComplete = false;
    std::string toolCall;         // For function calling
    json metadata;                 // Backend-specific metadata
};

class LLMHttpClient {
public:
    // Constructor and setup
    LLMHttpClient();
    ~LLMHttpClient();

    /**
     * Initialize HTTP client with configuration
     * @param backend Target LLM backend
     * @param config HTTP configuration
     * @param credentials Authentication credentials
     * @return True if initialization successful
     */
    bool initialize(
        LLMBackend backend,
        const HTTPConfig& config,
        const AuthCredentials& credentials
    );

    /**
     * Make synchronous HTTP request to LLM API
     * @param request API request
     * @return API response with parsed JSON body
     */
    APIResponse makeRequest(const APIRequest& request);

    /**
     * Make streaming HTTP request with chunk callback
     * @param request API request (stream must be true)
     * @param chunkCallback Called for each parsed streaming chunk
     * @return API response with metadata
     */
    APIResponse makeStreamingRequest(
        const APIRequest& request,
        std::function<void(const StreamChunk&)> chunkCallback
    );

    /**
     * Build completion request for Ollama API
     * @param prompt Input prompt
     * @param config Generation parameters
     * @return Formatted API request
     */
    APIRequest buildOllamaCompletionRequest(
        const std::string& prompt,
        const json& config
    );

    /**
     * Build chat completion request for Ollama API
     * @param messages Chat messages [{"role": "user|assistant", "content": "..."}]
     * @param config Generation parameters
     * @return Formatted API request
     */
    APIRequest buildOllamaChatRequest(
        const std::vector<json>& messages,
        const json& config
    );

    /**
     * Build chat completion request for OpenAI API
     * @param messages Chat messages
     * @param model Model identifier
     * @param config Generation parameters
     * @return Formatted API request
     */
    APIRequest buildOpenAIChatRequest(
        const std::vector<json>& messages,
        const std::string& model,
        const json& config
    );

    /**
     * Build message request for Anthropic API
     * @param messages Chat messages
     * @param model Model identifier
     * @param config Generation parameters
     * @return Formatted API request
     */
    APIRequest buildAnthropicMessageRequest(
        const std::vector<json>& messages,
        const std::string& model,
        const json& config
    );

    /**
     * Parse streaming response from Ollama
     * @param chunk Raw chunk data
     * @return Parsed stream chunk
     */
    StreamChunk parseOllamaStreamChunk(const std::string& chunk);

    /**
     * Parse streaming response from OpenAI
     * @param chunk Raw chunk data (SSE format)
     * @return Parsed stream chunk
     */
    StreamChunk parseOpenAIStreamChunk(const std::string& chunk);

    /**
     * Parse streaming response from Anthropic
     * @param chunk Raw chunk data
     * @return Parsed stream chunk
     */
    StreamChunk parseAnthropicStreamChunk(const std::string& chunk);

    /**
     * Test connectivity to API endpoint
     * @return True if endpoint is reachable and responding
     */
    bool testConnectivity();

    /**
     * Get available models from backend
     * @return JSON array of available models
     */
    json listAvailableModels();

    /**
     * Get current backend
     */
    LLMBackend getBackend() const { return m_backend; }

    /**
     * Get current configuration
     */
    HTTPConfig getConfig() const { return m_config; }

    /**
     * Update authentication credentials
     */
    void setCredentials(const AuthCredentials& credentials);

    /**
     * Get request statistics
     */
    struct RequestStats {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        int totalRetries = 0;
        int64_t totalLatencyMs = 0;
        int64_t totalTokensProcessed = 0;
    };

    RequestStats getStats() const { return m_stats; }
    void resetStats() { m_stats = RequestStats(); }

    /**
     * Set rate limit (requests per second)
     */
    void setRateLimit(double requestsPerSecond);

    /**
     * Check if rate limit allows next request
     */
    bool checkRateLimit();

private:
    // Internal HTTP operations
    APIResponse sendHTTPRequest(const APIRequest& request, bool retry = true);
    APIResponse sendHTTPStreamingRequest(
        const APIRequest& request,
        std::function<void(const StreamChunk&)> chunkCallback
    );

    // Request building
    std::map<std::string, std::string> buildAuthHeaders();
    std::map<std::string, std::string> buildDefaultHeaders();
    json validateAndSanitizeRequest(const json& request);

    // Response parsing
    APIResponse parseHTTPResponse(
        int statusCode,
        const std::string& body,
        int64_t latencyMs
    );
    json parseJSONResponse(const std::string& body);

    // Error handling and retry
    bool shouldRetry(int statusCode, int retryCount);
    int calculateBackoffDelay(int retryCount);  // Exponential backoff with jitter
    std::string formatErrorMessage(int statusCode, const std::string& body);

    // OAuth2 token management
    bool refreshOAuth2Token();
    bool isTokenExpired() const;

    // Stream parsing helpers
    std::vector<std::string> splitSSELines(const std::string& data);
    std::string extractJSONFromSSE(const std::string& line);

    // Member variables
    LLMBackend m_backend = LLMBackend::OLLAMA;
    HTTPConfig m_config;
    AuthCredentials m_credentials;
    RequestStats m_stats;

    // Rate limiting
    std::mutex m_rateLimitMutex;
    double m_requestsPerSecond = 100.0;  // Default: 100 req/s
    int64_t m_lastRequestTime = 0;
    double m_tokenBucketTokens = 0.0;

    // Metrics tracking
    std::mutex m_statsMutex;
    std::chrono::high_resolution_clock::time_point m_initTime;

    // Connection pooling
    std::mutex m_connectionPoolMutex;
    std::queue<void*> m_connectionPool;  // Opaque connection handles
    int m_activeConnections = 0;

    // Logging
    std::shared_ptr<class Logger> m_logger;

    // Helper methods
    std::string generateRequestId();
    int64_t getCurrentTimestampMs();
    std::string urlEncode(const std::string& str);
    bool isValidURL(const std::string& url);
};

#endif // LLM_HTTP_CLIENT_H
