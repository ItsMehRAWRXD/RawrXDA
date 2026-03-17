#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef DELETE
#undef DELETE
#endif

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

// SCALAR-ONLY: Integrated HTTP/WebSocket server with no threading
// Runs synchronously in IDE event loop

namespace RawrXD {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
};

struct HttpResponse {
    int status_code;
    std::string body;
    std::string content_type;
    std::map<std::string, std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class ScalarServer {
public:
    ScalarServer(uint16_t port = 8080);
    ~ScalarServer();

    // Server control (scalar, no threading)
    bool Start();
    void Stop();
    void ProcessEvents();  // Called from IDE event loop
    bool IsRunning() const { return is_running_; }

    // Route registration
    void GET(const std::string& path, HttpHandler handler);
    void POST(const std::string& path, HttpHandler handler);
    void PUT(const std::string& path, HttpHandler handler);
    void HttpDelete(const std::string& path, HttpHandler handler);  // Renamed from DELETE to avoid Windows macro

    // WebSocket support (scalar)
    void OnWebSocketMessage(std::function<void(const std::string&, const std::string&)> handler);
    void SendWebSocketMessage(const std::string& client_id, const std::string& message);
    void BroadcastWebSocket(const std::string& message);

private:
    uint16_t port_;
    bool is_running_;
    int server_socket_;
    
    std::map<std::string, HttpHandler> get_routes_;
    std::map<std::string, HttpHandler> post_routes_;
    std::map<std::string, HttpHandler> put_routes_;
    std::map<std::string, HttpHandler> delete_routes_;
    
    std::map<std::string, int> websocket_clients_;
    std::function<void(const std::string&, const std::string&)> ws_message_handler_;
    
    // Scalar HTTP processing
    bool InitSocket();
    void CloseSocket();
    void AcceptConnection();
    void HandleRequest(int client_socket);
    HttpRequest ParseRequest(const std::string& raw_request);
    std::string BuildResponse(const HttpResponse& response);
    bool IsWebSocketUpgrade(const HttpRequest& request);
    void HandleWebSocketHandshake(int client_socket, const HttpRequest& request);
    void HandleWebSocketFrame(int client_socket, const std::string& client_id);
};

} // namespace RawrXD
