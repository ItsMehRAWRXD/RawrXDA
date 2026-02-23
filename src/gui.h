#pragma once
#include <string>
#include <atomic>

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

    // ---- Inference Engine Bridge (Phase 36: API Server -> Model) ----
    // Non-owning opaque handles set by the model loader when a GGUF is loaded.
    // Ownership is managed by the subsystem that creates them (ModelLoader / GPUBackend).
    // TODO: Replace void* with typed std::shared_ptr once interface unification is complete.
    void* loaded_model = nullptr;    // Non-owning: GGUFModel* or CPUInferenceEngine*
    void* gpu_context = nullptr;     // Non-owning: VulkanCompute or CPU fallback context
    void* inference_engine = nullptr; // Non-owning: RawrInference or CPUInferenceEngine
    std::atomic<bool> model_ready{false};
    std::string loaded_model_name;
    uint32_t context_size = 2048;
};
