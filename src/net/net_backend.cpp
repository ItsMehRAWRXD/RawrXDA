// net_backend.cpp
// C++ backend integration for MASM networking routines

#include "net_masm_bridge.h"
#include <string>
#include <vector>

// Example C++ wrapper for HTTP operations
class HttpClient {
public:
    static std::string Get(const std::string& url) {
        std::vector<char> buffer(8192);
        long long bytes_read = HttpGet(url.c_str(), buffer.data(), buffer.size());
        if (bytes_read > 0) {
            return std::string(buffer.data(), bytes_read);
        }
        return "";
    }

    static std::string Post(const std::string& url, const std::string& data) {
        std::vector<char> buffer(8192);
        long long bytes_read = HttpPost(url.c_str(), data.c_str(), data.size(), buffer.data());
        if (bytes_read > 0) {
            return std::string(buffer.data(), bytes_read);
        }
        return "";
    }
};

// Example C++ wrapper for WebSocket operations
class WebSocketClient {
private:
    void* socket_handle;

public:
    WebSocketClient(const std::string& host, long long port) {
        socket_handle = TcpConnect(host.c_str(), port);
    }

    bool Send(const std::string& data) {
        long long bytes_sent = WebSocketSend(socket_handle, data.c_str(), data.size());
        return bytes_sent > 0;
    }

    std::string Receive() {
        std::vector<char> buffer(8192);
        long long bytes_recv = WebSocketRecv(socket_handle, buffer.data(), buffer.size());
        if (bytes_recv > 0) {
            return std::string(buffer.data(), bytes_recv);
        }
        return "";
    }

    ~WebSocketClient() {
        // TODO: Close socket
    }
};

// Example C++ wrapper for TCP operations
class TcpClient {
private:
    void* socket_handle;

public:
    TcpClient(const std::string& host, long long port) {
        socket_handle = TcpConnect(host.c_str(), port);
    }

    bool Send(const std::string& data) {
        long long bytes_sent = TcpSend(socket_handle, data.c_str(), data.size());
        return bytes_sent > 0;
    }

    std::string Receive() {
        std::vector<char> buffer(8192);
        long long bytes_recv = TcpRecv(socket_handle, buffer.data(), buffer.size());
        if (bytes_recv > 0) {
            return std::string(buffer.data(), bytes_recv);
        }
        return "";
    }

    ~TcpClient() {
        // TODO: Close socket
    }
};
