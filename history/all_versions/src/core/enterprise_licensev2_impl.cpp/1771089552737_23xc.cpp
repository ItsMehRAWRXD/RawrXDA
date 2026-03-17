// ============================================================================
// enterprise_licensev2_impl.cpp — Minimal EnterpriseLicenseV2 Implementation
// ============================================================================
// Purpose: Provide minimal implementation for Phase 4 test executables
// Status: Stubbed for testing only, minimal functionality

#include "../../include/enterprise_license.h"
#include "../../include/license_anti_tampering.h"
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <unistd.h>
#include <cpuid.h>
#endif

namespace RawrXD::License {

// ============================================================================
// Static Instance (Singleton)
// ============================================================================
EnterpriseLicenseV2& EnterpriseLicenseV2::Instance() {
    static EnterpriseLicenseV2 instance;
    return instance;
}

// ============================================================================
// Lifecycle
// ============================================================================
LicenseResult EnterpriseLicenseV2::initialize() {
    if (m_initialized) return LicenseResult::error("Already initialized");
    
    m_hwid = getHardwareID();
    m_tier = LicenseTierV2::Community;
    m_currentKey.magic = 0;
    m_initialized = true;
    
    return LicenseResult::ok("Initialized");
}

void EnterpriseLicenseV2::shutdown() {
    m_initialized = false;
    std::memset(&m_currentKey, 0, sizeof(m_currentKey));
}

// ============================================================================
// Hardware ID Computation (CPUID based)
// ============================================================================
uint64_t EnterpriseLicenseV2::getHardwareID() const {
    uint64_t hwid = 0;
    
#ifdef _WIN32
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    hwid = static_cast<uint64_t>(cpuInfo[1]) << 32 | static_cast<uint64_t>(cpuInfo[2]);
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(0, &eax, &ebx, &ecx, &edx)) {
        hwid = static_cast<uint64_t>(ebx) << 32 | static_cast<uint64_t>(ecx);
    }
#endif
    
    // Fallback: use machine name hash
    if (hwid == 0) {
#ifdef _WIN32
        char computerName[256] = {};
        DWORD size = sizeof(computerName);
        if (GetComputerNameA(computerName, &size)) {
            for (size_t i = 0; i < size && computerName[i] != '\0'; ++i) {
                hwid = (hwid * 31) + static_cast<unsigned char>(computerName[i]);
            }
        }
#endif
    }
    
    return hwid ? hwid : 0x1234567890ABCDEFULL;  // Ultimate fallback
}

// ============================================================================
// Feature Queries
// ============================================================================
bool EnterpriseLicenseV2::isFeatureEnabled(FeatureID id) const {
    if (!m_initialized) return false;
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit < 64) {
        return (m_currentKey.features.lo & (1ULL << bit)) != 0;
    } else if (bit < 128) {
        return (m_currentKey.features.hi & (1ULL << (bit - 64))) != 0;
    }
    return false;
}

bool EnterpriseLicenseV2::isFeatureLicensed(FeatureID id) const {
    return isFeatureEnabled(id);
}

bool EnterpriseLicenseV2::isFeatureImplemented(FeatureID id) const {
    // For testing: all features implemented
    return true;
}

bool EnterpriseLicenseV2::gate(FeatureID id, const char* caller) {
    return isFeatureEnabled(id);
}

// ============================================================================
// Tier Queries
// ============================================================================
LicenseTierV2 EnterpriseLicenseV2::currentTier() const {
    return m_tier;
}

const TierLimits::Limits& EnterpriseLicenseV2::currentLimits() const {
    return TierLimits::forTier(m_tier);
}

FeatureMask EnterpriseLicenseV2::currentMask() const {
    return m_currentKey.features;
}

uint32_t EnterpriseLicenseV2::enabledFeatureCount() const {
    uint32_t count = 0;
    uint64_t lo = m_currentKey.features.lo;
    uint64_t hi = m_currentKey.features.hi;
    
    // Count set bits
    while (lo) { count += (lo & 1); lo >>= 1; }
    while (hi) { count += (hi & 1); hi >>= 1; }
    
    return count;
}

// ============================================================================
// Key Operations
// ============================================================================
LicenseResult EnterpriseLicenseV2::loadKeyFromFile(const char* path) {
    if (!path) return LicenseResult::error("Invalid parameter");
    
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return LicenseResult::error("File not found");
    
    DWORD bytesRead = 0;
    LicenseKeyV2 key = {};
    if (!ReadFile(hFile, &key, sizeof(key), &bytesRead, nullptr) || bytesRead != sizeof(key)) {
        CloseHandle(hFile);
        return LicenseResult::error("Invalid key format");
    }
    CloseHandle(hFile);
    
    return loadKeyFromMemory(&key, sizeof(key));
#else
    return LicenseResult::error("Not implemented");
#endif
}

