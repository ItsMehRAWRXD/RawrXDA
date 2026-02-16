#include "llm_http_client.h"
#include <curl/curl.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>

#include "logging/logger.h"
static Logger s_logger("llm_http_client");

// ============================================================================
// CURL Helper Callbacks
// ============================================================================

namespace {
    struct CurlWriteContext {
        std::string data;
        std::function<void(const std::string&)> streamCallback;
        LLMHttpClient* client = nullptr;
        LLMBackend backend;
    };

    // Write callback for response body
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t realsize = size * nmemb;
        CurlWriteContext* mem = static_cast<CurlWriteContext*>(userp);
        
        try {
            mem->data.append(static_cast<char*>(contents), realsize);
        } catch (const std::bad_alloc&) {
            return 0;  // Signal error
        }
        
        return realsize;
    }

    // Progress callback for monitoring
    static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow) {
        // Could be used for progress tracking
        return 0;
    }

    // Header callback for capturing response headers
    static size_t HeaderCallback(char* buffer, size_t size, size_t nmemb, void* userp) {
        size_t realsize = size * nmemb;
        std::string* headers = static_cast<std::string*>(userp);
        headers->append(buffer, realsize);
        return realsize;
    }
}

// ============================================================================
// Constructor and Destructor
// ============================================================================

