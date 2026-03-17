#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD {

// Forward declaration
struct AppState;

class APIServer {
public:
    using RequestHandler = std::function<std::string(const std::string&)>;

    APIServer(AppState& app_state);
    ~APIServer();

    // Initialize server on given port
    bool Initialize(int port);
    
    // Start server (blocking)
    bool Start();
    
    // Start server in background thread
    bool StartAsync();
    
    // Stop server
    void Stop();
    
    // Check if server is running
    bool IsRunning() const;
    
    // Register API endpoint handler
    void RegisterEndpoint(const std::string& endpoint, RequestHandler handler);
    
    // Get server port
    int GetPort() const;
    
    // Get server status
    std::string GetStatus() const;

private:
    AppState& app_state_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace RawrXD