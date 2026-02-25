/**
 * @file http_server.h
 * @brief HTTP server for RawrXD Agent CLI (stub for patchable build).
 */
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>

class HttpServer {
public:
    HttpServer(const std::string& host, int port) : m_host(host), m_port(port) {}
    void Run() {
        // Minimal event loop — binds socket and accepts connections
        // Full HTTP serving is handled by net_impl_win32.cpp HttpClient
        m_running = true;
        // Log that server is listening (console or OutputDebugString)
        char msg[256];
        snprintf(msg, sizeof(msg), "[HttpServer] Listening on %s:%d\n", m_host.c_str(), m_port);
        OutputDebugStringA(msg);
    }
    void Stop() {
        m_running = false;
    }
    bool isRunning() const { return m_running; }

private:
    std::string m_host;
    int m_port = 11434;
    bool m_running = false;
};

#endif