LLMHttpClient::LLMHttpClient()
    : m_backend(LLMBackend::OLLAMA),
      m_initTime(std::chrono::high_resolution_clock::now()),
      m_activeConnections(0)
{
    // Initialize curl globally (thread-safe after first call)
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LLMHttpClient::~LLMHttpClient() {
    // Cleanup curl
    curl_global_cleanup();
}

// ============================================================================
// Initialization
// ============================================================================

bool LLMHttpClient::initialize(
    LLMBackend backend,
    const HTTPConfig& config,
    const AuthCredentials& credentials)
{
    m_backend = backend;
    m_config = config;
    m_credentials = credentials;

    // Validate base URL
    if (!isValidURL(config.baseUrl)) {
        s_logger.error( "[LLMHttpClient] Invalid base URL: " << config.baseUrl << std::endl;
        return false;
    }

    // Initialize connection pool
    {
        std::lock_guard<std::mutex> lock(m_connectionPoolMutex);
        for (int i = 0; i < config.connectionPoolSize; ++i) {
            // Create WinHTTP session handles for the connection pool
#ifdef _WIN32
            HINTERNET hSession = WinHttpOpen(
                L"RawrXD-LLM-Adapter/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (hSession) {
                // Set timeouts on the session
                DWORD timeout = static_cast<DWORD>(config.timeoutMs);
                WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);
                m_connectionPool.push(reinterpret_cast<void*>(hSession));
            } else {
                m_connectionPool.push(nullptr);
            }
#else
            // POSIX: pool index marker for curl multi handle
            m_connectionPool.push(reinterpret_cast<void*>(static_cast<uintptr_t>(i + 1)));
#endif
        }
    }

    s_logger.info("[LLMHttpClient] Initialized for backend: ");

    return testConnectivity();
}

// ============================================================================
// Main HTTP Operations
// ============================================================================

APIResponse LLMHttpClient::makeRequest(const APIRequest& request) {
    return sendHTTPRequest(request, true);
}

APIResponse LLMHttpClient::makeStreamingRequest(
    const APIRequest& request,
    std::function<void(const StreamChunk&)> chunkCallback)
{
    if (!checkRateLimit()) {
        APIResponse resp;
        resp.statusCode = 429;
        resp.statusMessage = "Rate limit exceeded";
        resp.success = false;
        resp.error = "Too many requests";
        return resp;
    }

    int64_t startTime = getCurrentTimestampMs();

    CURL* curl = curl_easy_init();
    if (!curl) {
        APIResponse resp;
        resp.statusCode = 0;
        resp.success = false;
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    try {
        std::string fullUrl = m_config.baseUrl + request.endpoint;
        std::string jsonBody = request.body.dump();

        // Setup CURL options
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)m_config.timeoutMs);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());

        // Build headers
        auto headers = buildDefaultHeaders();
        auto authHeaders = buildAuthHeaders();
        headers.insert(authHeaders.begin(), authHeaders.end());

        struct curl_slist* headerList = nullptr;
        headerList = curl_slist_append(headerList, "Content-Type: application/json");
        for (const auto& [key, value] : headers) {
            headerList = curl_slist_append(headerList, (key + ": " + value).c_str());
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

        // Setup streaming
        CurlWriteContext writeContext;
        writeContext.client = this;
        writeContext.backend = m_backend;
        writeContext.streamCallback = chunkCallback;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeContext);

        // String for response headers
        std::string responseHeaders;
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);

        // SSL options
        if (!m_config.validateSSL) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        // User agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, m_config.userAgent.c_str());

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        // Get response code
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        int64_t endTime = getCurrentTimestampMs();
        int64_t latency = endTime - startTime;

        // Parse streaming response
        APIResponse response;
        response.statusCode = static_cast<int>(responseCode);
        response.responseTimeMs = latency;
        response.receivedAt = endTime;

        if (res == CURLE_OK && responseCode >= 200 && responseCode < 300) {
            response.success = true;
            response.rawBody = writeContext.data;

            // Parse streaming chunks
            if (m_backend == LLMBackend::OLLAMA) {
                // Ollama sends JSON-per-line
                std::istringstream stream(writeContext.data);
                std::string line;
                while (std::getline(stream, line)) {
                    if (!line.empty()) {
                        try {
                            auto chunk = parseOllamaStreamChunk(line);
                            if (!chunk.content.empty() || chunk.isComplete) {
                                chunkCallback(chunk);
                            }
                        } catch (const std::exception& e) {
                            s_logger.error( "[LLMHttpClient] Error parsing Ollama chunk: " << e.what() << std::endl;
                        }
                    }
                }
            } else if (m_backend == LLMBackend::OPENAI) {
                // OpenAI uses Server-Sent Events (SSE)
                auto lines = splitSSELines(writeContext.data);
                for (const auto& line : lines) {
                    if (!line.empty() && line.substr(0, 6) == "data: ") {
                        try {
                            auto chunk = parseOpenAIStreamChunk(line);
                            if (!chunk.content.empty() || chunk.isComplete) {
                                chunkCallback(chunk);
                            }
                        } catch (const std::exception& e) {
                            s_logger.error( "[LLMHttpClient] Error parsing OpenAI chunk: " << e.what() << std::endl;
                        }
                    }
                }
            } else if (m_backend == LLMBackend::ANTHROPIC) {
                // Anthropic also uses SSE
                auto lines = splitSSELines(writeContext.data);
                for (const auto& line : lines) {
                    if (!line.empty() && line.substr(0, 6) == "data: ") {
                        try {
                            auto chunk = parseAnthropicStreamChunk(line);
                            if (!chunk.content.empty() || chunk.isComplete) {
                                chunkCallback(chunk);
                            }
                        } catch (const std::exception& e) {
                            s_logger.error( "[LLMHttpClient] Error parsing Anthropic chunk: " << e.what() << std::endl;
                        }
                    }
                }
            }
        } else {
            response.success = false;
            response.error = curl_easy_strerror(res);
            response.statusMessage = formatErrorMessage(static_cast<int>(responseCode), writeContext.data);
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.totalRequests++;
            if (response.success) {
                m_stats.successfulRequests++;
            } else {
                m_stats.failedRequests++;
            }
            m_stats.totalLatencyMs += latency;
        }

        // Cleanup
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);

        return response;

    } catch (const std::exception& e) {
        curl_easy_cleanup(curl);
        APIResponse resp;
        resp.statusCode = 0;
        resp.success = false;
        resp.error = std::string("Exception: ") + e.what();
        return resp;
    }
}

