// ============================================================================
// offline_mode.cpp — Air-Gapped / Fully Offline Deployment
// ============================================================================
// Feature: AirGappedDeploy (Sovereign tier). Disables network, uses local only.
// ============================================================================

#include <cstdbool>

namespace RawrXD {

bool g_airGappedEnabled = false;

bool IsAirGappedModeEnabled() {
    return g_airGappedEnabled;
}

void SetAirGappedMode(bool enable) {
    g_airGappedEnabled = enable;
}

} // namespace RawrXD
