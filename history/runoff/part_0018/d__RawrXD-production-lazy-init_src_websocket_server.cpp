#include "websocket_server.h"
#include "app_state.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

WebSocketServer::WebSocketServer(AppState& app_state)
    : app_state_(app_state), is_running_(false), port_(0), max_connections_(100) {
}

WebSocketServer::~WebSocketServer() {
    Stop();
}

void WebSocketServer::LogWSEvent(const std::string& event, const std::string& details) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
              << "] [WebSocket] [" << event << "] " << details << std::endl;
}

bool WebSocketServer::Start(uint16_t port, int max_connections) {
    if (is_running_.load()) {
        LogWSEvent("WARN", "WebSocket server already running");
        return false;
    }
    
    port_ = port;
    max_connections_ = max_connections;
    is_running_ = true;
    
    LogWSEvent("INFO", "WebSocket server starting on port " + std::to_string(port));
    
    // Start server thread
    server_thread_ = std::make_unique<std::thread>(&WebSocketServer::AcceptConnections, this);
    
    // Start ping thread for keep-alive
    ping_thread_ = std::make_unique<std::thread>(&WebSocketServer::SendPeriodicPings, this);
    
    LogWSEvent("INFO", "WebSocket server started successfully");
    return true;
}

void WebSocketServer::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    is_running_ = false;
    LogWSEvent("INFO", "WebSocket server stopping...");
    
    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->is_active = false;
            closesocket(conn->socket_fd);
        }
        connections_.clear();
    }
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    if (ping_thread_ && ping_thread_->joinable()) {
        ping_thread_->join();
    }
    
    LogWSEvent("INFO", "WebSocket server stopped");
}

void WebSocketServer::AcceptConnections() {
    // Placeholder implementation
    // In production, this would create a socket, bind, listen, and accept connections
    LogWSEvent("INFO", "Accept connections thread started");
    
    while (is_running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // TODO: Accept new WebSocket connections
    }
}

void WebSocketServer::HandleConnection(std::shared_ptr<WSConnection> conn) {
    // Placeholder implementation
    LogWSEvent("INFO", "Handling connection from client: " + conn->client_id);
    
    while (conn->is_active.load() && is_running_.load()) {
        WSMessage msg;
        if (ReceiveFrame(conn, msg)) {
            if (msg.type == WSFrameType::TEXT) {
                // Handle message
                std::lock_guard<std::mutex> lock(handler_mutex_);
                if (message_handler_) {
                    message_handler_(conn->client_id, msg.payload);
                }
            } else if (msg.type == WSFrameType::PING) {
                SendPong(conn);
            } else if (msg.type == WSFrameType::CLOSE) {
                break;
            }
        }
    }
    
    RemoveConnection(conn->client_id);
}

void WebSocketServer::RemoveConnection(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [&client_id](const std::shared_ptr<WSConnection>& conn) {
                return conn->client_id == client_id;
            }),
        connections_.end()
    );
    LogWSEvent("INFO", "Removed connection: " + client_id);
}

bool WebSocketServer::PerformHandshake(int socket_fd) {
    // Placeholder - real implementation would perform WebSocket handshake
    return true;
}

std::string WebSocketServer::GenerateAcceptKey(const std::string& client_key) {
    // Placeholder - real implementation would generate proper Sec-WebSocket-Accept
    return client_key + "_accepted";
}

bool WebSocketServer::SendFrame(std::shared_ptr<WSConnection> conn, const WSMessage& msg) {
    if (!conn->is_active.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(conn->send_mutex);
    // Placeholder - real implementation would encode and send WebSocket frame
    return true;
}

bool WebSocketServer::ReceiveFrame(std::shared_ptr<WSConnection> conn, WSMessage& msg) {
    // Placeholder - real implementation would receive and decode WebSocket frame
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return false;
}

void WebSocketServer::SendPeriodicPings() {
    while (is_running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            if (conn->is_active.load()) {
                SendPing(conn);
            }
        }
    }
}

void WebSocketServer::SendPing(std::shared_ptr<WSConnection> conn) {
    WSMessage ping_msg;
    ping_msg.type = WSFrameType::PING;
    ping_msg.payload = "ping";
    SendFrame(conn, ping_msg);
}

void WebSocketServer::SendPong(std::shared_ptr<WSConnection> conn) {
    WSMessage pong_msg;
    pong_msg.type = WSFrameType::PONG;
    pong_msg.payload = "pong";
    SendFrame(conn, pong_msg);
}

void WebSocketServer::BroadcastMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& conn : connections_) {
        if (conn->is_active.load()) {
            WSMessage msg;
            msg.type = WSFrameType::TEXT;
            msg.payload = message;
            SendFrame(conn, msg);
        }
    }
}

bool WebSocketServer::SendToClient(const std::string& client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& conn : connections_) {
        if (conn->client_id == client_id && conn->is_active.load()) {
            WSMessage msg;
            msg.type = WSFrameType::TEXT;
            msg.payload = message;
            return SendFrame(conn, msg);
        }
    }
    return false;
}

void WebSocketServer::SetMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    message_handler_ = handler;
}

void WebSocketServer::StreamInferenceChunk(const std::string& client_id, const std::string& chunk) {
    // Format as SSE-style chunk for streaming
    std::string formatted = "data: " + chunk + "\n\n";
    SendToClient(client_id, formatted);
}

void WebSocketServer::StreamInferenceComplete(const std::string& client_id) {
    std::string done_msg = "data: [DONE]\n\n";
    SendToClient(client_id, done_msg);
}

void WebSocketServer::StreamInferenceError(const std::string& client_id, const std::string& error) {
    std::string error_msg = "error: " + error + "\n\n";
    SendToClient(client_id, error_msg);
}
