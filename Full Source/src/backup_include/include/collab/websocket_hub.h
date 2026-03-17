#ifndef WEBSOCKET_HUB_H
#define WEBSOCKET_HUB_H

// C++20 / Win32. WebSocket server; no Qt. Uses Winsock in impl.

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

struct WebSocketClient;  // Opaque: wraps SOCKET + state

class WebSocketHub
{
public:
    using MessageReceivedFn = std::function<void(const std::string& messageJson, void* client)>;

    WebSocketHub() = default;
    ~WebSocketHub();

    void setOnMessageReceived(MessageReceivedFn f) { m_onMessageReceived = std::move(f); }
    bool startServer(uint16_t port = 5173);
    void stopServer();
    void broadcastMessage(const std::string& messageJson);
    bool isRunning() const { return m_running.load(); }
    uint16_t getPort() const { return m_port; }

private:
    static void serverThreadFn(WebSocketHub* self);
    void serverLoop();
    void acceptOne();
    void handleClient(void* clientContext);
    bool sendTextToClient(void* clientContext, const std::string& text);
    static std::string makeWebSocketAcceptKey(const std::string& key);

    std::mutex m_clientsMutex;
    std::vector<void*> m_clients;
    MessageReceivedFn m_onMessageReceived;
    void* m_listenSocket = nullptr;  // SOCKET
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    uint16_t m_port = 0;
};

#endif // WEBSOCKET_HUB_H
