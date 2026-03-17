#include "backend/ollama_client.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

using json = nlohmann::json;

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
    
    // Production-ready JSON parsing
    try {
        json j = json::parse(response);
        if (j.contains("version") && j["version"].is_string()) {
            return j["version"].get<std::string>();
        }
        return "";
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error in getVersion: " << e.what() << std::endl;
        return "";
    }
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

OllamaResponse OllamaClient::parseResponse(const std::string& json_str) {
    OllamaResponse resp;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        if (j.contains("error") && j["error"].is_string()) {
            resp.error = true;
            resp.error_message = j["error"].get<std::string>();
        }

        if (j.contains("model") && j["model"].is_string()) {
            resp.model = j["model"].get<std::string>();
        }
        if (j.contains("response") && j["response"].is_string()) {
            resp.response = j["response"].get<std::string>();
        }
        if (j.contains("done") && j["done"].is_boolean()) {
            resp.done = j["done"].get<bool>();
        }

        auto to_u64 = [](const json& value) {
            return static_cast<uint64_t>(value.get<double>());
        };
        if (j.contains("total_duration") && j["total_duration"].is_number()) {
            resp.total_duration = to_u64(j["total_duration"]);
        }
        if (j.contains("prompt_eval_count") && j["prompt_eval_count"].is_number()) {
            resp.prompt_eval_count = to_u64(j["prompt_eval_count"]);
        }
        if (j.contains("eval_count") && j["eval_count"].is_number()) {
            resp.eval_count = to_u64(j["eval_count"]);
        }
        if (j.contains("load_duration") && j["load_duration"].is_number()) {
            resp.load_duration = to_u64(j["load_duration"]);
        }
        if (j.contains("prompt_eval_duration") && j["prompt_eval_duration"].is_number()) {
            resp.prompt_eval_duration = to_u64(j["prompt_eval_duration"]);
        }
        if (j.contains("eval_duration") && j["eval_duration"].is_number()) {
            resp.eval_duration = to_u64(j["eval_duration"]);
        }
    } catch (const std::exception& e) {
        // Log parsing error and return partial response
        std::cerr << "JSON parsing error in parseResponse: " << e.what() << std::endl;
        // resp already initialized with defaults
    }
    
    return resp;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str) {
    std::vector<OllamaModel> models;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        if (j.contains("models") && j["models"].is_array()) {
            auto models_array = j["models"].get<std::vector<json>>();
            for (const auto& model_json : models_array) {
                if (!model_json.is_object()) {
                    continue;
                }

                OllamaModel model;
                if (model_json.contains("name") && model_json["name"].is_string()) {
                    model.name = model_json["name"].get<std::string>();
                }
                if (model_json.contains("modified_at") && model_json["modified_at"].is_string()) {
                    model.modified_at = model_json["modified_at"].get<std::string>();
                }
                if (model_json.contains("size") && model_json["size"].is_number()) {
                    model.size = static_cast<uint64_t>(model_json["size"].get<double>());
                }
                if (model_json.contains("digest") && model_json["digest"].is_string()) {
                    model.digest = model_json["digest"].get<std::string>();
                }

                if (model_json.contains("details") && model_json["details"].is_object()) {
                    const auto& details = model_json["details"];
                    if (details.contains("format") && details["format"].is_string()) {
                        model.format = details["format"].get<std::string>();
                    }
                    if (details.contains("family") && details["family"].is_string()) {
                        model.family = details["family"].get<std::string>();
                    }
                    if (details.contains("parameter_size") && details["parameter_size"].is_string()) {
                        model.parameter_size = details["parameter_size"].get<std::string>();
                    }
                    if (details.contains("quantization_level") && details["quantization_level"].is_string()) {
                        model.quantization_level = details["quantization_level"].get<std::string>();
                    }
                }

                models.push_back(model);
            }
        }
    } catch (const std::exception& e) {
        // Log parsing error and return empty list
        std::cerr << "JSON parsing error in parseModels: " << e.what() << std::endl;
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
    
    if (on_chunk) on_chunk(resp.response);
    if (on_complete) on_complete(resp);
    
    return true;
}

#else
// Linux/Mac implementation would use libcurl here
#endif

} // namespace Backend
} // namespace RawrXD
