// overclock_vendor_stub.cpp
// Stub implementation for vendor-specific overclocking functions

#include "overclock_vendor.h"
#include <string>

namespace overclock_vendor {

static std::string g_lastError = "Not implemented";

bool DetectRyzenMaster(AppState& st) {
    (void)st;
    return false;
}

bool DetectAdrenalinCLI(AppState& st) {
    (void)st;
    return false;
}

bool ApplyCpuOffsetMhz(int offset) {
    (void)offset;
    // Stub - no actual hardware control
    g_lastError = "Overclock not supported (stub implementation)";
    return false;
}

bool ApplyCpuTargetAllCoreMhz(int mhz) {
    (void)mhz;
    // Stub - no actual hardware control
    g_lastError = "Overclock not supported (stub implementation)";
    return false;
}

bool ApplyGpuClockOffsetMhz(int offset) {
    (void)offset;
    // Stub - no actual hardware control
    g_lastError = "Overclock not supported (stub implementation)";
    return false;
}

const std::string& LastError() {
    return g_lastError;
}

} // namespace overclock_vendor

