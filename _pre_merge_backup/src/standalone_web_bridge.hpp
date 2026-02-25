#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

// Minimal, Qt-free local web bridge: HTTP (static file + /api/status) and optional WebSocket echo.
// This is intentionally dependency-light so it can build cleanly in Visual Studio with /MT.
class StandaloneWebBridgeServer {
public:
    struct Config {
        uint16_t http_port = 8080;
        uint16_t ws_port = 8081;
        uint16_t gguf_port = 11434;
        std::string gguf_host = "127.0.0.1";
        std::string web_root = ".";
        std::string default_model = "mistral";
    };

    explicit StandaloneWebBridgeServer(Config cfg);
    ~StandaloneWebBridgeServer();

    bool start();
    void stop();

    StandaloneWebBridgeServer(const StandaloneWebBridgeServer&) = delete;
    StandaloneWebBridgeServer& operator=(const StandaloneWebBridgeServer&) = delete;

private:
    void run_http();
    void run_ws();
    void handle_http_client(int client_socket);
    void handle_ws_client(int client_socket);
    void resolve_default_web_root();

    Config m_cfg;
    std::atomic<bool> m_running{false};
    std::thread m_http_thread;
    std::thread m_ws_thread;
    int m_http_listen_socket = -1;
    int m_ws_listen_socket = -1;

    std::atomic<uint64_t> m_http_requests{0};
    std::atomic<uint64_t> m_ws_requests{0};
    std::atomic<uint64_t> m_model_requests{0};
    uint64_t m_start_tick = 0;
};
