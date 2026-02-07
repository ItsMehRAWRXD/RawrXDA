#include "backend/ollama_client.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

namespace RawrXD {
namespace Backend {

OllamaClient::OllamaClient(const std::string& base_url)
    : m_base_url(base_url), m_timeout_seconds(300) {
}

OllamaClient::~OllamaClient() {
}

void OllamaClient::setBaseUrl(const std::string& url) {
    m_base_url = url;
}

bool OllamaClient::testConnection() {
    try {
        std::string version = getVersion();
        return !version.empty();
    } catch (...) {
        return false;
    }
}

std::string OllamaClient::getVersion() {
    std::string response = makeGetRequest("/api/version");
    // Parse JSON: {"version": "0.1.0"}
    size_t pos = response.find("\"version\"");
    if (pos != std::string::npos) {
        size_t start = response.find("\"", pos + 10) + 1;
        size_t end = response.find("\"", start);
        return response.substr(start, end - start);
    }
    return "";
}

bool OllamaClient::isRunning() {
    return testConnection();
}

std::vector<OllamaModel> OllamaClient::listModels() {
    std::string response = makeGetRequest("/api/tags");
    return parseModels(response);
}

OllamaResponse OllamaClient::generateSync(const OllamaGenerateRequest& request) {
    OllamaGenerateRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createGenerateRequestJson(sync_req);
    std::string response = makePostRequest("/api/generate", json);
    
    return parseResponse(response);
}

OllamaResponse OllamaClient::chatSync(const OllamaChatRequest& request) {
    OllamaChatRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createChatRequestJson(sync_req);
    std::string response = makePostRequest("/api/chat", json);
    
    return parseResponse(response);
}

bool OllamaClient::generate(const OllamaGenerateRequest& request,
                           StreamCallback on_chunk,
                           ErrorCallback on_error,
                           CompletionCallback on_complete) {
    std::string json = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", json, on_chunk, on_error, on_complete);
}

bool OllamaClient::chat(const OllamaChatRequest& request,
                       StreamCallback on_chunk,
                       ErrorCallback on_error,
                       CompletionCallback on_complete) {
    std::string json = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", json, on_chunk, on_error, on_complete);
}

std::vector<float> OllamaClient::embeddings(const std::string& model, const std::string& prompt) {
    // Create JSON: {"model": "...", "prompt": "..."}
    std::ostringstream oss;
    oss << "{\"model\":\"" << model << "\",\"prompt\":\"" << prompt << "\"}";
    
    std::string response = makePostRequest("/api/embeddings", oss.str());
    
    // Parse embeddings array from JSON
    std::vector<float> result;
    // Simple parser for array of numbers
    size_t pos = response.find("[");
    if (pos != std::string::npos) {
        size_t end = response.find("]", pos);
        std::string array_str = response.substr(pos + 1, end - pos - 1);
        
        std::istringstream iss(array_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            try {
                result.push_back(std::stof(token));
            } catch (...) {}
        }
    }
    
    return result;
}

// JSON creation helpers
std::string OllamaClient::createGenerateRequestJson(const OllamaGenerateRequest& req) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << req.model << "\",";
    oss << "\"prompt\":\"" << req.prompt << "\",";
    oss << "\"stream\":" << (req.stream ? "true" : "false");
    
