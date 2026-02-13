// Win32IDE_TelemetryPanel.cpp — Phase 17: Enterprise Telemetry & Compliance UI
// Win32 IDE panel for distributed tracing, audit trail inspection,
// compliance reporting, license management, metrics dashboard, and GDPR export.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include "../core/enterprise_telemetry_compliance.hpp"
#include <sstream>
#include <iomanip>
#include <commdlg.h>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initTelemetryPanel() {
    if (m_telemetryPanelInitialized) return;

    appendToOutput("[Telemetry] Phase 17 — Enterprise Telemetry & Compliance initialized.\n");
    m_telemetryPanelInitialized = true;
}

// ============================================================================
// Command Router — defined in Win32IDE_Telemetry.cpp (single definition)
// ============================================================================

// ============================================================================
// Command Handlers — Tracing
// ============================================================================

void Win32IDE::cmdTelTraceStatus() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto& s = tc.stats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║        ENTERPRISE TELEMETRY — TRACE STATUS                 ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Telemetry Level:   " << std::setw(10) << static_cast<int>(tc.getTelemetryLevel())
        << "                          ║\n"
        << "║  Total Spans:       " << std::setw(10) << s.totalSpans.load()
        << "                          ║\n"
        << "║  Active Spans:      " << std::setw(10) << s.activeSpans.load()
        << "                          ║\n"
        << "║  Completed Spans:   " << std::setw(10) << s.completedSpans.load()
        << "                          ║\n"
        << "║  Dropped Spans:     " << std::setw(10) << s.droppedSpans.load()
        << "                          ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

void Win32IDE::cmdTelStartSpan() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto sid = tc.startSpan("ide-manual-span", SpanKind::Internal);

    if (sid.isValid()) {
        tc.addSpanAttribute(sid, "source", "ide-panel");
        appendToOutput("[Telemetry] Span started: ID=" + std::to_string(sid.value) + "\n");
    } else {
        appendToOutput("[Telemetry] Failed to start span (telemetry may be off).\n");
    }
}

// ============================================================================
// Command Handlers — Audit
// ============================================================================

void Win32IDE::cmdTelAuditLog() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto entries = tc.queryAudit(AuditEventType::SystemStart, 0, 50);

    // Also get recent entries of all types
    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                TAMPER-EVIDENT AUDIT TRAIL                  ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total Entries: " << std::setw(10) << tc.getAuditCount()
        << "                              ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    if (entries.empty()) {
        oss << "║  No audit entries recorded yet.                            ║\n";
    } else {
        for (auto& e : entries) {
            oss << "║  [" << std::setw(8) << e.entryId << "] "
                << std::left << std::setw(12) << e.actor
                << " " << std::setw(16) << e.action
                << " hash=" << std::hex << e.entryHash << std::dec
                << "  ║\n";
        }
    }
    oss << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

void Win32IDE::cmdTelAuditVerify() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto result = tc.verifyAuditIntegrity();

    if (result.success) {
        appendToOutput("[Telemetry] ✓ Audit trail integrity verified — no tampering detected.\n");
    } else {
        appendToOutput("[Telemetry] ✗ AUDIT INTEGRITY FAILURE: " +
                       std::string(result.detail) + "\n");
    }
}

// ============================================================================
// Command Handlers — Compliance
// ============================================================================

void Win32IDE::cmdTelComplianceReport() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "compliance_report.json";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "json";

    if (GetSaveFileNameA(&ofn)) {
        auto& tc = EnterpriseTelemetryCompliance::instance();
        auto result = tc.generateComplianceReport(filePath, ComplianceStandard::None);
        appendToOutput("[Telemetry] " + std::string(result.detail) + ": " + filePath + "\n");
    }
}

