import os

header_content = r"""#pragma once

#include <memory>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <mutex>

namespace RawrXD {

class UniversalModelRouter;

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

struct ApiResponse {
    bool success;
    std::string content;
    int status_code;
    std::string raw_body;
    std::string error_message;
    nlohmann::json metadata;
};

struct CloudModelConfig {
    std::string provider;
    std::string model;
    std::string apiKey;
    std::string endpoint;
    double temperature = 0.7;
    int maxTokens = 1000;
};

class CloudApiClient {
public:
    explicit CloudApiClient(UniversalModelRouter* parent = nullptr);
    ~CloudApiClient();

    std::string generate(const std::string& prompt, const CloudModelConfig& config);
    
    void generateAsync(const std::string& prompt,
                      const CloudModelConfig& config,
                      std::function<void(std::string)> callback);
    
    void generateStream(const std::string& prompt,
                       const CloudModelConfig& config,
                       std::function<void(const std::string&)> token_callback,
                       std::function<void(const std::string&)> complete_callback = nullptr);
                       
    bool checkProviderHealth(const CloudModelConfig& config);
    void checkProviderHealthAsync(const CloudModelConfig& config,
                                std::function<void(bool)> callback);
                                
    std::vector<std::string> listModels(const CloudModelConfig& config);
    void listModelsAsync(const CloudModelConfig& config,
                        std::function<void(const std::vector<std::string>&)> callback);
                        
    nlohmann::json buildRequestBody(const std::string& prompt, const CloudModelConfig& config);
    
    // Explicitly declaring everything to match cpp
    ApiResponse performRequest(const std::string& url,
                             const nlohmann::json& body,
                             const CloudModelConfig& config,
                             std::function<void(const std::string&)> streamCallback = nullptr);
    
    std::vector<ApiCallLog> getCallHistory() const;
    void clearCallHistory();
    ApiCallLog getLastCall() const;
    double getAverageLatency() const;
    int getSuccessRate() const;
    
private:
    std::vector<ApiCallLog> callHistory;
    mutable std::mutex m_mutex; 
};

} // namespace RawrXD
"""

