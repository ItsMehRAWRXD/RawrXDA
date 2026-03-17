// main.cpp — Application entry point
// Converted from Qt (QApplication, QMainWindow::show) to pure C++17
// Preserves ALL original initialization: MainWindow, InferenceEngine, HealthCheckServer

#include "MainWindow.h"
#include "HexMagConsole.h"
#include "inference_engine.hpp"
#include "health_check_server.hpp"
#include "unified_hotpatch_manager.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

// Global shutdown flag
static std::atomic<bool> g_running{true};

static void signalHandler(int signum) {
    std::cout << "\n[Main] Signal " << signum << " received, shutting down..." << std::endl;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "[Main] WSAStartup failed: " << wsaResult << std::endl;
        return 1;
    }
#endif

    std::cout << "======================================" << std::endl;
    std::cout << "  RawrXD-Shell — AI Toolkit" << std::endl;
    std::cout << "  Advanced GGUF Model Loader" << std::endl;
    std::cout << "  Three-Layer Hotpatching System" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << std::endl;

    // Parse command-line arguments
    std::string modelPath;
    int serverPort = 8080;
    bool headless  = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--model" || arg == "-m") && i + 1 < argc) {
            modelPath = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            serverPort = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: RawrXD-Shell [options]" << std::endl;
            std::cout << "  --model, -m <path>   Path to GGUF model file" << std::endl;
            std::cout << "  --port, -p <port>    Health check server port (default: 8080)" << std::endl;
            std::cout << "  --help, -h           Show this help" << std::endl;
            return 0;
        }
    }

    // Create application components
    // Original Qt version:
    //   QApplication app(argc, argv);
    //   MainWindow window;
    //   window.resize(1200, 800);
    //   window.show();
    //   return app.exec();

    MainWindow window;
    window.resize(1200, 800);
    window.initialize();
    window.show();

    // Create inference engine
    InferenceEngine engine;
    window.console()->appendLog("InferenceEngine created");

    // Load model if specified
    if (!modelPath.empty()) {
        window.console()->appendLog("Loading model: " + modelPath);
        bool loaded = engine.loadModel(modelPath);
        if (!loaded) {
            window.console()->appendLog("Model load failed");
        } else {
            window.console()->appendLog("Model loaded successfully");
        }
    }

    // Start health check server
    HealthCheckServer healthServer;
    healthServer.registerEndpoint("/status", [&](const std::string&, const std::string&) -> std::string {
        return "{\"status\":\"running\",\"engine\":\"RawrXD-Shell\"}";
    });

    if (healthServer.start(serverPort)) {
        window.console()->appendLog("Health server started on port " + std::to_string(serverPort));
    } else {
        window.console()->appendLog("Failed to start health server");
    }

    std::cout << "[Main] Application running. Press Ctrl+C to exit." << std::endl;

    // Main loop (replaces QApplication::exec())
    while (g_running.load()) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }

    // Cleanup
    std::cout << "[Main] Shutting down..." << std::endl;
    healthServer.stop();
    window.close();

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[Main] Goodbye." << std::endl;
    return 0;
}
