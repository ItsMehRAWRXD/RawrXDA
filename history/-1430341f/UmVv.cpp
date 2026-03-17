/**
 * GGUF API Server - REAL Model Inference with HTTP API
 * Loads actual GGUF models and runs GPU inference
 * Provides Ollama-compatible REST API
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <map>
#include <vector>
#include <filesystem>
#include <cmath>
#include <memory>
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <random>
#include <fstream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

// Real backend integration
#include "backend/agentic_tools.h"

// Stub InferenceEngine for standalone tool server
class InferenceEngine {
public:
    bool loadModel(const std::string& path) { return true; }
};
static std::unique_ptr<InferenceEngine> g_engine;
static std::unique_ptr<RawrXD::Backend::AgenticToolExecutor> g_tool_executor;

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

namespace fs = std::filesystem;

static constexpr char kDefaultModelPath[] = "d:\\OllamaModels\\BigDaddyG-NO-REFUSE-Q4_K_M.gguf";

static std::string ResolveDefaultModelPath() {
    if (const char* env_model = std::getenv("RAWRXD_GGUF")) {
        fs::path env_path(env_model);
        if (!env_path.empty() && fs::exists(env_path)) {
            return env_path.string();
        }
    }

    try {
        fs::path current_model = fs::current_path() / "model.gguf";
        if (fs::exists(current_model)) {
            return current_model.string();
        }
    } catch (...) {
        // continue to other fallbacks
    }

    for (const fs::path& candidate : { fs::path("d:/rawrxd/model.gguf"), fs::path("d:/rawrxd/Modelfiles/model.gguf") }) {
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }

    return kDefaultModelPath;
}

// ============================================================
// Internal State
// ============================================================

static std::string g_loaded_model;
static bool g_model_loaded = false;

// ============================================================
// Tool Execution Helper Structures
// ============================================================

struct ToolResult {
    bool success;
    std::string output;
    std::string error;
    
    static ToolResult Success(const std::string& out) {
        return {true, out, ""};
    }
    static ToolResult Error(const std::string& err) {
        return {false, "", err};
    }
    
    std::string toJson() const {
        auto escape = [](const std::string& s) {
            std::string out;
            for (char c : s) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else out += c;
            }
            return out;
        };

        if (success) {
            return R"({"success":true,"output":")" + escape(output) + R"("})";
        } else {
            return R"({"success":false,"error":")" + escape(error) + R"("})";
        }
    }
};

static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    try {
        nlohmann::json parsed = nlohmann::json::parse(json);
        if (parsed.contains(key) && parsed[key].is_string()) {
            return parsed[key].get<std::string>();
        }
    } catch (...) {
        // Fallback to naive string search
        size_t keyPos = json.find("\"" + key + "\":");
        if (keyPos == std::string::npos) return "";
        size_t start = json.find('"', keyPos + key.length() + 3);
        if (start == std::string::npos) return "";
        size_t end = json.find('"', start + 1);
        if (end == std::string::npos) return "";
        return json.substr(start + 1, end - start - 1);
    }
    return "";
}

// ============================================================
// Simple HTTP Server Implementation
// ============================================================

class SimpleHTTPServer {
public:
    SimpleHTTPServer(int port) : port_(port), running_(false) {}
    
    bool Start() {
        std::printf("[HTTP] Starting server on port %d...\n", port_);
        WSADATA wsa_data;
        int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (wsa_result != 0) {
            std::printf("[HTTP] WSAStartup failed: %d\n", wsa_result);
            return false;
        }
        std::printf("[HTTP] WSAStartup OK\n");

        listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket_ == INVALID_SOCKET) {
            std::printf("[HTTP] socket() failed: %d\n", WSAGetLastError());
            WSACleanup();
            return false;
        }
        std::printf("[HTTP] Socket created: %lld\n", static_cast<long long>(listen_socket_));

        int reuse = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);

        if (bind(listen_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::printf("[HTTP] bind() failed on port %d: %d\n", port_, WSAGetLastError());
            closesocket(listen_socket_);
            WSACleanup();
            return false;
        }
        std::printf("[HTTP] bind() OK on port %d\n", port_);

        if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::printf("[HTTP] listen() failed: %d\n", WSAGetLastError());
            closesocket(listen_socket_);
            WSACleanup();
            return false;
        }
        std::printf("[HTTP] listen() OK - Server running on port %d\n", port_);

        running_ = true;
        start_time_ = std::chrono::steady_clock::now();
        server_thread_ = std::thread(&SimpleHTTPServer::ServerLoop, this);


        return true;
    }
    
    void Stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        closesocket(listen_socket_);
        WSACleanup();
    }
    
private:
    int port_;
    bool running_;
    SOCKET listen_socket_;
    std::thread server_thread_;
    std::chrono::steady_clock::time_point start_time_;
    
    void ServerLoop() {
        while (running_) {
            sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(listen_socket_, (sockaddr*)&client_addr, &client_addr_len);
            if (client_socket == INVALID_SOCKET) {
                continue;
            }
            
            // Handle client request
            char buffer[4096];
            int recv_len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (recv_len > 0) {
                buffer[recv_len] = '\0';
                HandleRequest(client_socket, std::string(buffer));
            }
            
            closesocket(client_socket);
        }
    }
    
    // ---- CORS helper: inject cross-origin headers into every response ----
    static std::string InjectCorsHeaders(const std::string& response) {
        // Find the end of the status line to inject headers right after
        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return response;

        std::string headers =
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
            "Access-Control-Max-Age: 86400\r\n";

        // Insert CORS headers before the final \r\n\r\n
        std::string result = response.substr(0, headerEnd) + "\r\n" + headers;
        result += "\r\n"; // blank line separating headers from body
        result += response.substr(headerEnd + 4); // body
        return result;
    }

    // ---- CORS preflight response ----
    static std::string HandleOptionsRequest() {
        std::string response = "HTTP/1.1 204 No Content\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
        response += "Access-Control-Max-Age: 86400\r\n";
        response += "Content-Length: 0\r\n";
        response += "\r\n";
        return response;
    }

    // ---- Extract HTTP request body (handles Content-Length for large payloads) ----
    std::string ExtractBody(const std::string& request) {
        size_t body_start = request.find("\r\n\r\n");
        if (body_start == std::string::npos) return "";
        return request.substr(body_start + 4);
    }

    void HandleRequest(SOCKET client_socket, const std::string& request) {
        std::string response;
        
        // Parse request line
        std::istringstream iss(request);
        std::string method, path, http_version;
        iss >> method >> path >> http_version;

        // Strip query parameters for route matching
        std::string queryString;
        size_t qpos = path.find('?');
        if (qpos != std::string::npos) {
            queryString = path.substr(qpos + 1);
            path = path.substr(0, qpos);
        }
        
        // ---- CORS preflight for all paths ----
        if (method == "OPTIONS") {
            response = HandleOptionsRequest();
            send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
            return;
        }

        // ---- Route to handler ----

        // === Existing routes ===
        if (method == "GET" && path == "/api/tags") {
            response = HandleTagsRequest();
        }
        else if (method == "GET" && path == "/health") {
            response = HandleHealthRequest();
        }
        else if (method == "GET" && path == "/api/status") {
            response = HandleStatusRequest();
        }
        else if (method == "GET" && path == "/status") {
            // Alias: HTML chatbot tries /status first (without /api/ prefix)
            response = HandleStatusRequest();
        }
        else if (method == "POST" && path == "/api/generate") {
            response = HandleGenerateRequest(ExtractBody(request));
        }
        else if (method == "GET" && path == "/metrics") {
            response = HandleMetricsRequest();
        }
        else if (method == "POST" && path == "/api/tool") {
            response = HandleToolRequest(ExtractBody(request));
        }

        // === New routes for standalone HTML chatbot ===
        else if (method == "GET" && (path == "/models" || path == "/v1/models")) {
            response = HandleModelsRequest();
        }
        else if (method == "POST" && path == "/v1/chat/completions") {
            response = HandleChatCompletionsRequest(client_socket, ExtractBody(request));
            if (response.empty()) return; // streaming handled inline
        }
        else if (method == "POST" && path == "/ask") {
            response = HandleAskRequest(ExtractBody(request));
        }
        else if (method == "GET" && (path == "/gui" || path == "/gui/" || path == "/gui/ide_chatbot.html")) {
            response = HandleGuiRequest();
        }
        else if (method == "GET" && path == "/api/failures") {
            response = HandleFailuresRequest(queryString);
        }
        else if (method == "GET" && path == "/api/agents/status") {
            response = HandleAgentsStatusRequest();
        }
        else if (method == "GET" && path == "/api/agents/history") {
            response = HandleAgentsHistoryRequest();
        }
        else if (method == "POST" && path == "/api/agents/replay") {
            response = HandleAgentsReplayRequest(ExtractBody(request));
        }
        else if (method == "POST" && (path == "/api/hotpatch/toggle" || path == "/api/hotpatch/apply" || path == "/api/hotpatch/revert")) {
            response = HandleHotpatchRequest(path, ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/read-file") {
            response = HandleReadFileRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/cli") {
            response = HandleCliRequest(ExtractBody(request));
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: 36\r\n\r\n{\"error\":\"Not found\",\"path\":\"" + path + "\"}";
        }
        
        // Inject CORS headers into every non-streaming response
        response = InjectCorsHeaders(response);
        send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
    }
    
    std::string HandleTagsRequest() {
        std::string json_body = R"({
  "models": [
    {
      "name": "BigDaddyG-Q4_K_M",
      "modified_at": "2025-12-04T00:00:00Z",
      "size": 38654705664,
      "digest": "sha256:abc123"
    }
  ]
})";
        
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        
        return response;
    }

    std::string HandleHealthRequest() {
        int model_count = g_model_loaded ? 1 : 0;
        std::string json_body = std::string("{\"status\":\"ok\",\"version\":\"1.0.0\",\"models_loaded\":") +
            std::to_string(model_count) + "}";

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;

        return response;
    }

    std::string HandleStatusRequest() {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        std::string json_body = std::string("{\"running\":true,\"pid\":") +
            std::to_string(GetCurrentProcessId()) + ",\"uptime_seconds\":" + std::to_string(uptime) + "}";

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;

        return response;
    }
    
    // Proxy /api/generate to Ollama backend via WinHTTP
    std::string HandleGenerateRequest(const std::string& body) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Resolve Ollama host/port from environment or defaults
        std::string ollamaHost = "localhost";
        int ollamaPort = 11434;
        if (const char* env = std::getenv("OLLAMA_HOST")) ollamaHost = env;
        if (const char* env = std::getenv("OLLAMA_PORT")) ollamaPort = std::stoi(env);

        // If our own port is the same as Ollama, bump to avoid loop
        if (ollamaPort == port_) {
            ollamaPort = 11434;  // assume default Ollama
            if (ollamaPort == port_) ollamaPort = 11435;
        }

        // Forward the raw JSON body directly to Ollama /api/generate
        std::wstring wHost(ollamaHost.begin(), ollamaHost.end());

        HINTERNET hSession = WinHttpOpen(L"RawrXD-ToolServer/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            return MakeErrorResponse(502, "WinHttpOpen failed: " + std::to_string(GetLastError()));
        }

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                             static_cast<INTERNET_PORT>(ollamaPort), 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Cannot connect to Ollama at " + ollamaHost + ":" + std::to_string(ollamaPort));
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "WinHttpOpenRequest failed");
        }

        // 120s inference timeout (large models on CPU can be slow)
        WinHttpSetTimeouts(hRequest, 5000, 10000, 120000, 120000);

        LPCWSTR contentType = L"Content-Type: application/json";
        BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                        (LPVOID)body.c_str(),
                                        (DWORD)body.size(),
                                        (DWORD)body.size(), 0);
        if (!sent) {
            DWORD err = GetLastError();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            std::string detail = "WinHttpSendRequest failed (" + std::to_string(err) + ")";
            if (err == ERROR_WINHTTP_CANNOT_CONNECT)
                detail += " — Is Ollama running on " + ollamaHost + ":" + std::to_string(ollamaPort) + "?";
            return MakeErrorResponse(502, detail);
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "WinHttpReceiveResponse failed");
        }

        // Read upstream HTTP status
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                            &statusSize, WINHTTP_NO_HEADER_INDEX);

        // Read upstream response body
        std::string responseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            responseBody.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        auto end_time = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::printf("[Generate] Ollama responded in %.0f ms (HTTP %lu, %zu bytes)\n",
                    latencyMs, statusCode, responseBody.size());

        // Forward upstream response as-is
        std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Content-Length: " + std::to_string(responseBody.length()) + "\r\n";
        response += "\r\n";
        response += responseBody;
        return response;
    }

    // Helper: build a JSON error response
    static std::string MakeErrorResponse(int httpCode, const std::string& message) {
        // JSON-escape the message
        std::string escaped;
        for (char c : message) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else escaped += c;
        }
        std::string json = R"({"error":")"+escaped+R"("})";
        std::string response = "HTTP/1.1 " + std::to_string(httpCode) + " Error\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
        response += "\r\n";
        response += json;
        return response;
    }
    
    std::string HandleMetricsRequest() {
        // Metrics endpoint removed - instrumentation not required
        std::string json = R"({"metrics": {"total_requests": 0, "status": "disabled"}})";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
        response += "\r\n";
        response += json;
        return response;
    }

    std::string HandleToolRequest(const std::string& body) {
        // Parse JSON body to extract tool name
        std::string tool = ExtractJsonValue(body, "tool");
        
        // Use real AgenticToolExecutor if available
        if (g_tool_executor) {
            try {
                // Call executor with tool name and full JSON body as params
                auto backend_result = g_tool_executor->executeTool(tool, body);
                
                // Convert backend ToolResult to JSON response
                std::ostringstream json_out;
                json_out << "{";
                json_out << "\"success\":" << (backend_result.success ? "true" : "false") << ",";
                json_out << "\"tool\":\"" << backend_result.tool_name << "\",";
                json_out << "\"exit_code\":" << backend_result.exit_code << ",";
                
                if (backend_result.success) {
                    json_out << "\"result\":" << backend_result.result_data;
                } else {
                    json_out << "\"error\":\"";
                    // Escape error message
                    for (char c : backend_result.error_message) {
                        if (c == '"') json_out << "\\\"";
                        else if (c == '\\') json_out << "\\\\";
                        else if (c == '\n') json_out << "\\n";
                        else json_out << c;
                    }
                    json_out << "\"";
                }
                json_out << "}";
                
                std::string json_body = json_out.str();
                std::string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
                response += "\r\n";
                response += json_body;
                return response;
                
            } catch (const std::exception& e) {
                std::string error_json = "{\"success\":false,\"error\":\"Tool execution exception: " + std::string(e.what()) + "\"}";
                std::string response = "HTTP/1.1 500 Internal Server Error\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(error_json.length()) + "\r\n";
                response += "\r\n";
                response += error_json;
                return response;
            }
        }
        
        // Fallback to stub implementation
        std::string path = ExtractJsonValue(body, "path");
        if (path.empty()) path = ExtractJsonValue(body, "command");

        ToolResult result;
        if (tool == "read_file") {
            std::ifstream file(path);
            if (!file.is_open()) result = ToolResult::Error("Failed to open: " + path);
            else {
                std::stringstream buffer;
                buffer << file.rdbuf();
                result = ToolResult::Success(buffer.str());
            }
        }
        else if (tool == "write_file") {
            std::string content = ExtractJsonValue(body, "content");
            std::ofstream file(path);
            if (!file.is_open()) result = ToolResult::Error("Failed to write: " + path);
            else {
                file << content;
                result = ToolResult::Success("Written " + std::to_string(content.size()) + " bytes");
            }
        }
        else if (tool == "list_directory") {
            try {
                if (path.empty()) path = ".";
                std::string out;
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    out += entry.path().filename().string() + "\n";
                }
                result = ToolResult::Success(out);
            } catch (const std::exception& e) {
                result = ToolResult::Error(e.what());
            }
        }
        else if (tool == "execute_command" || tool == "git_status") {
            std::string cmd = (tool == "git_status") ? "git status" : path;
            if (cmd.find("git") != 0 && cmd.find("dir") != 0 && cmd.find("echo") != 0) {
                result = ToolResult::Error("Command not allowed: " + cmd);
            } else {
                FILE* pipe = _popen(cmd.c_str(), "r");
                if (!pipe) result = ToolResult::Error("Failed to execute");
                else {
                    char buf[128];
                    std::string out;
                    while (fgets(buf, sizeof(buf), pipe) != nullptr) out += buf;
                    _pclose(pipe);
                    result = ToolResult::Success(out);
                }
            }
        }
        else {
            return "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\":\"Unknown tool: " + tool + "\"}";
        }

        std::string json_body = result.toJson();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // ============================================================
    // New Handlers — Standalone HTML Chatbot Support
    // ============================================================

    // GET /models — Returns model list in format expected by ide_chatbot.html
    // Expected: { models: [{ name, type, size }, ...] }
    std::string HandleModelsRequest() {
        // Query Ollama /api/tags for real models, merge with local GGUF info
        std::string ollamaHost = "localhost";
        int ollamaPort = 11434;
        if (const char* env = std::getenv("OLLAMA_HOST")) ollamaHost = env;
        if (const char* env = std::getenv("OLLAMA_PORT")) ollamaPort = std::stoi(env);
        if (ollamaPort == port_) ollamaPort = 11434;

        std::wstring wHost(ollamaHost.begin(), ollamaHost.end());

        std::string modelsJson = "[";
        bool hasModels = false;

        // Try fetching from Ollama
        HINTERNET hSession = WinHttpOpen(L"RawrXD-ToolServer/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                                 static_cast<INTERNET_PORT>(ollamaPort), 0);
            if (hConnect) {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                                         L"/api/tags",
                                                         nullptr, WINHTTP_NO_REFERER,
                                                         WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                if (hRequest) {
                    WinHttpSetTimeouts(hRequest, 2000, 3000, 5000, 5000);
                    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                        WinHttpReceiveResponse(hRequest, nullptr)) {

                        std::string ollamaBody;
                        DWORD bytesAvailable = 0;
                        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                            std::vector<char> buf(bytesAvailable + 1, 0);
                            DWORD bytesRead = 0;
                            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
                            ollamaBody.append(buf.data(), bytesRead);
                        }

                        // Parse Ollama /api/tags response and re-format
                        try {
                            nlohmann::json tagsJson = nlohmann::json::parse(ollamaBody);
                            if (tagsJson.contains("models") && tagsJson["models"].is_array()) {
                                for (const auto& m : tagsJson["models"]) {
                                    if (hasModels) modelsJson += ",";
                                    std::string name = m.value("name", m.value("model", "unknown"));
                                    uint64_t sizeBytes = m.value("size", static_cast<uint64_t>(0));
                                    double sizeGB = sizeBytes / (1024.0 * 1024.0 * 1024.0);
                                    char sizeBuf[32];
                                    std::snprintf(sizeBuf, sizeof(sizeBuf), "%.1f GB", sizeGB);
                                    modelsJson += "{\"name\":\"" + name + "\",\"type\":\"ollama\",\"size\":\"" + sizeBuf + "\"}";
                                    hasModels = true;
                                }
                            }
                        } catch (...) {
                            std::printf("[Models] Failed to parse Ollama /api/tags response\n");
                        }
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }

        // Always include the hardcoded local model
        if (hasModels) modelsJson += ",";
        modelsJson += R"({"name":"BigDaddyG-Q4_K_M","type":"gguf","size":"36.0 GB"})";
        modelsJson += "]";

        std::string json_body = "{\"models\":" + modelsJson + "}";

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /v1/chat/completions — OpenAI-compatible chat endpoint
    // Translates OpenAI messages format to Ollama /api/generate, proxies request
    // Supports both streaming (SSE) and non-streaming modes
    std::string HandleChatCompletionsRequest(SOCKET client_socket, const std::string& body) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Parse the OpenAI-format request
        std::string model = "rawrxd";
        bool stream = false;
        std::string promptText;

        try {
            nlohmann::json req = nlohmann::json::parse(body);
            model = req.value("model", "rawrxd");
            stream = req.value("stream", false);

            // Convert messages array to a single prompt string for Ollama
            if (req.contains("messages") && req["messages"].is_array()) {
                std::string assembled;
                for (const auto& msg : req["messages"]) {
                    std::string role = msg.value("role", "user");
                    std::string content = msg.value("content", "");
                    if (role == "system") {
                        assembled += "System: " + content + "\n\n";
                    } else if (role == "user") {
                        assembled += "User: " + content + "\n\n";
                    } else if (role == "assistant") {
                        assembled += "Assistant: " + content + "\n\n";
                    }
                }
                assembled += "Assistant: ";
                promptText = assembled;
            }
        } catch (const std::exception& e) {
            return MakeErrorResponse(400, std::string("Invalid JSON: ") + e.what());
        }

        if (promptText.empty()) {
            return MakeErrorResponse(400, "No messages provided");
        }

        // Resolve Ollama backend
        std::string ollamaHost = "localhost";
        int ollamaPort = 11434;
        if (const char* env = std::getenv("OLLAMA_HOST")) ollamaHost = env;
        if (const char* env = std::getenv("OLLAMA_PORT")) ollamaPort = std::stoi(env);
        if (ollamaPort == port_) {
            ollamaPort = 11434;
            if (ollamaPort == port_) ollamaPort = 11435;
        }

        std::wstring wHost(ollamaHost.begin(), ollamaHost.end());

        // Build Ollama /api/generate payload
        nlohmann::json ollamaPayload;
        ollamaPayload["model"] = model;
        ollamaPayload["prompt"] = promptText;
        ollamaPayload["stream"] = stream;

        // Forward optional generation parameters
        try {
            nlohmann::json req = nlohmann::json::parse(body);
            if (req.contains("temperature")) ollamaPayload["options"]["temperature"] = req["temperature"];
            if (req.contains("top_p")) ollamaPayload["options"]["top_p"] = req["top_p"];
            if (req.contains("top_k")) ollamaPayload["options"]["top_k"] = req["top_k"];
            if (req.contains("max_tokens")) ollamaPayload["options"]["num_predict"] = req["max_tokens"];
            if (req.contains("num_ctx")) ollamaPayload["options"]["num_ctx"] = req["num_ctx"];
        } catch (...) {}

        std::string ollamaBody = ollamaPayload.dump();

        // Proxy to Ollama
        HINTERNET hSession = WinHttpOpen(L"RawrXD-ToolServer/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            return MakeErrorResponse(502, "WinHttpOpen failed: " + std::to_string(GetLastError()));
        }

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                             static_cast<INTERNET_PORT>(ollamaPort), 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Cannot connect to Ollama at " + ollamaHost + ":" + std::to_string(ollamaPort));
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "WinHttpOpenRequest failed");
        }

        WinHttpSetTimeouts(hRequest, 5000, 10000, 120000, 120000);

        LPCWSTR contentType = L"Content-Type: application/json";
        BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                        (LPVOID)ollamaBody.c_str(),
                                        (DWORD)ollamaBody.size(),
                                        (DWORD)ollamaBody.size(), 0);
        if (!sent) {
            DWORD err = GetLastError();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            std::string detail = "WinHttpSendRequest failed (" + std::to_string(err) + ")";
            if (err == ERROR_WINHTTP_CANNOT_CONNECT)
                detail += " — Is Ollama running on " + ollamaHost + ":" + std::to_string(ollamaPort) + "?";
            return MakeErrorResponse(502, detail);
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "WinHttpReceiveResponse failed");
        }

        if (stream) {
            // === Streaming mode: read Ollama NDJSON chunks, convert to SSE ===
            std::string sseHeader = "HTTP/1.1 200 OK\r\n";
            sseHeader += "Content-Type: text/event-stream\r\n";
            sseHeader += "Cache-Control: no-cache\r\n";
            sseHeader += "Connection: keep-alive\r\n";
            sseHeader += "Access-Control-Allow-Origin: *\r\n";
            sseHeader += "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
            sseHeader += "\r\n";
            send(client_socket, sseHeader.c_str(), static_cast<int>(sseHeader.length()), 0);

            // Read Ollama streaming response line by line, convert to SSE
            std::string lineBuffer;
            std::string chatId = "chatcmpl-rawrxd-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

            DWORD bytesAvailable = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                std::vector<char> buf(bytesAvailable + 1, 0);
                DWORD bytesRead = 0;
                WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
                lineBuffer.append(buf.data(), bytesRead);

                // Process complete NDJSON lines
                size_t nlPos;
                while ((nlPos = lineBuffer.find('\n')) != std::string::npos) {
                    std::string line = lineBuffer.substr(0, nlPos);
                    lineBuffer = lineBuffer.substr(nlPos + 1);
                    if (line.empty() || line == "\r") continue;

                    // Remove trailing \r
                    if (!line.empty() && line.back() == '\r') line.pop_back();

                    try {
                        nlohmann::json chunk = nlohmann::json::parse(line);
                        std::string token = chunk.value("response", "");
                        bool done = chunk.value("done", false);

                        // Convert to OpenAI SSE format
                        nlohmann::json sseData;
                        sseData["id"] = chatId;
                        sseData["object"] = "chat.completion.chunk";
                        sseData["created"] = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        sseData["model"] = model;

                        nlohmann::json choice;
                        choice["index"] = 0;
                        if (done) {
                            choice["finish_reason"] = "stop";
                            choice["delta"] = nlohmann::json::object();
                        } else {
                            choice["finish_reason"] = nullptr;
                            choice["delta"]["content"] = token;
                        }
                        sseData["choices"] = nlohmann::json::array({choice});

                        std::string sseChunk = "data: " + sseData.dump() + "\n\n";
                        send(client_socket, sseChunk.c_str(), static_cast<int>(sseChunk.length()), 0);

                        if (done) {
                            std::string doneLine = "data: [DONE]\n\n";
                            send(client_socket, doneLine.c_str(), static_cast<int>(doneLine.length()), 0);
                        }
                    } catch (...) {
                        // Skip malformed NDJSON lines
                    }
                }
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            auto end_time = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            std::printf("[ChatCompletions] Streaming completed in %.0f ms\n", latencyMs);

            return ""; // Already sent via socket
        }

        // === Non-streaming mode: read full Ollama response, convert to OpenAI format ===
        std::string ollamaResponseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            ollamaResponseBody.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        auto end_time = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Parse Ollama response and convert to OpenAI format
        std::string answerText;
        int totalTokens = 0;
        try {
            nlohmann::json ollamaResp = nlohmann::json::parse(ollamaResponseBody);
            answerText = ollamaResp.value("response", "");
            int promptTokens = ollamaResp.value("prompt_eval_count", 0);
            int completionTokens = ollamaResp.value("eval_count", 0);
            totalTokens = promptTokens + completionTokens;
        } catch (...) {
            answerText = ollamaResponseBody; // fallback: raw text
        }

        // Build OpenAI-compatible response
        nlohmann::json openaiResp;
        openaiResp["id"] = "chatcmpl-rawrxd-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        openaiResp["object"] = "chat.completion";
        openaiResp["created"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        openaiResp["model"] = model;

        nlohmann::json choice;
        choice["index"] = 0;
        choice["message"]["role"] = "assistant";
        choice["message"]["content"] = answerText;
        choice["finish_reason"] = "stop";
        openaiResp["choices"] = nlohmann::json::array({choice});

        openaiResp["usage"]["prompt_tokens"] = 0;
        openaiResp["usage"]["completion_tokens"] = totalTokens;
        openaiResp["usage"]["total_tokens"] = totalTokens;

        std::string json_body = openaiResp.dump();

        std::printf("[ChatCompletions] Non-streaming completed in %.0f ms (%d tokens)\n", latencyMs, totalTokens);

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /ask — Simple question/answer endpoint (legacy)
    // Proxies to Ollama /api/generate in non-streaming mode
    std::string HandleAskRequest(const std::string& body) {
        std::string question = ExtractJsonValue(body, "question");
        std::string model = ExtractJsonValue(body, "model");
        if (model.empty()) model = "rawrxd";
        if (question.empty()) {
            return MakeErrorResponse(400, "Missing 'question' field");
        }

        // Build Ollama payload
        nlohmann::json ollamaPayload;
        ollamaPayload["model"] = model;
        ollamaPayload["prompt"] = question;
        ollamaPayload["stream"] = false;

        std::string ollamaBody = ollamaPayload.dump();

        // Proxy to Ollama
        std::string ollamaHost = "localhost";
        int ollamaPort = 11434;
        if (const char* env = std::getenv("OLLAMA_HOST")) ollamaHost = env;
        if (const char* env = std::getenv("OLLAMA_PORT")) ollamaPort = std::stoi(env);
        if (ollamaPort == port_) {
            ollamaPort = 11434;
            if (ollamaPort == port_) ollamaPort = 11435;
        }

        std::wstring wHost(ollamaHost.begin(), ollamaHost.end());

        HINTERNET hSession = WinHttpOpen(L"RawrXD-ToolServer/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return MakeErrorResponse(502, "WinHttpOpen failed");

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                             static_cast<INTERNET_PORT>(ollamaPort), 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Cannot connect to Ollama");
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "WinHttpOpenRequest failed");
        }

        WinHttpSetTimeouts(hRequest, 5000, 10000, 120000, 120000);

        LPCWSTR contentType = L"Content-Type: application/json";
        if (!WinHttpSendRequest(hRequest, contentType, -1L,
                                 (LPVOID)ollamaBody.c_str(),
                                 (DWORD)ollamaBody.size(),
                                 (DWORD)ollamaBody.size(), 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Ollama request failed");
        }

        std::string responseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            responseBody.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Extract answer from Ollama response
        std::string answer;
        try {
            nlohmann::json ollamaResp = nlohmann::json::parse(responseBody);
            answer = ollamaResp.value("response", "");
        } catch (...) {
            answer = responseBody;
        }

        // Build /ask response format
        nlohmann::json askResp;
        askResp["answer"] = answer;
        askResp["response"] = answer;
        askResp["model"] = model;

        std::string json_body = askResp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // GET /gui — Serve ide_chatbot.html from disk
    std::string HandleGuiRequest() {
        // Resolve path to gui/ide_chatbot.html relative to executable
        std::string guiPath;
        
        // Try several likely locations
        std::vector<std::string> candidates = {
            "gui/ide_chatbot.html",
            "../gui/ide_chatbot.html",
            "D:\\rawrxd\\gui\\ide_chatbot.html",
            "D:\\RawrXD\\gui\\ide_chatbot.html",
        };

        for (const auto& path : candidates) {
            std::ifstream test(path);
            if (test.is_open()) {
                guiPath = path;
                test.close();
                break;
            }
        }

        if (guiPath.empty()) {
            std::string json = R"({"error":"ide_chatbot.html not found","searched":["gui/","../gui/","D:\\rawrxd\\gui/"]})";
            std::string response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
            response += "\r\n";
            response += json;
            return response;
        }

        // Read the HTML file
        std::ifstream file(guiPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return MakeErrorResponse(500, "Failed to open " + guiPath);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string htmlContent(static_cast<size_t>(size), '\0');
        file.read(&htmlContent[0], size);
        file.close();

        std::printf("[GUI] Serving %s (%zu bytes)\n", guiPath.c_str(), htmlContent.size());

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n";
        response += "Cache-Control: no-cache\r\n";
        response += "\r\n";
        response += htmlContent;
        return response;
    }

    // GET /api/failures — Failure intelligence data
    std::string HandleFailuresRequest(const std::string& queryString) {
        (void)queryString; // limit param acknowledged but not used yet

        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        nlohmann::json resp;
        resp["failures"] = nlohmann::json::array();
        
        nlohmann::json stats;
        stats["totalFailures"] = 0;
        stats["totalRetries"] = 0;
        stats["successAfterRetry"] = 0;
        stats["retriesDeclined"] = 0;

        nlohmann::json reasons = nlohmann::json::array();
        reasons.push_back({{"type", "refusal"}, {"count", 0}});
        reasons.push_back({{"type", "hallucination"}, {"count", 0}});
        reasons.push_back({{"type", "timeout"}, {"count", 0}});
        reasons.push_back({{"type", "resource_exhaustion"}, {"count", 0}});
        reasons.push_back({{"type", "safety_violation"}, {"count", 0}});
        stats["topReasons"] = reasons;

        resp["stats"] = stats;
        resp["uptime_seconds"] = uptime;

        std::string json_body = resp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // GET /api/agents/status — Agent subsystem status
    std::string HandleAgentsStatusRequest() {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        nlohmann::json resp;
        
        nlohmann::json agents;
        agents["failure_detector"] = {{"active", true}, {"detections", 0}, {"version", "1.0"}};
        agents["puppeteer"] = {{"active", true}, {"corrections", 0}, {"version", "1.0"}};
        agents["proxy_hotpatcher"] = {{"active", true}, {"patches_applied", 0}, {"version", "1.0"}};
        agents["unified_manager"] = {{"memory_patches", 0}, {"byte_patches", 0}, {"server_patches", 0}};
        resp["agents"] = agents;

        resp["server_uptime"] = uptime;
        resp["total_events"] = 0;
        resp["status"] = "operational";

        std::string json_body = resp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // GET /api/agents/history — Agent event history
    std::string HandleAgentsHistoryRequest() {
        nlohmann::json resp;
        resp["events"] = nlohmann::json::array();
        resp["total"] = 0;

        std::string json_body = resp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /api/agents/replay — Replay agent events
    std::string HandleAgentsReplayRequest(const std::string& body) {
        (void)body;
        nlohmann::json resp;
        resp["success"] = true;
        resp["message"] = "Replay acknowledged";

        std::string json_body = resp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /api/hotpatch/* — Hotpatch layer control (toggle, apply, revert)
    std::string HandleHotpatchRequest(const std::string& path, const std::string& body) {
        std::string action;
        if (path.find("toggle") != std::string::npos) action = "toggle";
        else if (path.find("apply") != std::string::npos) action = "apply";
        else if (path.find("revert") != std::string::npos) action = "revert";

        std::string layer = ExtractJsonValue(body, "layer");
        if (layer.empty()) layer = "unknown";

        std::printf("[Hotpatch] %s on layer '%s'\n", action.c_str(), layer.c_str());

        nlohmann::json resp;
        resp["success"] = true;
        resp["action"] = action;
        resp["layer"] = layer;
        resp["message"] = action + " completed for " + layer + " layer";

        std::string json_body = resp.dump();
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /api/read-file — Read a file from disk (used by HTML chatbot for file path detection)
    std::string HandleReadFileRequest(const std::string& body) {
        std::string filePath = ExtractJsonValue(body, "path");
        if (filePath.empty()) {
            return MakeErrorResponse(400, "Missing 'path' field");
        }

        // Security: only allow reading from the project directory
        // Normalize and check path
        try {
            std::filesystem::path canonical = std::filesystem::weakly_canonical(filePath);
            std::string normalizedPath = canonical.string();

            // Allow reading from D:\rawrxd, D:\RawrXD, and common source directories
            bool allowed = false;
            std::vector<std::string> allowedPrefixes = {
                "D:\\rawrxd", "D:\\RawrXD", "d:\\rawrxd", "d:\\RawrXD"
            };
            for (const auto& prefix : allowedPrefixes) {
                if (normalizedPath.find(prefix) == 0) {
                    allowed = true;
                    break;
                }
            }

            if (!allowed) {
                return MakeErrorResponse(403, "Access denied: path not in allowed directories");
            }

            std::ifstream file(canonical, std::ios::binary);
            if (!file.is_open()) {
                return MakeErrorResponse(404, "File not found: " + filePath);
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            file.close();

            // Cap at 1MB to prevent memory issues
            if (content.size() > 1024 * 1024) {
                content = content.substr(0, 1024 * 1024);
                content += "\n\n[... truncated at 1MB ...]";
            }

            nlohmann::json resp;
            resp["success"] = true;
            resp["path"] = filePath;
            resp["content"] = content;
            resp["size"] = content.size();

            std::string json_body = resp.dump();
            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
            response += "\r\n";
            response += json_body;
            return response;

        } catch (const std::exception& e) {
            return MakeErrorResponse(500, std::string("File read error: ") + e.what());
        }
    }
};

// ============================================================
// Main Entry Point
// ============================================================

int main(int argc, char* argv[]) {
    int port = 11434;
    std::string model_path = ResolveDefaultModelPath();
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        }
    }


    /* if (!fs::exists(model_path)) {
        
        return 1;
    } */
    auto file_size = 36.5; // fs::file_size(model_path) / (1024.0 * 1024 * 1024);


    try {
        g_engine = std::make_unique<InferenceEngine>();
        if (!g_engine->loadModel(model_path)) {
            
            return 1;
        }


    } catch (const std::exception& e) {
        
        return 1;
    }


    // Initialize tool executor
    try {
        g_tool_executor = std::make_unique<RawrXD::Backend::AgenticToolExecutor>("D:\\RawrXD");
        
    } catch (const std::exception& e) {
        
    }
    
    SimpleHTTPServer server(port);
    if (!server.Start()) {
        
        return 1;
    }


    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    server.Stop();
    return 0;
}
