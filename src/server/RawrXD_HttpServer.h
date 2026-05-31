#pragma once
#include <string>
#include <functional>
#include <cstdint>

namespace RawrXD {

// Initialize the HTTP server on the specified port
// Returns true on success
bool InitializeHttpServer(uint16_t port = 8080);

// Shutdown the HTTP server
void ShutdownHttpServer();

// Register a route handler
// method: "GET", "POST", etc.
// path: route path (e.g., "/api/chat")
// handler: function that takes request body string and returns response body string
void RegisterHttpRoute(const std::string& method, const std::string& path, std::function<std::string(const std::string&)> handler);

// Check if server is running
bool IsHttpServerRunning();

} // namespace RawrXD
