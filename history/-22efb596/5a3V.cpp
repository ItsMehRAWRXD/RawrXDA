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
#include <chrono>
#include <map>
#include <string>

#include "telemetry/logger.h"
#include "telemetry/UnifiedTelemetryCore.h"
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace RawrXD {

namespace {
const char* LicenseStateName(LicenseState state) {
    switch (state) {
        case LicenseState::ValidEnterprise: return "ValidEnterprise";
        case LicenseState::ValidPro:        return "ValidPro";
        case LicenseState::ValidTrial:      return "ValidTrial";
        case LicenseState::ValidOEM:        return "ValidOEM";
        case LicenseState::Expired:         return "Expired";
        case LicenseState::HardwareMismatch:return "HardwareMismatch";
        case LicenseState::Tampered:        return "Tampered";
        case LicenseState::Invalid:         return "Invalid";
        default:                            return "Unknown";
    }
}

int64_t DurationMs(std::chrono::steady_clock::time_point start,
                   std::chrono::steady_clock::time_point end) {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

void EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel level,
                      const std::string& category,
                      const std::string& message,
                      const std::map<std::string, std::string>& tags = {}) {
    auto& telemetry = RawrXD::Telemetry::UnifiedTelemetryCore::Instance();
    if (!telemetry.IsInitialized()) return;
    telemetry.Emit(RawrXD::Telemetry::TelemetrySource::System, level,
                   category, message, 0.0, "", tags);
}

void EmitLicenseSpan(RawrXD::Telemetry::TelemetryLevel level,
                     const std::string& category,
                     const std::string& message,
                     int64_t durationMs,
                     const std::map<std::string, std::string>& tags = {}) {
    auto& telemetry = RawrXD::Telemetry::UnifiedTelemetryCore::Instance();
    if (!telemetry.IsInitialized()) return;
    telemetry.EmitSpan(RawrXD::Telemetry::TelemetrySource::System, level,
                       category, message, durationMs, tags);
}
}

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
    const auto startTime = std::chrono::steady_clock::now();

    Logger::instance().logInfo("license.init.start", {
        {"component", "EnterpriseLicense"}
    });
    EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                     "license.init", "start",
                     {{"component", "EnterpriseLicense"}});
    
    if (m_initialized) {
        Logger::instance().logInfo("license.init.skip", {
            {"reason", "already_initialized"}
        });
        EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Debug,
                         "license.init", "already_initialized");
        return true;  // Already initialized
    }
    
    std::cout << "[EnterpriseLicense] v2 — Initializing defense shield..." << std::endl;
    auto initStart = std::chrono::steady_clock::now();
    g_licenseInitCount.fetch_add(1, std::memory_order_relaxed);
    LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                    "license_init_start",
                    { {"phase", "shield"} });

    // ---- Phase 1: Shield Defense (5-layer anti-tamper) ----
    auto shieldStart = std::chrono::steady_clock::now();
    int32_t shieldResult = Shield_InitializeDefense();
    auto shieldEnd = std::chrono::steady_clock::now();
    auto shieldMs = std::chrono::duration_cast<std::chrono::milliseconds>(shieldEnd - shieldStart).count();
    LicenseObs().recordMetric("license.shield_init_ms", static_cast<float>(shieldMs), {}, "ms");
    if (shieldResult == 0) {
        std::cerr << "[EnterpriseLicense] SHIELD TAMPER DETECTED — defense layers failed"
                  << std::endl;
                Logger::instance().logError("license.shield.fail", {
                    {"result", "tamper"},
                    {"shield_result", "0"}
                });
                EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Error,
                                 "license.shield", "tamper_detected");
        LogLicenseEvent(AgenticObservability::LogLevel::ERROR,
                "license_shield_failed",
                { {"duration_ms", shieldMs} });
        // Don't abort — set tampered state but continue (silent degradation)
        m_initialized = true;
                const auto endTime = std::chrono::steady_clock::now();
                const int64_t durationMs = DurationMs(startTime, endTime);
                Logger::instance().logInfo("license.init.complete", {
                    {"result", "tampered"},
                    {"duration_ms", std::to_string(durationMs)}
                });
                EmitLicenseSpan(RawrXD::Telemetry::TelemetryLevel::Warning,
                                "license.init", "complete", durationMs,
                                {{"result", "tampered"}});
        LicenseState oldState = m_lastState;
        m_lastState = LicenseState::Tampered;
        notifyStateChange(oldState, m_lastState);
        return true;  // "Initialized" but tampered — community mode, degraded
            Logger::instance().logInfo("license.shield.ok", {
                {"result", "passed"}
            });
            EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                             "license.shield", "passed");
    }

    std::cout << "[EnterpriseLicense] Shield defense: ALL LAYERS PASSED" << std::endl;
    LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                    "license_shield_ok",
                    { {"duration_ms", shieldMs} });

    // ---- Phase 2: License System Init ----
    std::cout << "[EnterpriseLicense] Initializing license subsystem..." << std::endl;
    
    auto licenseStart = std::chrono::steady_clock::now();
    int64_t result = Enterprise_InitLicenseSystem();
    auto licenseEnd = std::chrono::steady_clock::now();
    auto licenseMs = std::chrono::duration_cast<std::chrono::milliseconds>(licenseEnd - licenseStart).count();
    LicenseObs().recordMetric("license.subsystem_init_ms", static_cast<float>(licenseMs), {}, "ms");
    
    if (result != 0) {
        std::cerr << "[EnterpriseLicense] License init returned status: 0x"
                  << std::hex << result << std::dec << std::endl;
        std::cerr << "[EnterpriseLicense] Continuing in Community mode (non-fatal)" 
                  << std::endl;
        g_licenseInitFailCount.fetch_add(1, std::memory_order_relaxed);
        LogLicenseEvent(AgenticObservability::LogLevel::WARN,
                        "license_init_nonfatal",
                        { {"status", static_cast<uint64_t>(result)},
                          {"duration_ms", licenseMs} });
        // Non-fatal: community mode still works (0 = success per contract)
    } else {
        std::cout << "[EnterpriseLicense] License subsystem initialized successfully" << std::endl;
        LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                        "license_init_ok",
                        { {"duration_ms", licenseMs} });
    }
    
    m_initialized = true;
    
    // Dev / License Creator: unlock all features when RAWRXD_ENTERPRISE_DEV=1
    int64_t devUnlock = Enterprise_DevUnlock();
    if (devUnlock != 0) {
        std::cout << "[EnterpriseLicense] Dev unlock active (RAWRXD_ENTERPRISE_DEV) — all features enabled"
                  << std::endl;
        LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                        "license_dev_unlock_active",
                        { {"feature_mask", GetFeatureMask()} });
    }
    
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
            std::cout << "[EnterpriseLicense] Enterprise license ACTIVE"
                      << " — Features: 0x" << std::hex << GetFeatureMask() 
                      << std::dec << std::endl;
            std::cout << "[EnterpriseLicense] 800B Dual-Engine: "
                      << (HasFeatureMask(LicenseFeature::DualEngine800B) ? "UNLOCKED" : "locked")
                      << std::endl;
            std::cout << "[EnterpriseLicense] Max Model: " << GetMaxModelSizeGB() 
                      << "GB | Max Context: " << GetMaxContextLength() << " tokens"
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
        case LicenseState::Tampered:
            std::cout << "[EnterpriseLicense] TAMPER DETECTED — degraded Community mode"
                      << std::endl;
            break;
        case LicenseState::ValidOEM:
            std::cout << "[EnterpriseLicense] OEM license ACTIVE"
                      << " — Features: 0x" << std::hex << GetFeatureMask()
                      << std::dec << std::endl;
            std::cout << "[EnterpriseLicense] Max Model: " << GetMaxModelSizeGB()
                      << "GB | Max Context: " << GetMaxContextLength() << " tokens"
                      << std::endl;
            break;
        case LicenseState::ValidPro:
            std::cout << "[EnterpriseLicense] Professional license ACTIVE"
                      << " — Features: 0x" << std::hex << GetFeatureMask()
                      << std::dec << std::endl;
            std::cout << "[EnterpriseLicense] Max Model: " << GetMaxModelSizeGB()
                      << "GB | Max Context: " << GetMaxContextLength() << " tokens"
                      << std::endl;
            break;
        default:
            std::cout << "[EnterpriseLicense] No valid license — Community mode"
                      << " (models limited to " << GetMaxModelSizeGB() << "GB)" 
                      << std::endl;
            break;
    }

    // When license has DualEngine800B, set g_800B_Unlocked for ASM/kernel path
    if (HasFeatureMask(LicenseFeature::DualEngine800B)) {
        (void)Enterprise_Unlock800BDualEngine();
    }

    auto initEnd = std::chrono::steady_clock::now();
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart).count();
    LicenseObs().recordMetric("license.init_total_ms", static_cast<float>(initMs), {}, "ms");
    LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                    "license_init_complete",
                    { {"duration_ms", initMs},
                      {"state", LicenseStateToString(newState)},
                      {"feature_mask", GetFeatureMask()} });

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
    
    std::cout << "[EnterpriseLicense] License subsystem shut down" << std::endl;
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
    // Read-only check — do NOT call Enterprise_Unlock800BDualEngine() here
    // as it mutates g_800B_Unlocked as a side effect
    return Enterprise_CheckFeature(static_cast<uint64_t>(EnterpriseFeature::DualEngine800B)) != 0;
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
        case LicenseState::ValidPro:        return "Professional";
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
        case LicenseState::ValidPro:        return 400;  // Pro: up to 400B
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
        case LicenseState::ValidPro:        return 128000;  // Pro: 128K
        case LicenseState::ValidTrial:      return 128000;  // Trial: 128K
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
        std::cerr << "[EnterpriseLicense] Cannot install — system not initialized" 
                  << std::endl;
        return false;
    }
    
    if (!licenseBlob || blobSize == 0 || !signature) {
        std::cerr << "[EnterpriseLicense] Invalid license data (null or zero-size)" 
                  << std::endl;
        return false;
    }
    
    LicenseState oldState = GetState();
    g_licenseInstallCount.fetch_add(1, std::memory_order_relaxed);
    
    std::cout << "[EnterpriseLicense] Installing license (" << blobSize 
              << " bytes)..." << std::endl;
    
    auto installStart = std::chrono::steady_clock::now();
    int64_t result = Enterprise_InstallLicense(licenseBlob, 
                                                static_cast<uint64_t>(blobSize),
                                                signature);
    auto installEnd = std::chrono::steady_clock::now();
    auto installMs = std::chrono::duration_cast<std::chrono::milliseconds>(installEnd - installStart).count();
    LicenseObs().recordMetric("license.install_ms", static_cast<float>(installMs), {}, "ms");
    
    if (result != 0) {
        std::cerr << "[EnterpriseLicense] License installation failed: 0x"
                  << std::hex << result << std::dec << std::endl;
                g_licenseInstallFailCount.fetch_add(1, std::memory_order_relaxed);
                LogLicenseEvent(AgenticObservability::LogLevel::ERROR,
                                                "license_install_failed",
                                                { {"status", static_cast<uint64_t>(result)},
                                                    {"duration_ms", installMs} });
        return false;
    }
    
    LicenseState newState = GetState();
    m_lastState = newState;
    
    std::cout << "[EnterpriseLicense] License installed successfully!" << std::endl;
    std::cout << "[EnterpriseLicense] Edition: " << GetEditionName() << std::endl;
    std::cout << "[EnterpriseLicense] Features: " << GetFeatureString() << std::endl;
    std::cout << "[EnterpriseLicense] Max Model: " << GetMaxModelSizeGB() 
              << "GB | Max Context: " << GetMaxContextLength() << " tokens" << std::endl;
    
    if (oldState != newState) {
        notifyStateChange(oldState, newState);
    }

    LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                    "license_install_ok",
                    { {"duration_ms", installMs},
                      {"state", LicenseStateToString(newState)},
                      {"feature_mask", GetFeatureMask()} });
    
    return true;
}

