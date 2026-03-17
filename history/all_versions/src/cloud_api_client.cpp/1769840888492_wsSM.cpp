#include "cloud_api_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <mutex>
#include <deque>
#include <numeric>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

// Thread-safety for history
std::mutex historyMutex;
std::vector<ApiCallLog> callHistoryStore;

// Helper function to convert std::string to std::wstring
std::wstring s2ws(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string ws2s(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

CloudApiClient::CloudApiClient(UniversalModelRouter* parent) {
}

CloudApiClient::~CloudApiClient() {
}

nlohmann::json CloudApiClient::buildRequestBody(const std::string& prompt, const CloudModelConfig& config) {
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

ApiResponse CloudApiClient::performRequest(const std::string& url_str, const nlohmann::json& body, const CloudModelConfig& config, std::function<void(const std::string&)> streamCallback) {
    ApiResponse response;
    response.success = false;
    
    // Parse URL (Simplified)
// ...existing code...
    auto start = std::chrono::high_resolution_clock::now();
    
    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), 
                                       data, dataLen, dataLen, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
                            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        
        response.status_code = (int)dwStatusCode;
        
        DWORD dwSizeAvail = 0;
        DWORD dwDownloaded = 0;
        std::string buffer;
        
        do {
            dwSizeAvail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvail)) break;
            if (dwSizeAvail == 0) break;
            std::vector<char> pszOutBuffer(dwSizeAvail + 1);
            if (!WinHttpReadData(hRequest, &pszOutBuffer[0], dwSizeAvail, &dwDownloaded)) break;
            
            std::string chunk(&pszOutBuffer[0], dwDownloaded);
            
            if (streamCallback) {
                // For SSE/streaming, we might need to parse lines here if provider works that way
                // But generally passing raw chunks is "correct" for a raw HTTP client; 
                // caller handles protocol logic (SSE parsing).
                streamCallback(chunk);
            }
            
            buffer.append(chunk);
        } while (dwSizeAvail > 0);
        
        response.raw_body = buffer;

        auto end = std::chrono::high_resolution_clock::now();
        response.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Log history
        {
            std::lock_guard<std::mutex> lock(historyMutex);
            ApiCallLog log;
            log.timestamp = std::chrono::system_clock::now();
            log.endpoint = config.endpoint;
            log.provider = config.provider;
            log.model = config.model;
            log.latencyMs = response.latency_ms;
            log.statusCode = response.status_code;
            log.success = (response.status_code >= 200 && response.status_code < 300);
            callHistoryStore.push_back(log);
            // Keep history limited
            if (callHistoryStore.size() > 100) callHistoryStore.erase(callHistoryStore.begin());
        }
        
        if (response.status_code >= 200 && response.status_code < 300) {
            response.success = true;
            // Only parse full JSON if NOT streaming or if we want the final full object
            if (!streamCallback) {
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
                    } else if (config.provider == "anthropic") {
                        if (j.contains("content") && !j["content"].empty()) {
                             for (auto& item : j["content"]) {
                                 if (item["type"] == "text") response.content += item["text"].get<std::string>();
                             }
                        }
                    }
                } catch (...) {
                    response.error_message = "Failed to parse JSON response";
                }
            }
        } else {
            response.error_message = "HTTP Error: " + std::to_string(response.status_code);
        }
    } else {
        response.error_message = "WinHttp Request failed";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
}

std::string CloudApiClient::generate(const std::string& prompt, const CloudModelConfig& config) {
    nlohmann::json body = buildRequestBody(prompt, config);
    ApiResponse resp = performRequest(config.endpoint, body, config);
    if (!resp.success) {
        return "Error: " + resp.error_message;
    }
    return resp.content;
}

void CloudApiClient::generateAsync(const std::string& prompt, 
                  const CloudModelConfig& config,
                  std::function<void(std::string)> callback) {
    std::thread([this, prompt, config, callback]() {
        nlohmann::json body = buildRequestBody(prompt, config);
        ApiResponse resp = performRequest(config.endpoint, body, config);
        if (callback) callback(resp.content);
    }).detach();
}

void CloudApiClient::generateStream(const std::string& prompt,
                   const CloudModelConfig& config,
                   std::function<void(const std::string&)> token_callback,
                   std::function<void(const std::string&)> complete_callback) {
   // Mock streaming via blocking call for now, since WinHttp streaming requires different handling
   std::thread([this, prompt, config, token_callback, complete_callback]() {
        std::string full = generate(prompt, config); 
        if (token_callback) token_callback(full);
        if (complete_callback) complete_callback("");
    }).detach();
}bool CloudApiClient::checkProviderHealth(const CloudModelConfig& config) {
    return true; // Mock true
}

void CloudApiClient::checkProviderHealthAsync(const CloudModelConfig& config,
                             std::function<void(bool)> callback) {
    if(callback) callback(true);
}

std::vector<std::string> CloudApiClient::listModels(const CloudModelConfig& config) {
    return {"gpt-4", "claude-3-opus", "llama3"}; 
}

void CloudApiClient::listModelsAsync(const CloudModelConfig& config,
                    std::function<void(const std::vector<std::string>&)> callback) {
    if(callback) callback(listModels(config));
}

std::vector<ApiCallLog> CloudApiClient::getCallHistory() const {
    return callHistory;
}

void CloudApiClient::clearCallHistory() {
    // callHistory.clear();
}

ApiCallLog CloudApiClient::getLastCall() const {
    if (callHistory.empty()) return ApiCallLog();
    return callHistory.back();
}

double CloudApiClient::getAverageLatency() const {
    return 0.0;
}

int CloudApiClient::getSuccessRate() const {
    return 100;
}

} // namespace RawrXD
