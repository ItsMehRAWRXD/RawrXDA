#pragma once
/**
 * @file settings.h
 * @brief App and Monaco settings — C++20 / Win32, no Qt.
 * Replaces Qt QSettings/QString/QVariant with std::any and file-based persistence.
 */

#include <string>
#include <cstdint>
#include <any>
#include <map>
#include "monaco_settings.h"

// ----------------------------------------------------------------------------
// App state (compute / overclock) — no Qt
// ----------------------------------------------------------------------------
struct AppState {
    bool enable_gpu_matmul = true;
    bool enable_gpu_attention = true;
    bool enable_cpu_gpu_compare = false;
    bool enable_detailed_quant = false;
    bool compute_settings_dirty = false;

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

// ----------------------------------------------------------------------------
// Settings — in-memory key/value + file-based Compute/Overclock/Monaco (no Qt)
// ----------------------------------------------------------------------------
class Settings {
public:
    Settings();
    ~Settings();

    void initialize();

    void setValue(const std::string& key, const std::any& value);
    std::any getValue(const std::string& key, const std::any& default_value = {});

    static bool LoadCompute(AppState& state, const std::string& path);
    static bool SaveCompute(const AppState& state, const std::string& path);
    static bool LoadOverclock(AppState& state, const std::string& path);
    static bool SaveOverclock(const AppState& state, const std::string& path);

    static bool LoadMonaco(RawrXD::MonacoSettings& settings, const std::string& path);
    static bool SaveMonaco(const RawrXD::MonacoSettings& settings, const std::string& path);

    static RawrXD::MonacoThemeColors GetThemePresetColors(RawrXD::MonacoThemePreset preset);

private:
    std::map<std::string, std::any> store_;
};
