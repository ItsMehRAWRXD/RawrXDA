// ============================================================================
// license_offline_validator.cpp — Offline License Validation Engine
// ============================================================================

#include "../include/license_offline_validator.h"
#include "../include/license_anti_tampering.h"
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#pragma comment(lib, "winhttp.lib")

namespace RawrXD::License {

// ============================================================================
// Global Offline Validator Instance
// ============================================================================

OfflineLicenseValidator g_offlineValidator;

// ============================================================================
// Cache File Path Constants
// ============================================================================

static const char CACHE_FILENAME[] = "rawrxd_license.cache";
static const char CACHE_MAGIC = 0x4D434C4F;  // "OLCM"
static const uint32_t CACHE_VERSION = 1;

// ============================================================================
// Offline Cache Manager Implementation
// ============================================================================

const char* OfflineCacheManager::getCachePath() {
    static const char path[] = "C:\\ProgramData\\RawrXD\\license.cache";
    return path;
}

bool OfflineCacheManager::loadCache(OfflineLicenseCache& cache) {
    std::ifstream file(getCachePath(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.read(reinterpret_cast<char*>(&cache), sizeof(OfflineLicenseCache));
    if (!file.good()) {
        return false;
    }

    // Verify magic and version
    if (!cache.isValid()) {
        return false;
    }

    // Verify integrity
    if (!verifyCacheIntegrity(cache)) {
        return false;
    }

    return true;
}

bool OfflineCacheManager::saveCache(const OfflineLicenseCache& cache) {
    // Ensure directory exists
    CreateDirectory("C:\\ProgramData\\RawrXD", nullptr);

    std::ofstream file(getCachePath(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&cache), sizeof(OfflineLicenseCache));
    return file.good();
}

bool OfflineCacheManager::clearCache() {
    return DeleteFile(getCachePath()) != FALSE;
}

bool OfflineCacheManager::verifyCacheIntegrity(const OfflineLicenseCache& cache) {
    // Compute HMAC over all fields except signature
    uint8_t computed[32];
    uint8_t key[8] = { 0x52, 0x58, 0x44, 0x4C, 0x00, 0x00, 0x00, 0x00 };

    size_t dataSize = offsetof(OfflineLicenseCache, signature);
    if (!AntiTampering::computeHMAC_SHA256(
        reinterpret_cast<const uint8_t*>(&cache),
        dataSize,
        key, sizeof(key),
        computed)) {
        return false;
    }

    // Constant-time comparison
    uint32_t diff = 0;
    for (size_t i = 0; i < 32; ++i) {
        diff |= computed[i] ^ cache.signature[i];
    }

    return diff == 0;
}

bool OfflineCacheManager::updateCacheFromLicense(const LicenseKeyV2& license) {
    OfflineLicenseCache cache = {};

    cache.magic = 0x4D434C4F;
    cache.version = 1;
    cache.lastValidated = static_cast<uint32_t>(std::time(nullptr));
    cache.cacheExpiry = cache.lastValidated + CACHE_VALIDITY_SECONDS;
    cache.licenseTier = license.tier;
    cache.licenseHWID = license.hwid;
    cache.featuresLow = license.features.lo;
    cache.featuresHigh = license.features.hi;
    cache.licenseExpiry = license.expiryDate;

    // Compute HMAC signature
    uint8_t key[8] = { 0x52, 0x58, 0x44, 0x4C, 0x00, 0x00, 0x00, 0x00 };
    size_t dataSize = offsetof(OfflineLicenseCache, signature);

    AntiTampering::computeHMAC_SHA256(
        reinterpret_cast<uint8_t*>(&cache),
        dataSize,
        key, sizeof(key),
        cache.signature);

    return saveCache(cache);
}

uint32_t OfflineCacheManager::getCacheTimestamp() {
    OfflineLicenseCache cache = {};
    if (!loadCache(cache)) {
        return 0;
    }
    return cache.lastValidated;
}

uint32_t OfflineCacheManager::getCacheExpiry() {
    OfflineLicenseCache cache = {};
    if (!loadCache(cache)) {
        return 0;
    }
    return cache.cacheExpiry;
}

// ============================================================================
// Offline License Validator Implementation
// ============================================================================

OfflineLicenseValidator::OfflineLicenseValidator()
    : m_gracePeriodSeconds(DEFAULT_GRACE_PERIOD_SECONDS),
      m_cacheValiditySeconds(CACHE_VALIDITY_SECONDS),
      m_offlineModeEnabled(false),
      m_cacheLoaded(false),
      m_lastErrorCode(0) {
    std::memset(&m_cache, 0, sizeof(m_cache));
    std::memset(&m_syncStatus, 0, sizeof(m_syncStatus));
    std::memset(m_validationReason, 0, sizeof(m_validationReason));

    m_syncStatus.state = SyncState::NOT_SYNCED;
    m_lastKnownTime = static_cast<uint32_t>(std::time(nullptr));

    loadCache();
}

OfflineLicenseValidator::~OfflineLicenseValidator() {
}

OfflineValidationResult OfflineLicenseValidator::validateOffline() {
    return validateUsingCache();
}

OfflineValidationResult OfflineLicenseValidator::validateWithFallback() {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));

    // Try online validation first
    auto result = validateOnline();
    if (result.success) {
        // Cache successful validation
        updateCacheFromCurrentLicense();
        m_syncStatus.state = SyncState::SYNCED_TODAY;
        m_syncStatus.lastSyncTime = now;
        return result;
    }

    // Fall back to cache
    result = validateUsingCache();
    if (result.success || result.mode == OfflineValidationMode::GRACE_PERIOD) {
        result.mode = OfflineValidationMode::TIMED_OUT;
        return result;
    }

    return result;
}

OfflineValidationResult OfflineLicenseValidator::validateOnline() {
    OfflineValidationResult result = {};
    result.mode = OfflineValidationMode::ONLINE;
    result.success = false;
    result.reason = "Online validation endpoint not configured (using cache fallback)";
    result.errorCode = static_cast<uint16_t>(OfflineValidationError::SYNC_FAILED);

    // Online validation: when implemented, call license server and fill result.
    // Callers use validateWithFallback() which falls back to cache on failure.

    return result;
}

OfflineValidationResult OfflineLicenseValidator::validateUsingCache() {
    OfflineValidationResult result = {};
    result.mode = OfflineValidationMode::CACHED;
    result.reason = "Using cached license validation";

    // Load cache if not already loaded
    if (!m_cacheLoaded) {
        if (!loadCache()) {
            result.success = false;
            result.reason = "Cache not found";
            result.errorCode = static_cast<uint16_t>(OfflineValidationError::CACHE_NOT_FOUND);
            return result;
        }
    }

    // Verify cache integrity
    if (!OfflineCacheManager::verifyCacheIntegrity(m_cache)) {
        result.success = false;
        result.reason = "Cache corrupted";
        result.errorCode = static_cast<uint16_t>(OfflineValidationError::CACHE_CORRUPTED);
        return result;
    }

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    result.cacheTimestamp = m_cache.lastValidated;

    // Check cache validity
    if (now > m_cache.cacheExpiry) {
        result.success = false;
        result.reason = "Cache validity expired";
        result.errorCode = static_cast<uint16_t>(OfflineValidationError::CACHE_EXPIRED);
        result.mode = OfflineValidationMode::OFFLINE;
        return result;
    }

    // Check HWID match
    auto& lic = EnterpriseLicenseV2::Instance();
    if (lic.getHardwareID() != m_cache.licenseHWID) {
        result.success = false;
        result.reason = "Hardware ID mismatch";
        result.errorCode = static_cast<uint16_t>(OfflineValidationError::INVALID_HWID);
        return result;
    }

    // Check license expiry with grace period
    result.inGracePeriod = false;
    if (m_cache.licenseExpiry > 0 && now > m_cache.licenseExpiry) {
        if (now > m_cache.licenseExpiry + m_gracePeriodSeconds) {
            result.success = false;
            result.reason = "License expired and grace period exceeded";
            result.errorCode = static_cast<uint16_t>(OfflineValidationError::GRACE_PERIOD_EXCEEDED);
            result.mode = OfflineValidationMode::OFFLINE;
            return result;
        }

        // In grace period
        result.success = true;
        result.inGracePeriod = true;
        result.mode = OfflineValidationMode::GRACE_PERIOD;
        result.reason = "License in grace period";
        return result;
    }

    // Check for clock skew
    if (detectClockTampering()) {
        result.success = false;
        result.reason = "System clock tampering detected";
        result.errorCode = static_cast<uint16_t>(OfflineValidationError::CLOCK_SKEW_DETECTED);
        return result;
    }

    result.success = true;
    result.mode = OfflineValidationMode::CACHED;
    result.reason = "License validated using cache";

    return result;
}

SyncStatus OfflineLicenseValidator::getSyncStatus() const {
    return m_syncStatus;
}

bool OfflineLicenseValidator::requestSync() {
    m_syncStatus.state = SyncState::NOT_SYNCED;
    return performOnlineSync();
}

bool OfflineLicenseValidator::waitForSync(uint32_t timeoutMs) {
    (void)timeoutMs;
    // If already synced, report success; otherwise performOnlineSync is synchronous stub.
    if (m_syncStatus.state == SyncState::SYNCED_TODAY || m_syncStatus.state == SyncState::SYNCED_RECENT) {
        return true;
    }
    return performOnlineSync();
}

bool OfflineLicenseValidator::isInGracePeriod() const {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (m_cache.licenseExpiry == 0) return false;
    if (now <= m_cache.licenseExpiry) return false;
    return (now <= m_cache.licenseExpiry + m_gracePeriodSeconds);
}

uint32_t OfflineLicenseValidator::getSecondsUntilExpiry() const {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (m_cache.licenseExpiry == 0) return UINT32_MAX;
    if (now >= m_cache.licenseExpiry) return 0;
    return m_cache.licenseExpiry - now;
}

uint32_t OfflineLicenseValidator::getSecondsInGracePeriod() const {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (m_cache.licenseExpiry == 0) return 0;
    if (now <= m_cache.licenseExpiry) return 0;

    uint32_t graceBoundary = m_cache.licenseExpiry + m_gracePeriodSeconds;
    if (now >= graceBoundary) return 0;

    return graceBoundary - now;
}

void OfflineLicenseValidator::setGracePeriodSeconds(uint32_t seconds) {
    m_gracePeriodSeconds = seconds;
}

void OfflineLicenseValidator::setCacheValiditySeconds(uint32_t seconds) {
    m_cacheValiditySeconds = seconds;
}

bool OfflineLicenseValidator::updateCacheFromCurrentLicense() {
    auto& lic = EnterpriseLicenseV2::Instance();
    LicenseKeyV2 key = {};
    key.tier = static_cast<uint8_t>(lic.getTier());
    key.hwid = lic.getHardwareID();
    key.features.lo = lic.getFeaturesLow();
    key.features.hi = lic.getFeaturesHigh();
    key.expiryDate = lic.getExpiryTimestamp();

    if (!OfflineCacheManager::updateCacheFromLicense(key)) {
        return false;
    }

    // Reload the cache after update
    m_cacheLoaded = false;
    return loadCache();
}

bool OfflineLicenseValidator::checkClockSkew() {
    return detectClockTampering();
}

void OfflineLicenseValidator::setOfflineModeEnabled(bool enabled) {
    m_offlineModeEnabled = enabled;
}

const char* OfflineLicenseValidator::getValidationReason() const {
    return m_validationReason;
}

bool OfflineLicenseValidator::blacklistLicense(uint64_t hwid) {
    const char* blPath = "C:\\ProgramData\\RawrXD\\blacklist.dat";
    CreateDirectoryA("C:\\ProgramData\\RawrXD", nullptr);

    // Append HWID to blacklist file (one 8-byte entry per HWID)
    std::ofstream f(blPath, std::ios::binary | std::ios::app);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(&hwid), sizeof(hwid));
    return f.good();
}