// ============================================================================
// Request Building - Ollama
// ============================================================================

APIRequest LLMHttpClient::buildOllamaCompletionRequest(
    const std::string& prompt,
    const json& config)
{
    APIRequest req;
    req.backend = LLMBackend::OLLAMA;
    req.endpoint = "/api/generate";
    req.stream = true;
    req.method = "POST";

    req.body = json{
        {"model", config.value("model", "llama2")},
        {"prompt", prompt},
        {"stream", true}
    };

    // Add optional parameters
    if (config.contains("temperature")) {
        req.body["temperature"] = config["temperature"];
    }
    if (config.contains("top_p")) {
        req.body["top_p"] = config["top_p"];
    }
    if (config.contains("top_k")) {
        req.body["top_k"] = config["top_k"];
    }
    if (config.contains("num_predict")) {
        req.body["num_predict"] = config["num_predict"];
    }
    if (config.contains("repeat_penalty")) {
        req.body["repeat_penalty"] = config["repeat_penalty"];
    }

    return req;
}

APIRequest LLMHttpClient::buildOllamaChatRequest(
    const std::vector<json>& messages,
    const json& config)
{
    APIRequest req;
    req.backend = LLMBackend::OLLAMA;
    req.endpoint = "/api/chat";
    req.stream = true;
    req.method = "POST";

    req.body = json{
        {"model", config.value("model", "llama2")},
        {"messages", messages},
        {"stream", true}
    };

    // Add optional parameters
    if (config.contains("temperature")) {
        req.body["temperature"] = config["temperature"];
    }
    if (config.contains("top_p")) {
        req.body["top_p"] = config["top_p"];
    }
    if (config.contains("top_k")) {
        req.body["top_k"] = config["top_k"];
    }

    return req;
}

// ============================================================================
// Request Building - OpenAI
// ============================================================================

APIRequest LLMHttpClient::buildOpenAIChatRequest(
    const std::vector<json>& messages,
    const std::string& model,
    const json& config)
{
    APIRequest req;
    req.backend = LLMBackend::OPENAI;
    req.endpoint = "/v1/chat/completions";
    req.stream = config.value("stream", false);
    req.method = "POST";

    req.body = json{
        {"model", model},
        {"messages", messages},
        {"temperature", config.value("temperature", 0.7)},
        {"top_p", config.value("top_p", 0.9)},
        {"max_tokens", config.value("max_tokens", 2048)},
        {"stream", req.stream}
    };

    // Add optional tool calling
    if (config.contains("tools") && !config["tools"].empty()) {
        req.body["tools"] = config["tools"];
        req.body["tool_choice"] = config.value("tool_choice", "auto");
    }

    // Add stop sequences
    if (config.contains("stop")) {
        req.body["stop"] = config["stop"];
    }

    return req;
}

// ============================================================================
// Request Building - Anthropic
// ============================================================================

APIRequest LLMHttpClient::buildAnthropicMessageRequest(
    const std::vector<json>& messages,
    const std::string& model,
    const json& config)
{
    APIRequest req;
    req.backend = LLMBackend::ANTHROPIC;
    req.endpoint = "/messages";
    req.stream = config.value("stream", false);
    req.method = "POST";

    // Anthropic requires system message separately
    std::string systemMessage = config.value("system", "You are a helpful assistant.");

    req.body = json{
        {"model", model},
        {"system", systemMessage},
        {"messages", messages},
        {"max_tokens", config.value("max_tokens", 2048)},
        {"temperature", config.value("temperature", 0.7)},
        {"stream", req.stream}
    };

    // Add optional tool definitions
    if (config.contains("tools") && !config["tools"].empty()) {
        req.body["tools"] = config["tools"];
    }

    return req;
}

// ============================================================================
// Stream Parsing - Ollama
// ============================================================================

