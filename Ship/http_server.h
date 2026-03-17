#pragma once

/**
 * @file http_server.h
 * @brief Stub: HTTP server for CLI/agent (used by Ship/src/cli_main.cpp).
 */

#include <string>

class HttpServer {
public:
    HttpServer(const std::string& host, int port);
    void Run();
private:
    std::string m_host;
    int m_port = 11434;
};
