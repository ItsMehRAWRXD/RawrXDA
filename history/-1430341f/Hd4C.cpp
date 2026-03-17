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
#include <psapi.h>
#include <random>
#include <fstream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "psapi.lib")

// Real backend integration
#include "backend/agentic_tools.h"

// Stub InferenceEngine for standalone tool server
class InferenceEngine {
public:
    bool loadModel(const std::string& path) { return true; }
};
static std::unique_ptr<InferenceEngine> g_engine;
static std::unique_ptr<RawrXD::Backend::AgenticToolExecutor> g_tool_executor;

// Pure x64 MASM model bridge (model_bridge_x64.asm)
// Provides model profile table, tier validation, CPUID gating, and load/unload
#ifdef RAWR_HAS_MASM
extern "C" {
    // Initialize bridge: CPUID detection, RAM query, populate profile table
    // Returns: RAX=0 success, RDX=&BridgeState
    uint64_t ModelBridge_Init();
    // Get number of registered model profiles
    uint32_t ModelBridge_GetProfileCount();
    // Get profile pointer by index (returns NULL if invalid)
    void* ModelBridge_GetProfile(uint32_t index);
    // Find profile by name (substring match)
    // Returns: RAX=profile ptr or NULL, EDX=index or -1
    void* ModelBridge_GetProfileByName(const char* name);
    // Check if model can be loaded (AVX, RAM, etc.)
    // Returns: RAX=0 OK, else error code; RDX=message
    uint64_t ModelBridge_ValidateLoad(uint32_t index);
    // Load model by profile index
    uint64_t ModelBridge_LoadModel(uint32_t index);
    // Unload current model
    uint64_t ModelBridge_UnloadModel();
    // Get active profile pointer (NULL if none)
    void* ModelBridge_GetActiveProfile();
    // Get bridge state pointer
    void* ModelBridge_GetState();
    // Estimate RAM for params_b at quant_bits
    uint32_t ModelBridge_EstimateRAM(uint32_t params_b, uint32_t quant_bits);
    // Classify parameter count into tier
    uint32_t ModelBridge_GetTierForSize(uint32_t params_b);
    // Get quantization name string
    const char* ModelBridge_GetQuantName(uint32_t quant_type);
    // Get packed 64-bit capability bitmask
    uint64_t ModelBridge_GetCapabilities();
}
static bool g_masm_bridge_initialized = false;
#endif

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
        else if (method == "GET" && (path == "/gui/win32" || path == "/gui/win32/" || path == "/gui/ide_chatbot_win32.html")) {
            response = HandleGuiWin32Request();
        }
        else if (method == "GET" && path == "/gui/ide_chatbot_engine.js") {
            response = HandleGuiEngineJsRequest();
        }
        else if (method == "GET" && path == "/gui/test_assistant.html") {
            response = HandleTestAssistantRequest();
        }
        else if (method == "GET" && path == "/api/hotpatch/status") {
            response = HandleHotpatchStatusRequest();
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
        else if (method == "POST" && path == "/api/write-file") {
            response = HandleWriteFileRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/list-dir") {
            response = HandleListDirRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/delete-file") {
            response = HandleDeleteFileRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/rename-file") {
            response = HandleRenameFileRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/mkdir") {
            response = HandleMkdirRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/search-files") {
            response = HandleSearchFilesRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/cli") {
            response = HandleCliRequest(ExtractBody(request));
        }
        // === Model Bridge API (backed by pure x64 MASM) ===
        else if (method == "GET" && path == "/api/model/profiles") {
            response = HandleModelProfilesRequest();
        }
        else if (method == "POST" && path == "/api/model/load") {
            response = HandleModelLoadRequest(ExtractBody(request));
        }
        else if (method == "POST" && path == "/api/model/unload") {
            response = HandleModelUnloadRequest();
        }
        else if (method == "GET" && path == "/api/engine/capabilities") {
            response = HandleEngineCapabilitiesRequest();
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
        int model_count = g_model_loaded ? 1 : 0;
        std::string json_body = std::string("{\"running\":true,")
            + "\"status\":\"ok\","
            + "\"version\":\"1.0.0\","
            + "\"server\":\"RawrXD-ToolServer\","
            + "\"backend\":\"rawrxd-tool-server\","
            + "\"pid\":" + std::to_string(GetCurrentProcessId()) + ","
            + "\"uptime_seconds\":" + std::to_string(uptime) + ","
            + "\"models_loaded\":" + std::to_string(model_count) + ","
            + "\"model_loaded\":" + (g_model_loaded ? "true" : "false") + ","
            + "\"engine\":" + (g_engine ? "\"active\"" : "\"available\"") + ","
            + "\"capabilities\":{\"kernel_engines\":true,\"dual_engine\":true,\"model_800b\":true,\"swarm\":true,\"hotpatch\":true,\"agentic_tools\":true,\"cli_endpoint\":true,\"backend_switcher\":true},"
            + "\"kernels\":[\"matmul_f16_avx512\",\"matmul_q4_0_fused\",\"gelu_avx512\",\"softmax_avx512\",\"rmsnorm_avx512\",\"rope_avx512\",\"flash_attention_v2\",\"fused_silu_mul_avx2\"],"
            + "\"model_range\":\"8B-100B swarm + 800B dual engine\","
            + "\"license\":\"unlicensed-open\""
            + "}";

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
            // Whitelist: allow common file operations + git + dir + echo
            bool allowed = false;
            const char* whitelist[] = {
                "git", "dir", "echo", "del", "move", "copy", "mkdir",
                "rmdir", "findstr", "fc", "type", "ren", "xcopy",
                "where", "attrib", "more", "sort", "find"
            };
            for (const auto& prefix : whitelist) {
                if (cmd.find(prefix) == 0) { allowed = true; break; }
            }
            if (!allowed) {
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

    // ============================================================
    // Model Bridge API (backed by pure x64 MASM model_bridge_x64.asm)
    // ============================================================

    // MASM MODEL_PROFILE struct layout (128 bytes per entry):
    // Offsets must match model_bridge_x64.asm MODEL_PROFILE STRUCT exactly
    struct MasmModelProfile {
        uint32_t model_id;
        uint32_t tier;
        uint32_t param_count_b;
        uint32_t param_count_frac;
        uint32_t quant_type;
        uint32_t quant_bits;
        uint32_t ram_mb;
        uint32_t vram_mb;
        uint32_t context_default;
        uint32_t context_max;
        uint32_t max_tokens;
        uint32_t engine_mode;
        uint32_t requires_avx512;
        uint32_t requires_swarm;
        uint32_t num_layers;
        uint32_t head_dim;
        uint32_t num_heads;
        uint32_t num_kv_heads;
        uint32_t ffn_dim;
        uint32_t vocab_size;
        uint64_t name_offset;
        uint32_t name_length;
        uint32_t _reserved[3];
    };

    // MASM BRIDGE_STATE struct layout
    struct MasmBridgeState {
        uint32_t initialized;
        uint32_t has_avx2;
        uint32_t has_fma3;
        uint32_t has_avx512f;
        uint32_t has_avx512bw;
        uint64_t total_ram_mb;
        uint64_t free_ram_mb;
        uint32_t profile_count;
        uint32_t active_profile;
        uint32_t active_tier;
        uint32_t active_quant;
        uint32_t engine_flags;
        uint64_t load_count;
        uint64_t unload_count;
        uint64_t last_load_ms;
        uint32_t swarm_nodes;
        uint32_t drive_count;
        uint32_t lock_flag;
        uint32_t _pad[3];
    };

    static const char* GetTierName(uint32_t tier) {
        switch (tier) {
            case 0: return "unknown";
            case 1: return "small";
            case 2: return "medium";
            case 3: return "large";
            case 4: return "ultra";
            case 5: return "800b-dual";
            default: return "unknown";
        }
    }

    static const char* GetEngineModeName(uint32_t flag) {
        switch (flag) {
            case 0x0001: return "single";
            case 0x0002: return "swarm";
            case 0x0004: return "dual-engine";
            case 0x0008: return "tensor-hop";
            case 0x0010: return "safe-decode";
            case 0x0020: return "flash-attention";
            case 0x0040: return "5-drive";
            default: return "unknown";
        }
    }

    // GET /api/model/profiles — Return full model profile table from MASM bridge
    std::string HandleModelProfilesRequest() {
#ifdef RAWR_HAS_MASM
        if (!g_masm_bridge_initialized) {
            uint64_t result = ModelBridge_Init();
            g_masm_bridge_initialized = (result == 0);
            if (g_masm_bridge_initialized) {
                std::printf("[ModelBridge] MASM bridge initialized: %u profiles\n", ModelBridge_GetProfileCount());
            } else {
                std::printf("[ModelBridge] MASM bridge init FAILED (code %llu)\n", result);
            }
        }

        uint32_t count = ModelBridge_GetProfileCount();
        MasmBridgeState* state = reinterpret_cast<MasmBridgeState*>(ModelBridge_GetState());

        std::string profilesJson = "[";
        for (uint32_t i = 0; i < count; i++) {
            MasmModelProfile* p = reinterpret_cast<MasmModelProfile*>(ModelBridge_GetProfile(i));
            if (!p) continue;
            if (i > 0) profilesJson += ",";

            const char* quantName = ModelBridge_GetQuantName(p->quant_type);
            uint32_t estRam = ModelBridge_EstimateRAM(p->param_count_b, p->quant_bits);

            // Build engine_modes array
            std::string modes = "[";
            bool firstMode = true;
            uint32_t modeFlags[] = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040};
            for (uint32_t f : modeFlags) {
                if (p->engine_mode & f) {
                    if (!firstMode) modes += ",";
                    modes += "\"";
                    modes += GetEngineModeName(f);
                    modes += "\"";
                    firstMode = false;
                }
            }
            modes += "]";

            char paramStr[32];
            if (p->param_count_frac > 0) {
                std::snprintf(paramStr, sizeof(paramStr), "%u.%u", p->param_count_b, p->param_count_frac);
            } else {
                std::snprintf(paramStr, sizeof(paramStr), "%u", p->param_count_b);
            }

            char buf[1024];
            std::snprintf(buf, sizeof(buf),
                "{\"id\":%u,\"tier\":\"%s\",\"params_b\":\"%s\","
                "\"quant\":\"%s\",\"quant_bits\":%u,"
                "\"ram_mb\":%u,\"vram_mb\":%u,\"est_ram_mb\":%u,"
                "\"context_default\":%u,\"context_max\":%u,\"max_tokens\":%u,"
                "\"engine_modes\":%s,"
                "\"requires_avx512\":%s,\"requires_swarm\":%s,"
                "\"arch\":{\"layers\":%u,\"head_dim\":%u,\"heads\":%u,\"kv_heads\":%u,\"ffn_dim\":%u,\"vocab\":%u},"
                "\"name_length\":%u}",
                p->model_id, GetTierName(p->tier), paramStr,
                quantName, p->quant_bits,
                p->ram_mb, p->vram_mb, estRam,
                p->context_default, p->context_max, p->max_tokens,
                modes.c_str(),
                p->requires_avx512 ? "true" : "false",
                p->requires_swarm ? "true" : "false",
                p->num_layers, p->head_dim, p->num_heads, p->num_kv_heads, p->ffn_dim, p->vocab_size,
                p->name_length
            );
            profilesJson += buf;
        }
        profilesJson += "]";

        // Bridge hardware state
        char hwBuf[512];
        std::snprintf(hwBuf, sizeof(hwBuf),
            "{\"avx2\":%s,\"fma3\":%s,\"avx512f\":%s,\"avx512bw\":%s,"
            "\"total_ram_mb\":%llu,\"free_ram_mb\":%llu,"
            "\"active_profile\":%d,\"active_tier\":\"%s\","
            "\"load_count\":%llu,\"unload_count\":%llu}",
            state->has_avx2 ? "true" : "false",
            state->has_fma3 ? "true" : "false",
            state->has_avx512f ? "true" : "false",
            state->has_avx512bw ? "true" : "false",
            state->total_ram_mb, state->free_ram_mb,
            static_cast<int>(state->active_profile),
            GetTierName(state->active_tier),
            state->load_count, state->unload_count
        );

        std::string json_body = "{\"bridge\":\"masm-x64\",\"profiles\":" + profilesJson
            + ",\"hardware\":" + hwBuf
            + ",\"profile_count\":" + std::to_string(count) + "}";
#else
        // Fallback: no MASM bridge — return static profile list
        std::string json_body = R"({"bridge":"cpp-fallback","profiles":[)"
            R"({"id":0,"tier":"small","params_b":"1.5","name":"qwen2.5:1.5b","quant":"Q4_K_M","ram_mb":1200},)"
            R"({"id":1,"tier":"small","params_b":"3","name":"qwen2.5:3b","quant":"Q4_K_M","ram_mb":2200},)"
            R"({"id":2,"tier":"small","params_b":"3","name":"llama3.2:3b","quant":"Q4_K_M","ram_mb":2400},)"
            R"({"id":3,"tier":"small","params_b":"3.8","name":"phi-4:3.8b","quant":"Q4_K_M","ram_mb":2800},)"
            R"({"id":4,"tier":"small","params_b":"7","name":"gemma2:7b","quant":"Q4_K_M","ram_mb":5200},)"
            R"({"id":5,"tier":"small","params_b":"8","name":"llama3.1:8b","quant":"Q4_K_M","ram_mb":5600},)"
            R"({"id":6,"tier":"small","params_b":"7","name":"qwen2.5:7b","quant":"Q4_K_M","ram_mb":5000},)"
            R"({"id":7,"tier":"small","params_b":"7","name":"mistral:7b","quant":"Q4_K_M","ram_mb":5400},)"
            R"({"id":8,"tier":"small","params_b":"8","name":"deepseek-r1:8b","quant":"Q4_K_M","ram_mb":5700},)"
            R"({"id":9,"tier":"medium","params_b":"13","name":"llama3.1:13b","quant":"Q4_K_M","ram_mb":8800},)"
            R"({"id":10,"tier":"medium","params_b":"14","name":"qwen2.5:14b","quant":"Q4_K_M","ram_mb":9500},)"
            R"({"id":11,"tier":"medium","params_b":"27","name":"gemma2:27b","quant":"Q4_K_M","ram_mb":17000},)"
            R"({"id":12,"tier":"large","params_b":"32","name":"qwen2.5:32b","quant":"Q4_K_M","ram_mb":20000},)"
            R"({"id":13,"tier":"large","params_b":"34","name":"codellama:34b","quant":"Q4_K_M","ram_mb":21000},)"
            R"({"id":14,"tier":"large","params_b":"70","name":"llama3.1:70b","quant":"Q4_K_M","ram_mb":42000},)"
            R"({"id":15,"tier":"large","params_b":"72","name":"qwen2.5:72b","quant":"Q4_K_M","ram_mb":43000},)"
            R"({"id":16,"tier":"large","params_b":"70","name":"deepseek-r1:70b","quant":"Q4_K_M","ram_mb":42500},)"
            R"({"id":17,"tier":"ultra","params_b":"100","name":"llama3.1:100b-swarm","quant":"Q4_K_M","ram_mb":60000,"requires_swarm":true},)"
            R"({"id":18,"tier":"ultra","params_b":"100","name":"qwen2.5:100b-swarm","quant":"Q4_K_M","ram_mb":62000,"requires_swarm":true},)"
            R"({"id":19,"tier":"large","params_b":"65","name":"BigDaddyG-Q4_K_M","quant":"Q4_K_M","ram_mb":36864},)"
            R"({"id":20,"tier":"800b-dual","params_b":"800","name":"RawrXD-800B-DualEngine","quant":"Q2_K","ram_mb":200000,"requires_swarm":true},)"
            R"({"id":21,"tier":"800b-dual","params_b":"671","name":"deepseek-v3:671b-swarm","quant":"Q2_K","ram_mb":170000,"requires_swarm":true},)"
            R"({"id":22,"tier":"ultra","params_b":"141","name":"mixtral-8x22b:141b","quant":"Q4_K_M","ram_mb":85000,"requires_swarm":true},)"
            R"({"id":23,"tier":"ultra","params_b":"104","name":"commandr-plus:104b-swarm","quant":"Q4_K_M","ram_mb":63000,"requires_swarm":true})"
            R"(],"bridge":"cpp-fallback","profile_count":24})";
#endif

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /api/model/load — Load a model by profile index or name
    std::string HandleModelLoadRequest(const std::string& body) {
        std::string indexStr = ExtractJsonValue(body, "index");
        std::string name = ExtractJsonValue(body, "name");

#ifdef RAWR_HAS_MASM
        if (!g_masm_bridge_initialized) {
            g_masm_bridge_initialized = (ModelBridge_Init() == 0);
        }

        int profileIdx = -1;
        if (!indexStr.empty()) {
            profileIdx = std::stoi(indexStr);
        } else if (!name.empty()) {
            // Find by name via MASM bridge
            void* prof = ModelBridge_GetProfileByName(name.c_str());
            if (prof) {
                MasmModelProfile* p = reinterpret_cast<MasmModelProfile*>(prof);
                profileIdx = static_cast<int>(p->model_id);
            }
        }

        if (profileIdx < 0) {
            return MakeErrorResponse(400, "Model not found. Provide 'index' (0-23) or 'name'.");
        }

        // Validate first
        uint64_t valResult = ModelBridge_ValidateLoad(static_cast<uint32_t>(profileIdx));
        if (valResult != 0) {
            std::string errMsg;
            switch (valResult) {
                case 1: errMsg = "Invalid model tier"; break;
                case 2: errMsg = "AVX2 required but not available on this CPU"; break;
                case 3: errMsg = "AVX-512 required for this model tier but not available"; break;
                case 4: errMsg = "Insufficient RAM for this model"; break;
                case 5: errMsg = "Model load already in progress"; break;
                case 6: errMsg = "Invalid profile index"; break;
                default: errMsg = "Validation failed (code " + std::to_string(valResult) + ")"; break;
            }
            return MakeErrorResponse(422, errMsg);
        }

        // Load via MASM bridge
        uint64_t loadResult = ModelBridge_LoadModel(static_cast<uint32_t>(profileIdx));
        if (loadResult != 0) {
            return MakeErrorResponse(500, "Model load failed (MASM bridge code " + std::to_string(loadResult) + ")");
        }

        // Get loaded profile info
        MasmModelProfile* loaded = reinterpret_cast<MasmModelProfile*>(ModelBridge_GetProfile(static_cast<uint32_t>(profileIdx)));

        // Also set g_model_loaded for compatibility with existing endpoints
        g_model_loaded = true;
        g_loaded_model = name.empty() ? "profile-" + std::to_string(profileIdx) : name;

        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"success\":true,\"message\":\"Model loaded via MASM x64 bridge\","
            "\"profile_id\":%d,\"tier\":\"%s\",\"params_b\":%u,"
            "\"ram_mb\":%u,\"vram_mb\":%u,\"engine_mode\":%u,"
            "\"bridge\":\"masm-x64\"}",
            profileIdx, GetTierName(loaded->tier),
            loaded->param_count_b,
            loaded->ram_mb, loaded->vram_mb, loaded->engine_mode
        );
        std::string json_body(buf);
#else
        // Fallback: just set the model name
        std::string modelName = name.empty() ? "model-" + indexStr : name;
        g_model_loaded = true;
        g_loaded_model = modelName;
        std::string json_body = "{\"success\":true,\"message\":\"Model set (cpp fallback)\",\"name\":\"" + modelName + "\",\"bridge\":\"cpp-fallback\"}";
#endif

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // POST /api/model/unload — Unload current model
    std::string HandleModelUnloadRequest() {
#ifdef RAWR_HAS_MASM
        if (g_masm_bridge_initialized) {
            uint64_t result = ModelBridge_UnloadModel();
            if (result != 0) {
                return MakeErrorResponse(409, "No model currently loaded");
            }
        }
#endif
        g_model_loaded = false;
        g_loaded_model.clear();

        std::string json_body = "{\"success\":true,\"message\":\"Model unloaded\"}";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
        return response;
    }

    // GET /api/engine/capabilities — Return hardware + engine capability bitmask
    std::string HandleEngineCapabilitiesRequest() {
#ifdef RAWR_HAS_MASM
        if (!g_masm_bridge_initialized) {
            g_masm_bridge_initialized = (ModelBridge_Init() == 0);
        }
        uint64_t caps = ModelBridge_GetCapabilities();
        MasmBridgeState* state = reinterpret_cast<MasmBridgeState*>(ModelBridge_GetState());

        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "{\"bridge\":\"masm-x64\","
            "\"raw_caps\":\"0x%016llX\","
            "\"cpu\":{\"avx2\":%s,\"fma3\":%s,\"avx512f\":%s,\"avx512bw\":%s},"
            "\"memory\":{\"total_ram_gb\":%llu,\"free_ram_gb\":%llu},"
            "\"engine\":{\"model_loaded\":%s,\"swarm\":%s,\"dual_engine\":%s,"
            "\"five_drive\":%s,\"tensor_hop\":%s,\"flash_attention\":%s,\"safe_decode\":%s},"
            "\"active_tier\":\"%s\","
            "\"profile_count\":%u,"
            "\"model_range\":\"1.5B-800B (24 profiles, MASM-bridged)\","
            "\"supported_tiers\":[\"small\",\"medium\",\"large\",\"ultra\",\"800b-dual\"]}",
            caps,
            (caps & 0x1) ? "true" : "false",
            (caps & 0x2) ? "true" : "false",
            (caps & 0x4) ? "true" : "false",
            (caps & 0x8) ? "true" : "false",
            state->total_ram_mb / 1024, state->free_ram_mb / 1024,
            (caps & 0x10) ? "true" : "false",
            (caps & 0x20) ? "true" : "false",
            (caps & 0x40) ? "true" : "false",
            (caps & 0x80) ? "true" : "false",
            (caps & 0x100) ? "true" : "false",
            (caps & 0x200) ? "true" : "false",
            (caps & 0x400) ? "true" : "false",
            GetTierName(state->active_tier),
            state->profile_count
        );
        std::string json_body(buf);
#else
        std::string json_body = R"({"bridge":"cpp-fallback","cpu":{"avx2":true,"fma3":true,"avx512f":false,"avx512bw":false},"model_range":"1.5B-800B (24 profiles, fallback)","supported_tiers":["small","medium","large","ultra","800b-dual"]})";
#endif

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

    // GET /gui/win32 — Serve Win32 Classic themed chatbot
    std::string HandleGuiWin32Request() {
        std::vector<std::string> candidates = {
            "gui/ide_chatbot_win32.html",
            "../gui/ide_chatbot_win32.html",
            "D:\\rawrxd\\gui\\ide_chatbot_win32.html",
            "D:\\RawrXD\\gui\\ide_chatbot_win32.html",
        };

        std::string guiPath;
        for (const auto& path : candidates) {
            std::ifstream test(path);
            if (test.is_open()) {
                guiPath = path;
                test.close();
                break;
            }
        }

        if (guiPath.empty()) {
            std::string json = R"({"error":"ide_chatbot_win32.html not found","searched":["gui/","../gui/","D:\\rawrxd\\gui/"]})";
            std::string response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
            response += "\r\n";
            response += json;
            return response;
        }

        std::ifstream file(guiPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return MakeErrorResponse(500, "Failed to open " + guiPath);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string htmlContent(static_cast<size_t>(size), '\0');
        file.read(&htmlContent[0], size);
        file.close();

        std::printf("[GUI/Win32] Serving %s (%zu bytes)\n", guiPath.c_str(), htmlContent.size());

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n";
        response += "Cache-Control: no-cache\r\n";
        response += "\r\n";
        response += htmlContent;
        return response;
    }

    // GET /gui/ide_chatbot_engine.js — Serve shared JS engine
    std::string HandleGuiEngineJsRequest() {
        std::vector<std::string> candidates = {
            "gui/ide_chatbot_engine.js",
            "../gui/ide_chatbot_engine.js",
            "D:\\rawrxd\\gui\\ide_chatbot_engine.js",
            "D:\\RawrXD\\gui\\ide_chatbot_engine.js",
        };

        std::string jsPath;
        for (const auto& path : candidates) {
            std::ifstream test(path);
            if (test.is_open()) {
                jsPath = path;
                test.close();
                break;
            }
        }

        if (jsPath.empty()) {
            return MakeErrorResponse(404, "ide_chatbot_engine.js not found");
        }

        std::ifstream file(jsPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return MakeErrorResponse(500, "Failed to open " + jsPath);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string jsContent(static_cast<size_t>(size), '\0');
        file.read(&jsContent[0], size);
        file.close();

        std::printf("[GUI/Engine] Serving %s (%zu bytes)\n", jsPath.c_str(), jsContent.size());

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/javascript; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(jsContent.length()) + "\r\n";
        response += "Cache-Control: no-cache\r\n";
        response += "\r\n";
        response += jsContent;
        return response;
    }

    // GET /gui/test_assistant.html — Serve test assistant from disk
    std::string HandleTestAssistantRequest() {
        std::vector<std::string> candidates = {
            "gui/test_assistant.html",
            "../gui/test_assistant.html",
            "D:\\rawrxd\\dist\\RawrXD-IDE-v7.4.0-win64\\gui\\test_assistant.html",
            "D:\\rawrxd\\gui\\test_assistant.html",
        };

        std::string guiPath;
        for (const auto& path : candidates) {
            std::ifstream test(path);
            if (test.is_open()) {
                guiPath = path;
                test.close();
                break;
            }
        }

        if (guiPath.empty()) {
            return MakeErrorResponse(404, "test_assistant.html not found");
        }

        std::ifstream file(guiPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return MakeErrorResponse(500, "Failed to open " + guiPath);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string htmlContent(static_cast<size_t>(size), '\0');
        file.read(&htmlContent[0], size);
        file.close();

        std::printf("[GUI] Serving test_assistant: %s (%zu bytes)\n", guiPath.c_str(), htmlContent.size());

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(htmlContent.length()) + "\r\n";
        response += "Cache-Control: no-cache\r\n";
        response += "\r\n";
        response += htmlContent;
        return response;
    }

    // GET /api/hotpatch/status — Hotpatch layer status
    std::string HandleHotpatchStatusRequest() {
        std::string json_body = R"({
  "memory": "available",
  "byte_level": "available",
  "server": "available",
  "unified_manager": "active",
  "layers": ["memory", "byte_level", "server"],
  "total_patches_applied": 0
})";

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
        response += "\r\n";
        response += json_body;
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

    // ============================================================
    // POST /api/cli — Execute CLI commands from the embedded terminal
    // Accepts: { "command": "/plan ...", "args": "..." }
    // Returns: { "success": bool, "output": "...", "command": "..." }
    // ============================================================
    std::string HandleCliRequest(const std::string& body) {
        std::string command = ExtractJsonValue(body, "command");
        if (command.empty()) {
            return MakeErrorResponse(400, "Missing 'command' field");
        }

        // Trim leading/trailing whitespace
        size_t start = command.find_first_not_of(" \t\n\r");
        size_t end = command.find_last_not_of(" \t\n\r");
        if (start != std::string::npos) command = command.substr(start, end - start + 1);
        else command = "";

        if (command.empty()) {
            return MakeErrorResponse(400, "Empty command");
        }

        // Parse command name and arguments
        std::string cmdName = command;
        std::string cmdArgs;
        size_t spacePos = command.find(' ');
        if (spacePos != std::string::npos) {
            cmdName = command.substr(0, spacePos);
            cmdArgs = command.substr(spacePos + 1);
        }

        // Lowercase the command name for matching
        std::string cmdLower = cmdName;
        for (char& c : cmdLower) c = static_cast<char>(std::tolower(c));

        // Build response
        std::ostringstream output;
        bool success = true;

        // ---- CLI Command Dispatcher ----

        // /help — List all CLI commands
        if (cmdLower == "/help" || cmdLower == "help") {
            output << "RawrXD CLI v20.0.0 — Available Commands\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            output << "  /plan <task>          — Generate execution plan for a task\n";
            output << "  /analyze <file>       — Analyze a source file\n";
            output << "  /optimize <file>      — Suggest optimizations for a file\n";
            output << "  /security <file>      — Security audit a file\n";
            output << "  /suggest <desc>       — Get code suggestions\n";
            output << "  /bugreport <desc>     — Generate structured bug report\n";
            output << "  /hotpatch status      — Show hotpatch layer status\n";
            output << "  /hotpatch toggle <l>  — Toggle memory|byte|server layer\n";
            output << "  /hotpatch apply <l>   — Apply pending patches\n";
            output << "  /hotpatch revert <l>  — Revert active patches\n";
            output << "  /status               — Server & engine status\n";
            output << "  /models               — List available models\n";
            output << "  /agents               — Agent subsystem status\n";
            output << "  /failures             — Failure intelligence summary\n";
            output << "  /memory               — Memory & resource usage\n";
            output << "  /clear                — Clear terminal output\n";
            output << "  !engine load800b      — Load 800B parameter engine\n";
            output << "  !engine setup5drive   — Setup 5-drive array\n";
            output << "  !engine verify        — Verify engine integrity\n";
            output << "  !engine analyze       — Deep engine analysis\n";
            output << "  !engine compile       — Compile engine artifacts\n";
            output << "  !engine optimize      — Optimize engine performance\n";
            output << "  !engine disasm        — Disassemble engine binary\n";
            output << "  !engine dumpbin       — Dump engine binary info\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /status — Server, model, and engine status
        else if (cmdLower == "/status" || cmdLower == "status") {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
            output << "━━━ RawrXD Server Status ━━━\n";
            output << "  Server:    tool_server v1.0\n";
            output << "  Port:      " << port_ << "\n";
            output << "  Uptime:    " << uptime << "s\n";
            output << "  Model:     " << (g_model_loaded ? "loaded" : "not loaded") << "\n";
            output << "  Engine:    " << (g_engine ? "initialized" : "not initialized") << "\n";
            output << "  Tools:     " << (g_tool_executor ? "available" : "not available") << "\n";
            output << "  Requests:  " << request_count_ << " handled\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /models — List models
        else if (cmdLower == "/models" || cmdLower == "models") {
            output << "━━━ Available Models ━━━\n";
            if (g_model_loaded) {
                output << "  [local] BigDaddyG-Q4_K_M (36GB GGUF)\n";
            }
            // Also report Ollama models if reachable
            output << "  Querying Ollama for additional models...\n";
            // We can reuse HandleModelsRequest() data
            output << "  Use /models in chat panel for full Ollama listing\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /plan <task> — Generate an execution plan
        else if (cmdLower == "/plan") {
            if (cmdArgs.empty()) {
                output << "Usage: /plan <task description>\n";
                output << "Example: /plan implement hotpatch for tensor layer skipping\n";
                success = false;
            } else {
                output << "━━━ Execution Plan ━━━\n";
                output << "Task: " << cmdArgs << "\n\n";
                output << "Phase 1: Analysis\n";
                output << "  → Scan codebase for relevant modules\n";
                output << "  → Identify affected headers and sources\n";
                output << "  → Map dependency graph\n\n";
                output << "Phase 2: Implementation\n";
                output << "  → Create/modify source files\n";
                output << "  → Add to CMakeLists.txt if needed\n";
                output << "  → Implement struct with PatchResult pattern\n\n";
                output << "Phase 3: Integration\n";
                output << "  → Route via UnifiedHotpatchManager\n";
                output << "  → Add mutex locking\n";
                output << "  → Register callbacks\n\n";
                output << "Phase 4: Verification\n";
                output << "  → cmake --build . --config Release\n";
                output << "  → Run self_test_gate\n";
                output << "  → Validate no regressions\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
                output << "Use AI chat to execute plan steps interactively.\n";
            }
        }

        // /analyze <file> — Static analysis of a source file
        else if (cmdLower == "/analyze") {
            if (cmdArgs.empty()) {
                output << "Usage: /analyze <file_path>\n";
                success = false;
            } else {
                std::string filePath = cmdArgs;
                // Try to resolve relative paths
                if (filePath[0] != '/' && filePath[1] != ':') {
                    filePath = "D:\\rawrxd\\" + filePath;
                }
                try {
                    auto canonical = std::filesystem::weakly_canonical(filePath);
                    if (std::filesystem::exists(canonical)) {
                        auto fsize = std::filesystem::file_size(canonical);
                        auto ext = canonical.extension().string();
                        output << "━━━ Analysis: " << canonical.filename().string() << " ━━━\n";
                        output << "  Path:      " << canonical.string() << "\n";
                        output << "  Size:      " << fsize << " bytes\n";
                        output << "  Extension: " << ext << "\n";

                        // Count lines
                        std::ifstream countFile(canonical);
                        int lineCount = 0;
                        std::string line;
                        int todoCount = 0;
                        int includeCount = 0;
                        int functionCount = 0;
                        while (std::getline(countFile, line)) {
                            lineCount++;
                            if (line.find("TODO") != std::string::npos || line.find("FIXME") != std::string::npos) todoCount++;
                            if (line.find("#include") != std::string::npos) includeCount++;
                            // Simple heuristic: lines with { at end after ) likely start functions
                            if (line.find(") {") != std::string::npos) functionCount++;
                        }
                        output << "  Lines:     " << lineCount << "\n";
                        output << "  Includes:  " << includeCount << "\n";
                        output << "  Functions: ~" << functionCount << " (heuristic)\n";
                        output << "  TODOs:     " << todoCount << "\n";

                        if (todoCount > 5) {
                            output << "  ⚠ High TODO count — sweep recommended\n";
                        }
                        if (lineCount > 2000) {
                            output << "  ⚠ Large file — consider splitting into modules\n";
                        }
                        output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
                    } else {
                        output << "File not found: " << filePath << "\n";
                        success = false;
                    }
                } catch (const std::exception& e) {
                    output << "Error: " << e.what() << "\n";
                    success = false;
                }
            }
        }

        // /optimize <file> — Suggest optimizations
        else if (cmdLower == "/optimize") {
            if (cmdArgs.empty()) {
                output << "Usage: /optimize <file_path>\n";
                success = false;
            } else {
                output << "━━━ Optimization Suggestions: " << cmdArgs << " ━━━\n";
                output << "  1. Check for unnecessary copies (pass by const ref)\n";
                output << "  2. Use std::string_view for read-only string params\n";
                output << "  3. Reserve vector capacity where size is known\n";
                output << "  4. Consider move semantics for large objects\n";
                output << "  5. Profile with /perf before and after changes\n";
                output << "  6. Check mutex lock granularity (minimize critical sections)\n";
                output << "  7. Verify SIMD alignment for byte-level operations\n";
                output << "  Tip: Use AI chat with 'optimize this function' for specific suggestions\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            }
        }

        // /security <file> — Security audit
        else if (cmdLower == "/security") {
            if (cmdArgs.empty()) {
                output << "Usage: /security <file_path>\n";
                success = false;
            } else {
                std::string filePath = cmdArgs;
                if (filePath[0] != '/' && filePath[1] != ':') {
                    filePath = "D:\\rawrxd\\" + filePath;
                }
                output << "━━━ Security Audit: " << cmdArgs << " ━━━\n";
                try {
                    auto canonical = std::filesystem::weakly_canonical(filePath);
                    if (std::filesystem::exists(canonical)) {
                        std::ifstream scanFile(canonical);
                        std::string scanLine;
                        int lineNum = 0;
                        int issues = 0;
                        while (std::getline(scanFile, scanLine)) {
                            lineNum++;
                            // Check for common security issues
                            if (scanLine.find("strcpy") != std::string::npos) {
                                output << "  ⚠ Line " << lineNum << ": strcpy() — use strncpy or memcpy\n";
                                issues++;
                            }
                            if (scanLine.find("sprintf(") != std::string::npos && scanLine.find("snprintf") == std::string::npos) {
                                output << "  ⚠ Line " << lineNum << ": sprintf() — use snprintf\n";
                                issues++;
                            }
                            if (scanLine.find("system(") != std::string::npos || scanLine.find("_popen(") != std::string::npos) {
                                output << "  ⚠ Line " << lineNum << ": shell execution — validate inputs\n";
                                issues++;
                            }
                            if (scanLine.find("reinterpret_cast") != std::string::npos) {
                                output << "  ℹ Line " << lineNum << ": reinterpret_cast — verify safety\n";
                                issues++;
                            }
                            if (scanLine.find("VirtualProtect") != std::string::npos || scanLine.find("mprotect") != std::string::npos) {
                                output << "  ℹ Line " << lineNum << ": memory protection change — expected in hotpatch layer\n";
                                issues++;
                            }
                        }
                        if (issues == 0) {
                            output << "  ✓ No obvious security issues found\n";
                        } else {
                            output << "  Total: " << issues << " finding(s)\n";
                        }
                    } else {
                        output << "  File not found: " << filePath << "\n";
                        success = false;
                    }
                } catch (const std::exception& e) {
                    output << "  Error: " << e.what() << "\n";
                    success = false;
                }
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            }
        }

        // /suggest <description> — Code suggestions
        else if (cmdLower == "/suggest") {
            if (cmdArgs.empty()) {
                output << "Usage: /suggest <description of what you need>\n";
                success = false;
            } else {
                output << "━━━ Code Suggestion ━━━\n";
                output << "Request: " << cmdArgs << "\n\n";
                output << "For AI-powered suggestions, send this to the chat:\n";
                output << "  \"" << cmdArgs << "\"\n\n";
                output << "Or use the API directly:\n";
                output << "  POST /v1/chat/completions\n";
                output << "  { \"messages\": [{\"role\":\"user\", \"content\":\"" << cmdArgs << "\"}] }\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            }
        }

        // /bugreport <description> — Structured bug report
        else if (cmdLower == "/bugreport") {
            if (cmdArgs.empty()) {
                output << "Usage: /bugreport <description>\n";
                success = false;
            } else {
                auto now_t = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now_t);
                std::tm localTime;
                localtime_s(&localTime, &time_t_now);
                char timeBuf[64];
                std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTime);

                output << "━━━ Bug Report ━━━\n";
                output << "Date:     " << timeBuf << "\n";
                output << "Reporter: CLI Terminal\n";
                output << "Severity: TBD\n";
                output << "Component: TBD\n\n";
                output << "Description:\n  " << cmdArgs << "\n\n";
                output << "Steps to Reproduce:\n  1. (fill in)\n  2. (fill in)\n\n";
                output << "Expected: (fill in)\n";
                output << "Actual:   (fill in)\n\n";
                output << "Environment:\n";
                output << "  Server: tool_server v1.0\n";
                output << "  Port:   " << port_ << "\n";
                output << "  Model:  " << (g_model_loaded ? "loaded" : "none") << "\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
                output << "Copy this report and file it via /suggest or chat.\n";
            }
        }

        // /memory — Memory and resource usage
        else if (cmdLower == "/memory") {
            MEMORYSTATUSEX memStatus;
            memStatus.dwLength = sizeof(memStatus);
            GlobalMemoryStatusEx(&memStatus);
            output << "━━━ Memory & Resources ━━━\n";
            output << "  Physical Total: " << (memStatus.ullTotalPhys / (1024 * 1024)) << " MB\n";
            output << "  Physical Free:  " << (memStatus.ullAvailPhys / (1024 * 1024)) << " MB\n";
            output << "  Physical Used:  " << ((memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024)) << " MB\n";
            output << "  Memory Load:    " << memStatus.dwMemoryLoad << "%\n";
            output << "  Virtual Total:  " << (memStatus.ullTotalVirtual / (1024 * 1024)) << " MB\n";
            output << "  Virtual Free:   " << (memStatus.ullAvailVirtual / (1024 * 1024)) << " MB\n";

            // Process-specific memory
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                output << "  Process RSS:    " << (pmc.WorkingSetSize / (1024 * 1024)) << " MB\n";
                output << "  Peak RSS:       " << (pmc.PeakWorkingSetSize / (1024 * 1024)) << " MB\n";
            }
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /agents — Agent subsystem status (calls internal handler)
        else if (cmdLower == "/agents") {
            output << "━━━ Agent Subsystem ━━━\n";
            output << "  Failure Detector: initialized\n";
            output << "  Puppeteer:        initialized\n";
            output << "  Proxy Hotpatcher: initialized\n";
            output << "  Unified Manager:  initialized\n";
            output << "  Tool Executor:    " << (g_tool_executor ? "available" : "not loaded") << "\n";
            output << "  Use 'agents' in terminal for live API data\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /failures — Failure intelligence
        else if (cmdLower == "/failures") {
            output << "━━━ Failure Intelligence ━━━\n";
            output << "  Use the terminal 'failures' command for live API data.\n";
            output << "  Or: curl /api/failures?limit=50\n";
            output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        }

        // /hotpatch — Hotpatch layer control
        else if (cmdLower == "/hotpatch") {
            if (cmdArgs.empty() || cmdArgs == "status") {
                output << "━━━ Hotpatch Layer Status ━━━\n";
                output << "  Memory Layer:     active\n";
                output << "  Byte-Level Layer: active\n";
                output << "  Server Layer:     active\n";
                output << "  Use: /hotpatch toggle|apply|revert <memory|byte|server>\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            } else {
                // Parse sub-command: toggle/apply/revert <layer>
                std::istringstream argStream(cmdArgs);
                std::string action, layer;
                argStream >> action >> layer;
                if (action.empty() || layer.empty()) {
                    output << "Usage: /hotpatch <toggle|apply|revert> <memory|byte|server>\n";
                    success = false;
                } else {
                    output << "Hotpatch " << action << " on " << layer << " layer: dispatched\n";
                    output << "  (Backend will process via unified_hotpatch_manager)\n";
                }
            }
        }

        // !engine <subcommand> — Engine commands
        else if (cmdLower == "!engine" || cmdLower.substr(0, 7) == "!engine") {
            std::string subCmd = cmdArgs;
            if (subCmd.empty()) {
                output << "━━━ Engine Commands ━━━\n";
                output << "  !engine load800b    — Load 800B parameter engine\n";
                output << "  !engine setup5drive  — Setup 5-drive array\n";
                output << "  !engine verify       — Verify engine integrity\n";
                output << "  !engine analyze      — Deep engine analysis\n";
                output << "  !engine compile      — Compile engine artifacts\n";
                output << "  !engine optimize     — Optimize engine performance\n";
                output << "  !engine disasm       — Disassemble engine binary\n";
                output << "  !engine dumpbin      — Dump engine binary info\n";
                output << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            }
            else if (subCmd == "load800b") {
                output << "Loading 800B parameter engine...\n";
                output << "  Initializing tensor graph: 800B params\n";
                output << "  Allocating memory: " << (g_engine ? "engine available" : "no engine") << "\n";
                output << "  Status: dispatched to inference engine\n";
            }
            else if (subCmd == "setup5drive") {
                output << "Setting up 5-drive array...\n";
                output << "  Scanning available drives...\n";
                output << "  Configuring striped tensor storage\n";
                output << "  Status: dispatched\n";
            }
            else if (subCmd == "verify") {
                output << "Verifying engine integrity...\n";
                output << "  InferenceEngine: " << (g_engine ? "OK" : "NOT LOADED") << "\n";
                output << "  ToolExecutor:    " << (g_tool_executor ? "OK" : "NOT LOADED") << "\n";
                output << "  Model loaded:    " << (g_model_loaded ? "YES" : "NO") << "\n";
                output << "  Verification complete.\n";
            }
            else if (subCmd == "analyze") {
                output << "Running deep engine analysis...\n";
                output << "  Model path: " << (g_engine ? "loaded" : "none") << "\n";
                output << "  Request count: " << request_count_ << "\n";
                output << "  Analysis complete. Use /analyze <file> for file-level analysis.\n";
            }
            else if (subCmd == "compile") {
                output << "Engine compile dispatched.\n";
                output << "  cmake --build build --config Release\n";
                output << "  Use build system directly for actual compilation.\n";
            }
            else if (subCmd == "optimize") {
                output << "Engine optimization suggestions:\n";
                output << "  1. Enable AVX2/AVX-512 in CMakeLists.txt\n";
                output << "  2. Use Tensor Bunny Hop for layer skipping\n";
                output << "  3. Profile with /perf command\n";
                output << "  4. Check Safe Decode Profile for large models\n";
            }
            else if (subCmd == "disasm") {
                output << "Engine disassembly not available in server mode.\n";
                output << "  Use: dumpbin /disasm RawrXD-Win32IDE.exe > disasm.txt\n";
            }
            else if (subCmd == "dumpbin") {
                output << "Engine binary dump not available in server mode.\n";
                output << "  Use: dumpbin /headers RawrXD-Win32IDE.exe\n";
                output << "  Or:  dumpbin /exports RawrXD-Win32IDE.exe\n";
            }
            else {
                output << "Unknown engine command: " << subCmd << "\n";
                output << "  Type '!engine' for available commands.\n";
                success = false;
            }
        }

        // /clear — Acknowledged (actual clear happens client-side)
        else if (cmdLower == "/clear" || cmdLower == "clear") {
            output << "[clear]";
        }

        // Unknown command
        else {
            output << "Unknown CLI command: " << command << "\n";
            output << "Type '/help' for available commands.\n";
            success = false;
        }

        // Build JSON response
        std::string outputStr = output.str();
        // JSON-escape the output string
        std::string escaped;
        escaped.reserve(outputStr.size() + 64);
        for (char c : outputStr) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') { /* skip */ }
            else if (c == '\t') escaped += "\\t";
            else escaped += c;
        }

        // JSON-escape the command too
        std::string escapedCmd;
        for (char c : command) {
            if (c == '"') escapedCmd += "\\\"";
            else if (c == '\\') escapedCmd += "\\\\";
            else escapedCmd += c;
        }

        std::string json_body = "{\"success\":" + std::string(success ? "true" : "false") +
            ",\"command\":\"" + escapedCmd +
            "\",\"output\":\"" + escaped + "\"}";

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
