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
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║         RawrXD Model Loader v1.0 - Initializing        ║\n";
    std::cout << "║         GPU-Accelerated GGUF Inference Engine          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";

    try {
        Settings::LoadCompute(g_app_state);
        Settings::LoadOverclock(g_app_state);
        telemetry::Initialize();
        InitializeApplication();

        g_app_state.running = true;
        std::cout << "\n✓ RawrXD Model Loader is ready\n";
        std::cout << "✓ Web API: http://localhost:11434\n";
        std::cout << "✓ Ollama compatible endpoints available\n\n";

        // Keep running
        std::cout << "Running... Press Ctrl+C to exit.\n\n";
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
                    std::cout << "[Telemetry] CPU "
                              << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "C" : "n/a")
                              << " | CPU Usage " << (snap.cpuUsagePercent >= 0 ? std::to_string((int)snap.cpuUsagePercent) + "%" : "n/a")
                              << " | GPU " << (snap.gpuVendor.empty() ? "Unknown" : snap.gpuVendor)
                              << " Temp " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "C" : "n/a")
                              << " | GPU Usage " << (snap.gpuUsagePercent >= 0 ? std::to_string((int)snap.gpuUsagePercent) + "%" : "n/a")
                              << "\n";
                }
            }
        }

        CleanupApplication();
        std::cout << "✓ RawrXD Model Loader shut down successfully\n\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "✗ Fatal error: " << e.what() << "\n";
        return 1;
    }
}

void InitializeApplication() {
    std::cout << "[1/3] Initializing GPU context...\n";
    
    VulkanCompute vulkan_compute;
    if (vulkan_compute.Initialize()) {
        auto device_info = vulkan_compute.GetDeviceInfo();
        std::cout << "✓ GPU Device: " << device_info.device_name << "\n";
        std::cout << "✓ Supports Compute: " << (device_info.supports_compute ? "Yes" : "No") << "\n";
    } else {
        std::cerr << "⚠ GPU initialization warning (CPU fallback available)\n";
    }

    std::cout << "\n[2/3] Initializing model loader...\n";
    GGUFLoader gguf_loader;
    std::cout << "✓ GGUF loader ready\n";

    std::cout << "\n[3/3] Initializing API server...\n";
    APIServer api_server(g_app_state);
    api_server.Start(11434);
    std::cout << "✓ API server initialized on port 11434\n";

    // Create models directory
    std::filesystem::path models_dir = std::filesystem::current_path() / "models";
    std::filesystem::create_directories(models_dir);
    std::cout << "\n✓ Models directory: " << models_dir.string() << "\n";

    // Scan for models
    int model_count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                std::cout << "  • " << entry.path().filename().string() << "\n";
                model_count++;
            }
        }
    } catch (...) {
        // Ignore errors in directory iteration
    }
    
    if (model_count == 0) {
        std::cout << "  (No models loaded yet)\n";
    }
}

void CleanupApplication() {
    std::cout << "\n[Shutdown] Cleaning up resources...\n";
    if (g_app_state.compute_settings_dirty) {
        if (Settings::SaveCompute(g_app_state)) {
            std::cout << "Saved compute settings." << std::endl;
        } else {
            std::cout << "Warning: failed to save compute settings." << std::endl;
        }
    }
    if (g_app_state.overclock_settings_dirty) {
        if (Settings::SaveOverclock(g_app_state)) {
            std::cout << "Saved overclock settings." << std::endl;
        } else {
            std::cout << "Warning: failed to save overclock settings." << std::endl;
        }
    }
    telemetry::Shutdown();
}
