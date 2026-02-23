#include "gui.h"
#include "settings.h"
#include "overclock_vendor.h"
#include "cpu_inference_engine.h"
#include <iostream>

#include "logging/logger.h"
static Logger s_logger("gui");

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
    
    s_logger.info("GUI initialized: ");
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
    s_logger.info("Rendering main window...");
}

void GUI::RenderChatWindow(AppState& state) {
    // Chat interface
    s_logger.info("Rendering chat window...");
}

void GUI::RenderModelBrowserWindow(AppState& state) {
    // Model browser with search and download
    s_logger.info("Rendering model browser...");
}

void GUI::RenderSettingsWindow(AppState& state) {
    // Settings panel (console-mode simulation of toggles)
    s_logger.info("Rendering settings...");
    s_logger.info("  [Compute Settings]");
    s_logger.info("    enable_gpu_matmul=");
    s_logger.info("    enable_gpu_attention=");
    s_logger.info("    enable_cpu_gpu_compare=");
    s_logger.info("    enable_detailed_quant=");
    RenderOverclockPanel(state);
    // Auto-save if dirty
    if (state.compute_settings_dirty) {
        extern bool SaveCompute(const AppState&, const std::string&); // forward not used; using Settings namespace requires include
    }
}

void GUI::RenderOverclockPanel(AppState& state) {
    s_logger.info("\n  [Overclock Governor]");
    s_logger.info("    governor_mode=");
    if (state.target_all_core_mhz == 0) {
        s_logger.info("    target_all_core_mhz=auto (7800X3D baseline)");
    } else {
        s_logger.info("    target_all_core_mhz=");
    }
    s_logger.info("    boost_step_mhz=");
    s_logger.info("    thermal_caps.cpu=");
    s_logger.info("    thermal_caps.gpu_hotspot=");
    s_logger.info("    voltage_guard.max=");
    s_logger.info("    vendor_tools.ryzen_master=");
    s_logger.info("    vendor_tools.adrenalin_cli=");

    s_logger.info("    telemetry.cpu=");
    if (state.current_cpu_freq_mhz > 0 || state.current_cpu_temp_c > 0) {
        s_logger.info( state.current_cpu_freq_mhz << " MHz @ " << state.current_cpu_temp_c << "C";
    } else {
        s_logger.info("n/a");
    }
    s_logger.info( std::endl;

    s_logger.info("    telemetry.gpu=");
    if (state.current_gpu_freq_mhz > 0 || state.current_gpu_hotspot_c > 0) {
        s_logger.info( state.current_gpu_freq_mhz << " MHz @ " << state.current_gpu_hotspot_c << "C";
    } else {
        s_logger.info("n/a");
    }
    s_logger.info( std::endl;

    s_logger.info("    applied_offset=");
    s_logger.info("    applied_voltage=");
    s_logger.info("    PID (cpu): kp=");
    s_logger.info("    PID (gpu): kp=");
    if (state.current_cpu_temp_c > 0) {
        int headroom = (int)state.max_cpu_temp_c - (int)state.current_cpu_temp_c;
        s_logger.info("    cpu_headroom=");
    }
    if (state.current_gpu_hotspot_c > 0) {
        int gheadroom = (int)state.max_gpu_hotspot_c - (int)state.current_gpu_hotspot_c;
        s_logger.info("    gpu_headroom=");
    }

    if (!state.governor_status.empty()) {
        s_logger.info("    status=");
    }
    if (state.baseline_loaded) {
        s_logger.info("    baseline_detected_mhz=");
    }
    if (!state.governor_last_fault.empty()) {
        s_logger.info("    fault_last=");
    }

    if (state.current_cpu_temp_c > 0 && state.current_cpu_temp_c > state.max_cpu_temp_c) {
        s_logger.info("    !! CPU temperature exceeds cap -- governor should step down");
    }
    if (state.current_gpu_hotspot_c > 0 && state.current_gpu_hotspot_c > state.max_gpu_hotspot_c) {
        s_logger.info("    !! GPU hotspot above guard -- expect throttle");
    }

    s_logger.info("    actions=Apply Profile | Reset Offsets | Live Tune | Save Settings");
    s_logger.info("      - To apply a saved profile: call ApplyOverclockProfile(state)");
    s_logger.info("      - To reset offsets: call ResetOverclockOffsets(state)");

    // Auto-save if overclock settings modified
    if (state.overclock_settings_dirty) {
        Settings::SaveOverclock(state);
        state.overclock_settings_dirty = false;
        s_logger.info("    saved overclock settings to disk");
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
    s_logger.info("Download progress: ");
}

void GUI::RenderSystemStatus(AppState& state) {
    // Status bar with GPU info, loaded model info
    if (state.loaded_model) {
        s_logger.info("Status: Model loaded");
    }
    if (state.gpu_context) {
        s_logger.info("Status: GPU ready");
    }
}

void GUI::DisplayModelInfo(const std::string& model_path) {
    s_logger.info("Loading model: ");
}

void GUI::SendMessage(AppState& state, const std::string& message) {
    AddChatMessage(state, "user", message);

    // Route through CPUInferenceEngine if model is loaded
    if (state.model_ready.load() && state.inference_engine) {
        auto* engine = static_cast<RawrXD::CPUInferenceEngine*>(state.inference_engine);
        std::vector<int32_t> promptTokens = engine->Tokenize(message);
        std::vector<int32_t> generated = engine->Generate(promptTokens, 256);
        std::string response = engine->Detokenize(generated);
        if (response.empty()) response = "(empty response)";
        AddChatMessage(state, "assistant", response);
    } else {
        AddChatMessage(state, "assistant", "[No model loaded — type /load <model> to begin]");
    }
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
    s_logger.info("[Setting Toggled] ");
}
