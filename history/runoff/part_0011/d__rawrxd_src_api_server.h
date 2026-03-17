#pragma once
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <map>
#include <chrono>
#include "gui.h"

// Forward declarations
namespace RawrXD { class CPUInferenceEngine; }

struct JsonValue {
    bool is_object = false;
    bool is_string = false;
    std::string string_value;
    std::map<std::string, JsonValue> object_value;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string body;
    std::map<std::string, std::string> headers;
};

class APIServer {
public:
    explicit APIServer(AppState& state);
    ~APIServer();

    bool Start(uint16_t port);
    bool Stop();

private:
    void InitializeHttpServer();
    void HandleClientConnections();
    void ProcessPendingRequests();
    
    // Request Handling
    void HandleGenerateRequest(const std::string& request, std::string& response);
    void HandleChatCompletion(const std::string& request, std::string& response);
    
    // Core Logic
    std::string GenerateCompletion(const std::string& prompt);

    // Helpers
    JsonValue ParseJsonRequest(const std::string& request);
    std::string ExtractPromptFromRequest(const JsonValue& request);
    std::vector<ChatMessage> ExtractMessagesFromRequest(const JsonValue& request);
    bool ValidateMessageFormat(const std::vector<ChatMessage>& messages);
    std::string CreateErrorResponse(const std::string& message);
    std::string CreateGenerateResponse(const std::string& completion);
    std::string CreateChatCompletionResponse(const std::string& completion, const JsonValue& request);
    
    // Security
    bool ValidateRequest(const HttpRequest& request);
    bool CheckRateLimit(const std::string& client_id);
    void UpdateRateLimit(const std::string& client_id);
    void LogRequestMetrics(const std::string& endpoint, std::chrono::milliseconds duration, bool success);
    void UpdateConnectionMetrics(int active_connections);

    AppState& app_state_;
    std::atomic<bool> is_running_;
    uint16_t port_;
    std::unique_ptr<std::thread> server_thread_;
    
    // Network resources
    unsigned long long listen_socket_ = ~0ULL; 
    
    // Stats
    std::atomic<long> total_requests_{0};
    std::atomic<long> successful_requests_{0};
    std::atomic<long> failed_requests_{0};
    std::atomic<long> active_connections_{0};
    
    // Rate Limiting
    std::mutex rate_limit_mutex_;
    std::map<std::string, std::chrono::steady_clock::time_point> last_request_times_;
    std::map<std::string, int> request_counts_;
};

