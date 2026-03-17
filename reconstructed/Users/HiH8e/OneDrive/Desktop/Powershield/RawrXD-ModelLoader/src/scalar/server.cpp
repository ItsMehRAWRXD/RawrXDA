#include "scalar_server.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

#define NOMINMAX
#ifdef DELETE
#undef DELETE
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// SCALAR-ONLY: Integrated HTTP/WebSocket server with no threading

namespace RawrXD {

ScalarServer::ScalarServer(uint16_t port)
    : port_(port), is_running_(false), server_socket_(-1) {
}

ScalarServer::~ScalarServer() {
    Stop();
}

bool ScalarServer::Start() {
    if (is_running_) {
        std::cerr << "Server already running" << std::endl;
        return false;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    if (!InitSocket()) {
        std::cerr << "Failed to initialize socket" << std::endl;
        return false;
    }

    is_running_ = true;
    std::cout << "Scalar Server started on port " << port_ << " (integrated mode)" << std::endl;
    return true;
}

void ScalarServer::Stop() {
    if (!is_running_) return;
    
    is_running_ = false;
    CloseSocket();
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    std::cout << "Scalar Server stopped" << std::endl;
}

void ScalarServer::ProcessEvents() {
    if (!is_running_) return;
    
    // Scalar: non-blocking accept and process
    AcceptConnection();
}

bool ScalarServer::InitSocket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // Set non-blocking mode (scalar event processing)
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server_socket_, FIONBIO, &mode);
#else
    int flags = fcntl(server_socket_, F_GETFL, 0);
    fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK);
#endif

    // Reuse address
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_socket_, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port " << port_ << std::endl;
        CloseSocket();
        return false;
    }

    if (listen(server_socket_, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        CloseSocket();
        return false;
    }

    return true;
}

void ScalarServer::CloseSocket() {
    if (server_socket_ >= 0) {
#ifdef _WIN32
        closesocket(server_socket_);
#else
        close(server_socket_);
#endif
        server_socket_ = -1;
    }
}

void ScalarServer::AcceptConnection() {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_socket = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
    if (client_socket < 0) {
        // No connection available (non-blocking)
        return;
    }

    // Scalar: handle request synchronously
    HandleRequest(client_socket);

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

void ScalarServer::HandleRequest(int client_socket) {
    char buffer[8192];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) return;
    
    buffer[bytes_received] = '\0';
    std::string raw_request(buffer);
    
    HttpRequest request = ParseRequest(raw_request);
    
    // Check for WebSocket upgrade
    if (IsWebSocketUpgrade(request)) {
        HandleWebSocketHandshake(client_socket, request);
        return;
    }
    
    // Route to appropriate handler (scalar)
    HttpResponse response;
    response.status_code = 404;
    response.body = "Not Found";
    response.content_type = "text/plain";
    
    HttpHandler handler = nullptr;
    
    if (request.method == "GET" && get_routes_.count(request.path)) {
        handler = get_routes_[request.path];
    } else if (request.method == "POST" && post_routes_.count(request.path)) {
        handler = post_routes_[request.path];
    } else if (request.method == "PUT" && put_routes_.count(request.path)) {
        handler = put_routes_[request.path];
    } else if (request.method == "DELETE" && delete_routes_.count(request.path)) {
        handler = delete_routes_[request.path];
    }
    
    if (handler) {
        response = handler(request);
    }
    
    std::string response_str = BuildResponse(response);
    send(client_socket, response_str.c_str(), response_str.length(), 0);
}

