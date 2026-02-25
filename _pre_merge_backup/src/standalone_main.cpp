#include "standalone_web_bridge.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

static uint16_t parse_u16(const char* s, uint16_t def) {
    if (!s || !*s) return def;
    long v = std::strtol(s, nullptr, 10);
    if (v <= 0 || v > 65535) return def;
    return static_cast<uint16_t>(v);
}

int main(int argc, char** argv) {
    StandaloneWebBridgeServer::Config cfg{};
    cfg.http_port = (argc > 1) ? parse_u16(argv[1], cfg.http_port) : cfg.http_port;
    cfg.ws_port = (argc > 2) ? parse_u16(argv[2], cfg.ws_port) : cfg.ws_port;
    cfg.gguf_port = (argc > 3) ? parse_u16(argv[3], cfg.gguf_port) : cfg.gguf_port;
    cfg.gguf_host = (argc > 4) ? argv[4] : cfg.gguf_host;
    cfg.web_root = (argc > 5) ? argv[5] : cfg.web_root;

    std::cout << "RawrXD-Standalone-WebBridge\n";
    std::cout << "HTTP: http://127.0.0.1:" << cfg.http_port << "\n";
    std::cout << "WS:   ws://127.0.0.1:" << cfg.ws_port << "/ws\n";
    std::cout << "WEB:  " << cfg.web_root << "\n";

    StandaloneWebBridgeServer server(cfg);
    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

#ifdef _WIN32
    static HANDLE g_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler([](DWORD ctrl) -> BOOL {
        if (ctrl == CTRL_C_EVENT || ctrl == CTRL_CLOSE_EVENT || ctrl == CTRL_BREAK_EVENT) {
            if (g_stop_event) SetEvent(g_stop_event);
            return TRUE;
        }
        return FALSE;
    }, TRUE);

    std::cout << "Running. Press Ctrl+C to stop...\n";
    WaitForSingleObject(g_stop_event, INFINITE);
#else
    std::cout << "Running. Press Ctrl+C to stop...\n";
    // Portable fallback: block on stdin until EOF.
    std::string line;
    while (std::getline(std::cin, line)) {}
#endif
    server.stop();
    return 0;
}
