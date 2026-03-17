#pragma once
#include <string>
#include <atomic>
#include <memory>

// Forward declaration
namespace RawrXD { class CPUInferenceEngine; }

struct AppState {
    std::string governor_status;
    std::atomic<float> current_cpu_temp{0.0f};
    std::atomic<float> current_gpu_temp{0.0f};
    std::atomic<float> current_cpu_power{0.0f};
    std::atomic<float> current_gpu_power{0.0f};
    
    std::string status_message;
    uint32_t boost_step_mhz = 25;
    
    // Feature flags
    bool ryzen_master_detected = false;
    bool adrenalin_cli_detected = false;

    // Inference Engine Integration
    std::string model_path = "models/model.gguf"; // Default
    std::shared_ptr<RawrXD::CPUInferenceEngine> inference_engine;
    bool loaded_model = false;
    // Legacy/Placeholder flags referenced by APIServer (preserving compatibility)
    bool gpu_context = true; 

    // Settings
    bool is_gpu_enabled = true;
    int thread_count = 8;
    int vram_limit_mb = 4096;
    bool enable_overclock_governor = false;
};
