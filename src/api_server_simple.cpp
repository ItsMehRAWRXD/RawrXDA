/**
 * Simple GGUF API Server - Lightweight Native Inference HTTP API
 * Provides real HTTP endpoints with actual inference backend via EngineRegistry
 * Working Winsock HTTP server for model serving
 */

#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include "gpu_enforcement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <time>
#include <sstream>
#include <vector>
#include "engine_iface.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma warning(disable : 4996)

// Global state
static std::atomic<bool> g_running(false);  // atomic with trivial init is OK
static SOCKET g_listen_socket = INVALID_SOCKET;
static std::chrono::steady_clock::time_point g_start_time;  // trivial

// LAZY SINGLETON: std::string has non-trivial constructor
inline std::string& GetActiveModel() {
    static std::string* inst = new std::string();
    return *inst;
}
#define g_active_model GetActiveModel()

static Engine* g_active_engine = nullptr;

// ============================================================
// HTTP Response Builder
// ============================================================

std::string BuildHttpResponse(int status_code, const std::string& content_type, const std::string& body) {
    std::string status_text = "200 OK";
    if (status_code == 400) status_text = "400 Bad Request";
    else if (status_code == 404) status_text = "404 Not Found";
    else if (status_code == 500) status_text = "500 Internal Server Error";
    
    std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

// ============================================================
// Endpoint Handlers
// ============================================================

std::string HandleHealthRequest() {
    std::string json = R"({"status":"ok","version":"1.0.0"})";
    return BuildHttpResponse(200, "application/json", json);
}

std::string HandleStatusRequest() {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - g_start_time).count();
    
    std::ostringstream json;
    json << R"({"status":"running","pid":)" << GetCurrentProcessId() 
         << R"(,"uptime_seconds":)" << uptime << R"(})";
    
    return BuildHttpResponse(200, "application/json", json.str());
}

std::string HandleTagsRequest() {
    // Build real model list from EngineRegistry
    // Try known engine names that could be registered
    static const char* engine_names[] = {
        "Sovereign-800B", "Sovereign-Small", "sovereign800b",
        "rawr-engine", "cpu-inference", nullptr
    };

    std::ostringstream json;
    json << R"({"models":[)";

    bool first = true;
    for (int i = 0; engine_names[i]; i++) {
        Engine* e = EngineRegistry::get(engine_names[i]);
        if (e) {
            if (!first) json << ",";
            first = false;
            auto now = std::time(nullptr);
            auto tm = *std::gmtime(&now);
            char ts[30];
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm);
            json << R"({"name":")" << e->name()
                 << R"(","modified_at":")" << ts
                 << R"(","size":0,"digest":"local"})";
        }
    }

    // If no engines registered, report empty
    if (first) {
        json << R"({"name":"none","modified_at":"","size":0,"digest":"no engines registered"})";
    }

    json << "]}";
    return BuildHttpResponse(200, "application/json", json.str());
}

// Crude JSON string value extractor (no external JSON library)
static std::string extract_json_string(const std::string& json,
                                        const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    pos++; // skip opening quote
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

std::string HandleGenerateRequest(const std::string& body) {
    // ---- Parse request body ----
    std::string model_name = extract_json_string(body, "model");
    std::string prompt = extract_json_string(body, "prompt");

    if (prompt.empty()) {
        return BuildHttpResponse(400, "application/json",
            R"({"error":"Missing 'prompt' field in request body"})");
    }

    // ---- Resolve inference engine ----
    Engine* engine = nullptr;
    if (!model_name.empty()) {
        engine = EngineRegistry::get(model_name);
    }
    // Fallback: use globally active engine, or try known names
    if (!engine) engine = g_active_engine;
    if (!engine) engine = EngineRegistry::get("Sovereign-800B");
    if (!engine) engine = EngineRegistry::get("Sovereign-Small");
    if (!engine) engine = EngineRegistry::get("sovereign800b");

    if (!engine) {
        return BuildHttpResponse(500, "application/json",
            R"({"error":"No inference engine available. Load a model first."})");
    }

    // ---- Build AgentRequest and run real inference ----
    AgentRequest req;
    req.prompt = prompt;
    req.mode = ASK;
    req.deep_thinking = false;
    req.deep_research = false;
    req.no_refusal = false;
    req.context_limit = 4096;

    auto t_start = std::chrono::steady_clock::now();

    std::string response_text = engine->infer(req);

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // ---- Compute real metrics ----
    // Estimate token count from response length (byte-level: 1 char ≈ 1 token)
    int eval_count = (int)response_text.size();
    double tokens_per_sec = (elapsed_ms > 0.0)
        ? (eval_count * 1000.0 / elapsed_ms) : 0.0;

    auto now = std::time(nullptr);
    auto tm = *std::gmtime(&now);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm);

    // ---- Escape response for JSON ----
    std::string escaped;
    escaped.reserve(response_text.size() + 32);
    for (char c : response_text) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:
                if (c >= 32) escaped += c;
                break;
        }
    }

    std::ostringstream json;
    json << R"({"model":")" << engine->name()
         << R"(","response":")" << escaped
         << R"(","created_at":")" << timestamp
         << R"(","done":true)"
         << R"(,"eval_count":)" << eval_count
         << R"(,"eval_duration_ms":)" << (int)elapsed_ms
         << R"(,"tokens_per_sec":)" << (int)tokens_per_sec
         << "}";

    return BuildHttpResponse(200, "application/json", json.str());
}

