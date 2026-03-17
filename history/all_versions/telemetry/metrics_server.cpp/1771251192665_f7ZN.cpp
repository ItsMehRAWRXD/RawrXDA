// telemetry/metrics_server.cpp
#include "metrics_server.hpp"
#include <sstream>
#include <iostream>

namespace RawrXD {

MetricsServer::MetricsServer() : server_socket_(INVALID_SOCKET), running_(false) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

MetricsServer::~MetricsServer() {
    stop();
    WSACleanup();
}

bool MetricsServer::start(int port) {
    if (running_) return true;
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET) return false;
    
    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<u_short>(port));
    
    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), 
             sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }
    
    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }
    
    running_ = true;
    server_thread_ = std::thread(&MetricsServer::serverThread, this);
    
    return true;
}

void MetricsServer::stop() {
    if (!running_) return;
    
    running_ = false;
    closesocket(server_socket_);
    server_socket_ = INVALID_SOCKET;
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void MetricsServer::setMetric(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_[name] = value;
}

void MetricsServer::incrementMetric(const std::string& name, double delta) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_[name] += delta;
}

void MetricsServer::serverThread() {
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket_, &readfds);
        
        struct timeval tv = {1, 0}; // 1 second timeout
        
        int result = select(0, &readfds, nullptr, nullptr, &tv);
        if (result > 0 && FD_ISSET(server_socket_, &readfds)) {
            SOCKET client_socket = accept(server_socket_, nullptr, nullptr);
            if (client_socket != INVALID_SOCKET) {
                std::thread(&MetricsServer::handleClient, this, client_socket).detach();
            }
        }
    }
}

void MetricsServer::handleClient(SOCKET client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        
        // Simple HTTP response for /metrics
        if (strstr(buffer, "GET /metrics")) {
            std::string response = generatePrometheusOutput();
            
            std::string http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(response.size()) + "\r\n"
                "\r\n" + response;
            
            send(client_socket, http_response.c_str(), 
                 static_cast<int>(http_response.size()), 0);
        }
    }
    
    closesocket(client_socket);
}

std::string MetricsServer::generatePrometheusOutput() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::stringstream ss;
    ss << "# RawrXD Native Metrics\n";
    
    for (const auto& metric : metrics_) {
        ss << "rawrxd_" << metric.first << " " << metric.second << "\n";
    }
    
    return ss.str();
}

} // namespace RawrXD