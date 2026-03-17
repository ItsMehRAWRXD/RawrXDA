#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

// SCALAR-ONLY: All threading/async removed - synchronous scalar operations

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace RawrXD {
namespace Backend {

// WebSocket message types
enum class WSMessageType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

struct WSMessage {
    WSMessageType type;
    std::vector<uint8_t> data;
    std::string text; // convenience for TEXT messages
};

// Callback types
using WSMessageCallback = std::function<void(const WSMessage&)>;
using WSConnectionCallback = std::function<void(const std::string& client_id)>;
using WSErrorCallback = std::function<void(const std::string& error)>;

// WebSocket connection state (scalar, no threading)
class WSConnection {
public:
    WSConnection(int socket_fd, const std::string& id);
    ~WSConnection();
    
    std::string getId() const { return m_id; }
    bool sendText(const std::string& message);
    bool sendBinary(const std::vector<uint8_t>& data);
    bool sendPing();
    bool close();
    bool isOpen() const { return m_is_open; }  // Scalar: no atomic needed
    
private:
    int m_socket;
    std::string m_id;
    bool m_is_open;  // Scalar: no atomic needed
    
    bool sendFrame(WSMessageType type, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> createFrame(WSMessageType type, const std::vector<uint8_t>& payload);
};

// WebSocket Server (scalar, no threading)
class WebSocketServer {
public:
    WebSocketServer(int port = 9001);
    ~WebSocketServer();
    
    // Server control
    bool start();
    void stop();
    bool isRunning() const { return m_running; }  // Scalar: no atomic needed
    int getPort() const { return m_port; }
    
    // Callbacks
    void onMessage(WSMessageCallback callback) { m_on_message = callback; }
    void onConnect(WSConnectionCallback callback) { m_on_connect = callback; }
    void onDisconnect(WSConnectionCallback callback) { m_on_disconnect = callback; }
    void onError(WSErrorCallback callback) { m_on_error = callback; }
    
    // Broadcasting
    void broadcast(const std::string& message);
    void broadcastBinary(const std::vector<uint8_t>& data);
    
    // Client management
    bool sendToClient(const std::string& client_id, const std::string& message);
    bool sendBinaryToClient(const std::string& client_id, const std::vector<uint8_t>& data);
    bool closeClient(const std::string& client_id);
    std::vector<std::string> getConnectedClients() const;
    size_t getClientCount() const;

private:
    int m_port;
    int m_server_socket;
    bool m_running;  // Scalar: no atomic needed
    
    // Client connections
    std::map<std::string, std::shared_ptr<WSConnection>> m_connections;
    
    // Callbacks
    WSMessageCallback m_on_message;
    WSConnectionCallback m_on_connect;
    WSConnectionCallback m_on_disconnect;
    WSErrorCallback m_on_error;
    
    // Server operations (scalar, synchronous)
    void acceptLoop();
    void handleClient(int client_socket, const std::string& client_id);
    bool performHandshake(int client_socket);
    WSMessage parseFrame(const std::vector<uint8_t>& frame_data);
    std::string generateClientId();
    
    // Socket utilities
    bool initializeSocket();
    void closeSocket();
};

// Helper: JSON-RPC message builder for browser communication
class BrowserMessage {
public:
    static std::string createRequest(const std::string& method, 
                                     const std::map<std::string, std::string>& params);
    static std::string createResponse(int id, const std::string& result);
    static std::string createError(int id, const std::string& error, int code = -1);
    static std::string createNotification(const std::string& method, 
                                         const std::map<std::string, std::string>& params);
};

} // namespace Backend
} // namespace RawrXD
