#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>
#include <string>
#include <cstdint>

// App state structure for settings
struct AppState {
    // Compute settings
    bool enable_gpu_matmul = true;
    // Enable MASM CPU backend (AVX2 intrinsics / MASM kernels)
    bool enable_masm_cpu_backend = true;
    bool enable_gpu_attention = true;
    bool enable_cpu_gpu_compare = false;
    bool enable_detailed_quant = false;
    bool compute_settings_dirty = false;
    
    // Overclock settings
    bool enable_overclock_governor = true;
    uint32_t target_all_core_mhz = 3600;
    uint32_t boost_step_mhz = 100;
    uint32_t max_cpu_temp_c = 85;
    uint32_t max_gpu_hotspot_c = 90;
    float max_core_voltage = 1.4f;
    float pid_kp = 0.1f;
    float pid_ki = 0.01f;
    float pid_kd = 0.05f;
    float pid_integral_clamp = 500.0f;
    float gpu_pid_kp = 0.1f;
    float gpu_pid_ki = 0.01f;
    float gpu_pid_kd = 0.05f;
    float gpu_pid_integral_clamp = 500.0f;
    bool overclock_settings_dirty = false;

    // Runtime telemetry and governor state
    bool ryzen_master_detected = false;
    bool adrenalin_cli_detected = false;
    std::string governor_status = "stopped";
    std::string governor_last_fault;
    int applied_core_offset_mhz = 0;
    int applied_gpu_offset_mhz = 0;
    uint32_t current_cpu_temp_c = 0;
    uint32_t current_gpu_hotspot_c = 0;
    uint32_t current_cpu_freq_mhz = 0;
    uint32_t current_gpu_freq_mhz = 0;
    float pid_integral = 0.0f;
    float pid_last_error = 0.0f;
    float pid_current_output = 0.0f;
    float thermal_headroom_c = 0.0f;
    float gpu_pid_integral = 0.0f;
    float gpu_pid_last_error = 0.0f;

    // Baseline profile persistence
    bool baseline_loaded = false;
    uint32_t baseline_detected_mhz = 0;
    int baseline_stable_offset_mhz = 0;

    // Model/API server state
    bool loaded_model = false;
    bool gpu_context = false;
};

class Settings {
public:
    Settings();
    ~Settings();
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
    // Qt-based settings (for GUI)
    void setValue(const QString& key, const QVariant& value);
    QVariant getValue(const QString& key, const QVariant& default_value = QVariant());
    
    // File-based settings (for compute/overclock)
    static bool LoadCompute(AppState& state, const std::string& path);
    static bool SaveCompute(const AppState& state, const std::string& path);
    static bool LoadOverclock(AppState& state, const std::string& path);
    static bool SaveOverclock(const AppState& state, const std::string& path);
    
private:
    QSettings* settings_;
};
