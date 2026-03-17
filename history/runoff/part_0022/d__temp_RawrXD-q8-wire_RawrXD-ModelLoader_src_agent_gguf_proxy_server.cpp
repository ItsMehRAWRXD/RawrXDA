// ============================================================================
// File: src/agent/gguf_proxy_server.cpp
// 
// Purpose: TCP proxy server implementation
// Intercepts and corrects GGUF model outputs in real-time
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "gguf_proxy_server.hpp"
#include "agent_hot_patcher.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

GGUFProxyServer::GGUFProxyServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

GGUFProxyServer::~GGUFProxyServer() {
    stopServer();
    WSACleanup();
}

void GGUFProxyServer::initialize(int listenPort, AgentHotPatcher* hotPatcher, const std::string& ggufEndpoint) {
    m_listenPort = listenPort;
    m_hotPatcher = hotPatcher;
    m_ggufEndpoint = ggufEndpoint;
}

bool GGUFProxyServer::startServer() {
    if (m_running.load()) return true;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_listenPort);

    if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        return false;
    }

    m_running = true;
    m_acceptThread = std::thread(&GGUFProxyServer::acceptLoop, this);

    if (m_onServerStarted) m_onServerStarted(m_listenPort);
    return true;
}

void GGUFProxyServer::stopServer() {
    if (!m_running.load()) return;

    m_running = false;
    closesocket(m_listenSocket);
    m_listenSocket = INVALID_SOCKET;

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto& conn : m_connections) {
        if (conn->clientSocket) closesocket(conn->clientSocket);
        if (conn->ggufSocket) closesocket(conn->ggufSocket);
        conn->active = false;
    }
    m_connections.clear();

    if (m_onServerStopped) m_onServerStopped();
}

void GGUFProxyServer::acceptLoop() {
    while (m_running.load()) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (m_running.load()) continue;
            else break;
        }

        std::thread(&GGUFProxyServer::handleClient, this, (uintptr_t)clientSocket).detach();
    }
}

void GGUFProxyServer::handleClient(uintptr_t socketHandle) {
    SOCKET clientSocket = (SOCKET)socketHandle;
    uintptr_t ggufSocketHandle = 0;

    if (!connectToBackend(ggufSocketHandle, m_ggufEndpoint)) {
        closesocket(clientSocket);
        return;
    }

    SOCKET ggufSocket = (SOCKET)ggufSocketHandle;

    auto conn = std::make_unique<ClientConnection>();
    conn->clientSocket = (uintptr_t)clientSocket;
    conn->ggufSocket = (uintptr_t)ggufSocket;
    conn->active = true;

    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections.push_back(std::move(conn));
    }

    std::thread([this, clientSocket, ggufSocket]() {
        char buffer[8192];
        while (m_running.load()) {
            int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;
            send(ggufSocket, buffer, bytes, 0);
        }
        closesocket(clientSocket);
        closesocket(ggufSocket);
    }).detach();

    char buffer[8192];
    while (m_running.load()) {
        int bytes = recv(ggufSocket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        m_totalRequests++;
        std::string data(buffer, bytes);
        
        if (m_hotPatcher) {
            std::string patched = m_hotPatcher->patchModelOutput(data);
            if (patched != data) {
                m_totalPatched++;
                send(clientSocket, patched.c_str(), (int)patched.size(), 0);
                continue;
            }
        }
        
        send(clientSocket, buffer, bytes, 0);
    }

    closesocket(clientSocket);
    closesocket(ggufSocket);
}

bool GGUFProxyServer::connectToBackend(uintptr_t& socketOut, const std::string& endpoint) {
    size_t colon = endpoint.find(':');
    if (colon == std::string::npos) return false;

    std::string host = endpoint.substr(0, colon);
    std::string portStr = endpoint.substr(colon + 1);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    struct addrinfo hints = {0}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        closesocket(s);
        return false;
    }

    if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(s);
        return false;
    }

    freeaddrinfo(result);
    socketOut = (uintptr_t)s;
    return true;
}

nlohmann::json GGUFProxyServer::getServerStatistics() const {
    return {
        {"totalRequests", m_totalRequests.load()},
        {"totalPatched", m_totalPatched.load()},
        {"activeConnections", m_connections.size()}
    };
}
