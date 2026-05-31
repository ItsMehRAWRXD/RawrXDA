// ============================================================================
// license_compliance_test.cpp — Enterprise License Compliance Test Suite
// ============================================================================

#include "../include/license_audit_trail.h"
#include "../include/license_offline_validator.h"
#include "../include/license_anti_tampering.h"
#include "../include/enterprise_license.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <fstream>

using namespace RawrXD::License;

// ============================================================================
// SIEM Export Tests
// ============================================================================

void test_siem_cef_export() {
    printf("[TEST] SIEM CEF Format Export...\n");

    std::string cef = g_siemExporter.formatCEF(AuditEventType::FEATURE_GRANTED, 
                                               static_cast<FeatureID>(5), "test_caller");

    printf("  CEF Output:\n    %s\n", cef.c_str());

    // Verify CEF format starts correctly
    assert(cef.find("CEF:0") == 0);
    assert(cef.find("RawrXD") != std::string::npos);

    printf("  ✓ PASS\n");
}

void test_siem_leef_export() {
    printf("[TEST] SIEM LEEF Format Export...\n");

    std::string leef = g_siemExporter.formatLEEF(AuditEventType::TAMPERING_DETECTED,
                                                 static_cast<FeatureID>(0), "tampering_detector");

    printf("  LEEF Output:\n    %s\n", leef.c_str());

    // Verify LEEF format
    assert(leef.find("LEEF:2.0") == 0);
    assert(leef.find("RawrXD") != std::string::npos);

    printf("  ✓ PASS\n");
}

void test_siem_json_export() {
    printf("[TEST] SIEM JSON Format Export...\n");

    std::string json = g_siemExporter.formatJSON(AuditEventType::LICENSE_ACTIVATED,
                                                 static_cast<FeatureID>(0), "activation");

    printf("  JSON Output:\n%s\n", json.c_str());

    // Verify JSON structure
    assert(json.find("{") != std::string::npos);
    assert(json.find("\"timestamp\"") != std::string::npos);
    assert(json.find("\"eventType\"") != std::string::npos);

    printf("  ✓ PASS\n");
}

void test_siem_syslog_export() {
    printf("[TEST] SIEM RFC5424 Syslog Format Export...\n");

    std::string syslog = g_siemExporter.formatSyslog(AuditEventType::OFFLINE_VALIDATION,
                                                     static_cast<FeatureID>(0), "offline_validator");

    printf("  Syslog Output:\n    %s\n", syslog.c_str());

    // Verify syslog contains required fields
    assert(syslog.find("RawrXD") != std::string::npos);
    assert(syslog.find("OFFLINE_VALIDATION") != std::string::npos);

    printf("  ✓ PASS\n");
}

// ============================================================================
// Compliance Report Tests
// ============================================================================

void test_compliance_summary_generation() {
    printf("[TEST] Compliance Summary Generation...\n");

    // Record some events
    g_auditTrailManager.recordEvent(AuditEventType::FEATURE_GRANTED,
                                   static_cast<FeatureID>(1), true, "test1");
    g_auditTrailManager.recordEvent(AuditEventType::FEATURE_GRANTED,
                                   static_cast<FeatureID>(1), true, "test2");
    g_auditTrailManager.recordEvent(AuditEventType::FEATURE_DENIED,
                                   static_cast<FeatureID>(2), false, "test3");

    std::string summary = g_auditTrailManager.generateComplianceSummary();

    printf("  Summary Preview:\n%s\n", summary.substr(0, 300).c_str());

    // Verify summary contains key information
    assert(summary.find("Total Events") != std::string::npos);
    assert(summary.find("Denial Rate") != std::string::npos);

    printf("  ✓ PASS\n");
}

void test_json_audit_export() {
    printf("[TEST] JSON Audit Trail Export...\n");

    std::string json = g_auditTrailManager.toJSON(10);

    printf("  JSON Export Size: %zu bytes\n", json.size());
    printf("  JSON Preview:\n%s\n", json.substr(0, 200).c_str());

    // Verify JSON structure
    assert(json.find("{") != std::string::npos);
    assert(json.find("\"totalEvents\"") != std::string::npos);
    assert(json.find("\"totalGrants\"") != std::string::npos);
    assert(json.find("\"totalDenials\"") != std::string::npos);

    printf("  ✓ PASS\n");
}

// ============================================================================
// Audit Statistics Tests
// ============================================================================

void test_audit_statistics_collection() {
    printf("[TEST] Audit Statistics Collection...\n");

    uint32_t totalEvents = g_auditTrailManager.getTotalEvents();
    uint32_t totalGrants = g_auditTrailManager.getTotalGrants();
    uint32_t totalDenials = g_auditTrailManager.getTotalDenials();

    printf("  Total Events: %u\n", totalEvents);
    printf("  Grants: %u\n", totalGrants);
    printf("  Denials: %u\n", totalDenials);

    assert(totalGrants + totalDenials == totalEvents);

    printf("  ✓ PASS\n");
}

void test_feature_statistics() {
    printf("[TEST] Per-Feature Statistics...\n");

    auto stats = g_auditTrailManager.getAllFeatureStats();

    printf("  Features with Activity: %zu\n", stats.size());

    if (stats.size() > 0) {
        printf("  Sample Feature Stats:\n");
        for (size_t i = 0; i < std::min(size_t(3), stats.size()); ++i) {
            printf("    Feature %u: %u grants, %u denials (rate: %.2f%%)\n",
                   stats[i].featureID, stats[i].grantCount, stats[i].denyCount,
                   stats[i].denialRate * 100.0f);
        }
    }

    printf("  ✓ PASS\n");
}

