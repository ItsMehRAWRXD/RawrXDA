// ============================================================================
// RawrXD_HttpServer.cpp — Real Threaded HTTP Server (WinSock2)
// Replaces skeleton with actual socket listener + thread pool + route dispatch
// ============================================================================
#include "RawrXD_HttpServer.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <sstream>
#include <iostream>
#include <atomic>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {

// ============================================================================
// HTTP Request/Response structs
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    std::string ToString() const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << GetStatusText(status_code) << "\r\n";
        for (const auto& h : headers) {
            oss << h.first << ": " << h.second << "\r\n";
        }
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n";
        oss << "\r\n";
        oss << body;
        return oss.str();
    }

    static const char* GetStatusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 500: return "Internal Server Error";
            case 503: return "Service Unavailable";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// Route handlers
// ============================================================================
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

static std::mutex g_routesMutex;
static std::unordered_map<std::string, std::unordered_map<std::string, RouteHandler>> g_routes;

static void RegisterRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(g_routesMutex);
    g_routes[method][path] = handler;
}

static HttpResponse DispatchRoute(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(g_routesMutex);
    auto methodIt = g_routes.find(req.method);
    if (methodIt == g_routes.end()) {
        HttpResponse res; res.status_code = 405;
        res.body = "{\"error\":\"Method not allowed\"}";
        res.headers.push_back({"Content-Type", "application/json"});
        return res;
    }
    auto pathIt = methodIt->second.find(req.path);
    if (pathIt == methodIt->second.end()) {
        HttpResponse res; res.status_code = 404;
        res.body = "{\"error\":\"Not found\"}";
        res.headers.push_back({"Content-Type", "application/json"});
        return res;
    }
    return pathIt->second(req);
}

// ============================================================================
// HTTP parsing
// ============================================================================
static bool ParseHttpRequest(const std::string& raw, HttpRequest& req) {
    size_t pos = 0;
    size_t line_end = raw.find("\r\n", pos);
    if (line_end == std::string::npos) return false;

    std::string request_line = raw.substr(pos, line_end - pos);
    pos = line_end + 2;

    // Parse request line: METHOD PATH VERSION
    std::istringstream iss(request_line);
    iss >> req.method >> req.path >> req.version;
    if (req.method.empty() || req.path.empty()) return false;

    // Parse headers
    while (true) {
        line_end = raw.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string line = raw.substr(pos, line_end - pos);
        pos = line_end + 2;
        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading space
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) value = value.substr(start);
            req.headers.push_back({key, value});
        }
    }

    // Body follows the empty line
    if (pos + 2 <= raw.size()) {
        req.body = raw.substr(pos + 2);
    }

    return true;
}

// ============================================================================
// Socket handling
// ============================================================================
static std::atomic<bool> g_serverRunning{false};
static SOCKET g_listenSocket = INVALID_SOCKET;
static std::vector<std::thread> g_workerThreads;
static std::mutex g_clientMutex;
static std::queue<SOCKET> g_clientQueue;
static std::condition_variable g_clientCv;

static void HandleClient(SOCKET clientSocket) {
    char buffer[8192] = {};
    int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        closesocket(clientSocket);
        return;
    }
    buffer[received] = '\0';

    HttpRequest req;
    if (!ParseHttpRequest(std::string(buffer), req)) {
        HttpResponse res; res.status_code = 400;
        res.body = "{\"error\":\"Bad request\"}";
        res.headers.push_back({"Content-Type", "application/json"});
        std::string response = res.ToString();
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(clientSocket);
        return;
    }

    // Dispatch to route handler
    HttpResponse res = DispatchRoute(req);
    std::string response = res.ToString();
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    closesocket(clientSocket);
}

static void WorkerThread() {
    while (g_serverRunning) {
        SOCKET client;
        {
            std::unique_lock<std::mutex> lock(g_clientMutex);
            g_clientCv.wait(lock, [] { return !g_clientQueue.empty() || !g_serverRunning; });
            if (!g_serverRunning) break;
            if (g_clientQueue.empty()) continue;
            client = g_clientQueue.front();
            g_clientQueue.pop();
        }
        HandleClient(client);
    }
}

static void AcceptThread() {
    while (g_serverRunning) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_listenSocket, &readfds);

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int result = select(0, &readfds, nullptr, nullptr, &tv);
        if (result > 0 && FD_ISSET(g_listenSocket, &readfds)) {
            SOCKET client = accept(g_listenSocket, nullptr, nullptr);
            if (client != INVALID_SOCKET) {
                std::lock_guard<std::mutex> lock(g_clientMutex);
                g_clientQueue.push(client);
                g_clientCv.notify_one();
            }
        }
    }
}

// ============================================================================
// Public API
// ============================================================================
bool InitializeHttpServer(uint16_t port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[HttpServer] WSAStartup failed" << std::endl;
        return false;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) {
        std::cerr << "[HttpServer] socket() failed" << std::endl;
        WSACleanup();
        return false;
    }

    // Allow address reuse
    int reuse = 1;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(g_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] bind() failed on port " << port << ", trying fallback ports..." << std::endl;
        closesocket(g_listenSocket);

        // Try fallback ports 11435-11439
        for (uint16_t fallback = 11435; fallback <= 11439; ++fallback) {
            g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (g_listenSocket == INVALID_SOCKET) continue;
            setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
            addr.sin_port = htons(fallback);
            if (bind(g_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
                port = fallback;
                std::cout << "[HttpServer] Bound to fallback port " << port << std::endl;
                goto bound_ok;
            }
            closesocket(g_listenSocket);
        }

        std::cerr << "[HttpServer] All ports failed" << std::endl;
        WSACleanup();
        return false;
    }

bound_ok:
    if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] listen() failed" << std::endl;
        closesocket(g_listenSocket);
        WSACleanup();
        return false;
    }

    g_serverRunning = true;

    // Start accept thread
    g_workerThreads.emplace_back(AcceptThread);

    // Start worker threads (4 workers)
    for (int i = 0; i < 4; ++i) {
        g_workerThreads.emplace_back(WorkerThread);
    }

    std::cout << "[HttpServer] Listening on port " << port << " (4 worker threads)" << std::endl;
    return true;
}

void ShutdownHttpServer() {
    g_serverRunning = false;
    g_clientCv.notify_all();

    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }

    for (auto& t : g_workerThreads) {
        if (t.joinable()) t.join();
    }
    g_workerThreads.clear();

    WSACleanup();
    std::cout << "[HttpServer] Shutdown complete" << std::endl;
}

void RegisterHttpRoute(const std::string& method, const std::string& path, std::function<std::string(const std::string&)> handler) {
    RegisterRoute(method, path, [handler](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.body = handler(req.body);
        res.headers.push_back({"Content-Type", "application/json"});
        return res;
    });
}

bool IsHttpServerRunning() {
    return g_serverRunning;
}

} // namespace RawrXD
