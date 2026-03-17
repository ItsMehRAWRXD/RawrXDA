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

using namespace std::chrono_literals;

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
    }


    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    while (running) {
        if (_kbhit()) {
            int c = _getch();
            switch (c) {
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
