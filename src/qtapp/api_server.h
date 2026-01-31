#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>

#include <functional>
#include <unordered_map>

// Forward declarations
struct AppState;

// Simple JSON-like structure for request parsing
struct JsonValue {
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::unordered_map<std::string, JsonValue> object_value;
    bool is_string = false;
    bool is_array = false;
    bool is_object = false;
};

// Chat message structure for OpenAI compatibility
struct ChatMessage {
    std::string role;
    std::string content;
    std::string name;
};

// HTTP request structure
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::chrono::steady_clock::time_point received_time;
    std::string client_id;
};

// HTTP response structure 
struct HttpResponse {
    int status_code;
    std::string body;
    std::string content_type;
};

class APIServer {
public:
    explicit APIServer(AppState& app_state);
    ~APIServer();

    bool Start(uint16_t port);
    bool Stop();
    bool IsRunning() const { return is_running_.load(); }
    uint16_t GetPort() const { return port_; }

    // Request handlers
    void HandleGenerateRequest(const std::string& request, std::string& response);
    void HandleChatCompletionsRequest(const std::string& request, std::string& response);
    void HandleTagsRequest(std::string& response);
    void HandlePullRequest(const std::string& request, std::string& response);

    // Inference utilities
    std::string GenerateCompletion(const std::string& prompt);
    std::string GenerateChatCompletion(const std::vector<ChatMessage>& messages);

private:
    // Production HTTP server utilities
    void InitializeHttpServer();
    void ProcessPendingRequests();
    void HandleClientConnections();
    
    // JSON parsing utilities
    JsonValue ParseJsonRequest(const std::string& request);
    std::string ExtractPromptFromRequest(const JsonValue& request);
    std::vector<ChatMessage> ExtractMessagesFromRequest(const JsonValue& request);
    bool ValidateMessageFormat(const std::vector<ChatMessage>& messages);
    
    // Response generation utilities
    std::string CreateErrorResponse(const std::string& error_message);
    std::string CreateGenerateResponse(const std::string& completion);
    std::string CreateChatCompletionResponse(const std::string& completion, const JsonValue& request);
    
    // Logging utilities
    void LogRequestReceived(const std::string& endpoint, size_t content_length);
    void LogRequestCompleted(const std::string& endpoint, size_t response_length);
    void LogRequestError(const std::string& endpoint, const std::string& error);
    void LogServerError(const std::string& error);
    
    // Performance monitoring utilities
    void RecordRequestMetrics(const std::string& endpoint, 
                            std::chrono::milliseconds duration,
                            bool success);
    void UpdateConnectionMetrics(int active_connections);
    
    // Security utilities
    bool ValidateRequest(const HttpRequest& request);
    bool CheckRateLimit(const std::string& client_id);
    void UpdateRateLimit(const std::string& client_id);

private:
    AppState& app_state_;
    std::atomic<bool> is_running_;
    uint16_t port_;
    std::unique_ptr<std::thread> server_thread_;
    
    // Connection management
    std::queue<HttpRequest> pending_requests_;
    std::mutex request_queue_mutex_;
    std::atomic<int> active_connections_{0};
    
    // Rate limiting
    std::mutex rate_limit_mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_request_times_;
    std::unordered_map<std::string, int> request_counts_;
    
    // Performance metrics
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};
    std::chrono::steady_clock::time_point start_time_;
};