LicenseResult EnterpriseLicenseV2::loadKeyFromMemory(const void* data, size_t size) {
    if (!data || size != sizeof(LicenseKeyV2)) return LicenseResult::error("Invalid parameter");
    
    const LicenseKeyV2* key = static_cast<const LicenseKeyV2*>(data);
    LicenseResult result = validateKey(*key);
    if (!result.success) return result;
    
    std::memcpy(&m_currentKey, key, sizeof(m_currentKey));
    m_tier = static_cast<LicenseTierV2>(key->tier);
    
    return LicenseResult::ok("Key loaded");
}

LicenseResult EnterpriseLicenseV2::validateKey(const LicenseKeyV2& key) const {
    // Basic validation
    if (key.magic != 0x5258444C) return LicenseResult::error("Invalid magic");
    if (key.version != 2) return LicenseResult::error("Invalid version");
    if (key.tier > static_cast<uint32_t>(LicenseTierV2::Sovereign)) return LicenseResult::error("Invalid tier");
    
    // Time-based validation
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (key.issueDate > now) return LicenseResult::error("Future issue date");
    if (key.expiryDate < now) return LicenseResult::error("Expired");
    
    return LicenseResult::ok("Valid");
}

// ============================================================================
// Status Queries
// ============================================================================
LicenseStatus EnterpriseLicenseV2::getStatus() const {
    LicenseStatus status = {};
    status.tier = m_tier;
    status.enabledFeatures = enabledFeatureCount();
    status.isOffline = false;
    status.daysUntilExpiry = 365;
    status.graceDaysRemaining = 0;
    
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (m_currentKey.expiryDate > now) {
        status.daysUntilExpiry = (m_currentKey.expiryDate - now) / 86400;
    }
    
    return status;
}

const LicenseKeyV2& EnterpriseLicenseV2::currentKey() const {
    return m_currentKey;
}

// ============================================================================
// Audit Trail Access
// ============================================================================
size_t EnterpriseLicenseV2::getAuditEntryCount() const {
    return m_auditCount;
}

const LicenseAuditEntry* EnterpriseLicenseV2::getAuditEntries() const {
    return m_auditTrail;
}

void EnterpriseLicenseV2::clearAuditTrail() {
    std::memset(m_auditTrail, 0, sizeof(m_auditTrail));
    m_auditCount = 0;
    m_auditHead = 0;
}

// ============================================================================
// Manifest Queries (Stub implementations)
// ============================================================================
const FeatureDefV2& EnterpriseLicenseV2::getFeatureDef(FeatureID id) const {
    static FeatureDefV2 dummyDef = {};
    return dummyDef;
}

uint32_t EnterpriseLicenseV2::countByTier(LicenseTierV2 tier) const {
    return 0;  // Stub
}

uint32_t EnterpriseLicenseV2::countImplemented() const {
    return 61;  // Stub: all features
}

uint32_t EnterpriseLicenseV2::countWiredToUI() const {
    return 61;  // Stub
}

uint32_t EnterpriseLicenseV2::countTested() const {
    return 61;  // Stub
}

// ============================================================================
// Callbacks
// ============================================================================
void EnterpriseLicenseV2::onLicenseChange(LicenseChangeCallback cb) {
    if (m_callbackCount < MAX_CALLBACKS && cb) {
        m_callbacks[m_callbackCount++] = cb;
    }
}

// ============================================================================
// Dev Unlock
// ============================================================================
LicenseResult EnterpriseLicenseV2::devUnlock() {
    m_tier = LicenseTierV2::Sovereign;
    m_currentKey.tier = static_cast<uint32_t>(LicenseTierV2::Sovereign);
    m_currentKey.features.lo = 0xFFFFFFFFFFFFFFFFULL;
    m_currentKey.features.hi = 0xFFFFFFFFFFFFFFFFULL;
    return LicenseResult::ok("Dev unlock enabled");
}

// ============================================================================
// HWID Hex
// ============================================================================
void EnterpriseLicenseV2::getHardwareIDHex(char* buf, size_t bufLen) const {
    if (buf && bufLen >= 17) {
        snprintf(buf, bufLen, "%016llX", static_cast<unsigned long long>(m_hwid));
    }
}

} // namespace RawrXD::License
