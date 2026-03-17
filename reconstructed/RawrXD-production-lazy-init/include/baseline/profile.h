#pragma once

#include <string>
#include "settings.h"

namespace baseline_profile {

bool Load(AppState& state, const std::string& path = "baseline_profile.json");
bool Save(const AppState& state, const std::string& path = "baseline_profile.json");

} // namespace baseline_profile
