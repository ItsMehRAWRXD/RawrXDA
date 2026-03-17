// ============================================================================
// enterprise_license.cpp — C++20 Singleton Bridge Implementation (v2)
// ============================================================================
// Wraps the MASM Enterprise License System for integration with:
//   - StreamingEngineRegistry (engine gating)
//   - Win32IDE (status display, license import UI)
//   - API Server (feature negotiation)
//   - Diagnostics (license health reporting)
//
// v2 Changes:
//   - Singleton Instance() pattern (thread-safe Meyer's singleton)
//   - Shield_InitializeDefense() called before license init
//   - InstallLicenseFromFile() for .rawrlic file loading
//   - Model size / context length tier limits
//   - LicenseGuard RAII implementation
//   - License change callbacks
//   - C-link exports for ASM ↔ C++ callback bridge
//
// All crypto, anti-tamper, and hardware fingerprinting lives in ASM.
// This file provides thread-safe C++ accessors and engine registry hooks.
// ============================================================================

#include "enterprise_license.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>

#include "logging/logger.h"
static Logger s_logger("enterprise_license");

namespace RawrXD {

// ============================================================================
// Singleton
// ============================================================================
EnterpriseLicense& EnterpriseLicense::Instance() {
    static EnterpriseLicense instance;
    return instance;
}

// ============================================================================
// Initialize
// ============================================================================
bool EnterpriseLicense::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;  // Already initialized
    }
    
    s_logger.info("[EnterpriseLicense] v2 — Initializing defense shield...");

    // ---- Phase 1: Shield Defense (5-layer anti-tamper) ----
    int32_t shieldResult = Shield_InitializeDefense();
    if (shieldResult == 0) {
        s_logger.error("[EnterpriseLicense] SHIELD TAMPER DETECTED — defense layers failed");
        // Don't abort — set tampered state but continue (silent degradation)
        m_initialized = true;
        LicenseState oldState = m_lastState;
        m_lastState = LicenseState::Tampered;
        notifyStateChange(oldState, m_lastState);
        return true;  // "Initialized" but tampered — community mode, degraded
    }

    s_logger.info("[EnterpriseLicense] Shield defense: ALL LAYERS PASSED");

    // ---- Phase 2: License System Init ----
    s_logger.info("[EnterpriseLicense] Initializing license subsystem...");
    
    int64_t result = Enterprise_InitLicenseSystem();
    
    if (result != 0) {
        std::ostringstream oss;
        oss << "[EnterpriseLicense] License init failed with status: 0x" << std::hex << result << std::dec;
        s_logger.error(oss.str());
        s_logger.error("[EnterpriseLicense] Continuing in Community mode (non-fatal)");
        // Non-fatal: community mode still works
    }
    
    m_initialized = true;
    
    // Track state changes
    LicenseState oldState = m_lastState;
    LicenseState newState = GetState();
    m_lastState = newState;
    
    if (oldState != newState) {
        notifyStateChange(oldState, newState);
    }

    // Log what we got
    switch (newState) {
        case LicenseState::ValidEnterprise:
            s_logger.info("[EnterpriseLicense] Enterprise license ACTIVE");
            s_logger.info("[EnterpriseLicense] 800B Dual-Engine: ");
            s_logger.info("[EnterpriseLicense] Max Model: ");
            break;
        case LicenseState::ValidTrial:
            s_logger.info("[EnterpriseLicense] Trial license active (limited features)");
            break;
        case LicenseState::Expired:
            s_logger.info("[EnterpriseLicense] License EXPIRED — running Community mode");
            break;
        case LicenseState::HardwareMismatch:
            s_logger.info("[EnterpriseLicense] Hardware mismatch — running Community mode");
            break;
        case LicenseState::Tampered:
            s_logger.info("[EnterpriseLicense] TAMPER DETECTED — degraded Community mode");
            break;
        default:
            s_logger.info("[EnterpriseLicense] No valid license — Community mode");
            break;
    }

    // When license has DualEngine800B, set g_800B_Unlocked for ASM/kernel path
    if (HasFeatureMask(LicenseFeature::DualEngine800B)) {
        (void)Enterprise_Unlock800BDualEngine();
    }

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void EnterpriseLicense::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) return;
    
    Enterprise_Shutdown();
    
    LicenseState oldState = m_lastState;
    m_lastState = LicenseState::Invalid;
    m_initialized = false;
    
    if (oldState != LicenseState::Invalid) {
        notifyStateChange(oldState, LicenseState::Invalid);
    }
    
    s_logger.info("[EnterpriseLicense] License subsystem shut down");
}

