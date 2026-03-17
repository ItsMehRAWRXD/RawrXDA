/**
 * @file RawrXD_GGUFProxyServer.cpp
 * @brief Win32/Winsock2 GGUF Model Proxy Server implementation
 */

#include "RawrXD_GGUFProxyServer.hpp"
#include <sstream>
#include <chrono>
#include <iostream>

namespace RawrXD::Agent {

GGUFProxyServer::GGUFProxyServer() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "[GGUFProxyServer] WSAStartup failed\n";
    }
}

GGUFProxyServer::~GGUFProxyServer() {
    StopServer();
    WSACleanup();
}

void GGUFProxyServer::Initialize(int listen_port, const std::string& gguf_endpoint) {
    listen_port_ = listen_port;
    gguf_endpoint_ = gguf_endpoint;
}

bool GGUFProxyServer::StartServer() {
    if (listening_) return true;

    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        std::cerr << "[GGUFProxyServer] Failed to create socket: " << WSAGetLastError() << "\n";
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(listen_port_));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[GGUFProxyServer] Bind failed: " << WSAGetLastError() << "\n";
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[GGUFProxyServer] Listen failed: " << WSAGetLastError() << "\n";
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    listening_ = true;
    shutdown_ = false;

    accept_thread_ = std::make_unique<std::thread>([this] { AcceptConnections(); });

    std::cout << "[GGUFProxyServer] Server started on port " << listen_port_ << "\n";
    return true;
}

void GGUFProxyServer::StopServer() {
    if (!listening_) return;

    listening_ = false;
    shutdown_ = true;

    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }

    if (accept_thread_ && accept_thread_->joinable()) {
        accept_thread_->join();
    }

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            if (conn.second->client_socket != INVALID_SOCKET) {
                closesocket(conn.second->client_socket);
            }
            if (conn.second->gguf_socket != INVALID_SOCKET) {
                closesocket(conn.second->gguf_socket);
            }
        }
        connections_.clear();
    }

    std::cout << "[GGUFProxyServer] Server stopped\n";
}

void GGUFProxyServer::AcceptConnections() {
    while (listening_ && !shutdown_) {
        sockaddr_in client_addr = {};
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(listen_socket_, (sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            if (!shutdown_) {
                std::cerr << "[GGUFProxyServer] Accept failed: " << WSAGetLastError() << "\n";
            }
            continue;
        }

        std::lock_guard<std::mutex> lock(connections_mutex_);
        if (connections_.size() >= (size_t)max_connections_) {
            std::cerr << "[GGUFProxyServer] Connection limit reached\n";
            closesocket(client_socket);
            total_errors_++;
            continue;
        }

        auto conn = std::make_unique<GGUFClientConnection>();
        conn->client_socket = client_socket;
        conn->created_time = std::chrono::system_clock::now().time_since_epoch().count();

        std::string client_id = "client_" + std::to_string(total_connections_++);
        conn->client_id = client_id;

        if (on_connection_cb_) {
            on_connection_cb_(client_id);
        }

        auto* conn_ptr = conn.get();
        connections_[client_id] = std::move(conn);

        // Handle connection in separate thread
        std::thread(&GGUFProxyServer::HandleClientConnection, this, conn_ptr).detach();
    }
}

void GGUFProxyServer::HandleClientConnection(GGUFClientConnection* conn) {
    if (!conn) return;

    const int BUFFER_SIZE = 65536;
    std::vector<char> buffer(BUFFER_SIZE);

    // Receive from client
    int bytes_received = recv(conn->client_socket, buffer.data(), BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        total_errors_++;
        return;
    }

    conn->request_buffer.assign(buffer.begin(), buffer.begin() + bytes_received);
    total_requests_++;

    // Forward to GGUF server
    ForwardToGGUF(conn);

    // Send response back to client
    SendResponseToClient(conn);

    // Cleanup
    closesocket(conn->client_socket);
    conn->client_socket = INVALID_SOCKET;
}

void GGUFProxyServer::ForwardToGGUF(GGUFClientConnection* conn) {
    // Parse GGUF endpoint
    std::string host_part = gguf_endpoint_;
    int port = 11434;  // Default Ollama port

    size_t colon_pos = host_part.find(':');
    if (colon_pos != std::string::npos) {
        port = std::stoi(host_part.substr(colon_pos + 1));
        host_part = host_part.substr(0, colon_pos);
    }

    // Create socket to GGUF server
    conn->gguf_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->gguf_socket == INVALID_SOCKET) {
        std::cerr << "[GGUFProxyServer] Failed to create GGUF socket\n";
        return;
    }

    // Set socket timeout
    DWORD timeout_val = socket_timeout_ms_;
    setsockopt(conn->gguf_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_val, sizeof(timeout_val));
    setsockopt(conn->gguf_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_val, sizeof(timeout_val));

    // Connect to GGUF
    sockaddr_in gguf_addr = {};
    gguf_addr.sin_family = AF_INET;
    gguf_addr.sin_port = htons(static_cast<u_short>(port));
    gguf_addr.sin_addr.s_addr = inet_addr(host_part.c_str());

    if (connect(conn->gguf_socket, (sockaddr*)&gguf_addr, sizeof(gguf_addr)) == SOCKET_ERROR) {
        std::cerr << "[GGUFProxyServer] Failed to connect to GGUF server: " << WSAGetLastError() << "\n";
        closesocket(conn->gguf_socket);
        conn->gguf_socket = INVALID_SOCKET;
        return;
    }

    // Send request to GGUF
    if (send(conn->gguf_socket, (const char*)conn->request_buffer.data(), (int)conn->request_buffer.size(), 0) == SOCKET_ERROR) {
        std::cerr << "[GGUFProxyServer] Failed to send to GGUF\n";
        closesocket(conn->gguf_socket);
        conn->gguf_socket = INVALID_SOCKET;
        return;
    }

    // Receive response from GGUF
    std::vector<char> buffer(65536);
    int bytes_received = recv(conn->gguf_socket, buffer.data(), (int)buffer.size(), 0);
    if (bytes_received > 0) {
        conn->response_buffer.assign(buffer.begin(), buffer.begin() + bytes_received);
    }

    closesocket(conn->gguf_socket);
    conn->gguf_socket = INVALID_SOCKET;
}

void GGUFProxyServer::SendResponseToClient(GGUFClientConnection* conn) {
    if (conn->response_buffer.empty()) {
        // Send error response
        std::string error_msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(conn->client_socket, error_msg.c_str(), (int)error_msg.size(), 0);
        return;
    }

    // Send response back to client
    if (send(conn->client_socket, (const char*)conn->response_buffer.data(), (int)conn->response_buffer.size(), 0) == SOCKET_ERROR) {
        std::cerr << "[GGUFProxyServer] Failed to send response to client\n";
    }
}

std::string GGUFProxyServer::GetServerStatistics() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    std::ostringstream oss;
    oss << "{\n"
        << "  \"active_connections\": " << connections_.size() << ",\n"
        << "  \"total_connections\": " << total_connections_ << ",\n"
        << "  \"total_requests\": " << total_requests_ << ",\n"
        << "  \"total_errors\": " << total_errors_ << ",\n"
        << "  \"listening_port\": " << listen_port_ << "\n"
        << "}\n";
    return oss.str();
}

} // namespace RawrXD::Agent
