// ============================================================================
// license_offline_sync_config.cpp — Offline License Sync Configuration
// ============================================================================

#include "../include/license_offline_validator.h"
#include <cstring>
#include <cstdio>
#include <windows.h>
#include <shlobj.h>

namespace RawrXD::License {

// ============================================================================
// Offline Sync Configuration Manager
// ============================================================================

class OfflineSyncConfigManager {
public:
    // Configuration defaults
    static constexpr uint32_t DEFAULT_GRACE_PERIOD = 90 * 24 * 3600;      // 90 days
    static constexpr uint32_t DEFAULT_CACHE_VALIDITY = 30 * 24 * 3600;    // 30 days
    static constexpr uint32_t DEFAULT_SYNC_INTERVAL = 24 * 3600;          // 1 day
    static constexpr uint32_t DEFAULT_SYNC_TIMEOUT = 5000;                // 5 seconds

    struct SyncConfig {
        uint32_t gracePeriodSeconds;
        uint32_t cacheValiditySeconds;
        uint32_t syncIntervalSeconds;
        uint32_t syncTimeoutMs;
        bool     enableAutoSync;
        bool     enableOfflineMode;
        char     cacheLocation[256];
        char     auditLocation[256];
    };

    // Get default configuration
    static SyncConfig getDefaultConfig() {
        SyncConfig config = {};

        config.gracePeriodSeconds = DEFAULT_GRACE_PERIOD;
        config.cacheValiditySeconds = DEFAULT_CACHE_VALIDITY;
        config.syncIntervalSeconds = DEFAULT_SYNC_INTERVAL;
        config.syncTimeoutMs = DEFAULT_SYNC_TIMEOUT;
        config.enableAutoSync = true;
        config.enableOfflineMode = true;

        // Set standard paths
        strcpy_s(config.cacheLocation, sizeof(config.cacheLocation),
                      "C:\\ProgramData\\RawrXD\\license.cache");
        strcpy_s(config.auditLocation, sizeof(config.auditLocation),
                      "C:\\ProgramData\\RawrXD\\audit.log");

        return config;
    }

    // Load configuration from file
    static bool loadConfig(SyncConfig& config) {
        const char* configPath = getConfigPath();

        FILE* file = nullptr;
        errno_t err = fopen_s(&file, configPath, "r");
        if (err != 0 || !file) {
            // No config file, use defaults
            config = getDefaultConfig();
            return true;
        }

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\n') continue;

            // Parse key=value pairs
            char* equals = strchr(line, '=');
            if (!equals) continue;

            *equals = '\0';
            const char* key = line;
            const char* value = equals + 1;

            // Trim whitespace
            while (*value == ' ' || *value == '\t') value++;

            // Remove trailing newline
            size_t len = strlen(value);
            if (len > 0 && value[len - 1] == '\n') {
                const_cast<char*>(value)[len - 1] = '\0';
            }

            // Parse values
            if (strcmp(key, "grace_period_days") == 0) {
                config.gracePeriodSeconds = atoi(value) * 24 * 3600;
            } else if (strcmp(key, "cache_validity_days") == 0) {
                config.cacheValiditySeconds = atoi(value) * 24 * 3600;
            } else if (strcmp(key, "sync_interval_hours") == 0) {
                config.syncIntervalSeconds = atoi(value) * 3600;
            } else if (strcmp(key, "sync_timeout_ms") == 0) {
                config.syncTimeoutMs = atoi(value);
            } else if (strcmp(key, "enable_auto_sync") == 0) {
                config.enableAutoSync = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "enable_offline_mode") == 0) {
                config.enableOfflineMode = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "cache_location") == 0) {
                strncpy_s(config.cacheLocation, sizeof(config.cacheLocation),
                               value, 255);
            } else if (strcmp(key, "audit_location") == 0) {
                strncpy_s(config.auditLocation, sizeof(config.auditLocation),
                               value, 255);
            }
        }

        fclose(file);
        return true;
    }

    // Save configuration to file
    static bool saveConfig(const SyncConfig& config) {
        const char* configPath = getConfigPath();

        // Ensure directory exists
        CreateDirectory("C:\\ProgramData\\RawrXD", nullptr);

        FILE* file = nullptr;
        errno_t err = fopen_s(&file, configPath, "w");
        if (err != 0 || !file) {
            return false;
        }

        fprintf(file, "# RawrXD License Offline Sync Configuration\n");
        fprintf(file, "# Generated: Phase 4 Production Deployment\n\n");

        fprintf(file, "# Grace period after license expiry (days)\n");
        fprintf(file, "grace_period_days=%u\n\n", config.gracePeriodSeconds / (24 * 3600));

        fprintf(file, "# Cache validity period (days)\n");
        fprintf(file, "cache_validity_days=%u\n\n", config.cacheValiditySeconds / (24 * 3600));

        fprintf(file, "# Online sync interval (hours)\n");
        fprintf(file, "sync_interval_hours=%u\n\n", config.syncIntervalSeconds / 3600);

        fprintf(file, "# Sync attempt timeout (milliseconds)\n");
        fprintf(file, "sync_timeout_ms=%u\n\n", config.syncTimeoutMs);

        fprintf(file, "# Enable automatic background sync\n");
        fprintf(file, "enable_auto_sync=%s\n\n", config.enableAutoSync ? "true" : "false");

        fprintf(file, "# Allow operation in offline mode\n");
        fprintf(file, "enable_offline_mode=%s\n\n", config.enableOfflineMode ? "true" : "false");

        fprintf(file, "# Cache file location\n");
        fprintf(file, "cache_location=%s\n\n", config.cacheLocation);

        fprintf(file, "# Audit log location\n");
        fprintf(file, "audit_location=%s\n\n", config.auditLocation);

        fclose(file);
        return true;
    }

    // Get configuration file path
    static const char* getConfigPath() {
        static char path[256] = {};
        if (path[0] == '\0') {
            strcpy_s(path, sizeof(path), "C:\\ProgramData\\RawrXD\\license.conf");
        }
        return path;
    }

    // Apply configuration to global validator
    static bool applyConfigToValidator(OfflineLicenseValidator& validator) {
        SyncConfig config;
        if (!loadConfig(config)) {
            return false;
        }

        validator.setGracePeriodSeconds(config.gracePeriodSeconds);
        validator.setCacheValiditySeconds(config.cacheValiditySeconds);
        validator.setOfflineModeEnabled(config.enableOfflineMode);

        return true;
    }
};

