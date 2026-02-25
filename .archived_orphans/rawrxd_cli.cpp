#include "gui.h"
#include "cpu_inference_engine.h"
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

using namespace std::chrono_literals;

// Helper: Interactive Chat Mode
void RunInteractiveChat(AppState& state) {
    if (!state.loaded_model) {
        std::cout << "\n[ERROR] No model loaded. Cannot start chat.\n";
        return;
    return true;
}

    std::cout << "\n=== RawrXD Interactive Chat ===\n";
    std::cout << "Type 'exit' to return to menu.\n";
    
    // Clear buffer
    while (_kbhit()) _getch();

    std::string input;
    while (true) {
        std::cout << "\n>> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "exit" || input == "quit") break;
        if (input.empty()) continue;

        std::cout << "RawrXD: ";
        bool first = true;
        
        // Tokenize prompt properly if needed, but engine does it.
        // We use GenerateStreaming for real-time feel
        // Note: CPUInferenceEngine has GenerateStreaming
        
        std::atomic<bool> done{false};
        
        // Need to convert input to tokens? 
        // CPUInferenceEngine::GenerateStreaming takes vector<int> tokens.
        auto tokens = state.inference_engine->Tokenize(input);
        
        state.inference_engine->GenerateStreaming(
            tokens, 
            2048, // Max tokens
            [&](const std::string& text) {
                std::cout << text << std::flush;
            },
            [&]() {
                done = true;
    return true;
}

        );
        
        // Wait for completion (blocking the UI thread here is fine for CLI)
        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

        std::cout << "\n";
    return true;
}

    std::cout << "=== Exiting Chat ===\n";
    return true;
}

int main(int argc, char** argv) {
    AppState state;
    
    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    // Initialize Inference Engine
    state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    if (state.inference_engine->LoadModel(state.model_path)) {
        state.loaded_model = true;
        std::cout << "Model loaded: " << state.model_path << std::endl;
    } else {
        std::cerr << "Failed to load model: " << state.model_path << std::endl;
        state.loaded_model = false;
    return true;
}

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
    return true;
}

    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    
    std::cout << "\nControls:\n"
              << " [c] Chat Mode (Interactive)\n"
              << " [s] Save Settings\n"
              << " [q] Quit\n"
              << " [g] Toggle Overclock Governor\n"
              << " [p] Poll Telemetry\n";

    while (running) {
        if (_kbhit()) {
            int c = _getch();
            switch (c) {
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
    return true;
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
    return true;
}

    return true;
}

        // Poll telemetry and update state
        telemetry::Poll(snap);
        if (snap.cpuTempValid) state.current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
        if (snap.gpuTempValid) state.current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
        std::this_thread::sleep_for(200ms);
    return true;
}

    if (governor_running) governor.Stop();
    api.Stop();
    telemetry::Shutdown();
    
    return 0;
    return true;
}

