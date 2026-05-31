#include "model_host_client.h"
#include <windows.h>
#include <wininet.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "wininet.lib")

namespace RawrXD {

ModelHostClient::ModelHostClient() 
    : connected_(false), modelLoaded_(false), timeoutSeconds_(30), httpClient_(nullptr) {
}

ModelHostClient::~ModelHostClient() {
    Disconnect();
}

bool ModelHostClient::Connect(const std::string& endpoint) {
    endpoint_ = endpoint;
    connected_ = true;
    return true;
}

void ModelHostClient::Disconnect() {
    connected_ = false;
    modelLoaded_ = false;
    if (httpClient_) {
        InternetCloseHandle(static_cast<HINTERNET>(httpClient_));
        httpClient_ = nullptr;
    }
}

bool ModelHostClient::IsConnected() const {
    return connected_;
}

std::vector<ModelHostClient::ModelInfo> ModelHostClient::ListModels() {
    std::vector<ModelInfo> models;
    
    if (!connected_) return models;
    
    std::string response;
    if (SendRequest("/api/tags", "", response)) {
        try {
            Json::Value root;
            Json::Reader reader;
            if (reader.parse(response, root)) {
                const Json::Value& models_array = root["models"];
                for (const auto& model : models_array) {
                    ModelInfo info;
                    info.name = model["name"].asString();
                    info.path = model["path"].asString();
                    info.size = model["size"].asUInt64();
                    info.format = model["format"].asString();
                    info.loaded = model["loaded"].asBool();
                    models.push_back(info);
                }
            }
        } catch (...) {
            fprintf(stderr, "[ModelHostClient] Parse error, returning empty list\n");
        }
    }
    
    return models;
}

bool ModelHostClient::LoadModel(const std::string& modelPath, ProgressCallback callback) {
    if (!connected_) return false;
    
    Json::Value request;
    request["model"] = modelPath;
    
    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, request);
    
    std::string response;
    if (SendRequest("/api/generate", body, response)) {
        modelLoaded_ = true;
        if (callback) callback(1.0f, "Model loaded");
        return true;
    }
    
    return false;
}

bool ModelHostClient::UnloadModel() {
    modelLoaded_ = false;
    return true;
}

bool ModelHostClient::IsModelLoaded() const {
    return modelLoaded_;
}

ModelHostClient::GenerationResult ModelHostClient::Generate(const GenerationParams& params) {
    GenerationResult result;
    result.success = false;
    
    if (!connected_ || !modelLoaded_) {
        result.error = "Not connected or no model loaded";
        return result;
    }
    
    Json::Value request;
    request["model"] = "current";
    request["prompt"] = params.prompt;
    request["max_tokens"] = params.maxTokens;
    request["temperature"] = params.temperature;
    request["top_p"] = params.topP;
    request["top_k"] = params.topK;
    request["repeat_penalty"] = params.repeatPenalty;
    request["stream"] = false;
    
    if (!params.stopSequence.empty()) {
        request["stop"] = params.stopSequence;
    }
    
    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, request);
    
    std::string response;
    if (SendRequest("/api/generate", body, response)) {
        try {
            Json::Value root;
            Json::Reader reader;
            if (reader.parse(response, root)) {
                result.text = root["response"].asString();
                result.success = true;
                result.tokensGenerated = root["eval_count"].asInt();
                result.tokensPerSecond = root["eval_rate"].asFloat();
            }
        } catch (...) {
            result.error = "Failed to parse response";
        }
    } else {
        result.error = "Request failed";
    }
    
    return result;
}

bool ModelHostClient::GenerateStreaming(const GenerationParams& params, TokenCallback callback) {
    if (!connected_ || !modelLoaded_) return false;
    
    Json::Value request;
    request["model"] = "current";
    request["prompt"] = params.prompt;
    request["max_tokens"] = params.maxTokens;
    request["temperature"] = params.temperature;
    request["top_p"] = params.topP;
    request["top_k"] = params.topK;
    request["repeat_penalty"] = params.repeatPenalty;
    request["stream"] = true;
    
    if (!params.stopSequence.empty()) {
        request["stop"] = params.stopSequence;
    }
    
    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, request);
    
    // For streaming, we'd need to handle chunked responses
    // This is a simplified implementation
    std::string response;
    if (SendRequest("/api/generate", body, response)) {
        // Parse streaming response (simplified)
        std::istringstream stream(response);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            try {
                Json::Value root;
                Json::Reader reader;
                if (reader.parse(line, root) && callback) {
                    callback(root["response"].asString());
                }
            } catch (...) {
                fprintf(stderr, "[ModelHostClient] Skipping invalid line\n");
            }
        }
        return true;
    }
    
    return false;
}

bool ModelHostClient::SendRequest(const std::string& path, const std::string& body, std::string& response) {
    if (endpoint_.empty()) return false;
    
    // Parse endpoint URL
    std::string host;
    std::string urlPath = path;
    INTERNET_PORT port = 80;
    bool https = false;
    
    if (endpoint_.find("https://") == 0) {
        https = true;
        port = 443;
        host = endpoint_.substr(8);
    } else if (endpoint_.find("http://") == 0) {
        host = endpoint_.substr(7);
    } else {
        host = endpoint_;
    }
    
    // Extract port if specified
    size_t portPos = host.find(':');
    if (portPos != std::string::npos) {
        port = static_cast<INTERNET_PORT>(std::stoi(host.substr(portPos + 1)));
        host = host.substr(0, portPos);
    }
    
    // Extract path from endpoint if present
    size_t pathPos = host.find('/');
    if (pathPos != std::string::npos) {
        urlPath = host.substr(pathPos) + path;
        host = host.substr(0, pathPos);
    }
    
    HINTERNET hInternet = InternetOpenA("RawrXD ModelHostClient", 
                                         INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) return false;
    
    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), port, nullptr, nullptr, 
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }
    
    DWORD flags = https ? INTERNET_FLAG_SECURE : 0;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, body.empty() ? "GET" : "POST", 
                                          urlPath.c_str(), nullptr, nullptr, nullptr, 
                                          flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }
    
    // Set timeout
    DWORD timeout = timeoutSeconds_ * 1000;
    InternetSetOption(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOption(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    
    // Set content type for POST
    if (!body.empty()) {
        const char* headers = "Content-Type: application/json";
        HttpAddRequestHeadersA(hRequest, headers, static_cast<DWORD>(strlen(headers)), 
                               HTTP_ADDREQ_FLAG_ADD);
    }
    
    // Send request
    BOOL result = HttpSendRequestA(hRequest, nullptr, 0, 
                                    body.empty() ? nullptr : const_cast<char*>(body.c_str()), 
                                    static_cast<DWORD>(body.length()));
    if (!result) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }
    
    // Read response
    char buffer[4096];
    DWORD bytesRead;
    response.clear();
    
    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    return true;
}

} // namespace RawrXD