// ============================================================================
// Global Configuration Instance
// ============================================================================

static OfflineSyncConfigManager::SyncConfig g_syncConfig;

// ============================================================================
// Public API for Configuration Management
// ============================================================================

extern "C" {

/**
 * Initialize offline sync configuration
 * Creates default config if not present
 */
bool initializeOfflineSyncConfig() {
    // Ensure directory exists
    CreateDirectory("C:\\ProgramData\\RawrXD", nullptr);

    // Try to load existing config
    if (!OfflineSyncConfigManager::loadConfig(g_syncConfig)) {
        // Create default config
        g_syncConfig = OfflineSyncConfigManager::getDefaultConfig();
    }

    // Save config to file
    return OfflineSyncConfigManager::saveConfig(g_syncConfig);
}

/**
 * Get current offline sync configuration
 */
const void* getOfflineSyncConfig() {
    return &g_syncConfig;
}

/**
 * Update offline sync configuration
 */
bool updateOfflineSyncConfig(uint32_t gracePeriodDays, uint32_t cacheValidityDays,
                             uint32_t syncIntervalHours, bool enableAutoSync) {
    g_syncConfig.gracePeriodSeconds = gracePeriodDays * 24 * 3600;
    g_syncConfig.cacheValiditySeconds = cacheValidityDays * 24 * 3600;
    g_syncConfig.syncIntervalSeconds = syncIntervalHours * 3600;
    g_syncConfig.enableAutoSync = enableAutoSync;

    return OfflineSyncConfigManager::saveConfig(g_syncConfig);
}

/**
 * Get grace period in seconds
 */
uint32_t getGracePeriodSeconds() {
    return g_syncConfig.gracePeriodSeconds;
}

/**
 * Get cache validity in seconds
 */
uint32_t getCacheValiditySeconds() {
    return g_syncConfig.cacheValiditySeconds;
}

/**
 * Get sync interval in seconds
 */
uint32_t getSyncIntervalSeconds() {
    return g_syncConfig.syncIntervalSeconds;
}

/**
 * Check if offline mode is enabled
 */
bool isOfflineModeEnabled() {
    return g_syncConfig.enableOfflineMode;
}

/**
 * Check if auto-sync is enabled
 */
bool isAutoSyncEnabled() {
    return g_syncConfig.enableAutoSync;
}

}  // extern "C"

}  // namespace RawrXD::License
