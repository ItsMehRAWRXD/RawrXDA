#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>

namespace RawrXD {

// Forward declarations
class Settings;
class APIServer;
class Telemetry;
class OverclockGovernor;
class OverclockVendor;

// Main application state structure
struct AppState {
    // Model loading state
    bool model_loaded = false;
    std::string model_path;
    std::string model_name;
    std::string model_format;  // "gguf", "safetensors", etc.
    int64_t model_size_bytes = 0;
    
    // GPU/Compute context
    void* gpu_context = nullptr;
    void* loaded_model = nullptr;
    bool enable_gpu_matmul = true;
    bool enable_gpu_attention = true;
    bool enable_cpu_gpu_compare = false;
    bool enable_detailed_quant = false;
    
    // Overclocking state
    bool enable_overclock_governor = false;
    uint32_t target_all_core_mhz = 0;
    uint32_t boost_step_mhz = 50;
    uint32_t max_cpu_temp_c = 85;
    uint32_t max_gpu_hotspot_c = 85;
    float max_core_voltage = 1.4f;
    float pid_kp = 0.1f;
    float pid_ki = 0.01f;
    float pid_kd = 0.05f;
    float pid_integral_clamp = 10.0f;
    float gpu_pid_kp = 0.1f;
    float gpu_pid_ki = 0.01f;
    float gpu_pid_kd = 0.05f;
    float gpu_pid_integral_clamp = 10.0f;
    bool overclock_settings_dirty = false;
    
    // Telemetry state
    bool telemetry_enabled = true;
    std::string telemetry_endpoint = "http://localhost:8080/telemetry";
    
    // API server state
    bool api_server_enabled = false;
    uint16_t api_server_port = 11434;
    
    // Settings paths
    std::string compute_settings_path = "config/compute.cfg";
    std::string overclock_settings_path = "config/overclock.cfg";
    std::string telemetry_settings_path = "config/telemetry.cfg";
    bool compute_settings_dirty = false;
    
    // UI state
    bool show_advanced_options = false;
    bool show_telemetry_panel = false;
    bool show_overclock_panel = false;
    
    // Agentic state
    bool agentic_mode_enabled = false;
    std::string current_agent_plan;
    std::vector<std::string> agent_tool_history;
    
    // Constructor
    AppState() = default;
    
    // Prevent copying (large state)
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
};

// GUI main application class
class GUI {
public:
    GUI();
    ~GUI();
    
    // Initialize the GUI
    bool Initialize(AppState& state);
    
    // Run the main event loop
    int Run();
    
    // Shutdown
    void Shutdown();
    
    // Get the application state
    AppState& GetState() { return *app_state_; }
    
private:
    AppState* app_state_ = nullptr;
    bool initialized_ = false;
    
    // Platform-specific implementation
    void* platform_impl_ = nullptr;
};

} // namespace RawrXD
