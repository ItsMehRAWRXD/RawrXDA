#include "standalone_web_bridge.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>

// Global flag for clean shutdown
volatile bool g_running = true;

void signalHandler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int httpPort = 8080;
    int wsPort = 8081;
    int ggufPort = 11434;
    std::string ggufEndpoint = "http://localhost:11434";
    std::string webRoot = ".";

    if (argc > 1) httpPort = std::atoi(argv[1]);
    if (argc > 2) wsPort = std::atoi(argv[2]);
    if (argc > 3) ggufPort = std::atoi(argv[3]);
    if (argc > 4) ggufEndpoint = argv[4];
    if (argc > 5) webRoot = argv[5];

    std::cout << "=========================================" << std::endl;
    std::cout << "   RawrXD Standalone Web Bridge Server" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "HTTP Port: " << httpPort << std::endl;
    std::cout << "WebSocket Port: " << wsPort << std::endl;
    std::cout << "GGUF Port: " << ggufPort << std::endl;
    std::cout << "GGUF Endpoint: " << ggufEndpoint << std::endl;
    std::cout << "Web Root: " << webRoot << std::endl;
    std::cout << "=========================================" << std::endl;

    // Set up signal handlers for clean shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    try {
        // Create agent hot patcher (placeholder - implement as needed)
        AgentHotPatcher* hotPatcher = nullptr; // TODO: Create actual instance

        // Create and initialize the standalone web bridge server
        StandaloneWebBridgeServer server(httpPort, wsPort, hotPatcher);

        if (!server.initialize()) {
            std::cerr << "[Main] Failed to initialize server" << std::endl;
            return 1;
        }

        // Configure static file serving
        server.serveStaticFiles(webRoot);

        // Start the server
        if (!server.start()) {
            std::cerr << "[Main] Failed to start server" << std::endl;
            return 1;
        }

        std::cout << std::endl;
        std::cout << "🚀 Server started successfully!" << std::endl;
        std::cout << "🌐 Open your browser to: " << server.getServerUrl() << std::endl;
        std::cout << "📁 Serving files from: " << webRoot << std::endl;
        std::cout << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        std::cout << "=========================================" << std::endl;

        // Main loop - keep running until signal received
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n[Main] Shutting down server..." << std::endl;
        server.stop();

        std::cout << "[Main] Server stopped. Goodbye!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}