#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <filesystem>
#include <optional>
#include <regex>

#include <nlohmann/json.hpp>
#include "ai_model_caller.h"

#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace RawrXD; 

// ============================================================
// Thread Pool Implementation
// ============================================================

class ServerThreadPool {
public:
    ServerThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ServerThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// ============================================================
// Helper Structs
// ============================================================

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string remote_addr;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

// ============================================================
// GGUFAlpacaServer
// ============================================================

class GGUFAlpacaServer {
public:
    GGUFAlpacaServer(int port) 
        : m_port(port), m_pool(8), m_running(false) {
        
        // Initialize WinSock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
        }
    }

    ~GGUFAlpacaServer() {
        stop();
        WSACleanup();
    }

    void addEndpoint(const std::string& path, std::function<json(const json&)> handler) {
        m_endpoints[path] = handler;
    }

    void run() {
        start();
    }

    void start() {
        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
             std::cerr << "Socket creation failed" << std::endl;
             return;
        }

        int opt = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY; 
        serverAddr.sin_port = htons(m_port);

        if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed" << std::endl;
            return;
        }

        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed" << std::endl;
            return;
        }

        m_running = true;
        std::cout << "Server started on port " << m_port << std::endl;

        while (m_running) {
            sockaddr_in clientAddr;
            int clientLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientLen);

            if (clientSocket == INVALID_SOCKET) {
                continue;
            }

            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
            std::string remoteIp(ipStr);

            // Dispatch to thread pool
            m_pool.enqueue([this, clientSocket, remoteIp]() {
                this->handleConnection(clientSocket, remoteIp);
            });
        }
    }

    void stop() {
        m_running = false;
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
    }

private:
    int m_port;
    ServerThreadPool m_pool;
    std::atomic<bool> m_running;
    SOCKET m_listenSocket = INVALID_SOCKET;
    std::unordered_map<std::string, std::function<json(const json&)>> m_endpoints;

    void handleConnection(SOCKET clientSocket, const std::string& remoteIp) {
        struct SocketGuard {
            SOCKET s;
            ~SocketGuard() { closesocket(s); }
        } guard{clientSocket};

        DWORD timeout = 30000; 
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        std::string rawRequest;
        char buffer[4096];
        int bytesRead;

        while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            rawRequest.append(buffer, bytesRead);
            if (rawRequest.find("\r\n\r\n") != std::string::npos && 
                rawRequest.length() > std::string::npos) break; // Simplified
            if (rawRequest.length() > 81920) break; // Larger limit
            
            // Check content length to break? 
            // For now rely on simplified
        }
        
        if (rawRequest.empty()) return;

        auto req = parseRequest(rawRequest);
        req.remote_addr = remoteIp;
        
        // Read body rest if needed
        if (req.headers.count("Content-Length")) {
            size_t contentLen = std::stoi(req.headers["Content-Length"]);
            size_t headerEnd = rawRequest.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                headerEnd += 4;
                size_t currentBodyLen = rawRequest.length() - headerEnd;
                if (currentBodyLen < contentLen) {
                    size_t remaining = contentLen - currentBodyLen;
                     while (remaining > 0) {
                        int chunk = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (chunk <= 0) break;
                        rawRequest.append(buffer, chunk);
                        remaining -= chunk;
                    }
                }
                req.body = rawRequest.substr(headerEnd);
            }
        }

        HttpResponse res;
        handleRequest(req, res);
        sendResponse(clientSocket, res);
    }

    HttpRequest parseRequest(const std::string& raw) {
        HttpRequest req;
        std::istringstream stream(raw);
        std::string line;

        std::getline(stream, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        std::istringstream lineStream(line);
        lineStream >> req.method >> req.path >> req.version;

        while (std::getline(stream, line) && line != "\r") {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                while (!val.empty() && val[0] == ' ') val.erase(0, 1);
                req.headers[key] = val;
            }
        }
        return req;
    }

    void sendResponse(SOCKET s, const HttpResponse& res) {
        std::ostringstream ss;
        ss << "HTTP/1.1 " << res.status << " " << (res.status == 200 ? "OK" : "Error") << "\r\n";
        ss << "Content-Type: " << res.content_type << "\r\n";
        ss << "Content-Length: " << res.body.length() << "\r\n";
        ss << "Connection: close\r\n";
        ss << "Access-Control-Allow-Origin: *\r\n"; 
        for(const auto& h : res.headers) {
            ss << h.first << ": " << h.second << "\r\n";
        }
        ss << "\r\n";
        ss << res.body;

        std::string data = ss.str();
        send(s, data.c_str(), (int)data.length(), 0);
    }

    void handleRequest(const HttpRequest& req, HttpResponse& res) {
        try {
            // Check dynamic endpoints
            if (m_endpoints.count(req.path)) {
                json bodyJson;
                if (!req.body.empty()) {
                    try {
                        bodyJson = json::parse(req.body);
                    } catch (...) {}
                }
                
                json respJson = m_endpoints[req.path](bodyJson);
                res.body = respJson.dump();
                return;
            }

            // Default
            res.status = 404;
            res.body = R"({"error":"Endpoint not found"})";

        } catch (const std::exception& e) {
            res.status = 500;
            res.body = json{{"error", e.what()}}.dump();
        }
    }
};