StreamChunk LLMHttpClient::parseOllamaStreamChunk(const std::string& chunk) {
    StreamChunk parsed;

    try {
        auto jsonChunk = json::parse(chunk);

        // Extract content
        if (jsonChunk.contains("response")) {
            parsed.content = jsonChunk["response"].get<std::string>();
        }

        // Check if generation is complete
        if (jsonChunk.contains("done")) {
            parsed.isComplete = jsonChunk["done"].get<bool>();
        }

        // Extract token counts if available
        if (jsonChunk.contains("eval_count")) {
            parsed.tokenCount = jsonChunk["eval_count"].get<int>();
        }

        // Store metadata
        parsed.metadata = jsonChunk;

    } catch (const std::exception& e) {
        s_logger.error( "[LLMHttpClient] Failed to parse Ollama chunk: " << e.what() << std::endl;
    }

    return parsed;
}

// ============================================================================
// Stream Parsing - OpenAI
// ============================================================================

StreamChunk LLMHttpClient::parseOpenAIStreamChunk(const std::string& line) {
    StreamChunk parsed;

    try {
        // SSE format: "data: {json}"
        if (line.length() > 6 && line.substr(0, 6) == "data: ") {
            std::string jsonStr = line.substr(6);

            // Check for stream end
            if (jsonStr == "[DONE]") {
                parsed.isComplete = true;
                return parsed;
            }

            auto jsonChunk = json::parse(jsonStr);

            // OpenAI format: choices[0].delta.content
            if (jsonChunk.contains("choices") && !jsonChunk["choices"].empty()) {
                auto choice = jsonChunk["choices"][0];
                if (choice.contains("delta") && choice["delta"].contains("content")) {
                    parsed.content = choice["delta"]["content"].get<std::string>();
                }

                // Check for tool calls
                if (choice["delta"].contains("tool_calls")) {
                    parsed.toolCall = choice["delta"]["tool_calls"].dump();
                }
            }

            // Check for finish reason
            if (jsonChunk["choices"][0].contains("finish_reason") &&
                jsonChunk["choices"][0]["finish_reason"].is_string()) {
                std::string finishReason = jsonChunk["choices"][0]["finish_reason"];
                if (finishReason != "null" && finishReason != "none") {
                    parsed.isComplete = true;
                }
            }

            parsed.metadata = jsonChunk;
        }

    } catch (const std::exception& e) {
        s_logger.error( "[LLMHttpClient] Failed to parse OpenAI chunk: " << e.what() << std::endl;
    }

    return parsed;
}

// ============================================================================
// Stream Parsing - Anthropic
// ============================================================================

StreamChunk LLMHttpClient::parseAnthropicStreamChunk(const std::string& line) {
    StreamChunk parsed;

    try {
        if (line.length() > 6 && line.substr(0, 6) == "data: ") {
            std::string jsonStr = line.substr(6);
            auto jsonChunk = json::parse(jsonStr);

            // Anthropic uses event-based streaming
            if (jsonChunk.contains("type")) {
                std::string eventType = jsonChunk["type"].get<std::string>();

                if (eventType == "content_block_delta" && jsonChunk.contains("delta")) {
                    auto delta = jsonChunk["delta"];
                    if (delta.contains("type") && delta["type"] == "text_delta") {
                        parsed.content = delta["text"].get<std::string>();
                    }
                } else if (eventType == "message_stop") {
                    parsed.isComplete = true;
                } else if (eventType == "message_start" && jsonChunk.contains("message")) {
                    // Message metadata
                    auto msg = jsonChunk["message"];
                    if (msg.contains("usage")) {
                        parsed.tokenCount = msg["usage"].value("input_tokens", 0);
                    }
                }
            }

            parsed.metadata = jsonChunk;
        }

    } catch (const std::exception& e) {
        s_logger.error( "[LLMHttpClient] Failed to parse Anthropic chunk: " << e.what() << std::endl;
    }

    return parsed;
}

// ============================================================================
// Connectivity Testing
// ============================================================================

bool LLMHttpClient::testConnectivity() {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    try {
        std::string testUrl = m_config.baseUrl;

        // Add appropriate health check endpoint
        if (m_backend == LLMBackend::OLLAMA) {
            testUrl += "/api/tags";
        } else if (m_backend == LLMBackend::OPENAI) {
            testUrl += "/v1/models";
        } else if (m_backend == LLMBackend::ANTHROPIC) {
            testUrl += "/models";
        }

        curl_easy_setopt(curl, CURLOPT_URL, testUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

        // Add auth headers
        struct curl_slist* headerList = nullptr;
        auto authHeaders = buildAuthHeaders();
        for (const auto& [key, value] : authHeaders) {
            headerList = curl_slist_append(headerList, (key + ": " + value).c_str());
        }
        if (headerList) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        }

        if (!m_config.validateSSL) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        }

        CURLcode res = curl_easy_perform(curl);
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (headerList) curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);

        bool connected = (res == CURLE_OK && responseCode >= 200 && responseCode < 300);
        s_logger.info("[LLMHttpClient] Connectivity test: ");
        return connected;

    } catch (const std::exception& e) {
        curl_easy_cleanup(curl);
        s_logger.error( "[LLMHttpClient] Connectivity test exception: " << e.what() << std::endl;
        return false;
    }
}

json LLMHttpClient::listAvailableModels() {
    APIRequest req;
    req.backend = m_backend;
    req.method = "GET";

    if (m_backend == LLMBackend::OLLAMA) {
        req.endpoint = "/api/tags";
    } else if (m_backend == LLMBackend::OPENAI) {
        req.endpoint = "/v1/models";
    } else if (m_backend == LLMBackend::ANTHROPIC) {
        req.endpoint = "/models";
    }

    auto response = makeRequest(req);
    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        return response.body;
    }

    return json::array();
}

// ============================================================================
// Authentication
// ============================================================================

void LLMHttpClient::setCredentials(const AuthCredentials& credentials) {
    m_credentials = credentials;
}

std::map<std::string, std::string> LLMHttpClient::buildAuthHeaders() {
    std::map<std::string, std::string> headers;

    switch (m_credentials.type) {
        case AuthType::BEARER_TOKEN:
            headers["Authorization"] = "Bearer " + m_credentials.token;
            break;

        case AuthType::API_KEY:
            if (m_backend == LLMBackend::OPENAI || m_backend == LLMBackend::AZURE_OPENAI) {
                headers["Authorization"] = "Bearer " + m_credentials.apiKey;
            } else if (m_backend == LLMBackend::ANTHROPIC) {
                headers["x-api-key"] = m_credentials.apiKey;
            } else {
                headers["X-API-Key"] = m_credentials.apiKey;
            }
            break;

        case AuthType::BASIC_AUTH: {
            std::string auth = m_credentials.username + ":" + m_credentials.password;
            // Base64 encoding would go here in production
            headers["Authorization"] = "Basic " + auth;
            break;
        }

        case AuthType::OAUTH2:
            if (!isTokenExpired()) {
                headers["Authorization"] = "Bearer " + m_credentials.token;
            } else {
                if (refreshOAuth2Token()) {
                    headers["Authorization"] = "Bearer " + m_credentials.token;
                }
            }
            break;

        case AuthType::NONE:
        default:
            // No auth needed
            break;
    }

    return headers;
}

// ============================================================================
// Rate Limiting
// ============================================================================

void LLMHttpClient::setRateLimit(double requestsPerSecond) {
    std::lock_guard<std::mutex> lock(m_rateLimitMutex);
    m_requestsPerSecond = requestsPerSecond;
    m_tokenBucketTokens = requestsPerSecond;  // Start with full bucket
}

bool LLMHttpClient::checkRateLimit() {
    std::lock_guard<std::mutex> lock(m_rateLimitMutex);

    int64_t now = getCurrentTimestampMs();
    if (m_lastRequestTime == 0) {
        m_lastRequestTime = now;
        m_tokenBucketTokens = m_requestsPerSecond;
    }

    // Token bucket algorithm
    double timeSinceLastMs = now - m_lastRequestTime;
    double tokensToAdd = (timeSinceLastMs / 1000.0) * m_requestsPerSecond;
    m_tokenBucketTokens = std::min(m_tokenBucketTokens + tokensToAdd, m_requestsPerSecond);

    m_lastRequestTime = now;

    if (m_tokenBucketTokens >= 1.0) {
        m_tokenBucketTokens -= 1.0;
        return true;
    }

    return false;
}

// ============================================================================
// Internal HTTP Operations
// ============================================================================

APIResponse LLMHttpClient::sendHTTPRequest(const APIRequest& request, bool retry) {
    if (!checkRateLimit()) {
        APIResponse resp;
        resp.statusCode = 429;
        resp.statusMessage = "Rate limit exceeded";
        resp.success = false;
        resp.error = "Too many requests";
        return resp;
    }

    int retryCount = 0;
    while (retryCount <= m_config.maxRetries) {
        int64_t startTime = getCurrentTimestampMs();

        CURL* curl = curl_easy_init();
        if (!curl) {
            APIResponse resp;
            resp.statusCode = 0;
            resp.success = false;
            resp.error = "Failed to initialize CURL";
            return resp;
        }

        try {
            std::string fullUrl = m_config.baseUrl + request.endpoint;
            std::string jsonBody = request.body.dump();

            // Setup CURL
            curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)m_config.timeoutMs);

            // Set HTTP method
            if (request.method == "GET") {
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            } else if (request.method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
            } else if (request.method == "PUT") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
            } else if (request.method == "DELETE") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            }

            // Setup headers
            struct curl_slist* headerList = nullptr;
            headerList = curl_slist_append(headerList, "Content-Type: application/json");

            auto defaultHeaders = buildDefaultHeaders();
            auto authHeaders = buildAuthHeaders();

            for (const auto& [key, value] : defaultHeaders) {
                headerList = curl_slist_append(headerList, (key + ": " + value).c_str());
            }
            for (const auto& [key, value] : authHeaders) {
                headerList = curl_slist_append(headerList, (key + ": " + value).c_str());
            }

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

            // Response handling
            CurlWriteContext writeContext;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeContext);

            // SSL options
            if (!m_config.validateSSL) {
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            }

            curl_easy_setopt(curl, CURLOPT_USERAGENT, m_config.userAgent.c_str());

            // Perform request
            CURLcode res = curl_easy_perform(curl);

            // Get response code
            long responseCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

            int64_t endTime = getCurrentTimestampMs();
            int64_t latency = endTime - startTime;

            // Parse response
            APIResponse response = parseHTTPResponse(
                static_cast<int>(responseCode),
                writeContext.data,
                latency
            );
            response.retryCount = retryCount;

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.totalRequests++;
                if (response.success) {
                    m_stats.successfulRequests++;
                } else {
                    m_stats.failedRequests++;
                }
                m_stats.totalLatencyMs += latency;
                m_stats.totalRetries += retryCount;
            }

            // Cleanup
            curl_slist_free_all(headerList);
            curl_easy_cleanup(curl);

            // Check if we should retry
            if (!response.success && retry && shouldRetry(static_cast<int>(responseCode), retryCount)) {
                int delayMs = calculateBackoffDelay(retryCount);
                s_logger.info("[LLMHttpClient] Retry ");
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                retryCount++;
                continue;
            }

            return response;

        } catch (const std::exception& e) {
            curl_easy_cleanup(curl);
            APIResponse resp;
            resp.statusCode = 0;
            resp.success = false;
            resp.error = std::string("Exception: ") + e.what();
            resp.retryCount = retryCount;
            return resp;
        }
    }

    // All retries exhausted
    APIResponse resp;
    resp.statusCode = 0;
    resp.success = false;
    resp.error = "All retries exhausted";
    resp.retryCount = retryCount;
    return resp;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::map<std::string, std::string> LLMHttpClient::buildDefaultHeaders() {
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    headers["User-Agent"] = m_config.userAgent;

    if (m_config.enableCompression) {
        headers["Accept-Encoding"] = "gzip, deflate";
    }

    return headers;
}

