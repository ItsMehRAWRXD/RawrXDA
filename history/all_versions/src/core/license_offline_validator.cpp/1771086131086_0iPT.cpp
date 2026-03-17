// ============================================================================
// license_offline_validator.cpp — Offline License Validation Engine
// ============================================================================

#include "../include/license_offline_validator.h"
#include "../include/license_anti_tampering.h"
#include <cstring>
#include <ctime>
#include <windows.h>
#include <fstream>

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
    cache.featuresLow = license.features.low;
    cache.featuresHigh = license.features.high;
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
    result.reason = "Online validation not yet implemented";
    result.errorCode = static_cast<uint16_t>(OfflineValidationError::SYNC_FAILED);

    // TODO: Implement actual online validation
    // For now, return failure to trigger fallback to cache

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
    // TODO: Implement async sync with timeout
    return false;
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
    // Note: We need the actual license key to update cache
    // This is a placeholder
    return true;
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
    // TODO: Implement blacklist storage
    return true;
}

bool OfflineLicenseValidator::isBlacklisted(uint64_t hwid) const {
    // TODO: Implement blacklist check
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
    // TODO: Implement actual online sync
    m_syncStatus.state = SyncState::SYNC_FAILED;
    m_syncStatus.lastErrorMessage = "Online sync not implemented";
    m_syncStatus.failureCount++;
    return false;
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
