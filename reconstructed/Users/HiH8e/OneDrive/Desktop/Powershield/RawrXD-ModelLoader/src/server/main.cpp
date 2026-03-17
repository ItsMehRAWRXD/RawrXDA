#include "api_server.h"
#include "settings.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_shutdown_requested{false};

void SignalHandler(int signal) {
    std::cout << "\nShutdown requested (signal " << signal << ")..." << std::endl;
    g_shutdown_requested.store(true);
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    std::cout << "RawrXD Inference Server" << std::endl;
    std::cout << "======================" << std::endl;
    
    // Create minimal app state (no GUI, settings initialized to defaults)
    AppState app_state;
    app_state.use_gpu = true;
    app_state.use_fallback_api = false;
    
    // Create and start API server
    APIServer server(app_state);
    
    uint16_t port = 11434;
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port number, using default 11434" << std::endl;
        }
    }
    
    if (!server.Start(port)) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }
    
    std::cout << "Server running on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // Keep server running until shutdown requested
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Stopping server..." << std::endl;
    server.Stop();
    
    std::cout << "Server stopped." << std::endl;
    return 0;
}
