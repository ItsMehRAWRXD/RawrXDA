// Win32IDE_Settings.h - Sovereign Persistence Layer Header
// RawrXD IDE - Vector 4 ZMM-Signed Settings with WSSR Recovery

#pragma once

#include <immintrin.h>
#include <map>
#include <string>
#include <vector>


// Forward declarations
enum class EditorTheme
{
    Dark,
    Light,
    Sovereign
};
struct TabState
{
    std::wstring file_path;
    uint32_t cursor_line = 0;
    uint32_t cursor_column = 0;
    bool is_dirty = false;
};
struct SovereignConfig
{
    // Vector 4 ZMM Signature (512-bit hardware-rooted integrity)
    __m512i zmm_signature;

    // Core IDE State
    std::vector<std::wstring> recent_files;
    std::map<std::wstring, std::wstring> keybindings;
    std::wstring extension_path;
    EditorTheme theme = EditorTheme::Sovereign;

    // TabManager Restore State
    std::vector<TabState> open_tabs;
    uint32_t active_tab_index = 0;

    // Performance Tuning
    uint32_t max_memory_mb = 192;
    uint32_t target_fps = 60;
    bool enable_vector7_autogen = true;

    // Model streaming / kernel tuning
    bool model_prefetch_enabled = true;
    bool model_workingset_lock_enabled = false;
    bool silence_privilege_warnings = true;
};

// Public API declarations
const SovereignConfig& GetSovereignConfig();
bool UpdateSovereignConfig(const SovereignConfig& new_config);
void HotReloadSettings();
void SettingsWatchdog();