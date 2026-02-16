// ============================================================================
// license_phase4_deployment.h — Phase 4 Production Deployment API
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Audit Tracking API
// ============================================================================

/**
 * Initialize the audit tracking system
 * @return true if initialization successful
 */
bool initializeAuditTracking();

/**
 * Record a feature access event
 * @param feature Feature ID
 * @param granted Whether access was granted
 * @param caller Module/caller name
 */
void recordFeatureAccess(uint32_t feature, bool granted, const char* caller);

/**
 * Record license activation
 * @param tier New license tier
 */
void recordLicenseActivation(uint32_t tier);

/**
 * Record tampering detection
 * @param tamperPattern Tampering pattern bitmask
 */
void recordTamperingDetected(uint16_t tamperPattern);

/**
 * Record offline validation result
 * @param successful Whether validation succeeded
 */
void recordOfflineValidation(bool successful);

/**
 * Get audit summary for compliance
 */
const char* getAuditSummary();

/**
 * Export audit trail to file
 * @param exportPath File path
 * @param format Export format ("json", "csv", "siem")
 * @return true if export successful
 */
bool exportAuditTrail(const char* exportPath, const char* format);

/**
 * Get total audit events
 */
uint32_t getAuditEventCount();

/**
 * Get total feature denials
 */
uint32_t getAuditDenialCount();

/**
 * Get feature denial rate
 */
float getFeatureDenialRate();

/**
 * Check if system is in anomalous state
 */
bool isSystemAnomalous();

// ============================================================================
// Offline Sync Configuration API
// ============================================================================

/**
 * Initialize offline sync configuration
 * @return true if successful
 */
bool initializeOfflineSyncConfig();

/**
 * Get grace period in seconds
 */
uint32_t getGracePeriodSeconds();

/**
 * Get cache validity in seconds
 */
uint32_t getCacheValiditySeconds();

/**
 * Get sync interval in seconds
 */
uint32_t getSyncIntervalSeconds();

/**
 * Check if offline mode is enabled
 */
bool isOfflineModeEnabled();

/**
 * Check if auto-sync is enabled
 */
bool isAutoSyncEnabled();

/**
 * Update offline sync configuration
 */
bool updateOfflineSyncConfig(uint32_t gracePeriodDays, uint32_t cacheValidityDays,
                             uint32_t syncIntervalHours, bool enableAutoSync);

// ============================================================================
// IDE Integration API
// ============================================================================

/**
 * Initialize license manager UI with IDE
 * @param ideWindow Handle to IDE main window
 * @return true if successful
 */
bool initializeLicenseManagerUI(void* ideWindow);

/**
 * Handle license manager menu command
 * @param menuID Menu command ID
 * @return true if command was handled
 */
bool handleLicenseManagerCommand(int menuID);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get tier name for display
 */
const char* getTierNameForDisplay(uint32_t tier);

/**
 * Format timestamp for display
 */
const char* formatTimestampForDisplay(uint32_t timestamp);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LICENSE_PHASE4_DEPLOYMENT_H
