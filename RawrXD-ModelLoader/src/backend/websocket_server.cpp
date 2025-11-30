#include "backend/websocket_server.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <random>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace RawrXD {
namespace Backend {

// WebSocket frame opcodes
const uint8_t WS_OP_CONT = 0x0;
const uint8_t WS_OP_TEXT = 0x1;
const uint8_t WS_OP_BINARY = 0x2;
const uint8_t WS_OP_CLOSE = 0x8;
const uint8_t WS_OP_PING = 0x9;
const uint8_t WS_OP_PONG = 0xA;

// WSConnection implementation
WSConnection::WSConnection(int socket_fd, const std::string& id)
    : m_socket(socket_fd), m_id(id), m_is_open(true) {
}

WSConnection::~WSConnection() {
    close();
}

bool WSConnection::sendText(const std::string& message) {
    std::vector<uint8_t> data(message.begin(), message.end());
    return sendFrame(WSMessageType::TEXT, data);
}

bool WSConnection::sendBinary(const std::vector<uint8_t>& data) {
    return sendFrame(WSMessageType::BINARY, data);
}

bool WSConnection::sendPing() {
    return sendFrame(WSMessageType::PING, {});
}

bool WSConnection::close() {
    if (!m_is_open) return false;
    
    m_is_open = false;
    sendFrame(WSMessageType::CLOSE, {});
    
#ifdef _WIN32
    closesocket(m_socket);
#else
    ::close(m_socket);
#endif
    
    return true;
}

bool WSConnection::sendFrame(WSMessageType type, const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(m_send_mutex);
    
    if (!m_is_open) return false;
    
    auto frame = createFrame(type, payload);
    
#ifdef _WIN32
    int sent = send(m_socket, (const char*)frame.data(), frame.size(), 0);
#else
    ssize_t sent = ::send(m_socket, frame.data(), frame.size(), 0);
#endif
    
    return sent == static_cast<int>(frame.size());
}

std::vector<uint8_t> WSConnection::createFrame(WSMessageType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    
    // Byte 0: FIN + opcode
    uint8_t opcode = WS_OP_TEXT;
    switch (type) {
        case WSMessageType::TEXT: opcode = WS_OP_TEXT; break;
        case WSMessageType::BINARY: opcode = WS_OP_BINARY; break;
        case WSMessageType::PING: opcode = WS_OP_PING; break;
        case WSMessageType::PONG: opcode = WS_OP_PONG; break;
        case WSMessageType::CLOSE: opcode = WS_OP_CLOSE; break;
    }
    
    frame.push_back(0x80 | opcode); // FIN=1
    
    // Byte 1+: MASK + payload length
    size_t len = payload.size();
    
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len)); // MASK=0, length
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }
    
    // Payload
    frame.insert(frame.end(), payload.begin(), payload.end());
    
    return frame;
}

// WebSocketServer implementation
WebSocketServer::WebSocketServer(int port)
    : m_port(port), m_server_socket(-1), m_running(false) {
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebSocketServer::~WebSocketServer() {
    stop();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool WebSocketServer::start() {
    if (m_running) return false;
    
    if (!initializeSocket()) {
        return false;
    }
    
    m_running = true;
    m_accept_thread = std::thread(&WebSocketServer::acceptLoop, this);
    
    return true;
}

void WebSocketServer::stop() {
    if (!m_running) return;
    
    m_running = false;
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        for (auto& [id, conn] : m_connections) {
            conn->close();
        }
        m_connections.clear();
    }
    
    closeSocket();
    
    if (m_accept_thread.joinable()) {
        m_accept_thread.join();
    }
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    for (auto& [id, conn] : m_connections) {
        conn->sendText(message);
    }
}

void WebSocketServer::broadcastBinary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    for (auto& [id, conn] : m_connections) {
        conn->sendBinary(data);
    }
}

bool WebSocketServer::sendToClient(const std::string& client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    auto it = m_connections.find(client_id);
    if (it != m_connections.end()) {
        return it->second->sendText(message);
    }
    
    return false;
}

std::vector<std::string> WebSocketServer::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    std::vector<std::string> clients;
    for (const auto& [id, _] : m_connections) {
        clients.push_back(id);
    }
    
    return clients;
}

size_t WebSocketServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    return m_connections.size();
}

bool WebSocketServer::initializeSocket() {
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_socket < 0) {
        return false;
    }
    
    // Allow port reuse
    int opt = 1;
#ifdef _WIN32
    setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(m_port);
    
    if (bind(m_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        closeSocket();
        return false;
    }
    
    if (listen(m_server_socket, 10) < 0) {
        closeSocket();
        return false;
    }
    
    return true;
}

void WebSocketServer::closeSocket() {
    if (m_server_socket >= 0) {
#ifdef _WIN32
        closesocket(m_server_socket);
#else
        ::close(m_server_socket);
#endif
        m_server_socket = -1;
    }
}

void WebSocketServer::acceptLoop() {
    while (m_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
#ifdef _WIN32
        SOCKET client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
#else
        int client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
#endif
            if (m_running && m_on_error) {
                m_on_error("Accept failed");
            }
            continue;
        }
        
        // Perform WebSocket handshake
        if (!performHandshake(client_socket)) {
#ifdef _WIN32
            closesocket(client_socket);
#else
            ::close(client_socket);
#endif
            continue;
        }
        
        // Create client ID and connection
        std::string client_id = generateClientId();
        
        {
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            m_connections[client_id] = std::make_shared<WSConnection>(client_socket, client_id);
        }
        
        if (m_on_connect) {
            m_on_connect(client_id);
        }
        
        // Handle client in new thread
        std::thread client_thread(&WebSocketServer::handleClient, this, client_socket, client_id);
        client_thread.detach();
    }
}

