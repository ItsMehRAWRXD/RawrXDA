// native_core/http_native_server.cpp
#include "http_native_server.hpp"
#include <cstring>

using namespace RawrXD::Native;

bool HttpNativeServer::Start(std::function<std::string(const std::string&)> handler) {
    handler_ = handler;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listenSocket_);
        return false;
    }

    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket_);
        return false;
    }

    running_ = true;
    listenerThread_ = std::thread(&HttpNativeServer::ListenLoop, this);
    return true;
}

void HttpNativeServer::ListenLoop() {
    while (running_) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET client = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (client != INVALID_SOCKET) {
            std::thread(&HttpNativeServer::HandleClient, this, client).detach();
        }
    }
}

void HttpNativeServer::HandleClient(SOCKET client) {
    char buffer[4096];
    int received = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        closesocket(client);
        return;
    }
    buffer[received] = '\0';

    // Simple HTTP/1.1 response
    std::string response = handler_(std::string(buffer));
    std::string http = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
                       std::to_string(response.size()) + "\r\n\r\n" + response;

    send(client, http.c_str(), static_cast<int>(http.size()), 0);
    closesocket(client);
}

void HttpNativeServer::Stop() {
    running_ = false;
    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
    if (listenerThread_.joinable()) listenerThread_.join();
    WSACleanup();
}