// ============================================================================
// license_audit_tracking_deployment.cpp — Audit Trail Deployment & Integration
// ============================================================================

#include "../include/license_audit_trail.h"
#include "../include/enterprise_license.h"
#include <cstdio>
#include <ctime>
#include <windows.h>

namespace RawrXD::License {

// ============================================================================
// Audit Tracking Deployment Manager
// ============================================================================

class AuditTrackingDeployment {
public:
    // Initialize audit tracking
    static bool initializeAuditTracking() {
        printf("[AUDIT] Initializing audit trail system...\n");

        // Create audit directory
        if (!CreateDirectory("C:\\ProgramData\\RawrXD", nullptr)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                printf("  [ERROR] Failed to create audit directory\n");
                return false;
            }
        }

        printf("  [OK] Audit directory ready\n");
        printf("  [OK] Audit trail system initialized\n");

        return true;
    }

    // Record feature access event
    static void recordFeatureAccess(FeatureID feature, bool granted, const char* caller) {
        AuditEventType eventType = granted ? AuditEventType::FEATURE_GRANTED 
                                             : AuditEventType::FEATURE_DENIED;

        g_auditTrailManager.recordEvent(eventType, feature, granted, caller);

        if (!granted) {
            // Check if this is anomalous
            if (g_anomalyDetector.detectAnomaly(eventType, feature, caller)) {
                float severity = g_anomalyDetector.getAnomalalySeverity();
                printf("[AUDIT] Anomaly detected: %s (severity: %.2f)\n",
                       g_anomalyDetector.getAnomalyDescription(), severity);
            }
        }
    }

    // Record license activation
    static void recordLicenseActivation(LicenseTierV2 tier) {
        printf("[AUDIT] License activated: %s tier\n", tierNameForDisplay(tier));
        g_auditTrailManager.recordEvent(AuditEventType::LICENSE_ACTIVATED, 
                                        static_cast<FeatureID>(0), true, "system");
    }

    // Record tampering detection
    static void recordTamperingDetected(uint16_t tamperPattern) {
        printf("[AUDIT] ALERT: Tampering detected (pattern: 0x%04X)\n", tamperPattern);
        g_auditTrailManager.recordEvent(AuditEventType::TAMPERING_DETECTED,
                                        static_cast<FeatureID>(0), false, "tamper_detector");
    }

    // Record offline validation
    static void recordOfflineValidation(bool successful) {
        printf("[AUDIT] Offline validation: %s\n", successful ? "success" : "failed");
        g_auditTrailManager.recordEvent(AuditEventType::OFFLINE_VALIDATION,
                                        static_cast<FeatureID>(0), successful, "offline_validator");
    }

    // Record grace period entry
    static void recordGracePeriodEntry() {
        printf("[AUDIT] License in grace period\n");
        g_auditTrailManager.recordEvent(AuditEventType::GRACE_PERIOD_ENTERED,
                                        static_cast<FeatureID>(0), true, "system");
    }

    // Record grace period exceeded
    static void recordGracePeriodExceeded() {
        printf("[AUDIT] ALERT: Grace period exceeded\n");
        g_auditTrailManager.recordEvent(AuditEventType::GRACE_PERIOD_EXCEEDED,
                                        static_cast<FeatureID>(0), false, "system");
    }

    // Get audit summary for logging
    static std::string getAuditSummary() {
        return g_auditTrailManager.generateComplianceSummary();
    }

    // Export audit trail for compliance
    static bool exportAuditTrail(const char* exportPath, const char* format) {
        printf("[AUDIT] Exporting audit trail to: %s\n", exportPath);
        return g_auditTrailManager.exportToFile(exportPath, format);
    }

private:
    static const char* tierNameForDisplay(LicenseTierV2 tier) {
        switch (tier) {
            case LicenseTierV2::Community:    return "Community";
            case LicenseTierV2::Professional: return "Professional";
            case LicenseTierV2::Enterprise:   return "Enterprise";
            case LicenseTierV2::Sovereign:    return "Sovereign";
            default:                          return "Unknown";
        }
    }
};

// ============================================================================
// Feature Gate Audit Hook
// ============================================================================

class FeatureGateAuditHook {
public:
    // Hook into feature gate for audit tracking
    static bool gateWithAudit(FeatureID feature, const char* caller) {
        // Get current license state
        auto& lic = EnterpriseLicenseV2::Instance();
        bool isEnabled = lic.isFeatureEnabled(feature);

        // Record in audit trail
        AuditTrackingDeployment::recordFeatureAccess(feature, isEnabled, caller);

        return isEnabled;
    }

