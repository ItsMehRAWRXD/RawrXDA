// ============================================================================
// enterprise_license.cpp — C++ Bridge Implementation for Enterprise License
// ============================================================================
// Wraps the MASM Enterprise License System for integration with:
//   - StreamingEngineRegistry (engine gating)
//   - Win32IDE (status display, license import UI)
//   - API Server (feature negotiation)
//   - Diagnostics (license health reporting)
//
// All crypto, anti-tamper, and hardware fingerprinting lives in ASM.
// This file provides thread-safe C++ accessors and engine registry hooks.
// ============================================================================

#include "enterprise_license.h"
#include <iostream>
#include <cstring>

namespace RawrXD {

// ============================================================================
// Static Members
// ============================================================================
bool EnterpriseLicense::s_initialized = false;
std::mutex EnterpriseLicense::s_mutex;

// ============================================================================
// Initialize
// ============================================================================
bool EnterpriseLicense::initialize() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized) {
        return true;  // Already initialized
    }
    
    std::cout << "[EnterpriseLicense] Initializing license subsystem..." << std::endl;
    
    int64_t result = Enterprise_InitLicenseSystem();
    
    if (result != 0) {
        std::cerr << "[EnterpriseLicense] License init failed with status: 0x"
                  << std::hex << result << std::dec << std::endl;
        std::cerr << "[EnterpriseLicense] Continuing in Community mode (non-fatal)" 
                  << std::endl;
        // Non-fatal: community mode still works
    }
    
    s_initialized = true;
    
    // Log what we got
    LicenseState state = getState();
    switch (state) {
        case LicenseState::ValidEnterprise:
            std::cout << "[EnterpriseLicense] Enterprise license ACTIVE"
                      << " — Features: 0x" << std::hex << getFeatureMask() 
                      << std::dec << std::endl;
            std::cout << "[EnterpriseLicense] 800B Dual-Engine: "
                      << (isFeatureEnabled(LicenseFeature::DualEngine800B) ? "UNLOCKED" : "locked")
                      << std::endl;
            break;
        case LicenseState::ValidTrial:
            std::cout << "[EnterpriseLicense] Trial license active (limited features)"
                      << std::endl;
            break;
        case LicenseState::Expired:
            std::cout << "[EnterpriseLicense] License EXPIRED — running Community mode"
                      << std::endl;
            break;
        case LicenseState::HardwareMismatch:
            std::cout << "[EnterpriseLicense] Hardware mismatch — running Community mode"
                      << std::endl;
            break;
        default:
            std::cout << "[EnterpriseLicense] No valid license — Community mode"
                      << " (models limited to 70B)" << std::endl;
            break;
    }
    
    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void EnterpriseLicense::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_initialized) return;
    
    Enterprise_Shutdown();
    s_initialized = false;
    
    std::cout << "[EnterpriseLicense] License subsystem shut down" << std::endl;
}

// ============================================================================
// Feature Checks
// ============================================================================
bool EnterpriseLicense::isFeatureEnabled(uint64_t featureMask) {
    if (!s_initialized) return false;
    return Enterprise_CheckFeature(featureMask) != 0;
}

bool EnterpriseLicense::is800BUnlocked() {
    if (!s_initialized) return false;
    return Enterprise_Unlock800BDualEngine() != 0;
}

bool EnterpriseLicense::isEnterprise() {
    return getState() == LicenseState::ValidEnterprise;
}

// ============================================================================
// State Queries
// ============================================================================
LicenseState EnterpriseLicense::getState() {
    if (!s_initialized) return LicenseState::Invalid;
    return static_cast<LicenseState>(Enterprise_GetLicenseStatus());
}

std::string EnterpriseLicense::getFeatureString() {
    if (!s_initialized) return "RawrXD Community (License system not initialized)";
    
    char buffer[1024] = {};
    int64_t written = Enterprise_GetFeatureString(buffer, sizeof(buffer));
    
    if (written <= 0) {
        return "RawrXD Community (Limited to 70B models)";
    }
    
    return std::string(buffer, static_cast<size_t>(written));
}

uint64_t EnterpriseLicense::getHardwareHash() {
    return Enterprise_GenerateHardwareHash();
}

uint64_t EnterpriseLicense::getFeatureMask() {
    if (!s_initialized) return 0;
    return g_EnterpriseFeatures;
}

const char* EnterpriseLicense::getEditionName() {
    switch (getState()) {
        case LicenseState::ValidEnterprise: return "Enterprise";
        case LicenseState::ValidTrial:      return "Trial";
        case LicenseState::ValidOEM:        return "OEM";
        case LicenseState::Expired:         return "Expired";
        case LicenseState::HardwareMismatch: return "Hardware Mismatch";
        default:                            return "Community";
    }
}

// ============================================================================
// License Installation
// ============================================================================
bool EnterpriseLicense::installLicense(const void* licenseBlob, size_t blobSize,
                                        const void* signature) {
    if (!s_initialized) {
        std::cerr << "[EnterpriseLicense] Cannot install — system not initialized" 
                  << std::endl;
        return false;
    }
    
    if (!licenseBlob || blobSize == 0 || !signature) {
        std::cerr << "[EnterpriseLicense] Invalid license data (null or zero-size)" 
                  << std::endl;
        return false;
    }
    
    std::cout << "[EnterpriseLicense] Installing license (" << blobSize 
              << " bytes)..." << std::endl;
    
    int64_t result = Enterprise_InstallLicense(licenseBlob, 
                                                static_cast<uint64_t>(blobSize),
                                                signature);
    
    if (result != 0) {
        std::cerr << "[EnterpriseLicense] License installation failed: 0x"
                  << std::hex << result << std::dec << std::endl;
        return false;
    }
    
    std::cout << "[EnterpriseLicense] License installed successfully!" << std::endl;
    std::cout << "[EnterpriseLicense] Edition: " << getEditionName() << std::endl;
    std::cout << "[EnterpriseLicense] Features: " << getFeatureString() << std::endl;
    
    return true;
}

// ============================================================================
// Revalidation
// ============================================================================
bool EnterpriseLicense::revalidate() {
    if (!s_initialized) return false;
    
    int64_t result = Enterprise_ValidateLicense();
    return result == 0;
}

// ============================================================================
// Allocation Budget Check
// ============================================================================
bool EnterpriseLicense::checkAllocationBudget(uint64_t requestedBytes) {
    if (!s_initialized) {
        // Not initialized = community limits
        constexpr uint64_t COMMUNITY_LIMIT = 17179869184ULL;  // 16 GB
        return requestedBytes <= COMMUNITY_LIMIT;
    }
    
    return Streaming_CheckEnterpriseBudget(requestedBytes) != 0;
}

} // namespace RawrXD
