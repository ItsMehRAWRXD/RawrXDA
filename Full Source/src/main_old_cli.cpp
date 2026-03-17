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
#include "cpu_inference_engine.h"

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
        std::cout << "[Vulkan] Initialized on device: " << device_info.deviceName << std::endl;
    } else {
        std::cerr << "[Vulkan] Initialization failed. Falling back to CPU." << std::endl;
    }

    // Initialize CPU Inference Engine for AppState
    g_app_state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    std::string defaultModel = "models/decoder.gguf";
    if (std::filesystem::exists(defaultModel)) {
        std::cout << "Loading default model: " << defaultModel << std::endl;
        if (g_app_state.inference_engine->LoadModel(defaultModel)) {
            g_app_state.loaded_model = true;
            g_app_state.model_path = defaultModel;
            std::cout << "Model loaded successfully." << std::endl;
        } else {
            std::cerr << "Failed to load model." << std::endl;
        }
    }

    GGUFLoader gguf_loader;

    APIServer api_server(g_app_state);
    api_server.Start(11434);
    std::cout << "[API] Server listening on port 11434" << std::endl;

    // Interactive CLI Mode
    std::thread cliThread([&]() {
        std::string line;
        std::cout << "Enter prompt (or 'quit'): " << std::endl;
        while (g_app_state.running) {
             std::cout << "> ";
             if (!std::getline(std::cin, line)) break;
             if (line == "quit" || line == "exit") {
                 g_app_state.running = false; 
                 break; 
             }
             
             if (g_app_state.loaded_model && g_app_state.inference_engine) {
                 // Simple blocking inference
                 auto input = g_app_state.inference_engine->Tokenize(line);
                 auto output = g_app_state.inference_engine->Generate(input, 128);
                 std::string text = g_app_state.inference_engine->Detokenize(output);
                 std::cout << text << std::endl;
             } else {
                 std::cout << "Model not loaded. Place model in models/decoder.gguf" << std::endl;
             }
        }
    });
    cliThread.detach();

    // Create models directory


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
        std::cout << "No models found in " << models_dir << std::endl;
        std::cout << "Attempting to download default model..." << std::endl;
        // HFDownloader::DownloadDefault(); // Logic to be implemented
    }
}

void CleanupApplication() {
    
    if (g_app_state.compute_settings_dirty) {
        if (Settings::SaveCompute(g_app_state)) {
            std::cout << "Compute settings saved." << std::endl;
        } else {
            std::cerr << "Failed to save compute settings." << std::endl;
        }
    }
    if (g_app_state.overclock_settings_dirty) {
        if (Settings::SaveOverclock(g_app_state)) {
            std::cout << "Overclock settings saved." << std::endl;
        } else {
            std::cerr << "Failed to save overclock settings." << std::endl;
        }
    }
    telemetry::Shutdown();
}
