#include "gui.h"
#include "settings.h"
#include "overclock_vendor.h"
#include <iostream>

GUI::GUI()
    : window_width_(1200), window_height_(800), initialized_(false) {
}

GUI::~GUI() {
    Shutdown();
}

bool GUI::Initialize(uint32_t width, uint32_t height) {
    window_width_ = width;
    window_height_ = height;
    
    // ImGui initialization would happen here
    // ImGui::CreateContext();
    // ImGui_ImplWin32_Init();
    // ImGui_ImplVulkan_Init();
    
    std::cout << "GUI initialized: " << width << "x" << height << std::endl;
    initialized_ = true;
    return true;
}

void GUI::Render(AppState& state) {
    if (!initialized_) return;
    
    // ImGui frame setup
    // ImGui_ImplVulkan_NewFrame();
    // ImGui_ImplWin32_NewFrame();
    // ImGui::NewFrame();
    
    RenderMainWindow(state);
    RenderChatWindow(state);
    
    if (state.show_model_browser_window) {
        RenderModelBrowserWindow(state);
    }
    
    if (state.show_settings_window) {
        RenderSettingsWindow(state);
    }
    
    if (state.show_download_window) {
        RenderDownloadWindow(state);
    }
    
    RenderSystemStatus(state);
    
    // ImGui::Render();
}

void GUI::Shutdown() {
    if (initialized_) {
        // ImGui_ImplVulkan_Shutdown();
        // ImGui_ImplWin32_Shutdown();
        // ImGui::DestroyContext();
        initialized_ = false;
    }
}

bool GUI::ShouldClose() const {
    // Check window close flag
    return false;
}

void GUI::RenderMainWindow(AppState& state) {
    // Main window with menu bar and status
    std::cout << "Rendering main window..." << std::endl;
}

void GUI::RenderChatWindow(AppState& state) {
    // Chat interface
    std::cout << "Rendering chat window..." << std::endl;
}

void GUI::RenderModelBrowserWindow(AppState& state) {
    // Model browser with search and download
    std::cout << "Rendering model browser..." << std::endl;
}

void GUI::RenderSettingsWindow(AppState& state) {
    // Settings panel (console-mode simulation of toggles)
    std::cout << "Rendering settings..." << std::endl;
    std::cout << "  [Compute Settings]" << std::endl;
    std::cout << "    enable_gpu_matmul=" << (state.enable_gpu_matmul?"true":"false") << std::endl;
    std::cout << "    enable_gpu_attention=" << (state.enable_gpu_attention?"true":"false") << std::endl;
    std::cout << "    enable_cpu_gpu_compare=" << (state.enable_cpu_gpu_compare?"true":"false") << std::endl;
    std::cout << "    enable_detailed_quant=" << (state.enable_detailed_quant?"true":"false") << std::endl;
    RenderOverclockPanel(state);
    // Auto-save if dirty
    if (state.compute_settings_dirty) {
        extern bool SaveCompute(const AppState&, const std::string&); // forward not used; using Settings namespace requires include
    }
}

void GUI::RenderOverclockPanel(AppState& state) {
    std::cout << "\n  [Overclock Governor]" << std::endl;
    std::cout << "    governor_mode=" << (state.enable_overclock_governor?"adaptive-auto":"manual/disabled") << std::endl;
    if (state.target_all_core_mhz == 0) {
        std::cout << "    target_all_core_mhz=auto (7800X3D baseline)" << std::endl;
    } else {
        std::cout << "    target_all_core_mhz=" << state.target_all_core_mhz << std::endl;
    }
    std::cout << "    boost_step_mhz=" << state.boost_step_mhz << std::endl;
    std::cout << "    thermal_caps.cpu=" << state.max_cpu_temp_c << "C" << std::endl;
    std::cout << "    thermal_caps.gpu_hotspot=" << state.max_gpu_hotspot_c << "C" << std::endl;
    std::cout << "    voltage_guard.max=" << state.max_core_voltage << "V" << std::endl;
    std::cout << "    vendor_tools.ryzen_master=" << (state.ryzen_master_detected?"ready":"missing") << std::endl;
    std::cout << "    vendor_tools.adrenalin_cli=" << (state.adrenalin_cli_detected?"ready":"missing") << std::endl;

    std::cout << "    telemetry.cpu=";
    if (state.current_cpu_freq_mhz > 0 || state.current_cpu_temp_c > 0) {
        std::cout << state.current_cpu_freq_mhz << " MHz @ " << state.current_cpu_temp_c << "C";
    } else {
        std::cout << "n/a";
    }
    std::cout << std::endl;

    std::cout << "    telemetry.gpu=";
    if (state.current_gpu_freq_mhz > 0 || state.current_gpu_hotspot_c > 0) {
        std::cout << state.current_gpu_freq_mhz << " MHz @ " << state.current_gpu_hotspot_c << "C";
    } else {
        std::cout << "n/a";
    }
    std::cout << std::endl;

    std::cout << "    applied_offset=" << state.applied_core_offset_mhz << " MHz" << std::endl;
    std::cout << "    applied_voltage=" << state.applied_core_voltage << "V" << std::endl;
    std::cout << "    PID (cpu): kp=" << state.pid_kp << " ki=" << state.pid_ki << " kd=" << state.pid_kd << " integral_clamp=" << state.pid_integral_clamp << std::endl;
    std::cout << "    PID (gpu): kp=" << state.gpu_pid_kp << " ki=" << state.gpu_pid_ki << " kd=" << state.gpu_pid_kd << " integral_clamp=" << state.gpu_pid_integral_clamp << std::endl;
    if (state.current_cpu_temp_c > 0) {
        int headroom = (int)state.max_cpu_temp_c - (int)state.current_cpu_temp_c;
        std::cout << "    cpu_headroom=" << headroom << "C" << std::endl;
    }
    if (state.current_gpu_hotspot_c > 0) {
        int gheadroom = (int)state.max_gpu_hotspot_c - (int)state.current_gpu_hotspot_c;
        std::cout << "    gpu_headroom=" << gheadroom << "C" << std::endl;
    }

    if (!state.governor_status.empty()) {
        std::cout << "    status=" << state.governor_status << std::endl;
    }
    if (state.baseline_loaded) {
        std::cout << "    baseline_detected_mhz=" << state.baseline_detected_mhz << " stable_offset=" << state.baseline_stable_offset_mhz << std::endl;
    }
    if (!state.governor_last_fault.empty()) {
        std::cout << "    fault_last=" << state.governor_last_fault << std::endl;
    }

    if (state.current_cpu_temp_c > 0 && state.current_cpu_temp_c > state.max_cpu_temp_c) {
        std::cout << "    !! CPU temperature exceeds cap -- governor should step down" << std::endl;
    }
    if (state.current_gpu_hotspot_c > 0 && state.current_gpu_hotspot_c > state.max_gpu_hotspot_c) {
        std::cout << "    !! GPU hotspot above guard -- expect throttle" << std::endl;
    }

    std::cout << "    actions=Apply Profile | Reset Offsets | Live Tune | Save Settings" << std::endl;
    std::cout << "      - To apply a saved profile: call ApplyOverclockProfile(state)" << std::endl;
    std::cout << "      - To reset offsets: call ResetOverclockOffsets(state)" << std::endl;

    // Auto-save if overclock settings modified
    if (state.overclock_settings_dirty) {
        Settings::SaveOverclock(state);
        state.overclock_settings_dirty = false;
        std::cout << "    saved overclock settings to disk" << std::endl;
    }
}