bool EnterpriseLicense::InstallLicenseFromFile(const std::wstring& path) {
    // .rawrlic format: [blob_data...][512-byte RSA-4096 signature]
    constexpr size_t RSA_SIG_SIZE = 512;

#ifdef _WIN32
    // Convert wstring to narrow UTF-8 for ifstream
    int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath(len > 0 ? static_cast<size_t>(len) - 1 : 0, '\0');
    if (len > 0)
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &narrowPath[0], len, nullptr, nullptr);
#else
    std::string narrowPath(path.begin(), path.end());
#endif
    std::ifstream file(narrowPath.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[EnterpriseLicense] Cannot open license file" << std::endl;
        return false;
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize <= RSA_SIG_SIZE) {
        std::cerr << "[EnterpriseLicense] License file too small (need > "
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

    std::cout << "[EnterpriseLicense] Loading license from file (" 
              << blobSize << " byte blob + " << RSA_SIG_SIZE << " byte signature)"
              << std::endl;

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
    g_licenseRevalidateCount.fetch_add(1, std::memory_order_relaxed);
    auto revalidateStart = std::chrono::steady_clock::now();
    int64_t result = Enterprise_ValidateLicense();
    auto revalidateEnd = std::chrono::steady_clock::now();
    auto revalidateMs = std::chrono::duration_cast<std::chrono::milliseconds>(revalidateEnd - revalidateStart).count();
    LicenseObs().recordMetric("license.revalidate_ms", static_cast<float>(revalidateMs), {}, "ms");
    LicenseState newState = GetState();
    
    if (oldState != newState) {
        m_lastState = newState;
        notifyStateChange(oldState, newState);
    }

    if (result != 0) {
        g_licenseRevalidateFailCount.fetch_add(1, std::memory_order_relaxed);
        LogLicenseEvent(AgenticObservability::LogLevel::WARN,
                        "license_revalidate_failed",
                        { {"status", static_cast<uint64_t>(result)},
                          {"duration_ms", revalidateMs},
                          {"state", LicenseStateToString(newState)} });
    } else {
        LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                        "license_revalidate_ok",
                        { {"duration_ms", revalidateMs},
                          {"state", LicenseStateToString(newState)} });
    }
    
    return result == 0;
}