std::string HandleNotFound() {
    std::string json = R"({"error":"Endpoint not found"})";
    return BuildHttpResponse(404, "application/json", json);
}

// ============================================================
// HTTP Request Parser & Handler
// ============================================================

std::string HandleClientRequest(const std::string& request) {
    // Parse request line
    std::istringstream iss(request);
    std::string method, path, http_version;
    iss >> method >> path >> http_version;
    
    // Request received
    
    // Route to handlers
    if (method == "GET") {
        if (path == "/health") {
            return HandleHealthRequest();
        } 
        else if (path == "/api/status") {
            return HandleStatusRequest();
        }
        else if (path == "/api/tags") {
            return HandleTagsRequest();
        }
        else {
            return HandleNotFound();
        }
    } 
    else if (method == "POST") {
        if (path == "/api/generate") {
            // Extract body
            size_t body_start = request.find("\r\n\r\n");
            std::string body = (body_start != std::string::npos) 
                ? request.substr(body_start + 4) 
                : "";
            return HandleGenerateRequest(body);
        }
        else {
            return HandleNotFound();
        }
    }
    
    return HandleNotFound();
}

// ============================================================
// Server Loop
// ============================================================

void ServerLoop(int port) {
    // Server loop started
    
    while (g_running.load()) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(g_listen_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            if (WSAGetLastError() != WSAEINTR) {
                fprintf(stderr, "[APIServer] accept() failed: %d\n", WSAGetLastError());
            }
            continue;
        }
        
        // Read request
        char buffer[4096];
        int recv_len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            
            // Handle request
            std::string request(buffer);
            std::string response = HandleClientRequest(request);
            
            // Send response
            send(client_socket, response.c_str(), response.length(), 0);
        }
        
        closesocket(client_socket);
    }
    
    // Server loop exited
}

// ============================================================
// Initialization
// ============================================================

bool InitializeServer(int port) {
    // Initializing Winsock
    
    WSADATA wsa_data;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        // WSAStartup failed
        return false;
    }
    
    // Creating listen socket
    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        // socket() failed
        WSACleanup();
        return false;
    }
    
    // Allow socket reuse
    int reuse = 1;
    setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    
    // Bind
    sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(g_listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        // bind() failed
        closesocket(g_listen_socket);
        WSACleanup();
        return false;
    }
    
    // Listen
    if (listen(g_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        // listen() failed
        closesocket(g_listen_socket);
        WSACleanup();
        return false;
    }
    
    // Server listening on port
    return true;
}

// ============================================================
// Smoke Test Mode — Bounded, deterministic validation
// ============================================================

#include <chrono>
#include <thread>

static bool g_smoke_test_mode = false;
static int  g_smoke_timeout_sec = 10;

static std::chrono::steady_clock::time_point SmokeStart() {
    return std::chrono::steady_clock::now();
}

static bool SmokeTimedOut(const std::chrono::steady_clock::time_point& start) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= g_smoke_timeout_sec;
}

