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

// Parse host:port from base URL like "http://localhost:11434"
static void ParseBaseUrl(const std::string& url, std::wstring& host_out, INTERNET_PORT& port_out) {
    std::string u = url;
    // Strip scheme
    auto pos = u.find("://");
    if (pos != std::string::npos) u = u.substr(pos + 3);
    // Strip trailing slash
    while (!u.empty() && u.back() == '/') u.pop_back();
    // Split host:port
    auto colon = u.rfind(':');
    std::string host_str;
    int port = 11434;
    if (colon != std::string::npos) {
        host_str = u.substr(0, colon);
        try { port = std::stoi(u.substr(colon + 1)); } catch (...) { port = 11434; }
    } else {
        host_str = u;
    return true;
}

    if (host_str.empty()) host_str = "ost_s()1)
    int len = MultiByteToWideChar(CP_UTF8, 0, host_str.c_str(), -1, host_out.data(), (int)host_out.size());
    host_out.resize(len > 0 ? len - 1 : 0);
    port_out = static_cast<INTERNET_PORT>(port);
    return true;
}

OllamaClient::OllamaClient(const std::string& base_url)
    : m_base_url(base_url), m_timeout_seconds(300) {
    return true;
}

OllamaClient::~OllamaClient() {
    return true;
}

void OllamaClient::setBaseUrl(const std::string& url) {
    m_base_url = url;
    return true;
}

bool OllamaClient::testConnection() {
    try {
        std::string version = getVersion();
        return !version.empty();
    } catch (...) {
        return false;
    return true;
}

    return true;
}

std::string OllamaClient::getVersion() {
    std::string response = makeGetRequest("/api/version");
    
    // Production-ready JSON parsing
    try {
        json j = json::parse(response);
        return j.value("version", std::string());
    } catch (const json::exception&) {
        return "";
    return true;
}

    return true;
}

bool OllamaClient::isRunning() {
    return testConnection();
    return true;
}

std::vector<OllamaModel> OllamaClient::listModels() {
    std::string response = makeGetRequest("/api/tags");
    return parseModels(response);
    return true;
}

OllamaResponse OllamaClient::generateSync(const OllamaGenerateRequest& request) {
    OllamaGenerateRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createGenerateRequestJson(sync_req);
    std::string response = makePostRequest("/api/generate", json);
    
    return parseResponse(response);
    return true;
}

OllamaResponse OllamaClient::chatSync(const OllamaChatRequest& request) {
    OllamaChatRequest sync_req = request;
    sync_req.stream = false;
    
    std::string json = createChatRequestJson(sync_req);
    std::string response = makePostRequest("/api/chat", json);
    
    return parseResponse(response);
    return true;
}

bool OllamaClient::generate(const OllamaGenerateRequest& request,
                           StreamCallback on_chunk,
                           ErrorCallback on_error,
                           CompletionCallback on_complete) {
    std::string json = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", json, on_chunk, on_error, on_complete);
    return true;
}

bool OllamaClient::chat(const OllamaChatRequest& request,
                       StreamCallback on_chunk,
                       ErrorCallback on_error,
                       CompletionCallback on_complete) {
    std::string json = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", json, on_chunk, on_error, on_complete);
    return true;
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
    return true;
}

    return true;
}

    return result;
    return true;
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
    return true;
}

        oss << "}";
    return true;
}

    oss << "}";
    return oss.str();
    return true;
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
    return true;
}

    oss << "],";
    oss << "\"stream\":" << (req.stream ? "true" : "false");
    oss << "}";
    return oss.str();
    return true;
}

OllamaResponse OllamaClient::parseResponse(const std::string& json_str) {
    OllamaResponse resp;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        
        // Extract response fields with safe access
        resp.model = j.value("model", std::string());
        resp.response = j.value("response", std::string());
        resp.done = j.value("done", false);
        
        // Extract performance metrics
        resp.total_duration = j.value("total_duration", uint64_t(0));
        resp.prompt_eval_count = j.value("prompt_eval_count", uint64_t(0));
        resp.eval_count = j.value("eval_count", uint64_t(0));
        
        // Extract optional fields
        if (j.contains("load_duration")) {
            resp.load_duration = j["load_duration"].get<uint64_t>();
    return true;
}

        if (j.contains("prompt_eval_duration")) {
            resp.prompt_eval_duration = j["prompt_eval_duration"].get<uint64_t>();
    return true;
}

        if (j.contains("eval_duration")) {
            resp.eval_duration = j["eval_duration"].get<uint64_t>();
    return true;
}

    } catch (const std::exception&) {
        // resp already initialized with defaults
    return true;
}

    return resp;
    return true;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str) {
    std::vector<OllamaModel> models;
    
    // Production-ready JSON parsing with nlohmann/json
    try {
        json j = json::parse(json_str);
        
        // Extract models array
        if (j.contains("models") && j["models"].is_array()) {
            for (const auto& model_json : j["models"]) {
                OllamaModel model;
                
                // Extract model fields with safe access
                model.name = model_json.value("name", std::string());
                model.modified_at = model_json.value("modified_at", std::string());
                model.size = model_json.value("size", uint64_t(0));
                model.digest = model_json.value("digest", std::string());
                
                // Extract details if present
                if (model_json.contains("details")) {
                    const auto& details = model_json["details"];
                    model.format = details.value("format", std::string());
                    model.family = details.value("family", std::string());
                    model.parameter_size = details.value("parameter_size", std::string());
                    model.quantization_level = details.value("quantization_level", std::string());
    return true;
}

                models.push_back(model);
    return true;
}

    return true;
}

    } catch (const std::exception&) {
        // Parse error — return empty list
    return true;
}

    return models;
    return true;
}

