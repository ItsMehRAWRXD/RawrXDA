#include <iostream>
#include <string>
#include <csignal>
#include <vector>
#include <thread>
#include <chrono>
#include "memory_core.h"
#include "cpu_inference_engine.h"
#include "complete_server.h"

void SignalHandler(int signal) {
    std::cout << "\n[ENGINE] Exiting...\n";
    exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandler);
    std::cout << "[SYSTEM] RawrXD Engine Ready - Minimal Build\n";

    std::string model_path;
    uint16_t port = 8080;
    bool enable_http = true;
    bool enable_repl = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--no-http") {
            enable_http = false;
        } else if (arg == "--no-repl") {
            enable_repl = false;
        }
    }

    RawrXD::CPUInferenceEngine engine;
    if (!model_path.empty()) {
        std::cout << "[SYSTEM] Loading model: " << model_path << "\n";
        if (!engine.LoadModel(model_path)) {
            std::cout << "[SYSTEM] Model load failed. /complete will return empty results.\n";
        } else {
            std::cout << "[SYSTEM] Model loaded.\n";
        }
    } else {
        std::cout << "[SYSTEM] No model specified. /complete will return empty results.\n";
    }

    RawrXD::CompletionServer server;
    if (enable_http) {
        server.Start(port, &engine, model_path);
    }

    if (enable_repl) {
        std::string input;
        while (true) {
            std::cout << "RawrXD> ";
            std::getline(std::cin, input);
            if (input == "exit") break;
            std::cout << "Echo: " << input << "\n";
        }
    } else {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    server.Stop();
    return 0;
}
