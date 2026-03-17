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
#include <fstream>
#include "nlohmann/json.hpp"
#include "enterprise_telemetry_compliance.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace RawrXD {

namespace {
constexpr uint64_t kTrialDurationUs = 30ULL * 24ULL * 60ULL * 60ULL * 1000000ULL;

uint64_t nowUs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()).count());
}

const char* stateToString(LicenseState state) {
    switch (state) {
        case LicenseState::ValidEnterprise: return "Enterprise";
        case LicenseState::ValidPro:        return "Professional";
        case LicenseState::ValidTrial:      return "Trial";
        case LicenseState::ValidOEM:        return "OEM";
        case LicenseState::Expired:         return "Expired";
        case LicenseState::HardwareMismatch: return "HardwareMismatch";
        case LicenseState::Tampered:        return "Tampered";
        default:                            return "Community";
    }
}

LicenseTier mapTelemetryTier(LicenseState state) {
    switch (state) {
        case LicenseState::ValidEnterprise: return LicenseTier::Enterprise;
        case LicenseState::ValidOEM:        return LicenseTier::OEM;
        case LicenseState::ValidPro:        return LicenseTier::Professional;
        case LicenseState::ValidTrial:      return LicenseTier::Professional;
        default:                            return LicenseTier::Community;
    }
}

void appendFeature(uint64_t mask, uint64_t bit, const char* name,
                   std::vector<std::string>& out) {
    if ((mask & bit) != 0) out.emplace_back(name);
}

struct AzureADConfig {
    std::string provider;
    std::string clientId;
    std::string jwksUrl;
    std::string authority;
};

bool loadAzureADConfig(AzureADConfig* out) {
    if (!out) return false;
    std::ifstream file("config/enterprise.json", std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (content.empty()) return false;

    auto json = nlohmann::json::parse(content, nullptr, false);
    if (json.is_discarded()) return false;

    out->provider = json.value("provider", "");
    out->clientId = json.value("client_id", "");
    out->jwksUrl = json.value("jwks_url", "");
    out->authority = json.value("authority", "");

    if (out->provider != "azure-ad") return false;
    if (out->clientId.empty() || out->clientId == "your-client-id-here") return false;
    return true;
}

#ifdef _WIN32
const wchar_t* kLicenseRegistryKey = L"Software\\RawrXD\\License";
const wchar_t* kLicenseRegistryValue = L"LastLicensePath";

bool readRegistryLicensePath(std::wstring& outPath) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLicenseRegistryKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, kLicenseRegistryValue, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ || size < sizeof(wchar_t)) {
        RegCloseKey(key);
        return false;
    }

    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, kLicenseRegistryValue, nullptr, &type,
                         reinterpret_cast<BYTE*>(&buffer[0]), &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }
    RegCloseKey(key);

    if (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
    outPath = buffer;
    return !outPath.empty();
}

void writeRegistryLicensePath(const std::wstring& path) {
    if (path.empty()) return;
    HKEY key = nullptr;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kLicenseRegistryKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, &disp) != ERROR_SUCCESS) {
        return;
    }

    DWORD size = static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t));
    RegSetValueExW(key, kLicenseRegistryValue, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(path.c_str()), size);
    RegCloseKey(key);
}
#endif
} // namespace

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
    
    std::cout << "[EnterpriseLicense] v2 — Initializing defense shield..." << std::endl;

    // ---- Phase 1: Shield Defense (5-layer anti-tamper) ----
    int32_t shieldResult = Shield_InitializeDefense();
    if (shieldResult == 0) {
        std::cerr << "[EnterpriseLicense] SHIELD TAMPER DETECTED — defense layers failed"
                  << std::endl;
        // Don't abort — set tampered state but continue (silent degradation)
        m_initialized = true;
        LicenseState oldState = m_lastState;
        m_lastState = LicenseState::Tampered;
        notifyStateChange(oldState, m_lastState);
        return true;  // "Initialized" but tampered — community mode, degraded
    }

    std::cout << "[EnterpriseLicense] Shield defense: ALL LAYERS PASSED" << std::endl;

    // ---- Phase 2: License System Init ----
    std::cout << "[EnterpriseLicense] Initializing license subsystem..." << std::endl;
    
    int64_t result = Enterprise_InitLicenseSystem();
    
    if (result != 0) {
        std::cerr << "[EnterpriseLicense] License init failed with status: 0x"
                  << std::hex << result << std::dec << std::endl;
        std::cerr << "[EnterpriseLicense] Continuing in Community mode (non-fatal)" 
                  << std::endl;
        // Non-fatal: community mode still works
    }
    
    m_initialized = true;
    
    // Track state changes
    LicenseState oldState = m_lastState;
    LicenseState newState = GetState();
    m_lastState = newState;
    
    if (oldState != newState) {
        notifyStateChange(oldState, newState);
    } else {
        updateTelemetry(newState, "init");
    }

#ifdef _WIN32
    // Best-effort reload from last known license path (UI persistence)
    std::wstring storedPath;
    if (readRegistryLicensePath(storedPath)) {
        m_lastLicensePath.assign(storedPath.begin(), storedPath.end());
        if (GetState() == LicenseState::Invalid) {
            if (InstallLicenseFromFile(storedPath)) {
                std::cout << "[EnterpriseLicense] Loaded license from registry path" << std::endl;
            }
        }
    }