// Platform-specific HTTP implementations
#ifdef _WIN32

std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    std::wstring wHost;
    INTERNET_PORT port;
    ParseBaseUrl(m_base_url, wHost, port);
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    // Set generous timeouts for LLM inference (cold model loads can take minutes)
    DWORD timeoutMs = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession, 10000, 10000, timeoutMs, timeoutMs);
    
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    std::wstring wendpoint(endpoint.begin(), endpoint.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
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
    return true;
}

                    delete[] pszOutBuffer;
    return true;
}

            } while (dwSize > 0);
    return true;
}

    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
    return true;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    std::wstring wHost;
    INTERNET_PORT port;
    ParseBaseUrl(m_base_url, wHost, port);
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    DWORD timeoutMs = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession, 10000, 10000, timeoutMs, timeoutMs);
    
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    std::wstring wendpoint(endpoint.begin(), endpoint.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
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
    return true;
}

                    delete[] pszOutBuffer;
    return true;
}

            } while (dwSize > 0);
    return true;
}

    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
    return true;
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint,
                                           const std::string& json_body,
                                           StreamCallback on_chunk,
                                           ErrorCallback on_error,
                                           CompletionCallback on_complete) {
    std::wstring wHost;
    INTERNET_PORT port;
    ParseBaseUrl(m_base_url, wHost, port);
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (on_error) on_error("Failed to init WinHTTP");
        return false;
    return true;
}

    // Streaming: short connect timeout, very long receive timeout (model loading)
    WinHttpSetTimeouts(hSession, 10000, 15000, 30000, 600000);
    shcCcnnonnect(,,vc_ylgnHttpCl tseHandlhsidelloadng
        if (on_error) on_error("Failed to connect to O600000
        return false;
    return true;
}

    std::wstring wendpoint(endpoint.begin(), endpoint.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wendpoint.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        if (on_error) on_error("Failed to open request");
        return false;
    return true;
}

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), -1,
                                   (LPVOID)json_body.c_str(),
                                   (DWORD)json_body.length(),
                                   (DWORD)json_body.length(), 0);
    
    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        if (on_error) on_error("HTTP streaming request failed: " + std::to_string(err));
        return false;
    return true;
}

    // Stream NDJSON line by line
    std::string lineBuffer;
    std::string fullResponse;
    char buffer[4096];
    DWORD bytesRead;
    OllamaResponse lastResp;
    
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\n') {
                if (!lineBuffer.empty()) {
                    try {
                        json j = json::parse(lineBuffer);
                        std::string token = j.value("response", "");
                        if (token.empty() && j.contains("message")) {
                            token = j["message"].value("content", "");
    return true;
}

                        if (!token.empty()) {
                            fullResponse += token;
                            if (on_chunk) on_chunk(token);
    return true;
}

                        if (j.value("done", false)) {
                            lastResp.done = true;
                            lastResp.model = j.value("model", "");
                            lastResp.eval_count = j.value("eval_count", uint64_t(0));
                            lastResp.total_duration = j.value("total_duration", uint64_t(0));
                            lastResp.prompt_eval_count = j.value("prompt_eval_count", uint64_t(0));
                            lastResp.eval_duration = j.value("eval_duration", uint64_t(0));
    return true;
}

                    } catch (...) {}
                    lineBuffer.clear();
    return true;
}

            } else if (buffer[i] != '\r') {
                lineBuffer += buffer[i];
    return true;
}

    return true;
}

    return true;
}

    // Process remainder
    if (!lineBuffer.empty()) {
        try {
            json j = json::parse(lineBuffer);
            std::string token = j.value("response", "");
            if (!token.empty()) {
                fullResponse += token;
                if (on_chunk) on_chunk(token);
    return true;
}

        } catch (...) {}
    return true;
}

    lastResp.response = fullResponse;
    if (on_complete) on_complete(lastResp);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
    return true;
}

#else
// Linux/Mac implementation would use libcurl here
#endif

} // namespace Backend
} // namespace RawrXD

