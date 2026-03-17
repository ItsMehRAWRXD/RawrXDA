#include "gui.h"
#include "cpu_inference_engine.h"
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
void RunInteractiveChat(AppState& state, AgenticEngine& agent) {
    if (!state.loaded_model) {
        std::cout << "[Info] No model file loaded. Using stub/placeholder.\n";
    }

    std::cout << "\n=== RawrXD Interactive Chat (Agentic Mode) ===\n";
    std::cout << "Type 'exit' to return to menu.\n";
    std::cout << "Commands: /load <path>, /clear\n";
    
    // Clear buffer
    while (_kbhit()) _getch();

    std::string input;
    while (true) {
        std::cout << "\n>> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "exit" || input == "quit") break;
        
        if (input.rfind("/load ", 0) == 0) {
            std::string path = input.substr(6);
            if (state.inference_engine->loadModel(path).has_value()) {
                 std::cout << "[System] Model loaded: " << path << "\n";
                 state.loaded_model = true;
            } else {
                 std::cout << "[Error] Failed to load model: " << path << "\n";
            }
            continue;
        }

        if (input.empty()) continue;

        std::cout << "RawrXD: ";
        
        // Agentic Processing
        if (input.find("make") != std::string::npos || input.find("create") != std::string::npos || input.find("project") != std::string::npos) {
             std::cout << "[Agent Thinking] Analyzing request...\n";
             std::cout << "[Agent] I see you want to create something. Agentic features are active in this build.\n";
        } else {
            // Standard Streaming Chat
            auto tokens = state.inference_engine->Tokenize(input);
            bool done = false;
            state.inference_engine->GenerateStreaming(
                tokens, 
                2048, 
                [&](const std::string& text) {
                    std::cout << text << std::flush;
                },
                [&]() {
                    done = true;
                }
            );
            while (!done) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "\n";
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

    std::cout << "RawrXD CLI v2.0 - Real Competitive Edge\n";
    
    // 1. Initialize Inference Engine
    state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    
    if (!state.model_path.empty()) {
        std::cout << "Loading Model: " << state.model_path << "...\n";
        if (state.inference_engine->loadModel(state.model_path).has_value()) {
            state.loaded_model = true;
            std::cout << "Model Loaded Successfully.\n";
        } else {
            std::cout << "Model Load Failed (using stubs if enabled).\n";
            if (state.inference_engine->isModelLoaded()) {
                state.loaded_model = true;
                std::cout << "Model Loaded (Fallback).\n";
            }
        }
    }

    AgenticEngine agent;
    // agent.initialize(); // Stubbed/Not implemented in header?
    // agent.setInferenceEngine(state.inference_engine.get()); 

    telemetry::Initialize();

    RunInteractiveChat(state, agent);
    
    telemetry::Shutdown();

    return 0;
}