bool WebSocketServer::performHandshake(int client_socket) {
    // Read HTTP handshake request
    char buffer[4096];
#ifdef _WIN32
    int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
#else
    ssize_t received = ::recv(client_socket, buffer, sizeof(buffer) - 1, 0);
#endif
    
    if (received <= 0) return false;
    
    buffer[received] = '\0';
    std::string request(buffer);
    
    // Extract Sec-WebSocket-Key
    size_t key_pos = request.find("Sec-WebSocket-Key:");
    if (key_pos == std::string::npos) return false;
    
    size_t key_start = request.find_first_not_of(" ", key_pos + 18);
    size_t key_end = request.find("\r\n", key_start);
    std::string key = request.substr(key_start, key_end - key_start);
    
    // Calculate accept key (simplified - should use SHA1 + base64)
    std::string accept_key = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    // Send handshake response
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
    response << "\r\n";
    
    std::string resp_str = response.str();
#ifdef _WIN32
    int sent = send(client_socket, resp_str.c_str(), resp_str.length(), 0);
#else
    ssize_t sent = ::send(client_socket, resp_str.c_str(), resp_str.length(), 0);
#endif
    
    return sent == static_cast<int>(resp_str.length());
}

void WebSocketServer::handleClient(int client_socket, const std::string& client_id) {
    char buffer[4096];
    
    while (m_running) {
#ifdef _WIN32
        int received = recv(client_socket, buffer, sizeof(buffer), 0);
#else
        ssize_t received = ::recv(client_socket, buffer, sizeof(buffer), 0);
#endif
        
        if (received <= 0) {
            break;
        }
        
        // Parse WebSocket frame
        std::vector<uint8_t> frame_data(buffer, buffer + received);
        WSMessage message = parseFrame(frame_data);
        
        if (message.type == WSMessageType::CLOSE) {
            break;
        }
        
        if (m_on_message) {
            m_on_message(message);
        }
    }
    
    // Client disconnected
    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        m_connections.erase(client_id);
    }
    
    if (m_on_disconnect) {
        m_on_disconnect(client_id);
    }
}

WSMessage WebSocketServer::parseFrame(const std::vector<uint8_t>& frame_data) {
    WSMessage message;
    
    if (frame_data.size() < 2) return message;
    
    uint8_t opcode = frame_data[0] & 0x0F;
    bool masked = (frame_data[1] & 0x80) != 0;
    uint64_t payload_len = frame_data[1] & 0x7F;
    
    size_t header_len = 2;
    
    if (payload_len == 126) {
        payload_len = (frame_data[2] << 8) | frame_data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | frame_data[2 + i];
        }
        header_len = 10;
    }
    
    // Extract masking key if present
    std::vector<uint8_t> mask_key;
    if (masked) {
        mask_key.assign(frame_data.begin() + header_len, frame_data.begin() + header_len + 4);
        header_len += 4;
    }
    
    // Extract and unmask payload
    message.data.assign(frame_data.begin() + header_len, frame_data.end());
    
    if (masked) {
        for (size_t i = 0; i < message.data.size(); ++i) {
            message.data[i] ^= mask_key[i % 4];
        }
    }
    
    // Set message type
    switch (opcode) {
        case WS_OP_TEXT:
            message.type = WSMessageType::TEXT;
            message.text = std::string(message.data.begin(), message.data.end());
            break;
        case WS_OP_BINARY:
            message.type = WSMessageType::BINARY;
            break;
        case WS_OP_PING:
            message.type = WSMessageType::PING;
            break;
        case WS_OP_PONG:
            message.type = WSMessageType::PONG;
            break;
        case WS_OP_CLOSE:
            message.type = WSMessageType::CLOSE;
            break;
    }
    
    return message;
}

std::string WebSocketServer::generateClientId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << "client-";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }
    
    return oss.str();
}

// BrowserMessage helpers
std::string BrowserMessage::createRequest(const std::string& method,
                                         const std::map<std::string, std::string>& params) {
    std::ostringstream oss;
    oss << "{\"method\":\"" << method << "\",\"params\":{";
    
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) oss << ",";
        oss << "\"" << key << "\":\"" << value << "\"";
        first = false;
    }
    
    oss << "}}";
    return oss.str();
}

std::string BrowserMessage::createResponse(int id, const std::string& result) {
    std::ostringstream oss;
    oss << "{\"id\":" << id << ",\"result\":\"" << result << "\"}";
    return oss.str();
}

std::string BrowserMessage::createError(int id, const std::string& error, int code) {
    std::ostringstream oss;
    oss << "{\"id\":" << id << ",\"error\":{\"code\":" << code 
        << ",\"message\":\"" << error << "\"}}";
    return oss.str();
}

std::string BrowserMessage::createNotification(const std::string& method,
                                              const std::map<std::string, std::string>& params) {
    std::ostringstream oss;
    oss << "{\"method\":\"" << method << "\",\"params\":{";
    
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) oss << ",";
        oss << "\"" << key << "\":\"" << value << "\"";
        first = false;
    }
    
    oss << "}}";
    return oss.str();
}

} // namespace Backend
} // namespace RawrXD