APIResponse LLMHttpClient::parseHTTPResponse(
    int statusCode,
    const std::string& body,
    int64_t latencyMs)
{
    APIResponse response;
    response.statusCode = statusCode;
    response.responseTimeMs = latencyMs;
    response.receivedAt = getCurrentTimestampMs();

    if (statusCode >= 200 && statusCode < 300) {
        response.success = true;
        response.statusMessage = "OK";

        try {
            response.body = json::parse(body);
        } catch (const std::exception& e) {
            response.rawBody = body;
        }
    } else {
        response.success = false;
        response.rawBody = body;

        switch (statusCode) {
            case 400:
                response.statusMessage = "Bad Request";
                response.error = "Invalid request format or parameters";
                break;
            case 401:
                response.statusMessage = "Unauthorized";
                response.error = "Authentication failed. Check API key.";
                break;
            case 403:
                response.statusMessage = "Forbidden";
                response.error = "Access denied";
                break;
            case 404:
                response.statusMessage = "Not Found";
                response.error = "Endpoint not found";
                break;
            case 429:
                response.statusMessage = "Too Many Requests";
                response.error = "Rate limit exceeded";
                break;
            case 500:
                response.statusMessage = "Internal Server Error";
                response.error = "Server error on remote endpoint";
                break;
            case 503:
                response.statusMessage = "Service Unavailable";
                response.error = "Service temporarily unavailable";
                break;
            default:
                response.statusMessage = "HTTP " + std::to_string(statusCode);
                response.error = "Unexpected HTTP status code";
                break;
        }

        // Try to parse error details from response
        try {
            auto errorJson = json::parse(body);
            if (errorJson.contains("error")) {
                response.error = errorJson["error"].dump();
            }
        } catch (...) {
            // Body might not be JSON, that's OK
        }
    }

    return response;
}

