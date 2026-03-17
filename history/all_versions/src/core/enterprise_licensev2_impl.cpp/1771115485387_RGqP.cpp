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
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <unistd.h>
#include <cpuid.h>
#include <cstdio>
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
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit < TOTAL_FEATURES) {
        return g_FeatureManifest[bit].implemented;
    }
    return false;
}

bool EnterpriseLicenseV2::gate(FeatureID id, const char* caller) {
    bool granted = isFeatureEnabled(id);
    
    // Only audit failures or Sovereign tier features
    // Or maybe audit everything? The "Audit Trail" usually implies comprehensive logging.
    // However, for high-frequency checks (e.g. inside a loop), this mutex lock will be expensive.
    // The requirement is "Wire license audit trail on gate".
    // Let's log everything for now, or maybe just "access granted/denied".
    
    recordAudit(id, granted, caller, granted ? "Access Granted" : "Access Denied");
    
    return granted;
}

void EnterpriseLicenseV2::recordAudit(FeatureID id, bool granted, const char* caller, const char* detail) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LicenseAuditEntry& entry = m_auditTrail[m_auditHead];
    
#ifdef _WIN32
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    entry.timestamp = static_cast<uint64_t>(pc.QuadPart);
#else
    entry.timestamp = static_cast<uint64_t>(std::time(nullptr)); 
#endif

    entry.feature = id;
    entry.granted = granted;
    entry.caller = caller; // Assumes caller string is static (usually __FUNCTION__)
    entry.detail = detail; // Assumes detail is static
    
    m_auditHead = (m_auditHead + 1) % MAX_AUDIT_ENTRIES;
    if (m_auditCount < MAX_AUDIT_ENTRIES) {
        m_auditCount++;
    }
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
    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return LicenseResult::error("File not found or unreadable");
    LicenseKeyV2 key = {};
    size_t n = std::fread(&key, 1, sizeof(key), fp);
    std::fclose(fp);
    if (n != sizeof(key)) return LicenseResult::error("Invalid key format");
    return loadKeyFromMemory(&key, sizeof(key));
#endif
}

LicenseResult EnterpriseLicenseV2::loadKeyFromRegistry() {
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD\\License", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return LicenseResult::error("Registry key not found");
    }

    LicenseKeyV2 key = {};
    DWORD size = sizeof(key);
    DWORD type = REG_BINARY;
    
    LONG result = RegQueryValueExA(hKey, "KeyData", nullptr, &type, reinterpret_cast<LPBYTE>(&key), &size);
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        return LicenseResult::error("Failed to read registry value");
    }
    
    if (size != sizeof(key) || type != REG_BINARY) {
        return LicenseResult::error("Invalid registry data format");
    }
    
    return loadKeyFromMemory(&key, sizeof(key));
#else
    return LicenseResult::error("Registry not supported on this platform");
#endif
}

LicenseResult EnterpriseLicenseV2::saveKeyToRegistry(const LicenseKeyV2& key) {
#ifdef _WIN32
    HKEY hKey;
    DWORD disposition;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD\\License", 0, nullptr, 
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, &disposition) != ERROR_SUCCESS) {
        return LicenseResult::error("Failed to open registry key for writing");
    }

    LONG result = RegSetValueExA(hKey, "KeyData", 0, REG_BINARY, 
                                 reinterpret_cast<const BYTE*>(&key), sizeof(key));
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        return LicenseResult::error("Failed to write license to registry");
    }
    
    return LicenseResult::ok("License saved to registry");
#else
    return LicenseResult::error("Registry not supported on this platform");
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

// ============================================================================
// Internal Keys (for production, these should be obfuscated)
// ============================================================================
static const uint8_t g_VerificationSecret[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
};

LicenseResult EnterpriseLicenseV2::validateKey(const LicenseKeyV2& key) const {
    // Basic validation
    if (key.magic != 0x5258444C) return LicenseResult::error("Invalid magic");
    if (key.version != 2) return LicenseResult::error("Invalid version");
    if (key.tier > static_cast<uint32_t>(LicenseTierV2::Sovereign)) return LicenseResult::error("Invalid tier");
    
    // Time-based validation
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (key.issueDate > now) return LicenseResult::error("Future issue date");
    if (key.expiryDate != 0 && key.expiryDate < now) return LicenseResult::error("Expired");
    
    // Signature validation
    if (!verifySignature(key)) {
        return LicenseResult::error("Invalid signature");
    }

    // HWID Binding Check (if key has HWID set)
    if (key.hwid != 0 && key.hwid != m_hwid) {
        return LicenseResult::error("Hardware ID mismatch");
    }

    return LicenseResult::ok("Valid");
}

// ============================================================================
// Azure AD Integration (WinHTTP Implementation)
// ============================================================================
#ifdef _WIN32
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

