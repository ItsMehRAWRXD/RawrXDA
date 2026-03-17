#include "gui.h"
#include "../include/ai_integration_hub.h" // Use Hub instead of direct engine
// #include "cpu_inference_engine.h"
#include "agentic_engine.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <conio.h>
#include <string>
#include <filesystem>

using namespace std::chrono_literals;

// Helper: Interactive Chat Mode
void RunInteractiveChat(AppState& state, std::shared_ptr<AIIntegrationHub> hub) {
    if (!state.loaded_model) {
        std::cout << "[Info] Hub initialized (model status managed internally).\n";
    }

    std::cout << "\n=== RawrXD Interactive Chat (Hub Integration) ===\n";
    std::cout << "Type 'exit' to return to menu.\n";
    std::cout << "Commands: /load <path>, /audit <path>\n";
    
    // Clear buffer
    while (_kbhit()) _getch();

    std::string input;
    while (true) {
        std::cout << "\n>> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "exit" || input == "quit") break;
        
        // Hub handles commands like /audit. For /load, we can use Hub API.
        if (input.rfind("/load ", 0) == 0) {
            std::string path = input.substr(6);
            if (hub->loadModel(path)) {
                 std::cout << "[System] Model loaded: " << path << "\n";
                 state.loaded_model = true;
            } else {
                 std::cout << "[Error] Failed to load model: " << path << "\n";
            }
            continue;
        }

        if (input.empty()) continue;

        std::cout << "RawrXD: ";
        
        // Use Hub Chat (supports /audit)
        std::string response = hub->chat(input);
        std::cout << response << std::endl;
    }
    std::cout << "=== Exiting Chat ===\n";
}

int main(int argc, char** argv) {
    AppState state;
    
    // 0. Parse Args for Model
    if (argc > 1) {
        state.model_path = argv[1];
    }

    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    std::cout << "RawrXD CLI v2.0 - Hub Integration Build\n";
    
    // 1. Initialize Hub
    auto hub = std::make_shared<AIIntegrationHub>();
    hub->initialize(state.model_path);
    state.loaded_model = true; // Assume hub init works or handles it
    
    telemetry::Initialize();

    RunInteractiveChat(state, hub);
    
    telemetry::Shutdown();

    return 0;
}
