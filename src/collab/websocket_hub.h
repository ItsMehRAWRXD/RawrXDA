// ============================================================================
// websocket_hub.h — WebSocket Collaboration Hub (Win32 Winsock)
// ============================================================================
// RFC 6455 WebSocket server for real-time collaboration transport.
// No external dependencies — pure Win32/Winsock2.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>

namespace RawrXD {

class WebSocketHub {
public:
    WebSocketHub() = default;
    ~WebSocketHub();

    WebSocketHub(const WebSocketHub&) = delete;
    WebSocketHub& operator=(const WebSocketHub&) = delete;

    bool startServer(uint16_t port);
    void stopServer();
    bool isRunning() const { return m_running.load(); }
    uint16_t getPort() const { return m_port; }

    void broadcastMessage(const std::string& messageJson);
    bool sendTextToClient(void* clientContext, const std::string& text);

    size_t getClientCount() const {
        std::lock_guard<std::mutex> lk(m_clientsMutex);
        return m_clients.size();
    }

    // Callback: invoked on message receipt (message, clientCtx)
    std::function<void(const std::string&, void*)> m_onMessageReceived;

    // Setter alias for backward compat
    void setOnMessageReceived(std::function<void(const std::string&, void*)> fn) { m_onMessageReceived = std::move(fn); }

    // Callback: invoked when client connects/disconnects
    std::function<void(void*, bool /*connected*/)> m_onClientChanged;

private:
    static void serverThreadFn(WebSocketHub* self);
    void serverLoop();
    void acceptOne();
    void handleClient(void* clientContext);
    static std::string makeWebSocketAcceptKey(const std::string& key);

    void* m_listenSocket        = nullptr;
    uint16_t m_port             = 0;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;

    mutable std::mutex m_clientsMutex;
    std::vector<void*> m_clients;
};

} // namespace RawrXD