#endif

    m_aadConfigured = false;
    m_aadAuthority.clear();
    m_aadClientId.clear();
    m_aadJwksUrl.clear();

    AzureADConfig aad{};
    if (loadAzureADConfig(&aad)) {
        m_aadConfigured = true;
        m_aadAuthority = aad.authority;
        m_aadClientId = aad.clientId;
        m_aadJwksUrl = aad.jwksUrl;

        auto& telemetry = EnterpriseTelemetryCompliance::instance();
        telemetry.recordAudit(AuditEventType::ConfigChange,
                              "system",
                              "azure_ad",
                              "loaded",
                              std::string("authority=") + m_aadAuthority);
        telemetry.incrementCounter("azure_ad.config.loaded", 1.0);
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

uint64_t EnterpriseLicense::GetTrialRemainingSeconds() const {
    if (GetState() != LicenseState::ValidTrial || m_trialStartUs == 0) return 0;
    uint64_t now = nowUs();
    uint64_t expiry = m_trialStartUs + kTrialDurationUs;
    if (now >= expiry) return 0;
    return (expiry - now) / 1000000ULL;
}

bool EnterpriseLicense::IsAzureADConfigured() const {
    return m_aadConfigured;
}

std::string EnterpriseLicense::GetAzureADAuthority() const {
    return m_aadAuthority;
}

std::string EnterpriseLicense::GetAzureADClientId() const {
    return m_aadClientId;
}

std::string EnterpriseLicense::GetStoredLicensePath() const {
    return m_lastLicensePath;
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

    updateTelemetry(newState, "install");
    
    return true;
}

bool EnterpriseLicense::InstallLicenseFromFile(const std::wstring& path) {
    // .rawrlic format: [blob_data...][512-byte RSA-4096 signature]
    constexpr size_t RSA_SIG_SIZE = 512;

    // Convert wstring to narrow string for MinGW ifstream compatibility
    std::string narrowPath(path.begin(), path.end());
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

    bool ok = InstallLicense(blob, blobSize, sig);
#ifdef _WIN32
    if (ok) {
        writeRegistryLicensePath(path);
        m_lastLicensePath.assign(path.begin(), path.end());
    }
#endif
    return ok;
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

    updateTelemetry(newState, "revalidate");
    
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

    updateTelemetry(newState, "state_change");

    auto& telemetry = EnterpriseTelemetryCompliance::instance();
    if (newState == LicenseState::Expired) {
        telemetry.recordAudit(AuditEventType::LicenseCheck,
                              "system",
                              "license",
                              "expired",
                              "state=Expired");
        telemetry.incrementCounter("license.expired.count", 1.0);
    }

    if (oldState == LicenseState::Expired &&
        (newState == LicenseState::ValidEnterprise ||
         newState == LicenseState::ValidPro ||
         newState == LicenseState::ValidTrial ||
         newState == LicenseState::ValidOEM)) {
        telemetry.recordAudit(AuditEventType::LicenseCheck,
                              "system",
                              "license",
                              "renewed",
                              "state=Renewed");
        telemetry.incrementCounter("license.renewed.count", 1.0);
    }
}

void EnterpriseLicense::updateTelemetry(LicenseState state, const char* action) {
    auto& telemetry = EnterpriseTelemetryCompliance::instance();

    LicenseInfo info;
    info.licenseKey = "local-license";
    info.tier = mapTelemetryTier(state);
    info.valid = (state == LicenseState::ValidEnterprise ||
                  state == LicenseState::ValidPro ||
                  state == LicenseState::ValidTrial ||
                  state == LicenseState::ValidOEM);

    uint64_t now = nowUs();
    if (state == LicenseState::ValidTrial) {
        if (m_trialStartUs == 0) {
            m_trialStartUs = now;
        }
        info.issuedAt = m_trialStartUs;
        info.expiresAt = m_trialStartUs + kTrialDurationUs;
    } else {
        info.issuedAt = now;
        info.expiresAt = 0;
        if (state == LicenseState::Expired) {
            info.valid = false;
            info.expiresAt = now;
        }
    }

    uint64_t mask = GetFeatureMask();
    appendFeature(mask, LicenseFeature::DualEngine800B,    "dual_engine_800b", info.features);
    appendFeature(mask, LicenseFeature::AVX512Premium,     "avx512_premium", info.features);
    appendFeature(mask, LicenseFeature::DistributedSwarm,  "distributed_swarm", info.features);
    appendFeature(mask, LicenseFeature::GPUQuant4Bit,      "gpu_quant_4bit", info.features);
    appendFeature(mask, LicenseFeature::EnterpriseSupport, "enterprise_support", info.features);
    appendFeature(mask, LicenseFeature::UnlimitedContext,  "unlimited_context", info.features);
    appendFeature(mask, LicenseFeature::FlashAttention,    "flash_attention", info.features);
    appendFeature(mask, LicenseFeature::MultiGPU,          "multi_gpu", info.features);

    (void)telemetry.setLicense(info);

    std::string detail = std::string("state=") + stateToString(state);
    if (action) {
        detail += ", action=";
        detail += action;
    }

    telemetry.recordAudit(AuditEventType::LicenseCheck,
                          "system",
                          "license",
                          action ? action : "update",
                          detail);
    telemetry.incrementCounter("license.lifecycle.events", 1.0);

    if (state == LicenseState::Expired) {
        telemetry.incrementCounter("license.expired.count", 1.0);
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
}

} // extern "C"

} // namespace RawrXD