LicenseResult EnterpriseLicenseV2::requestAzureADLicense(const char* tenantId, const char* clientId) {
#ifdef _WIN32
    if (!tenantId || !clientId) return LicenseResult::error("Invalid Azure AD parameters");
    
    // 1. Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"RawrXD-License-Agent/2.0",  
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return LicenseResult::error("Failed to initialize HTTP session");

    // 2. Connect to Identity Provider (login.microsoftonline.com)
    HINTERNET hConnect = WinHttpConnect(hSession, L"login.microsoftonline.com", 
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return LicenseResult::error("Failed to connect to Azure AD");
    }

    // 3. Construct Token Endpoint Path
    // path: /{tenantId}/oauth2/v2.0/token
    wchar_t path[512];
    wchar_t wTenant[256];
    MultiByteToWideChar(CP_UTF8, 0, tenantId, -1, wTenant, 256);
    swprintf_s(path, 512, L"/%s/oauth2/v2.0/token", wTenant);

    // 4. Create Request
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path,
                                            NULL, WINHTTP_NO_REFERER, 
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return LicenseResult::error("Failed to open HTTP request");
    }

    // 5. Send Request (Device Code Flow or Client Creds - utilizing Client Creds for machine license)
    // For simplicity in this production audit, we assume a specific grant type payload is prepared elsewhere or
    // we send a basic request to prove connectivity. Real implementation needs client_secret or certificate.
    // We'll simulate a request with "grant_type=client_credentials" and placeholder secret.
    // In a real sovereign environment, this would use a mTLS certificate.
    
    const char* payload = "grant_type=client_credentials&scope=https%3A%2F%2Fmanagement.azure.com%2F.default";
    DWORD payloadLen = (DWORD)strlen(payload);
    
    BOOL bResults = WinHttpSendRequest(hRequest, 
                                       L"Content-Type: application/x-www-form-urlencoded",
                                       (DWORD)-1L, 
                                       (LPVOID)payload, 
                                       payloadLen, 
                                       payloadLen, 
                                       0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    LicenseResult result = LicenseResult::error("Azure AD request failed");
    
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, 
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 
                            WINHTTP_HEADER_NAME_BY_INDEX, 
                            &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
                            
        if (dwStatusCode == 200) {
            // Success - In a full implementation, we would parse the JSON JSON response to extract 'access_token'
            // and then exchange that token for a RawrXD license key via our own backend.
            // For now, we consider the connectivity and auth valid.
            result = LicenseResult::ok("Azure AD Authentication Successful");
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "Azure AD Error: %lu", dwStatusCode);
            result = LicenseResult::error(buf, dwStatusCode);
        }
    } else {
         result = LicenseResult::error("Failed to send/receive request", GetLastError());
    }

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return result;
    
#else
    return LicenseResult::error("Azure AD support requires Windows");
#endif
}

bool EnterpriseLicenseV2::verifySignature(const LicenseKeyV2& key) const {
    return RawrXD::License::AntiTampering::verifyLicenseKeyIntegrity(
        key, g_VerificationSecret, sizeof(g_VerificationSecret), 0 // We check HWID in validateKey
    );
}

void EnterpriseLicenseV2::signKey(LicenseKeyV2& key, const char* secret) const {
    if (!secret) return;
    
    // Compute HMAC of the key (excluding the signature field itself)
    // The signature field is at the end (last 32 bytes)
    size_t dataSize = sizeof(LicenseKeyV2) - 32;
    
    RawrXD::License::AntiTampering::computeHMAC_SHA256(
        reinterpret_cast<const uint8_t*>(&key), dataSize,
        reinterpret_cast<const uint8_t*>(secret), std::strlen(secret),
        key.signature
    );
}

LicenseResult EnterpriseLicenseV2::createKey(LicenseTierV2 tier, uint32_t durationDays,
                                           const char* signingSecret, LicenseKeyV2* outKey) const {
    if (!outKey || !signingSecret) return LicenseResult::error("Invalid parameters");
    
    std::memset(outKey, 0, sizeof(LicenseKeyV2));
    outKey->magic = 0x5258444C;
    outKey->version = 2;
    outKey->tier = static_cast<uint32_t>(tier);
    
    outKey->issueDate = static_cast<uint32_t>(std::time(nullptr));
    if (durationDays > 0) {
        outKey->expiryDate = outKey->issueDate + (durationDays * 24 * 3600);
    } else {
        outKey->expiryDate = 0; // Perpetual
    }
    
    // Set features based on tier
    outKey->features = TierPresets::forTier(tier);
    
    // Set limits
    const TierLimits::Limits& limits = TierLimits::forTier(tier);
    outKey->maxModelGB = limits.maxModelGB;
    outKey->maxContextTokens = limits.maxContextTokens;
    
    // Bind to current HWID by default for generated keys
    outKey->hwid = m_hwid; 
    
    // Sign
    signKey(*outKey, signingSecret);
    
    return LicenseResult::ok("Key created");
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
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < TOTAL_FEATURES) {
        return g_FeatureManifest[idx];
    }
    return dummyDef;
}

uint32_t EnterpriseLicenseV2::countByTier(LicenseTierV2 tier) const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].minTier == tier) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countImplemented() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].implemented) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countWiredToUI() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].wiredToUI) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countTested() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].tested) count++;
    }
    return count;
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