void GUI::ApplyOverclockProfile(AppState& state) {
    // If a user-defined target exists, apply it; otherwise use baseline
    int targetMhz = (int)state.target_all_core_mhz;
    if (targetMhz == 0 && state.baseline_loaded && state.baseline_detected_mhz > 0) {
        targetMhz = (int)state.baseline_detected_mhz + state.baseline_stable_offset_mhz;
    }
    if (targetMhz > 0) {
        bool ok = overclock_vendor::ApplyCpuTargetAllCoreMhz(targetMhz);
        state.governor_status = ok ? "profile-applied" : "profile-apply-failed";
        state.governor_last_fault = ok ? "" : overclock_vendor::LastError();
    } else {
        int offset = state.baseline_stable_offset_mhz;
        bool ok = overclock_vendor::ApplyCpuOffsetMhz(offset);
        if (ok) state.applied_core_offset_mhz = offset;
        state.governor_status = ok ? "offset-applied" : "offset-apply-failed";
        state.governor_last_fault = ok ? "" : overclock_vendor::LastError();
    }
    // Log action
    try {
        std::ofstream log("oc-session.log", std::ios::app);
        if (log.is_open()) {
            log << "GUI ApplyOverclockProfile target=" << targetMhz << " status=" << state.governor_status << "\n";
        }
    } catch (...) {}
}

void GUI::ResetOverclockOffsets(AppState& state) {
    bool okCpu = overclock_vendor::ApplyCpuOffsetMhz(0);
    bool okGpu = overclock_vendor::ApplyGpuClockOffsetMhz(0);
    if (okCpu) state.applied_core_offset_mhz = 0;
    if (okGpu) state.applied_gpu_offset_mhz = 0;
    state.governor_status = (okCpu && okGpu) ? "offsets-reset" : "offsets-reset-failed";
    if (!okCpu && overclock_vendor::LastError().size()) state.governor_last_fault = overclock_vendor::LastError();
    // Log action
    try {
        std::ofstream log("oc-session.log", std::ios::app);
        if (log.is_open()) {
            log << "GUI ResetOverclockOffsets cpu_ok=" << okCpu << " gpu_ok=" << okGpu << " status=" << state.governor_status << "\n";
        }
    } catch (...) {}
}

void GUI::RenderDownloadWindow(AppState& state) {
    // Download progress window
    std::cout << "Download progress: " << state.download_progress.progress_percent << "%" << std::endl;
}

void GUI::RenderSystemStatus(AppState& state) {
    // Status bar with GPU info, loaded model info
    if (state.loaded_model) {
        std::cout << "Status: Model loaded" << std::endl;
    }
    if (state.gpu_context) {
        std::cout << "Status: GPU ready" << std::endl;
    }
}

void GUI::DisplayModelInfo(const std::string& model_path) {
    std::cout << "Loading model: " << model_path << std::endl;
}

void GUI::SendMessage(AppState& state, const std::string& message) {
    AddChatMessage(state, "user", message);
    // Generate response (would call model inference here)
    AddChatMessage(state, "assistant", "Response placeholder...");
}

void GUI::AddChatMessage(AppState& state, const std::string& role, const std::string& content) {
    ChatMessage msg;
    msg.role = role;
    msg.content = content;
    state.chat_history.push_back(msg);
}

void GUI::ToggleSetting(bool& setting, const char* name, AppState& state) {
    setting = !setting;
    state.compute_settings_dirty = true;
    std::cout << "[Setting Toggled] " << name << "=" << (setting?"true":"false") << std::endl;
}
