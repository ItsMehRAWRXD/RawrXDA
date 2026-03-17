#pragma once
#include <string>
struct AppState;
namespace baseline_profile {
    bool Load(AppState& state, const std::string& path="settings/oc_baseline.json");
    bool Save(const AppState& state, const std::string& path="settings/oc_baseline.json");
}