// ============================================================================
// Feature Checks
// ============================================================================
bool EnterpriseLicense::HasFeature(EnterpriseFeature feature) const {
    if (!m_initialized) return false;
    return Enterprise_CheckFeature(static_cast<uint64_t>(feature)) != 0;
}

bool EnterpriseLicense::HasFeatureMask(uint64_t featureMask) const {
    if (!m_initialized) return false;
    return Enterprise_CheckFeature(featureMask) != 0;
}

bool EnterpriseLicense::Is800BUnlocked() const {
    if (!m_initialized) return false;
    return Enterprise_Unlock800BDualEngine() != 0;
}

bool EnterpriseLicense::IsEnterprise() const {
    return GetState() == LicenseState::ValidEnterprise;
}

// ============================================================================
// State Queries
// ============================================================================
LicenseState EnterpriseLicense::GetState() const {
    if (!m_initialized) return LicenseState::Invalid;
    return static_cast<LicenseState>(Enterprise_GetLicenseStatus());
}

std::string EnterpriseLicense::GetFeatureString() const {
    if (!m_initialized) return "RawrXD Community (License system not initialized)";
    
    char buffer[1024] = {};
    int64_t written = Enterprise_GetFeatureString(buffer, sizeof(buffer));
    
    if (written <= 0) {
        return "RawrXD Community (Limited to 70B models)";
    }
    
    return std::string(buffer, static_cast<size_t>(written));
}

uint64_t EnterpriseLicense::GetHardwareHash() const {
    return Enterprise_GenerateHardwareHash();
}

uint64_t EnterpriseLicense::GetFeatureMask() const {
    if (!m_initialized) return 0;
    return g_EnterpriseFeatures;
}

const char* EnterpriseLicense::GetEditionName() const {
    switch (GetState()) {
        case LicenseState::ValidEnterprise: return "Enterprise";
        case LicenseState::ValidTrial:      return "Trial";
        case LicenseState::ValidOEM:        return "OEM";
        case LicenseState::Expired:         return "Expired";
        case LicenseState::HardwareMismatch: return "Hardware Mismatch";
        case LicenseState::Tampered:        return "Tampered";
        default:                            return "Community";
    }
}

// ============================================================================
// Model Size & Context Limits
// ============================================================================
uint64_t EnterpriseLicense::GetMaxModelSizeGB() const {
    switch (GetState()) {
        case LicenseState::ValidEnterprise: return 800;  // 800B parameter models
        case LicenseState::ValidOEM:        return 800;
        case LicenseState::ValidTrial:      return 180;  // Up to 180B trial
        default:                            return 70;   // Community: 70B max
    }
}

uint64_t EnterpriseLicense::GetMaxContextLength() const {
    if (HasFeatureMask(LicenseFeature::UnlimitedContext)) {
        return 200000;  // 200K tokens — enterprise unlimited
    }
    
    switch (GetState()) {
        case LicenseState::ValidEnterprise: return 200000;  // 200K
        case LicenseState::ValidOEM:        return 200000;
        case LicenseState::ValidTrial:      return 128000;  // 128K
        default:                            return 32000;   // 32K community
    }
}

uint32_t EnterpriseLicense::GetShieldState() const {
    // Shield state is stored in ASM global g_ShieldState
    // We read it indirectly via the integrity check
    // For now, return the defense init result cached during Initialize()
    if (!m_initialized) return 0;
    if (m_lastState == LicenseState::Tampered) return 0;
    return 0x1F;  // All 5 layers passed if we got past init without tamper
}

