#include <iostream>
#include <memory>
#include <filesystem>
#include <chrono>
#include <thread>
#include "gguf_loader.h"
#include "vulkan_compute.h"
#include "hf_downloader.h"
#include "api_server.h"
#include "gui.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "telemetry.h"

AppState g_app_state; // unified state with GUI + compute settings

void InitializeApplication();
void CleanupApplication();

int main(int argc, char* argv[]) {


    try {
        Settings::LoadCompute(g_app_state);
        Settings::LoadOverclock(g_app_state);
        telemetry::Initialize();
        InitializeApplication();

        g_app_state.running = true;


        // Keep running
        
        OverclockGovernor governor;
        if (g_app_state.enable_overclock_governor) {
            governor.Start(g_app_state);
        }

        telemetry::TelemetrySnapshot snap{};
        uint64_t lastPrint = 0;
        while (g_app_state.running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (telemetry::Poll(snap)) {
                if (snap.cpuTempValid) {
                    g_app_state.current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
                }
                if (snap.gpuTempValid) {
                    g_app_state.current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
                }
                // Print every 5 seconds or if overclock governor enabled (higher visibility)
                if (snap.timeMs - lastPrint >= 5000 || g_app_state.enable_overclock_governor) {
                    lastPrint = snap.timeMs;
                    
                }
            }
        }

        CleanupApplication();
        
        return 0;

    } catch (const std::exception& e) {
        
        return 1;
    }
}

void InitializeApplication() {


    VulkanCompute vulkan_compute;
    if (vulkan_compute.Initialize()) {
        auto device_info = vulkan_compute.GetDeviceInfo();


    } else {
        
    }


    GGUFLoader gguf_loader;


    APIServer api_server(g_app_state);
    api_server.Start(11434);


    // Create models directory
    std::filesystem::path models_dir = std::filesystem::current_path() / "models";
    std::filesystem::create_directories(models_dir);


    // Scan for models
    int model_count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                
                model_count++;
            }
        }
    } catch (...) {
        // Ignore errors in directory iteration
    }
    
    if (model_count == 0) {
        
    }
}

void CleanupApplication() {
    
    if (g_app_state.compute_settings_dirty) {
        if (Settings::SaveCompute(g_app_state)) {
            
        } else {
            
        }
    }
    if (g_app_state.overclock_settings_dirty) {
        if (Settings::SaveOverclock(g_app_state)) {
            
        } else {
            
        }
    }
    telemetry::Shutdown();
}