void Win32IDE::cmdTelShowViolations() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto violations = tc.getViolations(0, true);

    if (violations.empty()) {
        appendToOutput("[Telemetry] No unresolved compliance violations.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Telemetry] Unresolved Violations (" << violations.size() << "):\n";
    for (auto& v : violations) {
        oss << "  [V" << v.violationId << "] policy=" << v.policyId
            << " — " << v.description << "\n";
    }
    appendToOutput(oss.str());
}

// ============================================================================
// Command Handlers — License
// ============================================================================

void Win32IDE::cmdTelLicenseStatus() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto tier = tc.getCurrentTier();
    auto result = tc.validateLicense();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                   LICENSE STATUS                           ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Tier:              ";

    switch (tier) {
        case LicenseTier::Community:    oss << "Community   "; break;
        case LicenseTier::Professional: oss << "Professional"; break;
        case LicenseTier::Enterprise:   oss << "Enterprise  "; break;
        case LicenseTier::OEM:          oss << "OEM         "; break;
    }

    oss << "                              ║\n"
        << "║  Valid:             " << (result.success ? "YES" : "NO ")
        << "                                       ║\n";

    if (!result.success) {
        oss << "║  Reason:            " << std::left << std::setw(38)
            << result.detail << "║\n";
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}

void Win32IDE::cmdTelUsageMeter() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto meter = tc.getUsageMeter();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                    USAGE METERING                          ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Inferences:        " << std::setw(12) << meter.inferenceCount.load()
        << "                        ║\n"
        << "║  Tokens Processed:  " << std::setw(12) << meter.tokensProcessed.load()
        << "                        ║\n"
        << "║  Models Loaded:     " << std::setw(12) << meter.modelsLoaded.load()
        << "                        ║\n"
        << "║  Patches Applied:   " << std::setw(12) << meter.patchesApplied.load()
        << "                        ║\n"
        << "║  API Calls:         " << std::setw(12) << meter.apiCallCount.load()
        << "                        ║\n"
        << "║  Bytes Transferred: " << std::setw(12) << meter.bytesTransferred.load()
        << "                        ║\n"
        << "║  Active Users:      " << std::setw(12) << meter.activeUsers.load()
        << "                        ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

// ============================================================================
// Command Handlers — Metrics
// ============================================================================

void Win32IDE::cmdTelMetricsDashboard() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto metrics = tc.getMetrics("");

    if (metrics.empty()) {
        appendToOutput("[Telemetry] No metrics recorded.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Telemetry] Metrics (" << metrics.size() << " recent):\n";

    size_t shown = 0;
    for (auto it = metrics.rbegin(); it != metrics.rend() && shown < 50; ++it, ++shown) {
        oss << "  " << std::left << std::setw(30) << it->name
            << " = " << std::fixed << std::setprecision(2) << it->value;

        switch (it->type) {
            case MetricType::Counter:   oss << " [counter]";   break;
            case MetricType::Gauge:     oss << " [gauge]";     break;
            case MetricType::Histogram: oss << " [histogram]"; break;
            case MetricType::Summary:   oss << " [summary]";   break;
        }
        oss << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdTelMetricsFlush() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto result = tc.flushMetrics();
    appendToOutput("[Telemetry] " + std::string(result.detail) + "\n");
}

// ============================================================================
// Command Handlers — Export / GDPR
// ============================================================================

void Win32IDE::cmdTelExportAudit() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "audit_export.json";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "json";

    if (GetSaveFileNameA(&ofn)) {
        auto& tc = EnterpriseTelemetryCompliance::instance();
        auto result = tc.exportAuditLog(filePath);
        appendToOutput("[Telemetry] " + std::string(result.detail) + ": " + filePath + "\n");
    }
}

void Win32IDE::cmdTelExportOTLP() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "telemetry_otlp.json";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "json";

    if (GetSaveFileNameA(&ofn)) {
        auto& tc = EnterpriseTelemetryCompliance::instance();
        auto result = tc.exportTelemetryOTLP(filePath);
        appendToOutput("[Telemetry] " + std::string(result.detail) + ": " + filePath + "\n");
    }
}

void Win32IDE::cmdTelGDPRExport() {
    appendToOutput("[Telemetry] GDPR Export: Enter user ID via command palette.\n");
    // In production, prompt for user ID and output path
}

void Win32IDE::cmdTelGDPRDelete() {
    appendToOutput("[Telemetry] GDPR Delete: Enter user ID via command palette.\n"
                   "  WARNING: This will permanently redact all user data.\n");
}

void Win32IDE::cmdTelSetLevel() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto current = tc.getTelemetryLevel();

    // Cycle through levels
    int next = (static_cast<int>(current) + 1) % 7;
    tc.setTelemetryLevel(static_cast<TelemetryLevel>(next));

    const char* names[] = {"Off", "Critical", "Error", "Warning", "Info", "Debug", "Trace"};
    appendToOutput("[Telemetry] Level set to: " + std::string(names[next]) + "\n");
}

void Win32IDE::cmdTelShowStats() {
    auto& tc = EnterpriseTelemetryCompliance::instance();
    auto& s = tc.stats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║       ENTERPRISE TELEMETRY & COMPLIANCE — STATISTICS       ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total Spans:         " << std::setw(10) << s.totalSpans.load()
        << "                        ║\n"
        << "║  Active Spans:        " << std::setw(10) << s.activeSpans.load()
        << "                        ║\n"
        << "║  Completed Spans:     " << std::setw(10) << s.completedSpans.load()
        << "                        ║\n"
        << "║  Dropped Spans:       " << std::setw(10) << s.droppedSpans.load()
        << "                        ║\n"
        << "║  Audit Entries:       " << std::setw(10) << s.auditEntries.load()
        << "                        ║\n"
        << "║  Policy Violations:   " << std::setw(10) << s.policyViolations.load()
        << "                        ║\n"
        << "║  License Checks:      " << std::setw(10) << s.licenseChecks.load()
        << "                        ║\n"
        << "║  Metrics Recorded:    " << std::setw(10) << s.metricsRecorded.load()
        << "                        ║\n"
        << "║  Exports Completed:   " << std::setw(10) << s.exportsCompleted.load()
        << "                        ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}