bool OfflineLicenseValidator::isBlacklisted(uint64_t hwid) const {
    const char* blPath = "C:\\ProgramData\\RawrXD\\blacklist.dat";
    std::ifstream f(blPath, std::ios::binary);
    if (!f.is_open()) return false;

    uint64_t stored = 0;
    while (f.read(reinterpret_cast<char*>(&stored), sizeof(stored))) {
        if (stored == hwid) return true;
    }
    return false;
}

bool OfflineLicenseValidator::loadCache() {
    if (OfflineCacheManager::loadCache(m_cache)) {
        m_cacheLoaded = true;
        return true;
    }
    return false;
}

bool OfflineLicenseValidator::performOnlineSync() {
    const char* urlEnv = std::getenv("RAWRXD_LICENSE_SERVER_URL");
    if (!urlEnv || !urlEnv[0]) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "Online sync not configured (set RAWRXD_LICENSE_SERVER_URL)";
        m_syncStatus.failureCount++;
        return false;
    }
    if (!m_cacheLoaded && !loadCache()) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "No cache loaded to sync (load license cache first)";
        m_syncStatus.failureCount++;
        return false;
    }

    // Parse URL: expect https://host or https://host/path
    std::string url(urlEnv);
    std::string host, path = "/validate";
    size_t start = 0;
    if (url.find("https://") == 0) start = 8;
    else if (url.find("http://") == 0) start = 7;
    size_t slash = url.find('/', start);
    if (slash != std::string::npos) {
        host = url.substr(start, slash - start);
        path = url.substr(slash);
        if (path.empty()) path = "/validate";
    } else {
        host = url.substr(start);
    }
    if (host.empty()) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "Invalid RAWRXD_LICENSE_SERVER_URL (no host)";
        m_syncStatus.failureCount++;
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-OfflineValidator/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "WinHttpOpen failed";
        m_syncStatus.failureCount++;
        return false;
    }

    std::wstring whost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(),
        (url.find("https://") == 0) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "WinHttpConnect failed";
        m_syncStatus.failureCount++;
        return false;
    }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        (url.find("https://") == 0) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "WinHttpOpenRequest failed";
        m_syncStatus.failureCount++;
        return false;
    }

    char bodyBuf[128];
    int bodyLen = snprintf(bodyBuf, sizeof(bodyBuf), "{\"hwid\":\"%llu\"}", (unsigned long long)m_cache.licenseHWID);
    std::wstring headers = L"Content-Type: application/json\r\n";

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(),
        bodyBuf, bodyLen, bodyLen, 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "WinHttpSendRequest failed";
        m_syncStatus.failureCount++;
        return false;
    }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "WinHttpReceiveResponse failed";
        m_syncStatus.failureCount++;
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr, &statusCode, &statusSize, nullptr);
    char responseBuf[512] = {};
    DWORD readLen = 0;
    WinHttpReadData(hRequest, responseBuf, sizeof(responseBuf) - 1, &readLen);
    responseBuf[readLen] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (statusCode != 200) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "License server returned non-200";
        m_syncStatus.failureCount++;
        return false;
    }

    // Response must indicate success: "valid":true or "success":true
    std::string resp(responseBuf);
    bool ok = (resp.find("\"valid\":true") != std::string::npos) ||
              (resp.find("\"success\":true") != std::string::npos) ||
              (resp.find("\"valid\": true") != std::string::npos) ||
              (resp.find("\"success\": true") != std::string::npos);
    if (!ok) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "License server response did not indicate success";
        m_syncStatus.failureCount++;
        return false;
    }

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    m_cache.lastValidated = now;
    m_cache.cacheExpiry = now + m_cacheValiditySeconds;
    if (!OfflineCacheManager::saveCache(m_cache)) {
        m_syncStatus.state = SyncState::SYNC_FAILED;
        m_syncStatus.lastErrorMessage = "Failed to save cache after sync";
        m_syncStatus.failureCount++;
        return false;
    }

    m_syncStatus.state = SyncState::SYNCED_RECENT;
    m_syncStatus.lastSyncTime = now;
    m_syncStatus.nextSyncTime = now + (m_cacheValiditySeconds / 2);
    m_syncStatus.failureCount = 0;
    m_syncStatus.lastErrorMessage = nullptr;
    return true;
}

bool OfflineLicenseValidator::checkExpiryWithGracePeriod(uint32_t expiryTime, uint32_t now) const {
    if (expiryTime == 0) return true;  // No expiry
    if (now <= expiryTime) return true;  // Not expired
    if (now <= expiryTime + m_gracePeriodSeconds) return true;  // Within grace period
    return false;  // Expired and grace period exceeded
}

bool OfflineLicenseValidator::detectClockTampering() {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));

    // Check if time has gone backwards significantly (> 1 hour)
    if (now < m_lastKnownTime && (m_lastKnownTime - now) > 3600) {
        return true;
    }

    // Update last known time
    if (now > m_lastKnownTime) {
        m_lastKnownTime = now;
    }

    return false;
}

uint32_t OfflineLicenseValidator::getLastKnownTime() const {
    return m_lastKnownTime;
}

}  // namespace RawrXD::License
