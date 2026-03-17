#pragma once
#include <string>
#include <vector>
#include <memory>
#include "gguf_loader.h"
// Forward declare VulkanCompute to avoid requiring Vulkan headers in tests
class VulkanCompute;
#include "hf_downloader.h"

struct ChatMessage {
    std::string role;  // "user" or "assistant"
    std::string content;
};

struct APIKeyConfig {
    std::string openai_key;
    std::string anthropic_key;
    std::string huggingface_token;
};

struct AppState {
    std::vector<ModelInfo> available_models;
    std::string selected_model_path;
    std::unique_ptr<GGUFLoader> loaded_model;
    VulkanCompute* gpu_context{nullptr};
    
    std::vector<ChatMessage> chat_history;
    std::string current_input;
    
    APIKeyConfig api_keys;
    bool use_gpu;
    bool use_fallback_api;
    std::string selected_fallback_provider;  // "openai" or "anthropic"
    
    DownloadProgress download_progress;
    bool show_download_window;
    bool show_settings_window;
    bool show_model_browser_window;
    
    std::string hf_search_query;

    
    // Compute feature settings (persistent)
    bool enable_gpu_matmul{true};
    bool enable_gpu_attention{true};
    bool enable_cpu_gpu_compare{false};
    bool enable_detailed_quant{false};
    // Internal change tracking for persistence
    bool compute_settings_dirty{false};
    // Runtime control
    bool running{true};

    // Overclock / performance governor settings (persistent)
    bool enable_overclock_governor{false};
    uint32_t target_all_core_mhz{0};       // 0 = auto-detect
    uint32_t boost_step_mhz{25};           // step applied per successful stability interval
    uint32_t max_cpu_temp_c{85};           // safety threshold
    uint32_t max_gpu_hotspot_c{95};        // safety threshold
    float max_core_voltage{1.30f};         // upper voltage guard (not applied directly without vendor tool)
    bool overclock_settings_dirty{false};
    
    // Current telemetry values
    uint32_t cpu_freq_mhz{0};
    uint32_t gpu_freq_mhz{0};
    bool governor_enabled{false};
    // Runtime telemetry + vendor detection fed by backend
    bool ryzen_master_detected{false};
    bool adrenalin_cli_detected{false};
    uint32_t current_cpu_freq_mhz{0};
    uint32_t current_gpu_freq_mhz{0};
    uint32_t current_cpu_temp_c{0};
    uint32_t current_gpu_hotspot_c{0};
    int32_t applied_core_offset_mhz{0};
    int32_t applied_gpu_offset_mhz{0};
    float applied_core_voltage{0.0f};
    std::string governor_status;           // short status line shown in UI
    std::string governor_last_fault;       // most recent safety trip or vendor error

    // PID control parameters (persisted)
    float pid_kp{0.4f};
    float pid_ki{0.05f};
    float pid_kd{0.1f};
    float pid_integral{0.0f};
    float pid_last_error{0.0f};
    float pid_integral_clamp{100.0f}; // max abs value for integral wind-up

    // GPU PID control
    float gpu_pid_kp{0.4f};
    float gpu_pid_ki{0.05f};
    float gpu_pid_kd{0.1f};
    float gpu_pid_integral{0.0f};
    float gpu_pid_last_error{0.0f};
    float gpu_pid_integral_clamp{100.0f};

    // Baseline profiling
    uint32_t baseline_detected_mhz{0};
    int baseline_stable_offset_mhz{0};
    bool baseline_loaded{false};
};

class GUI {
public:
    GUI();
    ~GUI();

    bool Initialize(uint32_t width = 1200, uint32_t height = 800);
    void Render(AppState& state);
    void Shutdown();
    
    bool ShouldClose() const;
    
private:
    uint32_t window_width_;
    uint32_t window_height_;
    bool initialized_;
    
    // Windows
    void RenderMainWindow(AppState& state);
    void RenderChatWindow(AppState& state);
    void RenderModelBrowserWindow(AppState& state);
    void RenderSettingsWindow(AppState& state);
    void RenderOverclockPanel(AppState& state);
    void RenderDownloadWindow(AppState& state);
    void RenderSystemStatus(AppState& state);
    
    // Helpers
    void DisplayModelInfo(const std::string& model_path);
    void SendMessage(AppState& state, const std::string& message);
    void AddChatMessage(AppState& state, const std::string& role, const std::string& content);
    void ToggleSetting(bool& setting, const char* name, AppState& state);
    // Overclock actions
    void ApplyOverclockProfile(AppState& state);
    void ResetOverclockOffsets(AppState& state);
};

// Helper functions for Windows SDK paths
inline std::string GetWindowsSdkDir() {
    const char* env = getenv("WindowsSdkDir");
    if (env) return std::string(env);
    return "/usr/include";
}

inline std::string GetVCToolsInstallDir() {
    const char* env = getenv("VCToolsInstallDir");
    if (env) return std::string(env);
    return "/usr/include";
}