// ============================================================================
// License Installation
// ============================================================================
bool EnterpriseLicense::InstallLicense(const void* licenseBlob, size_t blobSize,
                                        const void* signature) {
    if (!m_initialized) {
        s_logger.error( "[EnterpriseLicense] Cannot install — system not initialized" 
                  << std::endl;
        return false;
    }
    
    if (!licenseBlob || blobSize == 0 || !signature) {
        s_logger.error( "[EnterpriseLicense] Invalid license data (null or zero-size)" 
                  << std::endl;
        return false;
    }
    
    LicenseState oldState = GetState();
    
    s_logger.info("[EnterpriseLicense] Installing license (");
    
    int64_t result = Enterprise_InstallLicense(licenseBlob, 
                                                static_cast<uint64_t>(blobSize),
                                                signature);
    
    if (result != 0) {
        s_logger.error( "[EnterpriseLicense] License installation failed: 0x"
                  << std::hex << result << std::dec << std::endl;
        return false;
    }
    
    LicenseState newState = GetState();
    m_lastState = newState;
    
    s_logger.info("[EnterpriseLicense] License installed successfully!");
    s_logger.info("[EnterpriseLicense] Edition: ");
    s_logger.info("[EnterpriseLicense] Features: ");
    s_logger.info("[EnterpriseLicense] Max Model: ");
    
    if (oldState != newState) {
        notifyStateChange(oldState, newState);
    }
    
    return true;
}

bool EnterpriseLicense::InstallLicenseFromFile(const std::wstring& path) {
    // .rawrlic format: [blob_data...][512-byte RSA-4096 signature]
    constexpr size_t RSA_SIG_SIZE = 512;

    // Convert wstring to narrow string for MinGW ifstream compatibility
    std::string narrowPath(path.begin(), path.end());
    std::ifstream file(narrowPath.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        s_logger.error( "[EnterpriseLicense] Cannot open license file" << std::endl;
        return false;
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize <= RSA_SIG_SIZE) {
        s_logger.error( "[EnterpriseLicense] License file too small (need > "
                  << RSA_SIG_SIZE << " bytes, got " << fileSize << ")" << std::endl;
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    size_t blobSize = fileSize - RSA_SIG_SIZE;
    const void* blob = data.data();
    const void* sig  = data.data() + blobSize;

    s_logger.info("[EnterpriseLicense] Loading license from file (");

    return InstallLicense(blob, blobSize, sig);
}

bool EnterpriseLicense::InstallLicenseFromFile(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    return InstallLicenseFromFile(wpath);
}

// ============================================================================
// Revalidation
// ============================================================================
bool EnterpriseLicense::Revalidate() {
    if (!m_initialized) return false;
    
    LicenseState oldState = GetState();
    int64_t result = Enterprise_ValidateLicense();
    LicenseState newState = GetState();
    
    if (oldState != newState) {
        m_lastState = newState;
        notifyStateChange(oldState, newState);
    }
    
    return result == 0;
}

// ============================================================================
// Allocation Budget Check
// ============================================================================
bool EnterpriseLicense::CheckAllocationBudget(uint64_t requestedBytes) const {
    if (!m_initialized) {
        // Not initialized = community limits
        constexpr uint64_t COMMUNITY_LIMIT = 17179869184ULL;  // 16 GB
        return requestedBytes <= COMMUNITY_LIMIT;
    }
    
    return Streaming_CheckEnterpriseBudget(requestedBytes) != 0;
}

// ============================================================================
// Callbacks
// ============================================================================
void EnterpriseLicense::OnLicenseChange(LicenseChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(std::move(callback));
}

void EnterpriseLicense::notifyStateChange(LicenseState oldState, LicenseState newState) {
    // Call C-link export (ASM-visible)
    EnterpriseStateChanged(static_cast<uint32_t>(oldState),
                           static_cast<uint32_t>(newState));

    // Call registered C++ callbacks
    for (const auto& cb : m_callbacks) {
        if (cb) {
            cb(oldState, newState);
        }
    }
}

// ============================================================================
// LicenseGuard RAII Implementation
// ============================================================================
LicenseGuard::LicenseGuard(EnterpriseFeature required)
    : m_granted(false)
    , m_required(required)
{
    m_granted = EnterpriseLicense::Instance().HasFeature(required);
}

// ============================================================================
// C-Link Callback Exports (called from C++, addressable from ASM)
// ============================================================================
extern "C" {

void EnterpriseLog(const char* message) {
    if (message) {
        s_logger.info("[EnterpriseLicense:ASM] ");
    }
}

void EnterpriseStateChanged(uint32_t oldState, uint32_t newState) {
    s_logger.info("[EnterpriseLicense] State transition: ");
}

} // extern "C"

} // namespace RawrXD
