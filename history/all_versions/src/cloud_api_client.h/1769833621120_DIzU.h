// cloud_api_client.h - Universal Cloud API Client for Multiple Providers
#ifndef CLOUD_API_CLIENT_H
#define CLOUD_API_CLIENT_H


#include <memory>
#include <functional>
#include <map>

class UniversalModelRouter;
struct ModelConfig;

// Structured logging
struct ApiCallLog {
    std::string timestamp;
    std::string provider;
    std::string model;
    std::string endpoint;
    std::string request_body;
    std::string response_body;
    int status_code;
    int64_t latency_ms;
    bool success;
    std::string error_message;
};

// API response structure
struct ApiResponse {
    bool success;
    std::string content;
    int status_code;
    std::string raw_body;
    int64_t latency_ms;
    std::string error_message;
    void* metadata;
};

// Streaming response structure
struct StreamingResponse {
    std::string chunk;
    bool is_final;
    std::string error;
};

class UniversalModelRouter;

// Cloud API Client
class CloudApiClient {

public:
    explicit CloudApiClient(UniversalModelRouter* parent = nullptr);
    ~CloudApiClient();

    // Synchronous generation (blocking)
    std::string generate(const std::string& prompt, const ModelConfig& config);
    
    // Asynchronous generation
    void generateAsync(const std::string& prompt, 
                      const ModelConfig& config,
                      std::function<void(const ApiResponse&)> callback);
    
    // Streaming generation
    void generateStream(const std::string& prompt,
                       const ModelConfig& config,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void(const std::string&)> error_callback = nullptr);
    
    // Provider health check
    bool checkProviderHealth(const ModelConfig& config);
    void checkProviderHealthAsync(const ModelConfig& config,
                                 std::function<void(bool)> callback);
    
    // Model listing
    std::vector<std::string> listModels(const ModelConfig& config);
    void listModelsAsync(const ModelConfig& config,
                        std::function<void(const std::vector<std::string>&)> callback);
    
    // Request building (for debugging/logging)
    void* buildRequestBody(const std::string& prompt, const ModelConfig& config);
    
    // Logging and metrics
    std::vector<ApiCallLog> getCallHistory() const;
    void clearCallHistory();
    ApiCallLog getLastCall() const;
    double getAverageLatency() const;
    int getSuccessRate() const;  // 0-100


    void generationCompleted(const ApiResponse& response);
    void generationFailed(const std::string& error);
    void streamChunkReceived(const std::string& chunk);
    void streamingCompleted();
    void streamingFailed(const std::string& error);
    void healthCheckCompleted(bool healthy);
    void modelListReceived(const std::vector<std::string>& models);

private:
    void onNetworkReplyFinished(void** reply);

private:
    // API request execution
    ApiResponse executeRequest(const std::string& endpoint,
                             const std::string& method,
                             const void*& body,
                             const std::string& api_key,
                             const std::map<std::string, std::string>& headers = {});
    
    // Response parsing by backend
    std::string parseAnthropicResponse(const void*& response);
    std::string parseOpenAIResponse(const void*& response);
    std::string parseGoogleResponse(const void*& response);
    std::string parseMoonshotResponse(const void*& response);
    std::string parseAzureOpenAIResponse(const void*& response);
    std::string parseAwsBedrockResponse(const void*& response);
    
    // Request building by backend
    void* buildAnthropicRequest(const std::string& prompt, const ModelConfig& config);
    void* buildOpenAIRequest(const std::string& prompt, const ModelConfig& config);
    void* buildGoogleRequest(const std::string& prompt, const ModelConfig& config);
    void* buildMoonshotRequest(const std::string& prompt, const ModelConfig& config);
    void* buildAzureOpenAIRequest(const std::string& prompt, const ModelConfig& config);
    void* buildAwsBedrockRequest(const std::string& prompt, const ModelConfig& config);
    
    // Endpoint mapping
    struct ApiEndpoint {
        std::string base_url;
        std::string chat_endpoint;
        std::string model_list_endpoint;
        std::function<void*(const std::string&, const ModelConfig&)> request_builder;
        std::function<std::string(const void*&)> response_parser;
    };
    
    // API endpoint definitions
    void initializeApiEndpoints();
    std::map<int, ApiEndpoint> api_endpoints;  // keyed by ModelBackend enum
    
    // Network management
    std::unique_ptr<void*> network_manager;
    std::map<void**, std::function<void(const std::string&)>> pending_callbacks;
    
    // Call history and metrics
    std::vector<ApiCallLog> call_history;
    static const int MAX_HISTORY_SIZE = 1000;
    
    // Error handling
    std::string formatErrorResponse(int status_code, const std::string& body);
    void logApiCall(const ApiCallLog& log);
};

#endif // CLOUD_API_CLIENT_H