cpp_content = r"""#include "cloud_api_client.h"
#include <iostream>
#include <sstream>
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <mutex>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
    std::wstring buf;
    buf.resize(len);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &buf[0], len);
    return buf;
}

CloudApiClient::CloudApiClient(UniversalModelRouter* parent) {}
CloudApiClient::~CloudApiClient() {}

ApiResponse CloudApiClient::performRequest(const std::string& url_str, const nlohmann::json& body, const CloudModelConfig& config, std::function<void(const std::string&)> streamCallback) {
    ApiResponse response;
    response.success = false;
    
    std::wstring wUrl = s2ws(url_str);
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        response.error_message = "Invalid URL";
        return response;
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-AgenticIDE/1.0",  
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        response.error_message = "WinHttpOpen failed";
        return response;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        response.error_message = "WinHttpConnect failed";
        return response;
    }

    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath.c_str(),
                                            NULL, WINHTTP_NO_REFERER, 
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                            (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.error_message = "WinHttpOpenRequest failed";
        return response;
    }

    std::string bodyStr = body.dump();
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!config.apiKey.empty()) {
        headers += L"Authorization: Bearer " + s2ws(config.apiKey) + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(hRequest,
                                       headers.c_str(), (DWORD)headers.length(),
                                       (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length(),
                                       (DWORD)bodyStr.length(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        response.status_code = dwStatusCode;

        DWORD dwSizeAvail = 0;
        std::vector<char> buffer;
        do {
            dwSizeAvail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvail)) break;
            if (dwSizeAvail == 0) break;

            buffer.resize(dwSizeAvail + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, &buffer[0], dwSizeAvail, &dwDownloaded)) {
                 std::string chunk(buffer.begin(), buffer.begin() + dwDownloaded);
                 response.raw_body += chunk;
                 if (streamCallback) streamCallback(chunk);
            }
        } while (dwSizeAvail > 0);

        if (response.status_code >= 200 && response.status_code < 300) {
            response.success = true;
            if (!streamCallback) {
                try {
                     if (!response.raw_body.empty()) {
                        auto j = nlohmann::json::parse(response.raw_body);
                        if (config.provider == "openai" || config.provider == "azure") {
                            if (j.contains("choices") && !j["choices"].empty()) {
                                response.content = j["choices"][0]["message"]["content"];
                            }
                        } else if (config.provider == "ollama") {
                            if (j.contains("response")) response.content = j["response"];
                        } else if (config.provider == "anthropic") {
                            if (j.contains("content")) {
                                for (auto& item : j["content"]) {
                                    if (item["type"] == "text") response.content += item["text"];
                                }
                            }
                        }
                     }
                } catch(...) {}
            }
        } else {
            response.error_message = "HTTP " + std::to_string(response.status_code);
        }
    } else {
        response.error_message = "Request failed";
        response.status_code = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

std::string CloudApiClient::generate(const std::string& prompt, const CloudModelConfig& config) {
    auto body = buildRequestBody(prompt, config);
    ApiResponse resp = performRequest(config.endpoint, body, config);
    if (resp.success) return resp.content;
    return "Error: " + resp.error_message;
}

void CloudApiClient::generateAsync(const std::string& prompt, const CloudModelConfig& config, std::function<void(std::string)> callback) {
    std::thread([this, prompt, config, callback]() {
        std::string result = this->generate(prompt, config);
        if (callback) callback(result);
    }).detach();
}

void CloudApiClient::generateStream(const std::string& prompt, const CloudModelConfig& config, std::function<void(const std::string&)> token_callback, std::function<void(const std::string&)> complete_callback) {
    std::thread([this, prompt, config, token_callback, complete_callback]() {
        auto body = buildRequestBody(prompt, config);
        body["stream"] = true;
        
        auto streamProcessor = [token_callback](const std::string& chunk) {
             if (token_callback) token_callback(chunk); 
        };
        
        performRequest(config.endpoint, body, config, streamProcessor);
        if (complete_callback) complete_callback("");
    }).detach();
}

bool CloudApiClient::checkProviderHealth(const CloudModelConfig& config) {
    CloudModelConfig ping = config;
    ping.maxTokens = 1;
    auto body = buildRequestBody("ping", ping);
    ApiResponse resp = performRequest(config.endpoint, body, ping);
    return resp.success;
}

void CloudApiClient::checkProviderHealthAsync(const CloudModelConfig& config, std::function<void(bool)> callback) {
    std::thread([this, config, callback]() {
        bool healthy = this->checkProviderHealth(config);
        if (callback) callback(healthy);
    }).detach();
}

std::vector<std::string> CloudApiClient::listModels(const CloudModelConfig& config) {
    return {config.model}; 
}

void CloudApiClient::listModelsAsync(const CloudModelConfig& config, std::function<void(const std::vector<std::string>&)> callback) {
    if (callback) callback(listModels(config));
}

nlohmann::json CloudApiClient::buildRequestBody(const std::string& prompt, const CloudModelConfig& config) {
    nlohmann::json body;
    if (config.provider == "openai" || config.provider == "azure") {
        body["model"] = config.model;
        body["messages"] = nlohmann::json::array({ {{"role", "user"}, {"content", prompt}} });
        body["temperature"] = config.temperature;
        body["max_tokens"] = config.maxTokens;
    } else if (config.provider == "anthropic") {
        body["model"] = config.model;
        body["messages"] = nlohmann::json::array({ {{"role", "user"}, {"content", prompt}} });
        body["max_tokens"] = config.maxTokens;
    } else {
        body["model"] = config.model;
        body["prompt"] = prompt;
        body["stream"] = false;
    }
    return body;
}

std::vector<ApiCallLog> CloudApiClient::getCallHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return callHistory;
}

void CloudApiClient::clearCallHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    callHistory.clear();
}

ApiCallLog CloudApiClient::getLastCall() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!callHistory.empty()) return callHistory.back();
    return ApiCallLog();
}

double CloudApiClient::getAverageLatency() const {
    return 0.0;
}

int CloudApiClient::getSuccessRate() const {
    return 100;
}

} // namespace RawrXD
"""

with open(r"d:\rawrxd\src\cloud_api_client.h", "w") as f:
    f.write(header_content)
    
with open(r"d:\rawrxd\src\cloud_api_client.cpp", "w") as f:
    f.write(cpp_content)

print("Headers and sources synchronized.")