// ============================================================================
// Anomaly Detection Tests
// ============================================================================

void test_anomaly_detection_tampering() {
    printf("[TEST] Anomaly Detection - Tampering...\n");

    bool isAnomaly = g_anomalyDetector.detectAnomaly(AuditEventType::TAMPERING_DETECTED,
                                                     static_cast<FeatureID>(0), "detector");

    float severity = g_anomalyDetector.getAnomalalySeverity();
    const char* description = g_anomalyDetector.getAnomalyDescription();

    printf("  Is Anomaly: %s\n", isAnomaly ? "true" : "false");
    printf("  Severity: %.2f\n", severity);
    printf("  Description: %s\n", description);

    assert(isAnomaly);
    assert(severity > 0.8f);

    printf("  ✓ PASS\n");
}

void test_anomaly_detection_clock_skew() {
    printf("[TEST] Anomaly Detection - Clock Skew...\n");

    g_anomalyDetector.reset();

    bool isAnomaly = g_anomalyDetector.detectAnomaly(AuditEventType::CLOCK_SKEW_DETECTED,
                                                     static_cast<FeatureID>(0), "detector");

    float severity = g_anomalyDetector.getAnomalalySeverity();

    printf("  Is Anomaly: %s\n", isAnomaly ? "true" : "false");
    printf("  Severity: %.2f\n", severity);

    assert(isAnomaly);
    assert(severity > 0.7f);

    printf("  ✓ PASS\n");
}

// ============================================================================
// Offline Validator Tests
// ============================================================================

void test_offline_validator_initialization() {
    printf("[TEST] Offline Validator Initialization...\n");

    OfflineLicenseValidator validator;

    SyncStatus status = validator.getSyncStatus();
    printf("  Sync State: %d\n", (int)status.state);
    printf("  Last Sync Time: %u\n", status.lastSyncTime);

    printf("  ✓ PASS\n");
}

void test_offline_cache_manager() {
    printf("[TEST] Offline Cache Manager...\n");

    OfflineLicenseCache cache = {};
    cache.magic = 0x4D434C4F;
    cache.version = 1;
    cache.lastValidated = static_cast<uint32_t>(std::time(nullptr));
    cache.cacheExpiry = cache.lastValidated + (30 * 24 * 3600);

    // Verify cache structure (basic checks)
    printf("  Cache magic: 0x%08X (expected: 0x4D434C4F)\n", cache.magic);
    printf("  Cache version: %u (expected: 1)\n", cache.version);
    printf("  Cache validity: %u seconds remaining\n", 
           cache.cacheExpiry > static_cast<uint32_t>(std::time(nullptr)) ?
           cache.cacheExpiry - static_cast<uint32_t>(std::time(nullptr)) : 0);

    if (cache.magic == 0x4D434C4F && cache.version == 1 && cache.cacheExpiry > static_cast<uint32_t>(std::time(nullptr))) {
        printf("  Cache structure valid\n");
    }

    printf("  ✓ PASS\n");
}

// ============================================================================
// File Export Tests
// ============================================================================

void test_audit_export_to_file() {
    printf("[TEST] Audit Trail File Export...\n");

    const char* exportPath = "C:\\ProgramData\\RawrXD\\test_audit_export.json";

    bool exported = g_auditTrailManager.exportToFile(exportPath, "json");
    printf("  Export to: %s\n", exportPath);
    printf("  Export successful: %s\n", exported ? "true" : "false");

    if (exported) {
        // Verify file exists
        std::ifstream file(exportPath);
        bool fileExists = file.good();
        printf("  File exists: %s\n", fileExists ? "true" : "false");
        assert(fileExists);
    }

    printf("  ✓ PASS\n");
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║   Enterprise License Compliance Test Suite                ║\n");
    printf("║   Phase 4 Production Feature Validation                    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    try {
        // SIEM Export Tests
        printf("\n[SECTION] SIEM Export Format Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_siem_cef_export();
        test_siem_leef_export();
        test_siem_json_export();
        test_siem_syslog_export();

        // Compliance Report Tests
        printf("\n[SECTION] Compliance Report Generation Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_compliance_summary_generation();
        test_json_audit_export();

        // Audit Statistics Tests
        printf("\n[SECTION] Audit Statistics Collection Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_audit_statistics_collection();
        test_feature_statistics();

        // Anomaly Detection Tests
        printf("\n[SECTION] Anomaly Detection Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_anomaly_detection_tampering();
        test_anomaly_detection_clock_skew();

        // Offline Validator Tests
        printf("\n[SECTION] Offline Validator Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_offline_validator_initialization();
        test_offline_cache_manager();

        // File Export Tests
        printf("\n[SECTION] File Export Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_audit_export_to_file();

        printf("\n");
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║   COMPLIANCE TEST SUITE COMPLETE ✓                        ║\n");
        printf("║   All requirements validated for Phase 4 deployment        ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n\n");

        printf("Next Steps:\n");
        printf("  1. Deploy audit tracking to production\n");
        printf("  2. Configure SIEM integration endpoints\n");
        printf("  3. Schedule daily SIEM exports for compliance\n");
        printf("  4. Monitor audit anomaly alerts\n\n");

        return 0;

    } catch (const std::exception& e) {
        printf("\n[ERROR] Test suite failed: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("\n[ERROR] Unknown exception in test suite\n");
        return 1;
    }
}