    if (!req.options.empty()) {
        oss << ",\"options\":{";
        bool first = true;
        for (const auto& [key, value] : req.options) {
            if (!first) oss << ",";
            oss << "\"" << key << "\":" << value;
            first = false;
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

std::string OllamaClient::createChatRequestJson(const OllamaChatRequest& req) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << req.model << "\",";
    oss << "\"messages\":[";
    
    for (size_t i = 0; i < req.messages.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"role\":\"" << req.messages[i].role << "\",";
        oss << "\"content\":\"" << req.messages[i].content << "\"}";
    }
    
    oss << "],";
    oss << "\"stream\":" << (req.stream ? "true" : "false");
    oss << "}";
    return oss.str();
}

OllamaResponse OllamaClient::parseResponse(const std::string& json) {
    OllamaResponse resp;
    
    // Simple JSON parser (production should use proper library)
    auto getValue = [&](const std::string& key) -> std::string {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        
        size_t colon = json.find(":", pos);
        size_t start = json.find("\"", colon) + 1;
        if (start == std::string::npos) return "";
        
        size_t end = json.find("\"", start);
        return json.substr(start, end - start);
    };
    
    auto getNumber = [&](const std::string& key) -> uint64_t {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        
        size_t colon = json.find(":", pos);
        size_t start = colon + 1;
        size_t end = json.find_first_of(",}", start);
        
        std::string num_str = json.substr(start, end - start);
        try {
            return std::stoull(num_str);
        } catch (...) {
            return 0;
        }
    };
    
    resp.model = getValue("model");
    resp.response = getValue("response");
    resp.done = json.find("\"done\":true") != std::string::npos;
    
    resp.total_duration = getNumber("total_duration");
    resp.prompt_eval_count = getNumber("prompt_eval_count");
    resp.eval_count = getNumber("eval_count");
    
    return resp;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json) {
    std::vector<OllamaModel> models;
    
    // Simple parser for models array
    size_t pos = json.find("\"models\"");
    if (pos != std::string::npos) {
        size_t array_start = json.find("[", pos);
        size_t array_end = json.find("]", array_start);
        
        std::string models_str = json.substr(array_start + 1, array_end - array_start - 1);
        
        // Parse each model object
        size_t obj_pos = 0;
        while ((obj_pos = models_str.find("{", obj_pos)) != std::string::npos) {
            size_t obj_end = models_str.find("}", obj_pos);
            std::string obj_str = models_str.substr(obj_pos, obj_end - obj_pos + 1);
            
            OllamaModel model;
            
            // Extract name
            size_t name_pos = obj_str.find("\"name\"");
            if (name_pos != std::string::npos) {
                size_t start = obj_str.find("\"", name_pos + 6) + 1;
                size_t end = obj_str.find("\"", start);
                model.name = obj_str.substr(start, end - start);
            }
            
            models.push_back(model);
            obj_pos = obj_end + 1;
        }
    }
    
    return models;
}

// Platform-specific HTTP implementations
#ifdef _WIN32

std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    // Parse URL
    std::string host = "localhost";
    int port = 11434;
    
    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    std::wstring wendpoint(endpoint.begin(), endpoint.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    BOOL bResults = WinHttpSendRequest(hRequest,
                                       WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    
    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        
        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            
            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    char* pszOutBuffer = new char[dwSize + 1];
                    ZeroMemory(pszOutBuffer, dwSize + 1);
                    
                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                        response.append(pszOutBuffer, dwDownloaded);
                    }
                    
                    delete[] pszOutBuffer;
                }
            } while (dwSize > 0);
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    std::wstring wendpoint(endpoint.begin(), endpoint.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    std::wstring headers = L"Content-Type: application/json\r\n";
    
    BOOL bResults = WinHttpSendRequest(hRequest,
                                       headers.c_str(), -1,
                                       (LPVOID)json_body.c_str(), json_body.length(),
                                       json_body.length(), 0);
    
    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        
        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            
            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    char* pszOutBuffer = new char[dwSize + 1];
                    ZeroMemory(pszOutBuffer, dwSize + 1);
                    
                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                        response.append(pszOutBuffer, dwDownloaded);
                    }
                    
                    delete[] pszOutBuffer;
                }
            } while (dwSize > 0);
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint,
                                           const std::string& json_body,
                                           StreamCallback on_chunk,
                                           ErrorCallback on_error,
                                           CompletionCallback on_complete) {
    // Streaming implementation using WinHTTP
    // For now, fall back to sync mode
    OllamaResponse resp;
    resp.response = makePostRequest(endpoint, json_body);
    resp.done = true;
    
    if (on_chunk) on_chunk(resp);
    if (on_complete) on_complete(resp);
    
    return true;
}

#else
// Linux/Mac implementation would use libcurl here
#endif

} // namespace Backend
} // namespace RawrXD
