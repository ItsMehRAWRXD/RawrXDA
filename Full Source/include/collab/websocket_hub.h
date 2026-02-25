#ifndef WEBSOCKET_HUB_H
#define WEBSOCKET_HUB_H

Please ensure everything is correct and present without lacking any source regardless if its supposed to not be in the repo or not IE keygen for unlock 800b kernel along with others IE titan agents that arent wired and toggable
// C++20 / Win32. WebSocket server; no Qt. Use platform WS API in impl.

#include <string>
#include <functional>
#include <vector>

struct WebSocketClient;  // Opaque per-client handle

class WebSocketHub
{
public:
    using MessageReceivedFn = std::function<void(const std::string& messageJson, void* client)>;

    WebSocketHub() = default;
    ~WebSocketHub();

    void setOnMessageReceived(MessageReceivedFn f) { m_onMessageReceived = std::move(f); }
    bool startServer(uint16_t port = 5173);
    void broadcastMessage(const std::string& messageJson);

private:
    void onNewConnection();
    void onTextMessageReceived(const std::string& message, void* client);
    void onSocketDisconnected(void* client);

    void* m_server = nullptr;
    std::vector<void*> m_clients;
    MessageReceivedFn m_onMessageReceived;
};

#endif // WEBSOCKET_HUB_H
