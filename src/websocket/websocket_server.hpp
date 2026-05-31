/**
 * @file websocket_server.hpp
 * @brief Production WebSocket Server for Real-Time Collaboration
 * 
 * Provides:
 * - WebSocket protocol (RFC 6455) implementation
 * - Real-time cursor sharing
 * - Live document synchronization
 * - Presence indicators
 * - Room-based collaboration
 * - Binary and text message support
 * - Ping/pong heartbeat
 * - Connection management
 * 
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD::WebSocket {

// ============================================================================
// WebSocket Configuration
// ============================================================================

struct WebSocketConfig {
    uint16_t port = 8080;
    std::string host = "0.0.0.0";
    uint32_t maxConnections = 1000;
    uint32_t maxMessageSize = 10 * 1024 * 1024;  // 10 MB
    uint32_t pingIntervalMs = 30000;              // 30 seconds
    uint32_t pongTimeoutMs = 10000;               // 10 seconds
    uint32_t connectionTimeoutMs = 60000;         // 1 minute
    bool enableCompression = true;
    bool enableSSL = false;
    std::string sslCertPath;
    std::string sslKeyPath;
    std::string originAllowed;
};

// ============================================================================
// WebSocket Frame Types
// ============================================================================

enum class OpCode : uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
};

// ============================================================================
// WebSocket Frame
// ============================================================================

struct WebSocketFrame {
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    OpCode opcode = OpCode::Text;
    bool masked = false;
    uint64_t payloadLength = 0;
    uint32_t maskingKey = 0;
    std::vector<uint8_t> payload;
    
    // Parse from raw bytes
    static bool parse(const std::vector<uint8_t>& data, WebSocketFrame& frame, size_t& bytesConsumed);
    
    // Serialize to raw bytes
    std::vector<uint8_t> serialize() const;
    
    // Check if valid
    bool isValid() const;
};

// ============================================================================
// WebSocket Message
// ============================================================================

struct WebSocketMessage {
    OpCode opcode = OpCode::Text;
    std::vector<uint8_t> data;
    std::string text() const { return std::string(data.begin(), data.end()); }
    bool isText() const { return opcode == OpCode::Text; }
    bool isBinary() const { return opcode == OpCode::Binary; }
    bool isClose() const { return opcode == OpCode::Close; }
    bool isPing() const { return opcode == OpCode::Ping; }
    bool isPong() const { return opcode == OpCode::Pong; }
};

// ============================================================================
// WebSocket Connection
// ============================================================================

class WebSocketConnection {
public:
    using MessageCallback = std::function<void(const WebSocketMessage&)>;
    using CloseCallback = std::function<void(uint16_t code, const std::string& reason)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    WebSocketConnection(SOCKET socket, const std::string& id);
    ~WebSocketConnection();
    
    // Connection info
    std::string getId() const { return m_id; }
    SOCKET getSocket() const { return m_socket; }
    std::string getRemoteAddress() const;
    uint16_t getRemotePort() const;
    bool isConnected() const { return m_connected; }
    std::chrono::steady_clock::time_point getConnectTime() const { return m_connectTime; }
    
    // Send messages
    bool sendText(const std::string& message);
    bool sendBinary(const std::vector<uint8_t>& data);
    bool sendPing(const std::vector<uint8_t>& payload = {});
    bool sendPong(const std::vector<uint8_t>& payload = {});
    bool sendClose(uint16_t code = 1000, const std::string& reason = "");
    
    // Receive handling
    void setMessageCallback(MessageCallback callback) { m_messageCallback = callback; }
    void setCloseCallback(CloseCallback callback) { m_closeCallback = callback; }
    void setErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }
    
    // Connection management
    void close(uint16_t code = 1000, const std::string& reason = "Normal closure");
    void markAsClosed() { m_connected = false; }
    
    // Statistics
    uint64_t getBytesReceived() const { return m_bytesReceived; }
    uint64_t getBytesSent() const { return m_bytesSent; }
    uint64_t getMessagesReceived() const { return m_messagesReceived; }
    uint64_t getMessagesSent() const { return m_messagesSent; }
    
    // User data (for application state)
    void setUserData(void* data) { m_userData = data; }
    void* getUserData() const { return m_userData; }
    
    // Room management
    void joinRoom(const std::string& roomId);
    void leaveRoom(const std::string& roomId);
    std::vector<std::string> getRooms() const;
    
private:
    SOCKET m_socket;
    std::string m_id;
    std::atomic<bool> m_connected{true};
    std::chrono::steady_clock::time_point m_connectTime;
    
    // Statistics
    std::atomic<uint64_t> m_bytesReceived{0};
    std::atomic<uint64_t> m_bytesSent{0};
    std::atomic<uint64_t> m_messagesReceived{0};
    std::atomic<uint64_t> m_messagesSent{0};
    
    // Callbacks
    MessageCallback m_messageCallback;
    CloseCallback m_closeCallback;
    ErrorCallback m_errorCallback;
    
    // User data
    void* m_userData = nullptr;
    
    // Rooms
    mutable std::mutex m_roomsMutex;
    std::vector<std::string> m_rooms;
    
    // Internal
    bool sendFrame(const WebSocketFrame& frame);
    bool receiveFrame(WebSocketFrame& frame);
    std::vector<uint8_t> receiveBytes(size_t size);
};

// ============================================================================
// WebSocket Room
// ============================================================================

class WebSocketRoom {
public:
    explicit WebSocketRoom(const std::string& id);
    ~WebSocketRoom() = default;
    
    std::string getId() const { return m_id; }
    
    // Connection management
    void addConnection(std::shared_ptr<WebSocketConnection> connection);
    void removeConnection(const std::string& connectionId);
    bool hasConnection(const std::string& connectionId) const;
    size_t getConnectionCount() const;
    std::vector<std::shared_ptr<WebSocketConnection>> getConnections() const;
    
    // Broadcast
    void broadcast(const std::string& message, const std::string& excludeId = "");
    void broadcastBinary(const std::vector<uint8_t>& data, const std::string& excludeId = "");
    
    // Room state
    void setState(const std::string& key, const std::string& value);
    std::string getState(const std::string& key) const;
    void clearState() { m_state.clear(); }
    
private:
    std::string m_id;
    mutable std::mutex m_mutex;
    std::map<std::string, std::weak_ptr<WebSocketConnection>> m_connections;
    std::map<std::string, std::string> m_state;
};

// ============================================================================
// WebSocket Server
// ============================================================================

class WebSocketServer {
public:
    using ConnectionCallback = std::function<void(std::shared_ptr<WebSocketConnection>)>;
    using MessageCallback = std::function<void(std::shared_ptr<WebSocketConnection>, const WebSocketMessage&)>;
    using RoomCallback = std::function<void(const std::string& roomId, const std::string& connectionId)>;
    
    explicit WebSocketServer(const WebSocketConfig& config = WebSocketConfig{});
    ~WebSocketServer();
    
    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Configuration
    void setConfig(const WebSocketConfig& config);
    WebSocketConfig getConfig() const { return m_config; }
    
    // Callbacks
    void setConnectionCallback(ConnectionCallback callback) { m_connectionCallback = callback; }
    void setMessageCallback(MessageCallback callback) { m_messageCallback = callback; }
    void setDisconnectionCallback(ConnectionCallback callback) { m_disconnectionCallback = callback; }
    void setRoomJoinCallback(RoomCallback callback) { m_roomJoinCallback = callback; }
    void setRoomLeaveCallback(RoomCallback callback) { m_roomLeaveCallback = callback; }
    
    // Connection management
    std::shared_ptr<WebSocketConnection> getConnection(const std::string& id) const;
    std::vector<std::shared_ptr<WebSocketConnection>> getAllConnections() const;
    size_t getConnectionCount() const;
    void closeConnection(const std::string& id, uint16_t code = 1000, const std::string& reason = "Normal closure");
    void closeAllConnections(uint16_t code = 1000, const std::string& reason = "Server shutdown");
    
    // Room management
    std::shared_ptr<WebSocketRoom> getRoom(const std::string& roomId);
    std::shared_ptr<WebSocketRoom> getOrCreateRoom(const std::string& roomId);
    void removeRoom(const std::string& roomId);
    std::vector<std::string> getRoomIds() const;
    size_t getRoomCount() const;
    
    // Broadcast
    void broadcast(const std::string& message, const std::string& excludeId = "");
    void broadcastBinary(const std::vector<uint8_t>& data, const std::string& excludeId = "");
    void broadcastToRoom(const std::string& roomId, const std::string& message, const std::string& excludeId = "");
    
    // Statistics
    uint64_t getTotalBytesReceived() const;
    uint64_t getTotalBytesSent() const;
    uint64_t getTotalMessagesReceived() const;
    uint64_t getTotalMessagesSent() const;
    uint64_t getTotalConnections() const;
    
private:
    WebSocketConfig m_config;
    SOCKET m_listenSocket = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    
    // Threads
    std::thread m_acceptThread;
    std::vector<std::thread> m_workerThreads;
    
    // Connections
    mutable std::mutex m_connectionsMutex;
    std::map<std::string, std::shared_ptr<WebSocketConnection>> m_connections;
    std::atomic<uint64_t> m_connectionCounter{0};
    
    // Rooms
    mutable std::mutex m_roomsMutex;
    std::map<std::string, std::shared_ptr<WebSocketRoom>> m_rooms;
    
    // Statistics
    std::atomic<uint64_t> m_totalBytesReceived{0};
    std::atomic<uint64_t> m_totalBytesSent{0};
    std::atomic<uint64_t> m_totalMessagesReceived{0};
    std::atomic<uint64_t> m_totalMessagesSent{0};
    std::atomic<uint64_t> m_totalConnections{0};
    
    // Callbacks
    ConnectionCallback m_connectionCallback;
    MessageCallback m_messageCallback;
    ConnectionCallback m_disconnectionCallback;
    RoomCallback m_roomJoinCallback;
    RoomCallback m_roomLeaveCallback;
    
    // Internal
    bool initializeWinsock();
    void cleanupWinsock();
    bool createListenSocket();
    void acceptLoop();
    void handleConnection(std::shared_ptr<WebSocketConnection> connection);
    bool performHandshake(std::shared_ptr<WebSocketConnection> connection);
    std::string generateAcceptKey(const std::string& clientKey);
    std::string generateConnectionId();
    void cleanupStaleConnections();
};

// ============================================================================
// WebSocket Client
// ============================================================================

class WebSocketClient {
public:
    using MessageCallback = std::function<void(const WebSocketMessage&)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    WebSocketClient();
    ~WebSocketClient();
    
    // Connection
    bool connect(const std::string& url);
    bool connect(const std::string& host, uint16_t port, const std::string& path = "/", bool secure = false);
    void disconnect(uint16_t code = 1000, const std::string& reason = "Normal closure");
    bool isConnected() const { return m_connected; }
    
    // Send messages
    bool sendText(const std::string& message);
    bool sendBinary(const std::vector<uint8_t>& data);
    bool sendPing(const std::vector<uint8_t>& payload = {});
    
    // Callbacks
    void setMessageCallback(MessageCallback callback) { m_messageCallback = callback; }
    void setConnectionCallback(ConnectionCallback callback) { m_connectionCallback = callback; }
    void setDisconnectionCallback(ConnectionCallback callback) { m_disconnectionCallback = callback; }
    void setErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }
    
    // Statistics
    uint64_t getBytesReceived() const { return m_bytesReceived; }
    uint64_t getBytesSent() const { return m_bytesSent; }
    
private:
    SOCKET m_socket = INVALID_SOCKET;
    std::atomic<bool> m_connected{false};
    std::thread m_receiveThread;
    
    // Statistics
    std::atomic<uint64_t> m_bytesReceived{0};
    std::atomic<uint64_t> m_bytesSent{0};
    
    // Callbacks
    MessageCallback m_messageCallback;
    ConnectionCallback m_connectionCallback;
    ConnectionCallback m_disconnectionCallback;
    ErrorCallback m_errorCallback;
    
    // Internal
    bool performHandshake(const std::string& host, uint16_t port, const std::string& path, bool secure);
    void receiveLoop();
    bool sendFrame(const WebSocketFrame& frame);
    bool receiveFrame(WebSocketFrame& frame);
};

// ============================================================================
// Real-Time Collaboration Types
// ============================================================================

namespace Collaboration {

// Cursor position
struct CursorPosition {
    std::string fileId;
    int line = 0;
    int column = 0;
    std::string selection;  // Optional selection text
};

// Document operation
struct DocumentOperation {
    enum class Type {
        Insert,
        Delete,
        Replace
    };
    
    Type type = Type::Insert;
    std::string fileId;
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string text;
    uint64_t version = 0;
    uint64_t timestamp = 0;
    std::string userId;
};

// Presence information
struct Presence {
    std::string userId;
    std::string userName;
    std::string fileId;
    CursorPosition cursor;
    std::string status;  // "active", "idle", "away"
    std::chrono::steady_clock::time_point lastActivity;
    std::string color;  // User color for cursor
};

// Document state
struct DocumentState {
    std::string fileId;
    std::string content;
    uint64_t version = 0;
    std::string language;
    std::chrono::steady_clock::time_point lastModified;
    std::string lastModifiedBy;
};

// Collaboration message types
enum class MessageType : uint8_t {
    CursorMove = 1,
    CursorBroadcast = 2,
    DocumentEdit = 3,
    DocumentSync = 4,
    PresenceJoin = 5,
    PresenceLeave = 6,
    PresenceUpdate = 7,
    FileOpen = 8,
    FileClose = 9,
    ChatMessage = 10,
    UserTyping = 11,
    SelectionChange = 12,
    RequestSync = 13,
    AcknowledgeEdit = 14,
    RejectEdit = 15
};

// Collaboration message
struct CollaborationMessage {
    MessageType type = MessageType::CursorMove;
    std::string userId;
    std::string roomId;
    uint64_t timestamp = 0;
    std::string payload;  // JSON payload
    
    // Serialize
    std::string serialize() const;
    static CollaborationMessage deserialize(const std::string& json);
};

} // namespace Collaboration

// ============================================================================
// Collaboration Manager
// ============================================================================

class CollaborationManager {
public:
    using PresenceCallback = std::function<void(const Collaboration::Presence&)>;
    using DocumentCallback = std::function<void(const Collaboration::DocumentOperation&)>;
    using ChatCallback = std::function<void(const std::string& userId, const std::string& message)>;
    
    CollaborationManager();
    ~CollaborationManager();
    
    // Initialize with WebSocket server
    bool initialize(WebSocketServer* server);
    void shutdown();
    
    // Room management
    std::string createRoom(const std::string& fileId);
    void destroyRoom(const std::string& roomId);
    std::shared_ptr<WebSocketRoom> getRoom(const std::string& roomId);
    
    // User management
    void userJoined(const std::string& roomId, const std::string& userId, const std::string& userName);
    void userLeft(const std::string& roomId, const std::string& userId);
    std::vector<Collaboration::Presence> getPresence(const std::string& roomId);
    
    // Document operations
    void applyEdit(const std::string& roomId, const Collaboration::DocumentOperation& op);
    void broadcastEdit(const std::string& roomId, const Collaboration::DocumentOperation& op, const std::string& excludeUserId);
    Collaboration::DocumentState getDocumentState(const std::string& roomId);
    
    // Cursor operations
    void updateCursor(const std::string& roomId, const std::string& userId, const Collaboration::CursorPosition& cursor);
    void broadcastCursor(const std::string& roomId, const std::string& userId, const Collaboration::CursorPosition& cursor);
    
    // Chat
    void sendChatMessage(const std::string& roomId, const std::string& userId, const std::string& message);
    
    // Callbacks
    void setPresenceCallback(PresenceCallback callback) { m_presenceCallback = callback; }
    void setDocumentCallback(DocumentCallback callback) { m_documentCallback = callback; }
    void setChatCallback(ChatCallback callback) { m_chatCallback = callback; }
    
    // Statistics
    size_t getActiveRoomCount() const;
    size_t getActiveUserCount() const;
    
private:
    WebSocketServer* m_server = nullptr;
    
    mutable std::mutex m_mutex;
    std::map<std::string, std::vector<Collaboration::Presence>> m_roomPresence;
    std::map<std::string, Collaboration::DocumentState> m_documentStates;
    std::map<std::string, std::string> m_userColors;
    
    PresenceCallback m_presenceCallback;
    DocumentCallback m_documentCallback;
    ChatCallback m_chatCallback;
    
    // Internal
    void handleMessage(std::shared_ptr<WebSocketConnection> connection, const WebSocketMessage& message);
    void handleCursorMove(const Collaboration::CollaborationMessage& msg);
    void handleDocumentEdit(const Collaboration::CollaborationMessage& msg);
    void handlePresenceJoin(const Collaboration::CollaborationMessage& msg);
    void handlePresenceLeave(const Collaboration::CollaborationMessage& msg);
    void handleChatMessage(const Collaboration::CollaborationMessage& msg);
    
    std::string generateUserColor(const std::string& userId);
    std::string serializePresence(const std::vector<Collaboration::Presence>& presence);
};

} // namespace RawrXD::WebSocket