// ============================================================================
// Allocation Budget Check
// ============================================================================
bool EnterpriseLicense::CheckAllocationBudget(uint64_t requestedBytes) const {
    if (!m_initialized) {
        // Not initialized = community limits (must match manifest CommunityBudget)
        constexpr uint64_t COMMUNITY_LIMIT = 4ULL * 1024 * 1024 * 1024;  // 4 GB
        return requestedBytes <= COMMUNITY_LIMIT;
    }
    
    return Streaming_CheckEnterpriseBudget(requestedBytes) != 0;
}

// ============================================================================
// Callbacks
// ============================================================================
void EnterpriseLicense::OnLicenseChange(LicenseChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(callback);
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
        std::cout << "[EnterpriseLicense:ASM] " << message << std::endl;
    }
}

void EnterpriseStateChanged(uint32_t oldState, uint32_t newState) {
    std::cout << "[EnterpriseLicense] State transition: "
              << oldState << " -> " << newState << std::endl;
        g_licenseStateChangeCount.fetch_add(1, std::memory_order_relaxed);
        LogLicenseEvent(AgenticObservability::LogLevel::INFO,
                                        "license_state_change",
                                        { {"old_state", LicenseStateToString(static_cast<LicenseState>(oldState))},
                                            {"new_state", LicenseStateToString(static_cast<LicenseState>(newState))} });
}

} // extern "C"

} // namespace RawrXD