static void SmokeLog(const std::string& msg) {
    std::cout << "[SMOKE] " << msg << std::endl;
}

// Forward declare internal HTTP client helper for smoke tests
static std::string SmokeHttpGet(const std::string& path);

static bool Test_OllamaVersion(const std::chrono::steady_clock::time_point& start) {
    if (SmokeTimedOut(start)) {
        SmokeLog("TIMEOUT before Ollama version test");
        return false;
    }

    std::string response = SmokeHttpGet("/api/version");
    if (response.empty() || response.find("version") == std::string::npos) {
        SmokeLog("Ollama version FAILED");
        return false;
    }
    SmokeLog("Ollama version OK");
    return true;
}

static std::once_flag g_models_once;
static std::string    g_models_cached;
static bool           g_models_fetched = false;

static bool Test_ModelDiscovery(const std::chrono::steady_clock::time_point& start) {
    if (SmokeTimedOut(start)) {
        SmokeLog("TIMEOUT before model discovery test");
        return false;
    }

    std::call_once(g_models_once, [&]() {
        std::string response = SmokeHttpGet("/api/tags");
        if (!response.empty() && response.find("models") != std::string::npos) {
            g_models_cached = response;
            g_models_fetched = true;
        }
    });

    if (!g_models_fetched) {
        SmokeLog("Model discovery FAILED");
        return false;
    }
    SmokeLog("Model discovery OK");
    return true;
}

static int RunSmokeTest() {
    SmokeLog("Starting bounded smoke test suite");
    auto start = SmokeStart();

    bool ok = true;
    ok &= Test_OllamaVersion(start);
    ok &= Test_ModelDiscovery(start);

    if (!ok) {
        SmokeLog("FAIL");
        return 1;
    }
    SmokeLog("PASS");
    return 0;
}

// Minimal internal HTTP GET for smoke tests (connects to localhost:port)
static std::string SmokeHttpGet(const std::string& path) {
    // Build full URL from the port we would have listened on
    extern int g_smoke_target_port;
    std::string url = "http://127.0.0.1:" + std::to_string(g_smoke_target_port) + path;

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Smoke/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = { sizeof(urlComp) };
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength  = (DWORD)-1;
    WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp);

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring wpath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    INTERNET_PORT port = urlComp.nPort;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;
    DWORD dwSize = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        std::vector<char> buffer(dwSize + 1);
        DWORD dwRead = 0;
        WinHttpReadData(hRequest, buffer.data(), dwSize, &dwRead);
        buffer[dwRead] = '\0';
        response.append(buffer.data(), dwRead);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static int g_smoke_target_port = 11434;

// ============================================================
// Main Entry Point
// ============================================================

int main(int argc, char* argv[]) {
    // Mandatory GPU gate — the API server refuses to start without a GPU.
    rxd::gpu::require();

    int port = 11434;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (arg == "--smoke-test") {
            g_smoke_test_mode = true;
        } else if (arg == "--smoke-timeout" && i + 1 < argc) {
            g_smoke_timeout_sec = atoi(argv[++i]);
        }
    }
    
    g_smoke_target_port = port;

    // Smoke-test mode: bounded validation, no daemon loop
    if (g_smoke_test_mode) {
        // Start a background thread with the server so we can test against it
        std::thread serverThread([&port]() {
            if (!InitializeServer(port)) {
                std::cerr << "[SMOKE] Failed to initialize server on port " << port << "\n";
                return;
            }
            g_running = true;
            g_start_time = std::chrono::steady_clock::now();
            ServerLoop(port);
        });

        // Give the server a moment to come up
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        int code = RunSmokeTest();

        // Signal shutdown and clean up
        g_running = false;
        if (g_listen_socket != INVALID_SOCKET) {
            closesocket(g_listen_socket);
            g_listen_socket = INVALID_SOCKET;
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
        WSACleanup();
        return code;
    }
    
    // Print banner
    
    // Initialize server
    if (!InitializeServer(port)) {
        // Failed to initialize server
        return 1;
    }
    
    g_running = true;
    g_start_time = std::chrono::steady_clock::now();
    
    // Start server loop in main thread
    ServerLoop(port);
    
    // Cleanup
    closesocket(g_listen_socket);
    WSACleanup();
    
    return 0;
}