HttpRequest ScalarServer::ParseRequest(const std::string& raw_request) {
    HttpRequest request;
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse request line (scalar)
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path;
        
        // Parse query parameters
        size_t query_pos = request.path.find('?');
        if (query_pos != std::string::npos) {
            std::string query = request.path.substr(query_pos + 1);
            request.path = request.path.substr(0, query_pos);
            
            // Scalar query parsing
            size_t start = 0;
            while (start < query.length()) {
                size_t eq_pos = query.find('=', start);
                size_t amp_pos = query.find('&', start);
                
                if (eq_pos != std::string::npos) {
                    std::string key = query.substr(start, eq_pos - start);
                    std::string value;
                    
                    if (amp_pos != std::string::npos) {
                        value = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);
                        start = amp_pos + 1;
                    } else {
                        value = query.substr(eq_pos + 1);
                        start = query.length();
                    }
                    
                    request.query_params[key] = value;
                } else {
                    break;
                }
            }
        }
    }
    
    // Parse headers (scalar)
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2);
            
            // Remove \r if present
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            
            request.headers[key] = value;
        }
    }
    
    // Parse body (scalar)
    std::string body;
    while (std::getline(stream, line)) {
        body += line;
    }
    request.body = body;
    
    return request;
}

std::string ScalarServer::BuildResponse(const HttpResponse& response) {
    std::ostringstream stream;
    
    // Status line (scalar)
    stream << "HTTP/1.1 " << response.status_code << " ";
    switch (response.status_code) {
        case 200: stream << "OK"; break;
        case 404: stream << "Not Found"; break;
        case 500: stream << "Internal Server Error"; break;
        default: stream << "Unknown"; break;
    }
    stream << "\r\n";
    
    // Headers (scalar)
    stream << "Content-Type: " << response.content_type << "\r\n";
    stream << "Content-Length: " << response.body.length() << "\r\n";
    stream << "Connection: close\r\n";
    
    for (const auto& header : response.headers) {
        stream << header.first << ": " << header.second << "\r\n";
    }
    
    stream << "\r\n";
    stream << response.body;
    
    return stream.str();
}

bool ScalarServer::IsWebSocketUpgrade(const HttpRequest& request) {
    auto upgrade_it = request.headers.find("Upgrade");
    auto connection_it = request.headers.find("Connection");
    
    return upgrade_it != request.headers.end() && 
           connection_it != request.headers.end() &&
           upgrade_it->second == "websocket";
}

void ScalarServer::HandleWebSocketHandshake(int client_socket, const HttpRequest& request) {
    // Scalar WebSocket handshake
    auto key_it = request.headers.find("Sec-WebSocket-Key");
    if (key_it == request.headers.end()) return;
    
    // Generate accept key (simplified scalar version)
    std::string accept_key = key_it->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
    response << "\r\n";
    
    std::string response_str = response.str();
    send(client_socket, response_str.c_str(), response_str.length(), 0);
    
    // Store client (scalar)
    std::string client_id = "client_" + std::to_string(client_socket);
    websocket_clients_[client_id] = client_socket;
    
    std::cout << "WebSocket client connected: " << client_id << std::endl;
}

void ScalarServer::GET(const std::string& path, HttpHandler handler) {
    get_routes_[path] = handler;
}

void ScalarServer::POST(const std::string& path, HttpHandler handler) {
    post_routes_[path] = handler;
}

void ScalarServer::PUT(const std::string& path, HttpHandler handler) {
    put_routes_[path] = handler;
}

void ScalarServer::HttpDelete(const std::string& path, HttpHandler handler) {
    delete_routes_[path] = handler;
}

void ScalarServer::OnWebSocketMessage(std::function<void(const std::string&, const std::string&)> handler) {
    ws_message_handler_ = handler;
}

void ScalarServer::SendWebSocketMessage(const std::string& client_id, const std::string& message) {
    auto it = websocket_clients_.find(client_id);
    if (it == websocket_clients_.end()) return;
    
    // Scalar WebSocket frame encoding
    std::vector<uint8_t> frame;
    frame.push_back(0x81);  // FIN + text frame
    
    if (message.length() < 126) {
        frame.push_back(static_cast<uint8_t>(message.length()));
    } else if (message.length() < 65536) {
        frame.push_back(126);
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    }
    
    for (char c : message) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    send(it->second, (const char*)frame.data(), frame.size(), 0);
}

void ScalarServer::BroadcastWebSocket(const std::string& message) {
    for (const auto& client : websocket_clients_) {
        SendWebSocketMessage(client.first, message);
    }
}

} // namespace RawrXD
