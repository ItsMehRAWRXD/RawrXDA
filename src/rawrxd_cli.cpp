#include "gui.h"
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
    std::cout << "RawrXD CLI - Non-Qt IDE mode\n";
    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

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

    std::cout << "Commands: h=help, p=status, g=toggle governor, a=apply profile, r=reset offsets, +=inc offset, -=dec offset, s=save settings, q=quit" << std::endl;

    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    while (running) {
        if (_kbhit()) {
            int c = _getch();
            switch (c) {
            case 'h':
                std::cout << "h=help, p=status, g=toggle governor, a=apply profile, r=reset, +=inc, -=dec, s=save, q=quit" << std::endl;
                break;
            case 'p':
                telemetry::Poll(snap);
                std::cout << "CPU temp: " << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "C" : "n/a") << "\n";
                std::cout << "GPU temp: " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "C" : "n/a") << "\n";
                std::cout << "Governor status: " << state.governor_status << " applied_offset: " << state.applied_core_offset_mhz << "\n";
                break;
            case 'g':
                if (governor_running) { governor.Stop(); governor_running = false; state.governor_status = "stopped"; std::cout << "Governor stopped\n"; }
                else { governor.Start(state); governor_running = true; state.governor_status = "running"; std::cout << "Governor started\n"; }
                break;
            case 'a':
                // Apply profile - set all core target if configured
                if (state.target_all_core_mhz > 0) {
                    overclock_vendor::ApplyCpuTargetAllCoreMhz(state.target_all_core_mhz);
                    std::cout << "Applied all-core target: " << state.target_all_core_mhz << " MHz" << std::endl;
                } else {
                    std::cout << "No all-core target configured" << std::endl;
                }
                break;
            case 'r':
                overclock_vendor::ApplyCpuOffsetMhz(0);
                overclock_vendor::ApplyGpuClockOffsetMhz(0);
                state.applied_core_offset_mhz = 0;
                state.applied_gpu_offset_mhz = 0;
                std::cout << "Offsets reset" << std::endl;
                break;
            case '+':
            case '=':
                state.applied_core_offset_mhz += state.boost_step_mhz;
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                std::cout << "Increased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                break;
            case '-':
                state.applied_core_offset_mhz = std::max(0, state.applied_core_offset_mhz - (int)state.boost_step_mhz);
                overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                std::cout << "Decreased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                break;
            case 's':
                Settings::SaveCompute(state);
                Settings::SaveOverclock(state);
                std::cout << "Settings saved" << std::endl;
                break;
            case 'q':
                running = false; break;
            default:
                std::cout << "Unknown command: " << (char)c << std::endl;
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
    std::cout << "Exiting RawrXD CLI" << std::endl;
    return 0;
}
