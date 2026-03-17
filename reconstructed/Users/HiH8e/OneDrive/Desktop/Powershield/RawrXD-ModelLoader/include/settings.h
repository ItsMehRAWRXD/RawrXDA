#pragma once
#include <string>
struct AppState; // forward

namespace Settings {
    bool LoadCompute(AppState& state, const std::string& path="settings/compute.conf");
    bool SaveCompute(const AppState& state, const std::string& path="settings/compute.conf");
    bool LoadOverclock(AppState& state, const std::string& path="settings/overclock.conf");
    bool SaveOverclock(const AppState& state, const std::string& path="settings/overclock.conf");
}
