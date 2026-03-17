// ============================================================================
// license_offline_validator.h — Offline License Validation Engine
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include <cstdint>
#include <ctime>

namespace RawrXD::License {

// ============================================================================
// Offline Validation Constants
// ============================================================================

enum class OfflineValidationMode {
    ONLINE,          ///< Full online validation (requires network)
    CACHED,          ///< Use cached license validity
    GRACE_PERIOD,    ///< License expired but within grace period
    OFFLINE,         ///< Fully offline, minimal checks only
    TIMED_OUT        ///< Online validation timed out, fallback to cached
};

// Default grace period: 90 days after expiry
static constexpr uint32_t DEFAULT_GRACE_PERIOD_SECONDS = 90 * 24 * 3600;

// Cache validity: 30 days
static constexpr uint32_t CACHE_VALIDITY_SECONDS = 30 * 24 * 3600;

// Online validation timeout: 5 seconds
static constexpr uint32_t ONLINE_VALIDATION_TIMEOUT_MS = 5000;

// ============================================================================
// Offline Validation Results
// ============================================================================

struct OfflineValidationResult {
    bool                  success;
    OfflineValidationMode mode;
    const char*           reason;
    uint32_t              cacheTimestamp;  ///< When was the license last verified
    bool                  inGracePeriod;   ///< Is license in grace period?
    uint16_t              errorCode;       ///< Error code if failed
};

enum class OfflineValidationError : uint16_t {
    OK                              = 0x0000,
    CACHE_NOT_FOUND                 = 0x0001,
    CACHE_CORRUPTED                 = 0x0002,
    CACHE_EXPIRED                   = 0x0004,
    LICENSE_EXPIRED                 = 0x0008,
    LICENSE_REVOKED                 = 0x0010,
    LICENSE_BLACKLISTED             = 0x0020,
    GRACE_PERIOD_EXCEEDED           = 0x0040,
    INVALID_HWID                    = 0x0080,
    SYNC_FAILED                     = 0x0100,
    NETWORK_UNREACHABLE             = 0x0200,
    CLOCK_SKEW_DETECTED             = 0x0400,
    TAMPERING_DETECTED              = 0x0800
};

// ============================================================================
// Offline License Cache
// ============================================================================

struct OfflineLicenseCache {
    // Binary layout (must match serialized format)
    uint32_t magic;              ///< "OLCM" (0x4D434C4F)
    uint32_t version;            ///< Cache format version (1)
    uint32_t lastValidated;      ///< Timestamp of last successful online validation
    uint32_t cacheExpiry;        ///< When this cache entry expires
    uint32_t licenseTier;        ///< Cached tier (0-3)
    uint64_t licenseHWID;        ///< Cached hardware ID
    uint64_t featuresLow;        ///< Cached feature bitmask (low)
    uint64_t featuresHigh;       ///< Cached feature bitmask (high)
    uint32_t licenseExpiry;      ///< Original license expiry timestamp
    uint32_t reserved1;          ///< Reserved
    uint32_t reserved2;          ///< Reserved
    uint8_t  signature[32];      ///< HMAC-SHA256 signature (8-byte aligned)
    // Total: 128 bytes

    bool isValid() const {
        return magic == 0x4D434C4F && version == 1;
    }
};

// ============================================================================
// Offline Sync State
// ============================================================================

enum class SyncState {
    NOT_SYNCED,      ///< Never synced online
    SYNCED_TODAY,    ///< Synced online in last 24 hours
    SYNCED_RECENT,   ///< Synced online within grace period
    SYNC_EXPIRED,    ///< Last sync beyond cache validity
    SYNC_FAILED      ///< Last sync attempt failed
};

struct SyncStatus {
    SyncState       state;
    uint32_t        lastSyncTime;      ///< Timestamp of last successful sync
    uint32_t        nextSyncTime;      ///< When next sync is required
    uint32_t        failureCount;      ///< Consecutive sync failures
    const char*     lastErrorMessage;  ///< Error from last sync attempt
};

// ============================================================================
// Offline Validation Cache Manager
// ============================================================================

class OfflineCacheManager {
public:
    // Get the cache file path
    static const char* getCachePath();

    // Load cache from disk
    static bool loadCache(OfflineLicenseCache& cache);

    // Save cache to disk
    static bool saveCache(const OfflineLicenseCache& cache);

    // Clear cache from disk
    static bool clearCache();

    // Verify cache integrity (HMAC check)
    static bool verifyCacheIntegrity(const OfflineLicenseCache& cache);

    // Update cache with current license
    static bool updateCacheFromLicense(const LicenseKeyV2& license);

    // Get cache creation timestamp
    static uint32_t getCacheTimestamp();

    // Get cache expiry timestamp
    static uint32_t getCacheExpiry();
};

// ============================================================================
// Offline License Validator
// ============================================================================

class OfflineLicenseValidator {
public:
    OfflineLicenseValidator();
    ~OfflineLicenseValidator();

