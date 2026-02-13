/**
 * Simple GGUF API Server - Lightweight Ollama-Compatible HTTP API
 * Provides real HTTP endpoints with actual inference backend via EngineRegistry
 * Demonstrates working Winsock HTTP server for model serving
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include "engine_iface.h"

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

// Global state
static std::atomic<bool> g_running(false);
static SOCKET g_listen_socket = INVALID_SOCKET;
static std::chrono::steady_clock::time_point g_start_time;
static std::string g_active_model;   // currently loaded model name
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

    printf("[INFER] Engine=%s prompt_len=%zu response_len=%d %.1fms (%.0f tok/s)\n",
           engine->name(), prompt.size(), eval_count, elapsed_ms, tokens_per_sec);

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
    
    printf("[REQ] %s %s\n", method.c_str(), path.c_str());
    
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
    printf("[HTTP] Server loop started on port %d\n", port);
    
    while (g_running.load()) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(g_listen_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            if (WSAGetLastError() != WSAEINTR) {
                printf("[HTTP] accept() failed: %d\n", WSAGetLastError());
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
    
    printf("[HTTP] Server loop exited\n");
}

// ============================================================
// Initialization
// ============================================================

bool InitializeServer(int port) {
    printf("[HTTP] Initializing Winsock...\n");
    
    WSADATA wsa_data;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        printf("[HTTP] WSAStartup failed: %d\n", wsa_result);
        return false;
    }
    
    printf("[HTTP] Creating listen socket...\n");
    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        printf("[HTTP] socket() failed: %d\n", WSAGetLastError());
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
        printf("[HTTP] bind() failed: %d\n", WSAGetLastError());
        closesocket(g_listen_socket);
        WSACleanup();
        return false;
    }
    
    // Listen
    if (listen(g_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("[HTTP] listen() failed: %d\n", WSAGetLastError());
        closesocket(g_listen_socket);
        WSACleanup();
        return false;
    }
    
    printf("[HTTP] Server listening on port %d\n", port);
    return true;
}

// ============================================================
// Main Entry Point
// ============================================================

int main(int argc, char* argv[]) {
    int port = 11434;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   Simple GGUF API Server - Ollama-Compatible HTTP      ║\n");
    printf("║              Real HTTP Server Implementation           ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    // Initialize server
    if (!InitializeServer(port)) {
        printf("FATAL: Failed to initialize server\n");
        return 1;
    }
    
    g_running = true;
    g_start_time = std::chrono::steady_clock::now();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║         Listening for Inference Requests               ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("Endpoints:\n");
    printf("  GET  http://localhost:%d/health\n", port);
    printf("  GET  http://localhost:%d/api/tags\n", port);
    printf("  GET  http://localhost:%d/api/status\n", port);
    printf("  POST http://localhost:%d/api/generate\n\n", port);
    
    printf("Press Ctrl+C to exit.\n\n");
    
    // Start server loop in main thread
    ServerLoop(port);
    
    // Cleanup
    closesocket(g_listen_socket);
    WSACleanup();
    
    return 0;
}
