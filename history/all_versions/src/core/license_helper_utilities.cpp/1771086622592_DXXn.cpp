// ============================================================================
// license_helper_utilities.cpp — License System Helper Utilities
// ============================================================================

#include "../include/enterprise_license.h"
#include <cstring>
#include <ctime>

using namespace RawrXD::License;

// ============================================================================
// Tier Name Helpers
// ============================================================================

extern "C" const char* tierName(LicenseTierV2 tier) {
    switch (tier) {
        case LicenseTierV2::Community:    return "Community";
        case LicenseTierV2::Professional: return "Professional";
        case LicenseTierV2::Enterprise:   return "Enterprise";
        case LicenseTierV2::Sovereign:    return "Sovereign";
        default:                          return "Unknown";
    }
}

const char* getTierNameForDisplay(uint32_t tier) {
    return tierName(static_cast<LicenseTierV2>(tier));
}

// ============================================================================
// Timestamp Formatting Helpers
// ============================================================================

const char* formatTimestampForDisplay(uint32_t timestamp) {
    static char buf[64];
    time_t t = timestamp;
    struct tm* tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

// ============================================================================
// Feature Name Helpers
// ============================================================================

const char* getFeatureNameForID(uint32_t featureID) {
    static const char* featureNames[] = {
        "Core Engine",                          // 0
        "GPU Acceleration",                     // 1
        "Multi-GPU Support",                    // 2
        "Vision Processing",                    // 3
        "Audio Processing",                     // 4
        "Agent Framework",                      // 5
        "Swarm Intelligence",                   // 6
        "Hotpatching System",                   // 7
        "Code Analysis",                        // 8
        "Auto-Refactoring",                     // 9
        "Chain of Thought Reasoning",           // 10
        // Add more as needed
    };

    if (featureID < sizeof(featureNames) / sizeof(featureNames[0])) {
        return featureNames[featureID];
    }
    return "Unknown Feature";
}

// ============================================================================
// Statistics Helpers
// ============================================================================

extern "C" {

float getFeatureDenialRate() {
    return g_auditTrailManager.getDenialRate();
}

uint32_t getAuditEventCount() {
    return g_auditTrailManager.getTotalEvents();
}

uint32_t getAuditDenialCount() {
    return g_auditTrailManager.getTotalDenials();
}

bool isSystemAnomalous() {
    return g_auditTrailManager.isInAnomalousState();
}

}  // extern "C"
