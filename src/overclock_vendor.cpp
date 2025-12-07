#include "overclock_vendor.h"
#include "gui.h"
#include <cstdio>
#include <array>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <cstdlib>
#include <memory>

namespace overclock_vendor {

static std::string g_lastError;
static std::mutex g_lock;

static bool ExistsOne(const std::initializer_list<const char*> &paths) {
    for (auto p : paths) { if (std::filesystem::exists(p)) return true; }
    return false;
}

bool DetectRyzenMaster(AppState& st) {
    bool found = ExistsOne({
        "C:/Program Files/AMD/RyzenMaster/RyzenMaster.exe",
        "C:/Program Files (x86)/AMD/RyzenMaster/RyzenMaster.exe",
        "C:/AMD/RyzenMaster/RyzenMaster.exe"
    });
    st.ryzen_master_detected = found;
    return found;
}

bool DetectAdrenalinCLI(AppState& st) {
    // Adrenalin GUI mostly; CLI if present inside driver folder or amd-smi fallback
    bool found = ExistsOne({
        "C:/Program Files/AMD/AMD Software/AMD Software.exe",
        "C:/Program Files/AMD/Chipset/AMD Software.exe"
    });
    // We also treat amd-smi presence as CLI capability (telemetry layer handles real detection)
    if (!found) {
        if (ExistsOne({"C:/Windows/System32/amd-smi.exe", "C:/Program Files/AMD/amd-smi.exe"})) found = true;
    }
    st.adrenalin_cli_detected = found;
    return found;
}

bool ApplyCpuOffsetMhz(int offset) {
    std::lock_guard<std::mutex> guard(g_lock);
    // Real implementation would invoke vendor tool or driver API if available.
    std::cout << "[vendor] Request CPU offset=" << offset << " MHz" << std::endl;
    const char* ryzenCliEnv = std::getenv("RYZEN_MASTER_CLI");
    if (ryzenCliEnv && std::filesystem::exists(ryzenCliEnv)) {
        std::string cmd = std::string(ryzenCliEnv) + " --set-core-offset " + std::to_string(offset);
        std::cout << "[vendor] Executing: " << cmd << std::endl;
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "failed_to_execute_ryzen_cli"; return false; }
        std::array<char, 256> buf{};
        std::string out;
        while (fgets(buf.data(), (int)buf.size(), pipe)) out += buf.data();
        int rc = _pclose(pipe);
        if (rc != 0) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "ryzen_cli_failed:" + std::to_string(rc); std::cerr << out; return false; }
        { std::lock_guard<std::mutex> guard(g_lock); g_lastError.clear(); }
        return true;
    }
    // If Ryzen Master executable present, try a best-effort invocation (may require admin privileges).
    const std::initializer_list<const char*> rmPaths{ "C:/Program Files/AMD/RyzenMaster/RyzenMaster.exe", "C:/Program Files (x86)/AMD/RyzenMaster/RyzenMaster.exe", "C:/AMD/RyzenMaster/RyzenMaster.exe" };
    for (auto p : rmPaths) {
        if (std::filesystem::exists(p)) {
            // Hypothetical command, adapt if the CLI is known
            std::string cmd = std::string("\"") + p + "\" -setoc core=all,offset=" + std::to_string(offset);
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (!pipe) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "failed_to_execute_rm"; return false; }
            std::array<char, 256> buf{};
            std::string out;
            while (fgets(buf.data(), (int)buf.size(), pipe)) out += buf.data();
            int rc = _pclose(pipe);
            if (rc != 0) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "rm_cli_failed:" + std::to_string(rc); std::cerr << out; return false; }
            { std::lock_guard<std::mutex> guard(g_lock); g_lastError.clear(); }
            return true;
        }
    }
    // No tool found; remain in simulation mode.
    return true;
}

bool ApplyCpuTargetAllCoreMhz(int mhz) {
    std::lock_guard<std::mutex> guard(g_lock);
    std::cout << "[vendor] Request CPU all-core target=" << mhz << " MHz (simulation)" << std::endl;
    return true;
}

bool ApplyGpuClockOffsetMhz(int offset) {
    std::lock_guard<std::mutex> guard(g_lock);
    std::cout << "[vendor] Request GPU offset=" << offset << " MHz" << std::endl;
    const char* adrenalinEnv = std::getenv("ADRENALIN_CLI");
    if (adrenalinEnv && std::filesystem::exists(adrenalinEnv)) {
        std::string cmd = std::string(adrenalinEnv) + " --set-gpu-offset " + std::to_string(offset);
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "failed_to_execute_adrenalin_cli"; return false; }
        std::array<char, 256> buf{};
        std::string out;
        while (fgets(buf.data(), (int)buf.size(), pipe)) out += buf.data();
        int rc = _pclose(pipe);
        if (rc != 0) { std::lock_guard<std::mutex> guard(g_lock); g_lastError = "adrenalin_cli_failed:" + std::to_string(rc); std::cerr << out; return false; }
        { std::lock_guard<std::mutex> guard(g_lock); g_lastError.clear(); }
        return true;
    }
    // If AMD SMI is present, a limited command may be possible; nvidia-smi can also set clocks on NVIDIA GPUs.
    if (ExistsOne({"C:/Windows/System32/nvidia-smi.exe", "nvidia-smi"})) {
        // We don't apply a direct offset using nvidia-smi here; it's a noop for now with a log.
        std::cout << "[vendor] NVIDIA SMI present - clock offset application not implemented" << std::endl;
        return true;
    }
    if (ExistsOne({"C:/Windows/System32/amd-smi.exe", "C:/Program Files/AMD/amd-smi.exe", "amd-smi"})) {
        std::cout << "[vendor] AMD SMI present - clock offset application not implemented (sysctl varies)" << std::endl;
        return true;
    }
    // No real tool; simulation.
    return true;
}

const std::string& LastError() { return g_lastError; }

} // namespace overclock_vendor
