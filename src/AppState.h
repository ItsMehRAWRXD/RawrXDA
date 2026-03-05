#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <optional>
#include <chrono>

namespace RawrXD { class CPUInferenceEngine; }
class AgenticEngine;
class APIServer;
class OverclockGovernor;

struct AppState {
    std::string model_path;
    float temperature = 0.8f;
    float top_p = 0.95f;
    bool is_gpu_enabled = false;
    int thread_count = 8;
    int vram_limit_mb = 4096;
    uint32_t target_all_core_mhz = 0;
    bool baseline_loaded = false;
    uint32_t baseline_detected_mhz = 0;
    uint32_t current_cpu_freq_mhz = 0;
    uint32_t current_gpu_freq_mhz = 0;
    uint32_t max_cpu_temp_c = 95;
    uint32_t max_gpu_hotspot_c = 90;
    uint32_t current_cpu_temp_c = 0;
    uint32_t current_gpu_hotspot_c = 0;
    float pid_integral = 0;
    float pid_integral_clamp = 1000;
    float pid_last_error = 0;
    float pid_kp = 1.0;
    float pid_ki = 0.1;
    float pid_kd = 0.05;
    float pid_current_output = 0;
    float thermal_headroom_c = 0;
    float gpu_pid_integral = 0;
    float gpu_pid_integral_clamp = 1000;
    float gpu_pid_last_error = 0;
    float gpu_pid_kp = 1.0;
    float gpu_pid_ki = 0.1;
    float gpu_pid_kd = 0.05;
    int applied_core_offset_mhz = 0;
    std::string governor_last_fault = "";
    std::string governor_status = "idle";
    unsigned int current_cpu_temp = 0;
    unsigned int current_gpu_temp = 0;
    unsigned int current_gpu_power = 0;
    int baseline_stable_offset_mhz = 0;
    
    std::shared_ptr<RawrXD::CPUInferenceEngine> inference_engine;
    std::shared_ptr<AgenticEngine> agent_engine;
    std::unique_ptr<APIServer> api_server;
    std::unique_ptr<OverclockGovernor> governor;
    
    bool enable_max_mode = false;
    bool enable_deep_thinking = false;
    bool enable_deep_research = false;
    bool enable_no_refusal = false;
    bool enable_autocorrect = false;
};
