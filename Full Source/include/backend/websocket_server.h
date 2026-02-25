#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RawrXD {
namespace Backend {

enum class WSMessageType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

struct WSMessage {
    WSMessageType type{WSMessageType::TEXT};
    std::string text;
    std::vector<uint8_t> data;
};

class WSConnection {
public:
    WSConnection(int socket_fd, const std::string& id);
    ~WSConnection();

    bool sendText(const std::string& message);
    bool sendBinary(const std::vector<uint8_t>& data);
    bool sendPing();
    bool close();

    const std::string& id() const { return m_id; }
    bool isOpen() const { return m_is_open; }

private:
    bool sendFrame(WSMessageType type, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> createFrame(WSMessageType type, const std::vector<uint8_t>& payload);

    int m_socket;
    std::string m_id;
    bool m_is_open;
    std::mutex m_send_mutex;
};

class WebSocketServer {
public:
    using MessageCallback = std::function<void(const WSMessage&)>;
    using ConnectionCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    explicit WebSocketServer(int port);
    ~WebSocketServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    void broadcast(const std::string& message);
    void broadcastBinary(const std::vector<uint8_t>& data);
    bool sendToClient(const std::string& client_id, const std::string& message);

    std::vector<std::string> getConnectedClients() const;
    size_t getClientCount() const;

    void setOnMessage(MessageCallback cb) { m_on_message = std::move(cb); }
    void setOnConnect(ConnectionCallback cb) { m_on_connect = std::move(cb); }
    void setOnDisconnect(ConnectionCallback cb) { m_on_disconnect = std::move(cb); }
    void setOnError(ErrorCallback cb) { m_on_error = std::move(cb); }

private:
    bool initializeSocket();
    void closeSocket();
    void acceptLoop();
    bool performHandshake(int client_socket);
    void handleClient(int client_socket, const std::string& client_id);
    WSMessage parseFrame(const std::vector<uint8_t>& frame_data);
    std::string generateClientId();

    int m_port;
    int m_server_socket;
    bool m_running;
    std::thread m_accept_thread;
    std::unordered_map<std::string, std::shared_ptr<WSConnection>> m_connections;
    mutable std::mutex m_connections_mutex;

    MessageCallback m_on_message;
    ConnectionCallback m_on_connect;
    ConnectionCallback m_on_disconnect;
    ErrorCallback m_on_error;
};

struct BrowserMessage {
    static std::string createRequest(const std::string& method,
                                     const std::map<std::string, std::string>& params);
    static std::string createResponse(int id, const std::string& result);
    static std::string createError(int id, const std::string& error, int code);
    static std::string createNotification(const std::string& method,
                                          const std::map<std::string, std::string>& params);
};

} // namespace Backend
} // namespace RawrXD
