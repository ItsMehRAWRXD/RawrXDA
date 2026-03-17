#include "gui.h"
#include "cpu_inference_engine.h"
#include "agentic_engine.h"  // Added Agentic Engine
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
        std::cout << "\n[ERROR] No model loaded. Cannot start chat.\n";
        return;
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
            } else {
                 std::cout << "[Error] Failed to load model: " << path << "\n";
            }
            continue;
        }

        if (input.empty()) continue;

        std::cout << "RawrXD: ";
        
        // Agentic Processing (Allows for "Make a react server" etc)
        // Check if query implies a complex task
        if (input.find("make") != std::string::npos || input.find("create") != std::string::npos || input.find("project") != std::string::npos) {
             std::cout << "[Agent Thinking] Analyzing request...\n";
             json plan = agent.planTask(input);
             std::cout << "[Agent Plan] " << plan.dump(2) << "\n";
             std::string result = agent.executePlan(plan);
             std::cout << "\n[Agent Execution] " << result << "\n";
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
    } else {
        // Default or prompt
        // state.model_path = "models/llama-3-8b.gguf"; 
    }

    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    std::cout << "RawrXD CLI v2.0 - Real Competitive Edge\n";
    
    // 1. Initialize Inference Engine
    state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    
    // Loop until model Loaded
    while (!state.loaded_model) {
        if (state.model_path.empty()) {
            std::cout << "Enter model path (GGUF/Blob): ";
            std::getline(std::cin, state.model_path);
            if (state.model_path.empty()) continue;
        }
        
        // Remove quotes if user added them
        if (state.model_path.front() == '"') state.model_path.erase(0, 1);
        if (state.model_path.back() == '"') state.model_path.pop_back();

        if (state.inference_engine->loadModel(state.model_path).has_value()) {
            state.loaded_model = true;
            std::cout << "[Success] Model loaded: " << state.model_path << std::endl;
        } else {
            std::cerr << "[Error] Failed to load model: " << state.model_path << std::endl;
            state.model_path.clear(); // Reset to ask again
             // If arg provided failed, exit? No, interactive fallback.
        }
    }

    // 2. Initialize Agentic Engine
    AgenticEngine agent;
    agent.initialize();
    agent.setInferenceEngine(state.inference_engine.get());

    // Initialize telemetry
    telemetry::Initialize();

    // Start API server
    APIServer api(state);
    api.Start(11434);

    // Start governor if requested
    OverclockGovernor governor;
    std::atomic<bool> governor_running{false};
    if (state.enable_overclock_governor) {
        governor.Start(state);
        governor_running = true;
    }
    
    // Enter Main Loop
    RunInteractiveChat(state, agent);
    
    return 0;
}
            case 'c':
                RunInteractiveChat(state);
                // Print menu again
                std::cout << "\nControls:\n"
                          << " [c] Chat Mode (Interactive)\n"
                          << " [s] Save Settings\n"
                          << " [q] Quit\n";
                break;
            case 'h':
                
                break;
            case 'p':
                telemetry::Poll(snap);


                break;
            case 'g':
                if (governor_running) { governor.Stop(); governor_running = false; state.governor_status = "stopped";  }
                else { governor.Start(state); governor_running = true; state.governor_status = "running";  }
                break;
            case 'a':
                // Apply profile - set all core target if configured
                if (state.target_all_core_mhz > 0) {
                    overclock_vendor::ApplyCpuTargetAllCoreMhz(state.target_all_core_mhz);
                    
                } else {
                    
                }
                break;
            case 'r':
                overclock_vendor::ApplyCpuOffsetMhz(0);
                overclock_vendor::ApplyGpuClockOffsetMhz(0);
                state.applied_core_offset_mhz = 0;
                state.applied_gpu_offset_mhz = 0;
                
                break;
            case '+':
            case '=':
                state.applied_core_offset_mhz += state.boost_step_mhz;
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                
                break;
            case '-':
                state.applied_core_offset_mhz = std::max(0, state.applied_core_offset_mhz - (int)state.boost_step_mhz);
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                
                break;
            case 's':
                Settings::SaveCompute(state);
                Settings::SaveOverclock(state);
                
                break;
            case 'q':
                running = false; break;
            default:
                
            }
        }

        // Poll telemetry and update state
        telemetry::Poll(snap);
        if (snap.cpuTempValid) state.current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
        if (snap.gpuTempValid) state.current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
        std::this_thread::sleep_for(200ms);
    }

    if (governor_running) governor.Stop();
    api.Stop();
    telemetry::Shutdown();
    
    return 0;
}
