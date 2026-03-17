#include "settings.h"
#include "gui.h"
#include <filesystem>
#include <fstream>
#include <sstream>

static void EnsureSettingsDir(const std::string& path) {
    std::filesystem::path p(path);
    auto dir = p.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::error_code ec; std::filesystem::create_directories(dir, ec);
    }
}

bool Settings::LoadCompute(AppState& state, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0]=='#') continue;
        auto eq = line.find('=');
        if (eq==std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        bool b = (val=="1" || val=="true" || val=="TRUE");
        if (key=="enable_gpu_matmul") state.enable_gpu_matmul = b;
        else if (key=="enable_gpu_attention") state.enable_gpu_attention = b;
        else if (key=="enable_cpu_gpu_compare") state.enable_cpu_gpu_compare = b;
        else if (key=="enable_detailed_quant") state.enable_detailed_quant = b;
    }
    state.compute_settings_dirty = false;
    return true;
}

bool Settings::SaveCompute(const AppState& state, const std::string& path) {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << "# RawrXD Model Loader Compute Settings\n";
    ofs << "enable_gpu_matmul=" << (state.enable_gpu_matmul?"1":"0") << "\n";
    ofs << "enable_gpu_attention=" << (state.enable_gpu_attention?"1":"0") << "\n";
    ofs << "enable_cpu_gpu_compare=" << (state.enable_cpu_gpu_compare?"1":"0") << "\n";
    ofs << "enable_detailed_quant=" << (state.enable_detailed_quant?"1":"0") << "\n";
    return true;
}

bool Settings::LoadOverclock(AppState& state, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0]=='#') continue;
        auto eq = line.find('=');
        if (eq==std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        auto toBool = [&](const std::string& s){ return (s=="1"||s=="true"||s=="TRUE"); };
        try {
            if (key=="enable_overclock_governor") state.enable_overclock_governor = toBool(val);
            else if (key=="target_all_core_mhz") state.target_all_core_mhz = (uint32_t)std::stoul(val);
            else if (key=="boost_step_mhz") state.boost_step_mhz = (uint32_t)std::stoul(val);
            else if (key=="max_cpu_temp_c") state.max_cpu_temp_c = (uint32_t)std::stoul(val);
            else if (key=="max_gpu_hotspot_c") state.max_gpu_hotspot_c = (uint32_t)std::stoul(val);
            else if (key=="max_core_voltage") state.max_core_voltage = std::stof(val);
        } catch(...) { /* ignore malformed values */ }
    }
    state.overclock_settings_dirty = false;
    return true;
}

bool Settings::SaveOverclock(const AppState& state, const std::string& path) {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << "# RawrXD Model Loader Overclock Settings\n";
    ofs << "enable_overclock_governor=" << (state.enable_overclock_governor?"1":"0") << "\n";
    ofs << "target_all_core_mhz=" << state.target_all_core_mhz << "\n";
    ofs << "boost_step_mhz=" << state.boost_step_mhz << "\n";
    ofs << "max_cpu_temp_c=" << state.max_cpu_temp_c << "\n";
    ofs << "max_gpu_hotspot_c=" << state.max_gpu_hotspot_c << "\n";
    ofs << "max_core_voltage=" << state.max_core_voltage << "\n";
    return true;
}
