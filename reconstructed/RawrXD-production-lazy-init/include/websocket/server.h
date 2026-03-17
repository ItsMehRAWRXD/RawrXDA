#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Forward declaration
struct AppState;

// WebSocket frame types
enum class WSFrameType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

// WebSocket message structure
struct WSMessage {
    WSFrameType type;
    std::string payload;
    bool is_final = true;
};

// WebSocket connection
struct WSConnection {
    int socket_fd;
    std::string client_id;
    std::chrono::steady_clock::time_point last_ping;
    std::atomic<bool> is_active{true};
    std::mutex send_mutex;
};

// WebSocket server for real-time streaming
class WebSocketServer {
public:
    explicit WebSocketServer(AppState& app_state);
    ~WebSocketServer();
    
    // Start WebSocket server on specified port
    bool Start(uint16_t port, int max_connections = 100);
    
    // Stop WebSocket server
    void Stop();
    
    // Check if running
    bool IsRunning() const { return is_running_.load(); }
    
    // Get current port
    uint16_t GetPort() const { return port_; }
    
    // Get active connection count
    int GetActiveConnections() const { return static_cast<int>(connections_.size()); }
    
    // Broadcast message to all connected clients
    void BroadcastMessage(const std::string& message);
    
    // Send message to specific client
    bool SendToClient(const std::string& client_id, const std::string& message);
    
    // Register message handler
    using MessageHandler = std::function<void(const std::string& client_id, const std::string& message)>;
    void SetMessageHandler(MessageHandler handler);
    
    // Streaming inference support
    void StreamInferenceChunk(const std::string& client_id, const std::string& chunk);
    void StreamInferenceComplete(const std::string& client_id);
    void StreamInferenceError(const std::string& client_id, const std::string& error);

private:
    AppState& app_state_;
    std::atomic<bool> is_running_;
    uint16_t port_;
    int max_connections_;
    
    std::vector<std::shared_ptr<WSConnection>> connections_;
    std::mutex connections_mutex_;
    
    std::unique_ptr<std::thread> server_thread_;
    std::unique_ptr<std::thread> ping_thread_;
    
    MessageHandler message_handler_;
    std::mutex handler_mutex_;
    
    // Connection management
    void AcceptConnections();
    void HandleConnection(std::shared_ptr<WSConnection> conn);
    void RemoveConnection(const std::string& client_id);
    
    // WebSocket protocol handling
    bool PerformHandshake(int socket_fd);
    std::string GenerateAcceptKey(const std::string& client_key);
    
    // Frame encoding/decoding
    bool SendFrame(std::shared_ptr<WSConnection> conn, const WSMessage& msg);
    bool ReceiveFrame(std::shared_ptr<WSConnection> conn, WSMessage& msg);
    
    // Ping/pong for keep-alive
    void SendPeriodicPings();
    void SendPing(std::shared_ptr<WSConnection> conn);
    void SendPong(std::shared_ptr<WSConnection> conn);
    
    // Logging
    void LogWSEvent(const std::string& event, const std::string& details);
};