    // Validate license in offline mode
    OfflineValidationResult validateOffline();

    // Validate with online fallback (tries internet, falls back to cache)
    OfflineValidationResult validateWithFallback();

    // Force online validation
    OfflineValidationResult validateOnline();

    // Get current sync status
    SyncStatus getSyncStatus() const;

    // Request online sync (may be async)
    bool requestSync();

    // Wait for pending sync (blocks with timeout)
    bool waitForSync(uint32_t timeoutMs);

    // Check if we're within grace period
    bool isInGracePeriod() const;

    // Get time remaining until expiry
    uint32_t getSecondsUntilExpiry() const;

    // Get time remaining in grace period
    uint32_t getSecondsInGracePeriod() const;

    // Configure grace period (default 90 days)
    void setGracePeriodSeconds(uint32_t seconds);

    // Configure cache validity (default 30 days)
    void setCacheValiditySeconds(uint32_t seconds);

    // Manual cache update (call after online validation succeeds)
    bool updateCacheFromCurrentLicense();

    // Check for clock skew (detect tampering with system time)
    bool checkClockSkew();

    // Enable/disable offline mode
    void setOfflineModeEnabled(bool enabled);

    // Get detailed validation reason
    const char* getValidationReason() const;

    // Blacklist a license (revocation without server call)
    bool blacklistLicense(uint64_t hwid);

    // Check if license is blacklisted
    bool isBlacklisted(uint64_t hwid) const;

private:
    // Load cache from disk
    bool loadCache();

    // Validate using loaded cache
    OfflineValidationResult validateUsingCache();

    // Perform online sync (internal)
    bool performOnlineSync();

    // Helper: Check expiry with grace period
    bool checkExpiryWithGracePeriod(uint32_t expiryTime, uint32_t now) const;

    // Helper: Detect system time tampering
    bool detectClockTampering();

    // Helper: Get last known good system time
    uint32_t getLastKnownTime() const;

    // Members
    OfflineLicenseCache m_cache;
    SyncStatus          m_syncStatus;
    uint32_t            m_gracePeriodSeconds;
    uint32_t            m_cacheValiditySeconds;
    uint32_t            m_lastKnownTime;
    bool                m_offlineModeEnabled;
    bool                m_cacheLoaded;
    uint16_t            m_lastErrorCode;
    char                m_validationReason[256];
};

// ============================================================================
// Global Offline Validator Instance
// ============================================================================

extern OfflineLicenseValidator g_offlineValidator;

// ============================================================================
// Inline Helpers
// ============================================================================

inline bool isOfflineValidationSuccessful(const OfflineValidationResult& result) {
    return result.success || result.mode == OfflineValidationMode::GRACE_PERIOD;
}

inline const char* getModeString(OfflineValidationMode mode) {
    switch (mode) {
        case OfflineValidationMode::ONLINE:         return "ONLINE";
        case OfflineValidationMode::CACHED:         return "CACHED";
        case OfflineValidationMode::GRACE_PERIOD:   return "GRACE_PERIOD";
        case OfflineValidationMode::OFFLINE:        return "OFFLINE";
        case OfflineValidationMode::TIMED_OUT:      return "TIMED_OUT";
        default:                                    return "UNKNOWN";
    }
}

inline const char* getErrorString(OfflineValidationError error) {
    switch (error) {
        case OfflineValidationError::OK:                    return "OK";
        case OfflineValidationError::CACHE_NOT_FOUND:       return "CACHE_NOT_FOUND";
        case OfflineValidationError::CACHE_CORRUPTED:       return "CACHE_CORRUPTED";
        case OfflineValidationError::CACHE_EXPIRED:         return "CACHE_EXPIRED";
        case OfflineValidationError::LICENSE_EXPIRED:       return "LICENSE_EXPIRED";
        case OfflineValidationError::LICENSE_REVOKED:       return "LICENSE_REVOKED";
        case OfflineValidationError::LICENSE_BLACKLISTED:   return "LICENSE_BLACKLISTED";
        case OfflineValidationError::GRACE_PERIOD_EXCEEDED: return "GRACE_PERIOD_EXCEEDED";
        case OfflineValidationError::INVALID_HWID:          return "INVALID_HWID";
        case OfflineValidationError::SYNC_FAILED:           return "SYNC_FAILED";
        case OfflineValidationError::NETWORK_UNREACHABLE:   return "NETWORK_UNREACHABLE";
        case OfflineValidationError::CLOCK_SKEW_DETECTED:   return "CLOCK_SKEW_DETECTED";
        case OfflineValidationError::TAMPERING_DETECTED:    return "TAMPERING_DETECTED";
        default:                                            return "UNKNOWN";
    }
}

}  // namespace RawrXD::License
