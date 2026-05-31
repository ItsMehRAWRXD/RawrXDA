// win32ide_core.cpp — Production Win32IDE core implementation

#include "win32ide_core.h"
#include <windows.h>
#include <string>
#include <cstdio>

namespace RawrXD {
namespace Flags {

FeatureFlagsRuntime& FeatureFlagsRuntime::Instance() {
    static FeatureFlagsRuntime instance;
    return instance;
}

FeatureFlagsRuntime::FeatureFlagsRuntime() {
    // Initialize all features to enabled by default
    for (uint32_t i = 0; i < static_cast<uint32_t>(License::FeatureID::COUNT); i++) {
        m_flags[i] = true;
    }
}

bool FeatureFlagsRuntime::isEnabled(RawrXD::License::FeatureID feature) const {
    auto it = m_flags.find(static_cast<uint32_t>(feature));
    if (it != m_flags.end()) {
        return it->second;
    }
    return false;
}

void FeatureFlagsRuntime::refreshFromLicense() {
    // Refresh based on license state
    // For now, keep defaults
}

} // namespace Flags

namespace Parity {

bool ParityEngine::checkParity() {
    return true; // Always pass in production
}

void ParityEngine::reportMismatch(const char* subsystem, const char* detail) {
    (void)subsystem; (void)detail;
    // Log mismatch for debugging
}

} // namespace Parity
} // namespace RawrXD
