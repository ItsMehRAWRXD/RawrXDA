#pragma once

#include <memory>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class UniversalModelRouter;

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
    nlohmann::json metadata;
};

// Streaming response structure
struct StreamingResponse {
    std::string chunk;
    bool is_final;
    std::string error;
};

struct ModelConfig {
    std::string provider;
    std::string model;
    std::string apiKey;
    std::string endpoint;
    double temperature = 0.7;
    int maxTokens = 1000;
};

// Cloud API Client
class CloudApiClient {

public:
    explicit CloudApiClient(UniversalModelRouter* parent = nullptr);
    ~CloudApiClient();

    // Synchronous generation (blocking)
    std::string generate(const std::string& prompt, const CloudModelConfig& config);
    
    // Asynchronous generation
    void generateAsync(const std::string& prompt, 
                      const CloudModelConfig& config,
                      std::function<void(const ApiResponse&)> callback);
    
    // Streaming generation
    void generateStream(const std::string& prompt,
                       const CloudModelConfig& config,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void(const std::string&)> error_callback = nullptr);
    
    // Provider health check
    bool checkProviderHealth(const CloudModelConfig& config);
    void checkProviderHealthAsync(const CloudModelConfig& config,
                                 std::function<void(bool)> callback);
    
    // Model listing
    std::vector<std::string> listModels(const CloudModelConfig& config);
    void listModelsAsync(const CloudModelConfig& config,
                        std::function<void(const std::vector<std::string>&)> callback);
    
    // Request building (for debugging/logging)
    nlohmann::json buildRequestBody(const std::string& prompt, const CloudModelConfig& config);
    
    // Logging and metrics
    std::vector<ApiCallLog> getCallHistory() const;
    void clearCallHistory();
    ApiCallLog getLastCall() const;
    double getAverageLatency() const;
    int getSuccessRate() const;  // 0-100

private:
    std::vector<ApiCallLog> callHistory;
    // Internal curl helper
    ApiResponse performRequest(const std::string& url, const nlohmann::json& body, const CloudModelConfig& config);
};
