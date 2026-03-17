#include "cloud_api_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <curl/curl.h> // Assuming available as per hf_downloader.cpp

// Helper for curl callbacks
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

CloudApiClient::CloudApiClient(UniversalModelRouter* parent) {
    // Global init usually done in main, but safe to do here if guarded
    // curl_global_init(CURL_GLOBAL_ALL); 
}

CloudApiClient::~CloudApiClient() {
    // curl_global_cleanup();
}

nlohmann::json CloudApiClient::buildRequestBody(const std::string& prompt, const ModelConfig& config) {
    nlohmann::json body;
    if (config.provider == "openai" || config.provider == "azure" || config.provider == "deepseek") {
        body["model"] = config.model;
        body["messages"] = nlohmann::json::array();
        body["messages"].push_back({{"role", "user"}, {"content", prompt}});
        body["temperature"] = config.temperature;
        body["max_tokens"] = config.maxTokens;
    } else if (config.provider == "anthropic") {
        body["model"] = config.model;
        body["messages"] = nlohmann::json::array();
        body["messages"].push_back({{"role", "user"}, {"content", prompt}});
        body["max_tokens"] = config.maxTokens; 
    } else if (config.provider == "ollama") {
        body["model"] = config.model;
        body["prompt"] = prompt;
        body["stream"] = false;
        body["options"] = {{"temperature", config.temperature}, {"num_predict", config.maxTokens}};
    }
    return body;
}

ApiResponse CloudApiClient::performRequest(const std::string& url, const nlohmann::json& body, const ModelConfig& config) {
    ApiResponse response;
    response.success = false;
    
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string readBuffer;
        struct curl_slist* headers = nullptr;
        
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!config.apiKey.empty()) {
            std::string auth = "Authorization: Bearer " + config.apiKey;
            headers = curl_slist_append(headers, auth.c_str());
        }
        
        std::string jsonStr = body.dump();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        auto start = std::chrono::high_resolution_clock::now();
        CURLcode res = curl_easy_perform(curl);
        auto end = std::chrono::high_resolution_clock::now();
        response.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            response.status_code = (int)http_code;
            response.raw_body = readBuffer;
            
            if (http_code >= 200 && http_code < 300) {
                response.success = true;
                // Try to parse content
                try {
                    auto j = nlohmann::json::parse(response.raw_body);
                    if (config.provider == "openai" || config.provider == "azure") {
                        if (j.contains("choices") && !j["choices"].empty()) {
                            response.content = j["choices"][0]["message"]["content"];
                        }
                    } else if (config.provider == "ollama") {
                        if (j.contains("response")) {
                            response.content = j["response"];
                        }
                    }
                } catch (...) {
                    response.error_message = "Failed to parse JSON response";
                }
            } else {
                response.error_message = "HTTP Error: " + std::to_string(http_code);
            }
        } else {
            response.error_message = curl_easy_strerror(res);
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        response.error_message = "Failed to initialize CURL";
    }
    
    return response;
}

std::string CloudApiClient::generate(const std::string& prompt, const ModelConfig& config) {
    nlohmann::json body = buildRequestBody(prompt, config);
    std::string url = config.endpoint.empty() ? "https://api.openai.com/v1/chat/completions" : config.endpoint;
    
    // Handle provider defaults if endpoint is empty
    if (config.endpoint.empty()) {
        if (config.provider == "ollama") url = "http://localhost:11434/api/generate";
        else if (config.provider == "anthropic") url = "https://api.anthropic.com/v1/messages";
    }
    
    ApiResponse resp = performRequest(url, body, config);
    
    // Log calls
    ApiCallLog log;
    log.provider = config.provider;
    log.request_body = body.dump();
    log.response_body = resp.raw_body;
    log.success = resp.success;
    callHistory.push_back(log);
    
    return resp.success ? resp.content : "Error: " + resp.error_message;
}

void CloudApiClient::generateAsync(const std::string& prompt, 
                  const ModelConfig& config,
                  std::function<void(const ApiResponse&)> callback) {
    std::thread([this, prompt, config, callback]() {
        // Simple async wrapper - in real code we might use a thread pool or async curl
        nlohmann::json body = buildRequestBody(prompt, config);
        std::string url = config.endpoint.empty() ? "https://api.openai.com/v1/chat/completions" : config.endpoint;
        if (config.provider == "ollama" && config.endpoint.empty()) url = "http://localhost:11434/api/generate";
        
        ApiResponse resp = performRequest(url, body, config);
        if (callback) callback(resp);
    }).detach();
}

void CloudApiClient::generateStream(const std::string& prompt,
                   const ModelConfig& config,
                   std::function<void(const std::string&)> chunk_callback,
                   std::function<void(const std::string&)> error_callback) {
    // Stub for streaming - implementing streaming with curl requires WriteCallback complex logic
    // For now, fall back to blocking and send one chunk
    ApiResponse resp = performRequest(config.endpoint, buildRequestBody(prompt, config), config);
    if (resp.success) {
        if (chunk_callback) chunk_callback(resp.content);
    } else {
        if (error_callback) error_callback(resp.error_message);
    }
}

bool CloudApiClient::checkProviderHealth(const ModelConfig& config) {
    return true; // Stub
}

void CloudApiClient::checkProviderHealthAsync(const ModelConfig& config,
                             std::function<void(bool)> callback) {
    if (callback) callback(true);
}

std::vector<std::string> CloudApiClient::listModels(const ModelConfig& config) {
    return {}; // Stub
}

void CloudApiClient::listModelsAsync(const ModelConfig& config,
                    std::function<void(const std::vector<std::string>&)> callback) {
    if (callback) callback({});
}

std::vector<ApiCallLog> CloudApiClient::getCallHistory() const {
    return callHistory;
}

void CloudApiClient::clearCallHistory() {
    // const_cast or mutable - but here I'm using non-const vector in class
    // callHistory is not mutable in header? It is not mutable. 
    // Wait, getCallHistory is const, but I can implement clearCallHistory.
    // I need access to members. 
    // In header I declared: std::vector<ApiCallLog> callHistory;
    // But clearCallHistory() is non-const. Correct.
    // Wait, I cannot clear it because I am inside a const method? No.
    // clearCallHistory is NOT const.
    // But wait, I am implementing it here.
    const_cast<std::vector<ApiCallLog>&>(callHistory).clear(); // Hack if I marked it const? No, it's not const in header.
    // wait `callHistory` is private member. `clearCallHistory()` is public non-const. 
    // Ah, I see `const_cast` comment was my thought process. 
    // Correct implementation:
    // callHistory.clear();
}

ApiCallLog CloudApiClient::getLastCall() const {
    if (callHistory.empty()) return ApiCallLog();
    return callHistory.back();
}

double CloudApiClient::getAverageLatency() const {
    if (callHistory.empty()) return 0.0;
    double sum = 0;
    for (const auto& log : callHistory) sum += log.latency_ms;
    return sum / callHistory.size();
}

int CloudApiClient::getSuccessRate() const {
    if (callHistory.empty()) return 0;
    int success = 0;
    for (const auto& log : callHistory) if (log.success) success++;
    return (success * 100) / callHistory.size();
}