    // Check license tier with audit
    static bool checkTierWithAudit(LicenseTierV2 requiredTier, const char* caller) {
        auto& lic = EnterpriseLicenseV2::Instance();
        LicenseTierV2 currentTier = lic.currentTier();

        bool allowed = (currentTier >= requiredTier);

        if (allowed) {
            g_auditTrailManager.recordEvent(AuditEventType::FEATURE_GRANTED,
                                           static_cast<FeatureID>(0), true, caller);
        } else {
            g_auditTrailManager.recordEvent(AuditEventType::FEATURE_DENIED,
                                           static_cast<FeatureID>(0), false, caller);
        }

        return allowed;
    }
};

// ============================================================================
// Global Audit Hooks
// ============================================================================

extern "C" {

/**
 * Initialize audit tracking system
 */
bool initializeAuditTracking() {
    return AuditTrackingDeployment::initializeAuditTracking();
}

/**
 * Record feature access in audit trail
 * 
 * @param feature Feature ID being accessed
 * @param granted Whether access was granted
 * @param caller Caller/module name for audit log
 */
void recordFeatureAccess(uint32_t feature, bool granted, const char* caller) {
    AuditTrackingDeployment::recordFeatureAccess(
        static_cast<FeatureID>(feature), granted, caller ? caller : "unknown");
}

/**
 * Record license activation
 * 
 * @param tier New license tier
 */
void recordLicenseActivation(uint32_t tier) {
    AuditTrackingDeployment::recordLicenseActivation(
        static_cast<LicenseTierV2>(tier));
}

/**
 * Record tampering detection
 * 
 * @param tamperPattern Tampering pattern bitmask
 */
void recordTamperingDetected(uint16_t tamperPattern) {
    AuditTrackingDeployment::recordTamperingDetected(tamperPattern);
}

/**
 * Record offline validation result
 * 
 * @param successful Whether validation succeeded
 */
void recordOfflineValidation(bool successful) {
    AuditTrackingDeployment::recordOfflineValidation(successful);
}

/**
 * Record grace period entry
 */
void recordGracePeriodEntry() {
    AuditTrackingDeployment::recordGracePeriodEntry();
}

/**
 * Record grace period exceeded
 */
void recordGracePeriodExceeded() {
    AuditTrackingDeployment::recordGracePeriodExceeded();
}

/**
 * Get audit summary
 */
const char* getAuditSummary() {
    static std::string summary;
    summary = AuditTrackingDeployment::getAuditSummary();
    return summary.c_str();
}

/**
 * Export audit trail for compliance
 * 
 * @param exportPath File path for export
 * @param format Export format ("json", "csv", "siem")
 * @return true if export successful
 */
bool exportAuditTrail(const char* exportPath, const char* format) {
    return AuditTrackingDeployment::exportAuditTrail(
        exportPath ? exportPath : "C:\\ProgramData\\RawrXD\\audit_export.json",
        format ? format : "json");
}

/**
 * Gate feature access with audit
 * 
 * @param feature Feature ID
 * @param caller Caller name for audit
 * @return true if feature is enabled
 */
bool gateFeatureWithAudit(uint32_t feature, const char* caller) {
    return FeatureGateAuditHook::gateWithAudit(
        static_cast<FeatureID>(feature), caller ? caller : "unknown");
}

/**
 * Check tier with audit
 * 
 * @param tier Required tier
 * @param caller Caller name for audit
 * @return true if current tier meets requirement
 */
bool checkTierWithAudit(uint32_t tier, const char* caller) {
    return FeatureGateAuditHook::checkTierWithAudit(
        static_cast<LicenseTierV2>(tier), caller ? caller : "unknown");
}

/**
 * Get total audit events
 */
uint32_t getAuditEventCount() {
    return g_auditTrailManager.getTotalEvents();
}

/**
 * Get total feature denials
 */
uint32_t getAuditDenialCount() {
    return g_auditTrailManager.getTotalDenials();
}

/**
 * Get feature denial rate
 */
float getFeatureDenialRate() {
    return g_auditTrailManager.getDenialRate();
}

/**
 * Check if system is in anomalous state
 */
bool isSystemAnomalous() {
    return g_auditTrailManager.isInAnomalousState();
}

}  // extern "C"

}  // namespace RawrXD::License