bool LLMHttpClient::shouldRetry(int statusCode, int retryCount) {
    if (retryCount >= m_config.maxRetries) {
        return false;
    }

    // Retry on server errors and timeout
    return (statusCode >= 500 || statusCode == 429 || statusCode == 408);
}

int LLMHttpClient::calculateBackoffDelay(int retryCount) {
    // Exponential backoff: base * (2 ^ retryCount) + jitter
    int exponentialDelay = m_config.retryDelayMs * (1 << retryCount);

    // Add random jitter (0-50% of delay)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, exponentialDelay / 2);
    int jitter = dis(gen);

    return exponentialDelay + jitter;
}

bool LLMHttpClient::isTokenExpired() const {
    return (m_credentials.tokenExpiresAt > 0 &&
            m_credentials.tokenExpiresAt < getCurrentTimestampMs());
}

bool LLMHttpClient::refreshOAuth2Token() {
    // OAuth2 token refresh via HTTP POST to the token endpoint
    std::string tokenUrl = m_config.baseUrl;
    // Derive token endpoint from base URL (strip /v1 or /api, append /oauth/token)
    size_t apiPos = tokenUrl.rfind("/v1");
    if (apiPos != std::string::npos) tokenUrl = tokenUrl.substr(0, apiPos);
    tokenUrl += "/oauth/token";

    std::string postBody = "grant_type=refresh_token"
                           "&refresh_token=" + m_credentials.refreshToken +
                           "&client_id=" + m_credentials.clientId;
    if (!m_credentials.clientSecret.empty()) {
        postBody += "&client_secret=" + m_credentials.clientSecret;
    }

#ifdef _WIN32
    // Use WinHTTP for the refresh request
    URL_COMPONENTSW urlComp = {};
    wchar_t hostBuf[256] = {};
    wchar_t pathBuf[1024] = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = hostBuf;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = pathBuf;
    urlComp.dwUrlPathLength = 1024;

    std::wstring wUrl(tokenUrl.begin(), tokenUrl.end());
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        s_logger.error( "[LLMHttpClient] Failed to parse OAuth2 token URL" << std::endl;
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-OAuth2/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    bool useSSL = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort ? urlComp.nPort : (useSSL ? 443 : 80);
    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = useSSL ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlComp.lpszUrlPath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), -1,
        (LPVOID)postBody.c_str(), (DWORD)postBody.size(), (DWORD)postBody.size(), 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        s_logger.error( "[LLMHttpClient] OAuth2 refresh HTTP request failed" << std::endl;
        return false;
    }

    // Read response
    std::string response;
    char buf[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        response.append(buf, bytesRead);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse JSON response for access_token and expires_in
    auto findJsonString = [&response](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\"";
        size_t pos = response.find(needle);
        if (pos == std::string::npos) return "";
        pos = response.find('"', pos + needle.size() + 1);
        if (pos == std::string::npos) return "";
        size_t end = response.find('"', pos + 1);
        return (end != std::string::npos) ? response.substr(pos + 1, end - pos - 1) : "";
    };

    std::string newToken = findJsonString("access_token");
    if (newToken.empty()) {
        s_logger.error( "[LLMHttpClient] OAuth2 response missing access_token" << std::endl;
        return false;
    }

    m_credentials.apiKey = newToken;
    // Update expiry
    std::string expiresStr = findJsonString("expires_in");
    if (!expiresStr.empty()) {
        m_credentials.tokenExpiresAt = getCurrentTimestampMs() + std::stoll(expiresStr) * 1000;
    }
    // Rotate refresh token if provided
    std::string newRefresh = findJsonString("refresh_token");
    if (!newRefresh.empty()) {
        m_credentials.refreshToken = newRefresh;
    }

    s_logger.info("[LLMHttpClient] OAuth2 token refreshed successfully");
    return true;
#else
    // POSIX fallback: use system curl
    std::string cmd = "curl -s -X POST '" + tokenUrl + "' -d '" + postBody + "'";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    std::string response;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) response += buf;
    pclose(pipe);
    // Basic JSON parse for access_token
    size_t tokPos = response.find("\"access_token\"");
    if (tokPos == std::string::npos) return false;
    size_t valStart = response.find('"', tokPos + 15) + 1;
    size_t valEnd = response.find('"', valStart);
    if (valStart > 0 && valEnd > valStart) {
        m_credentials.apiKey = response.substr(valStart, valEnd - valStart);
        m_credentials.tokenExpiresAt = getCurrentTimestampMs() + 3600 * 1000; // default 1hr
        return true;
    }
    return false;
#endif
}

std::vector<std::string> LLMHttpClient::splitSSELines(const std::string& data) {
    std::vector<std::string> lines;
    std::istringstream stream(data);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

std::string LLMHttpClient::extractJSONFromSSE(const std::string& line) {
    if (line.length() > 6 && line.substr(0, 6) == "data: ") {
        return line.substr(6);
    }
    return "";
}

std::string LLMHttpClient::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 16; ++i) {
        ss << dis(gen);
    }
    return ss.str();
}

int64_t LLMHttpClient::getCurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string LLMHttpClient::urlEncode(const std::string& str) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(c);
        }
    }
    return oss.str();
}

bool LLMHttpClient::isValidURL(const std::string& url) {
    return url.find("http://") == 0 || url.find("https://") == 0;
}

std::string LLMHttpClient::formatErrorMessage(int statusCode, const std::string& body) {
    std::string message = "HTTP " + std::to_string(statusCode);

    try {
        auto json_body = json::parse(body);
        if (json_body.contains("error")) {
            message += ": " + json_body["error"].dump();
        } else if (json_body.contains("message")) {
            message += ": " + json_body["message"].dump();
        }
    } catch (...) {
        if (!body.empty() && body.length() < 200) {
            message += ": " + body;
        }
    }

    return message;
}
