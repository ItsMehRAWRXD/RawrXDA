#pragma once

class Metrics {
public:
    Metrics() = default;
    ~Metrics() = default;

    void start() {}
    void stop() {}
    // Lightweight stub for starting an internal metrics HTTP server
    bool startServer(int port = 9090) { (void)port; return true; }
};
