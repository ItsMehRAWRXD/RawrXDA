#pragma once
#include <string>

class HttpServer {
public:
    HttpServer(const std::string& host, int port);
    void Run();
};
