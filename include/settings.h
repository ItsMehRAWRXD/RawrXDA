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
};

class Settings {
public:
    Settings();
    ~Settings();
    
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
