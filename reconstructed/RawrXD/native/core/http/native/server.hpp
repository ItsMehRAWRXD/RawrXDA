// native_core/http_native_server.hpp
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <functional>
#include <string>

// Link with: ws2_32.lib (system lib—acceptable as it's OS component)

namespace RawrXD::Native {

class HttpNativeServer {
    SOCKET listenSocket_ = INVALID_SOCKET;
    std::thread listenerThread_;
    std::function<std::string(const std::string&)> handler_;
    bool running_ = false;
    uint16_t port_;

    void ListenLoop();
    void HandleClient(SOCKET client);

public:
    explicit HttpNativeServer(uint16_t port = 9090) : port_(port) {}
    ~HttpNativeServer() { Stop(); }

    bool Start(std::function<std::string(const std::string&)> handler);
    void Stop();
    bool IsRunning() const { return running_; }
};

} // namespace RawrXD::Native