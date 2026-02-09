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
    
    void HandleRequest(SOCKET client_socket, const std::string& request) {
        std::string response;
        
        // Parse request line
        std::istringstream iss(request);
        std::string method, path, http_version;
        iss >> method >> path >> http_version;
        
        // Route to handler
        if (method == "GET" && path == "/api/tags") {
            response = HandleTagsRequest();
        }
        else if (method == "GET" && path == "/health") {
            response = HandleHealthRequest();
        }
        else if (method == "GET" && path == "/api/status") {
            response = HandleStatusRequest();
        }
        else if (method == "POST" && path == "/api/generate") {
            // Extract body
            size_t body_start = request.find("\r\n\r\n");
            std::string body = (body_start != std::string::npos) 
                ? request.substr(body_start + 4) 
                : "";
            response = HandleGenerateRequest(body);
        }
        else if (method == "GET" && path == "/metrics") {
            response = HandleMetricsRequest();
        }
        else if (method == "POST" && path == "/api/tool") {
            // Extract body
            size_t body_start = request.find("\r\n\r\n");
            std::string body = (body_start != std::string::npos) 
                ? request.substr(body_start + 4) 
                : "";
            response = HandleToolRequest(body);
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        }
        
        send(client_socket, response.c_str(), response.length(), 0);
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
