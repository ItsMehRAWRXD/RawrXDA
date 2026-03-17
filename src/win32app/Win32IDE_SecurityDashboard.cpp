#include "Win32IDE.h"

#include "../core/problems_aggregator.hpp"
#include "../security/sbom_export.hpp"
#include "../security/dast_bridge.hpp"

#include <commdlg.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <cctype>

namespace {

std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string escapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string chooseSavePath(HWND owner, const char* title, const char* defaultName, const char* filter, const char* defExt) {
    OPENFILENAMEA ofn = {};
    char outFile[MAX_PATH] = {};
    strcpy_s(outFile, defaultName);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = outFile;
    ofn.nMaxFile = sizeof(outFile);
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;
    if (!GetSaveFileNameA(&ofn)) return {};
    return std::string(outFile);
}

int readLastSecurityTotal() {
    namespace fs = std::filesystem;
    std::ifstream in((fs::current_path() / ".rawrxd" / "security_dashboard_total.txt").string(), std::ios::binary);
    if (!in.is_open()) return -1;
    int v = -1;
    in >> v;
    return v;
}

void writeLastSecurityTotal(int total) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::current_path() / ".rawrxd", ec);
    std::ofstream out((fs::current_path() / ".rawrxd" / "security_dashboard_total.txt").string(),
                      std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;
    out << total;
}

std::string toLower(std::string v) {
    for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return v;
}

bool isSecuritySource(const std::string& source) {
    const std::string s = toLower(source);
    return s == "sast" || s == "sca" || s == "secrets" || s == "dast" || s == "security";
}

std::vector<RawrXD::ProblemEntry> collectSecurityFindings() {
    const auto all = ProblemsAggregator::instance().getProblems();
    std::vector<RawrXD::ProblemEntry> filtered;
    filtered.reserve(all.size());
    for (const auto& p : all) {
        if (isSecuritySource(p.source)) filtered.push_back(p);
    }
    return filtered;
}

std::string csvEscape(const std::string& s) {
    bool needsQuotes = false;
    for (char c : s) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return s;
    std::string out = "\"";
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

} // namespace

using RawrXD::ProblemsAggregator;
using RawrXD::Security::SbomComponent;
using RawrXD::Security::SbomExport;

void Win32IDE::initSecurityDashboard() {
    if (m_securityDashboardInitialized) {
        return;
    }
    m_securityDashboardInitialized = true;
}

void Win32IDE::ShowSecurityDashboard() {
    if (!m_securityDashboardInitialized) {
        initSecurityDashboard();
    }

    const auto all = collectSecurityFindings();
    const auto lsp = ProblemsAggregator::instance().getProblems("", "LSP");
    const auto sast = ProblemsAggregator::instance().getProblems("", "SAST");
    const auto sca = ProblemsAggregator::instance().getProblems("", "SCA");
    const auto secrets = ProblemsAggregator::instance().getProblems("", "Secrets");
    const auto dast = ProblemsAggregator::instance().getProblems("", "DAST");

    auto severityCounts = [](const std::vector<RawrXD::ProblemEntry>& items) {
        struct C { int err = 0; int warn = 0; int info = 0; int hint = 0; } c;
        for (const auto& p : items) {
            switch (p.severity) {
                case 1: c.err++; break;
                case 2: c.warn++; break;
                case 3: c.info++; break;
                case 4: c.hint++; break;
                default: c.info++; break;
            }
        }
        return c;
    };
    const auto sevAll = severityCounts(all);
    const int riskScore = (sevAll.err * 5) + (sevAll.warn * 3) + sevAll.info;

    std::unordered_map<std::string, int> fileFreq;
    for (const auto& p : all) {
        if (!p.path.empty()) fileFreq[p.path]++;
    }
    std::vector<std::pair<std::string, int>> hotFiles(fileFreq.begin(), fileFreq.end());
    std::sort(hotFiles.begin(), hotFiles.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (hotFiles.size() > 5) hotFiles.resize(5);

    std::string summary;
    summary += "=== RawrXD Security Dashboard ===\n\n";
    summary += "Total Security Findings: " + std::to_string(all.size()) + "\n";
    summary += "  SAST:     " + std::to_string(sast.size()) + "\n";
    summary += "  SCA:      " + std::to_string(sca.size()) + "\n";
    summary += "  Secrets:  " + std::to_string(secrets.size()) + "\n";
    summary += "  DAST:     " + std::to_string(dast.size()) + "\n";
    summary += "  Non-Security (LSP): " + std::to_string(lsp.size()) + "\n\n";
    const int prevTotal = readLastSecurityTotal();
    if (prevTotal >= 0) {
        const int delta = static_cast<int>(all.size()) - prevTotal;
        summary += "Delta since last dashboard: " + std::to_string(delta) + "\n\n";
    }
    writeLastSecurityTotal(static_cast<int>(all.size()));
    summary += "Severity:\n";
    summary += "  Errors:   " + std::to_string(sevAll.err) + "\n";
    summary += "  Warnings: " + std::to_string(sevAll.warn) + "\n";
    summary += "  Info:     " + std::to_string(sevAll.info) + "\n";
    summary += "  Hints:    " + std::to_string(sevAll.hint) + "\n\n";
    summary += "Risk Score: " + std::to_string(riskScore) + " (E*5 + W*3 + I)\n\n";

    const auto perSource = [&](const std::vector<RawrXD::ProblemEntry>& items) {
        return severityCounts(items);
    };
    const auto srcSast = perSource(sast);
    const auto srcSca = perSource(sca);
    const auto srcSecrets = perSource(secrets);
    const auto srcDast = perSource(dast);
    summary += "Severity by Source:\n";
    summary += "  SAST    E/W/I/H: " + std::to_string(srcSast.err) + "/" + std::to_string(srcSast.warn) + "/" +
               std::to_string(srcSast.info) + "/" + std::to_string(srcSast.hint) + "\n";
    summary += "  SCA     E/W/I/H: " + std::to_string(srcSca.err) + "/" + std::to_string(srcSca.warn) + "/" +
               std::to_string(srcSca.info) + "/" + std::to_string(srcSca.hint) + "\n";
    summary += "  Secrets E/W/I/H: " + std::to_string(srcSecrets.err) + "/" + std::to_string(srcSecrets.warn) + "/" +
               std::to_string(srcSecrets.info) + "/" + std::to_string(srcSecrets.hint) + "\n";
    summary += "  DAST    E/W/I/H: " + std::to_string(srcDast.err) + "/" + std::to_string(srcDast.warn) + "/" +
               std::to_string(srcDast.info) + "/" + std::to_string(srcDast.hint) + "\n\n";

    if (!hotFiles.empty()) {
        summary += "Top Affected Files:\n";
        for (const auto& [path, count] : hotFiles) {
            summary += "  - (" + std::to_string(count) + ") " + path + "\n";
        }
        summary += "\n";
    }

    std::time_t now = std::time(nullptr);
    summary += "Snapshot: " + std::string(std::ctime(&now));
    summary += "Use Security menu to run scans.\nUse Problems tab to inspect findings.";

    MessageBoxA(m_hwndMain, summary.c_str(), "Security Dashboard", MB_OK | MB_ICONINFORMATION);
}

bool Win32IDE::handleSecurityCommand(int commandId) {
    switch (commandId) {
        case IDM_SECURITY_SCAN_SECRETS:
            RunSecretsScan();
            return true;
        case IDM_SECURITY_SCAN_SAST:
            RunSastScan();
            return true;
        case IDM_SECURITY_SCAN_DEPENDENCIES:
            RunDependencyAudit();
            return true;
        case IDM_SECURITY_SCAN_ALL:
            RunSecurityFullScan();
            return true;
        case IDM_SECURITY_CLEAR_FINDINGS:
            ClearSecurityFindings();
            return true;
        case IDM_SECURITY_DASHBOARD:
            ShowSecurityDashboard();
            return true;
        case IDM_SECURITY_EXPORT_SBOM:
            ExportSBOM();
            return true;
        case IDM_SECURITY_IMPORT_DAST:
            RunDastImport();
            return true;
        case IDM_SECURITY_EXPORT_REPORT:
            ExportSecurityReport();
            return true;
        case IDM_SECURITY_EXPORT_SARIF:
            ExportSecuritySarifReport();
            return true;
        case IDM_SECURITY_EXPORT_HTML:
            ExportSecurityHtmlReport();
            return true;
        case IDM_SECURITY_EXPORT_MARKDOWN:
            ExportSecurityMarkdownReport();
            return true;
        case IDM_SECURITY_EXPORT_CSV:
            ExportSecurityCsvReport();
            return true;
        default:
            return false;
    }
}

void Win32IDE::ExportSBOM() {
    SbomExport exporter;
    std::vector<SbomComponent> components;
    const size_t count = exporter.scanDirectory(".", components);

    OPENFILENAMEA ofn = {};
    char outFile[MAX_PATH] = "rawrxd_sbom.json";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = outFile;
    ofn.nMaxFile = sizeof(outFile);
    ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameA(&ofn)) {
        return;
    }

    if (!exporter.writeCycloneDx(outFile, components, "RawrXD-IDE", "1.0.0")) {
        appendToOutput("[Security] Failed to export SBOM.\n", "Output", OutputSeverity::Error);
        MessageBoxA(m_hwndMain, "Failed to export SBOM JSON.", "SBOM Export", MB_OK | MB_ICONERROR);
        return;
    }

    appendToOutput("[Security] SBOM exported (" + std::to_string(count) + " components): " + outFile + "\n",
                   "Output", OutputSeverity::Info);
}

void Win32IDE::RunDastImport() {
    OPENFILENAMEA ofn = {};
    char inFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = inFile;
    ofn.nMaxFile = sizeof(inFile);
    ofn.lpstrFilter = "DAST Reports\0*.sarif;*.json\0JSON Files\0*.json\0SARIF Files\0*.sarif\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn)) {
        return;
    }

    RawrXD::ProblemsAggregator::instance().clear("DAST");
    DastBridge bridge;
    std::vector<DastFinding> findings;
    if (!bridge.importReport(inFile, findings)) {
        appendToOutput("[Security] DAST import failed: unsupported or invalid report.\n",
                       "Output", OutputSeverity::Error);
        MessageBoxA(m_hwndMain, "Failed to parse DAST report. Supported: SARIF and ZAP JSON.", "DAST Import",
                    MB_OK | MB_ICONERROR);
        return;
    }

    bridge.reportToProblems(findings);
    refreshProblemsView();
    appendToOutput("[Security] DAST report imported: " + std::to_string(findings.size()) +
                   " findings from " + std::string(inFile) + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::RunSecurityFullScan() {
    appendToOutput("[Security] Running full security scan (SAST + SCA + Secrets)...\n",
                   "Output", OutputSeverity::Info);
    RunSecretsScan();
    RunSastScan();
    RunDependencyAudit();
    refreshProblemsView();
    appendToOutput("[Security] Full security scan complete.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ClearSecurityFindings() {
    ProblemsAggregator::instance().clear("SAST");
    ProblemsAggregator::instance().clear("SCA");
    ProblemsAggregator::instance().clear("Secrets");
    ProblemsAggregator::instance().clear("DAST");
    refreshProblemsView();
    appendToOutput("[Security] Cleared SAST/SCA/Secrets/DAST findings.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ExportSecurityReport() {
    const auto all = collectSecurityFindings();
    const auto sast = ProblemsAggregator::instance().getProblems("", "SAST");
    const auto sca = ProblemsAggregator::instance().getProblems("", "SCA");
    const auto secrets = ProblemsAggregator::instance().getProblems("", "Secrets");
    const auto dast = ProblemsAggregator::instance().getProblems("", "DAST");

    const std::string outFile = chooseSavePath(m_hwndMain, "Export Unified Security Report",
                                                "rawrxd_security_report.json",
                                                "JSON Files\0*.json\0All Files\0*.*\0", "json");
    if (outFile.empty()) return;
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendToOutput("[Security] Failed to open report output path.\n", "Output", OutputSeverity::Error);
        return;
    }

    std::time_t now = std::time(nullptr);
    out << "{\n";
    out << "  \"generatedAt\": " << static_cast<long long>(now) << ",\n";
    out << "  \"summary\": {\n";
    out << "    \"total\": " << all.size() << ",\n";
    out << "    \"sast\": " << sast.size() << ",\n";
    out << "    \"sca\": " << sca.size() << ",\n";
    out << "    \"secrets\": " << secrets.size() << ",\n";
    out << "    \"dast\": " << dast.size() << "\n";
    out << "  },\n";
    out << "  \"findings\": [\n";
    for (size_t i = 0; i < all.size(); ++i) {
        const auto& p = all[i];
        out << "    {";
        out << "\"source\":\"" << escapeJson(p.source) << "\",";
        out << "\"severity\":" << p.severity << ",";
        out << "\"code\":\"" << escapeJson(p.code) << "\",";
        out << "\"message\":\"" << escapeJson(p.message) << "\",";
        out << "\"path\":\"" << escapeJson(p.path) << "\",";
        out << "\"line\":" << p.line << ",";
        out << "\"column\":" << p.column;
        out << "}";
        if (i + 1 < all.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    appendToOutput("[Security] Unified security report exported: " + outFile + "\n",
                   "Output", OutputSeverity::Info);
}

void Win32IDE::ExportSecuritySarifReport() {
    const auto all = collectSecurityFindings();
    const std::string outFile = chooseSavePath(m_hwndMain, "Export Security SARIF",
                                                "rawrxd_security.sarif",
                                                "SARIF Files\0*.sarif\0All Files\0*.*\0", "sarif");
    if (outFile.empty()) return;
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendToOutput("[Security] Failed to open SARIF output path.\n", "Output", OutputSeverity::Error);
        return;
    }

    out << "{\n";
    out << "  \"version\": \"2.1.0\",\n";
    out << "  \"$schema\": \"https://json.schemastore.org/sarif-2.1.0.json\",\n";
    out << "  \"runs\": [{\n";
    out << "    \"tool\": {\"driver\": {\"name\": \"RawrXD Security Aggregator\"}},\n";
    out << "    \"results\": [\n";
    for (size_t i = 0; i < all.size(); ++i) {
        const auto& p = all[i];
        out << "      {\n";
        out << "        \"ruleId\": \"" << escapeJson(p.ruleId.empty() ? p.code : p.ruleId) << "\",\n";
        out << "        \"level\": \"" << (p.severity == 1 ? "error" : (p.severity == 2 ? "warning" : "note")) << "\",\n";
        out << "        \"message\": {\"text\": \"" << escapeJson(p.message) << "\"}";
        if (!p.path.empty()) {
            out << ",\n        \"locations\": [{\"physicalLocation\": {\"artifactLocation\": {\"uri\": \"" << escapeJson(p.path)
                << "\"}, \"region\": {\"startLine\": " << (p.line > 0 ? p.line : 1)
                << ", \"startColumn\": " << (p.column > 0 ? p.column : 1) << "}}}]";
        }
        out << "\n      }";
        if (i + 1 < all.size()) out << ",";
        out << "\n";
    }
    out << "    ]\n";
    out << "  }]\n";
    out << "}\n";

    appendToOutput("[Security] SARIF report exported: " + outFile + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ExportSecurityHtmlReport() {
    const auto all = collectSecurityFindings();
    const std::string outFile = chooseSavePath(m_hwndMain, "Export Security HTML",
                                                "rawrxd_security_report.html",
                                                "HTML Files\0*.html\0All Files\0*.*\0", "html");
    if (outFile.empty()) return;
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendToOutput("[Security] Failed to open HTML output path.\n", "Output", OutputSeverity::Error);
        return;
    }

    out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>RawrXD Security Report</title>";
    out << "<style>body{font-family:Segoe UI,Arial,sans-serif;padding:16px;}table{border-collapse:collapse;width:100%;}"
           "th,td{border:1px solid #ddd;padding:6px;font-size:13px;}th{background:#f3f3f3;text-align:left;}"
           ".sev1{color:#b00020;font-weight:700}.sev2{color:#a05a00}.sev3{color:#005a9e}.sev4{color:#666}</style></head><body>";
    out << "<h2>RawrXD Unified Security Report</h2>";
    out << "<p>Total findings: " << all.size() << "</p>";
    out << "<table><thead><tr><th>Source</th><th>Severity</th><th>Code</th><th>Message</th><th>Path</th><th>Line</th></tr></thead><tbody>";
    for (const auto& p : all) {
        out << "<tr>";
        out << "<td>" << escapeHtml(p.source) << "</td>";
        out << "<td class=\"sev" << p.severity << "\">" << p.severity << "</td>";
        out << "<td>" << escapeHtml(p.code) << "</td>";
        out << "<td>" << escapeHtml(p.message) << "</td>";
        out << "<td>" << escapeHtml(p.path) << "</td>";
        out << "<td>" << p.line << "</td>";
        out << "</tr>";
    }
    out << "</tbody></table></body></html>";

    appendToOutput("[Security] HTML report exported: " + outFile + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ExportSecurityMarkdownReport() {
    const auto all = collectSecurityFindings();
    const std::string outFile = chooseSavePath(m_hwndMain, "Export Security Markdown",
                                               "rawrxd_security_report.md",
                                               "Markdown Files\0*.md\0All Files\0*.*\0", "md");
    if (outFile.empty()) return;
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendToOutput("[Security] Failed to open Markdown output path.\n", "Output", OutputSeverity::Error);
        return;
    }

    out << "# RawrXD Unified Security Report\n\n";
    out << "Total findings: **" << all.size() << "**\n\n";
    out << "| Source | Sev | Code | Message | Path | Line |\n";
    out << "|---|---:|---|---|---|---:|\n";
    for (const auto& p : all) {
        out << "| " << p.source
            << " | " << p.severity
            << " | " << p.code
            << " | " << p.message
            << " | " << p.path
            << " | " << p.line << " |\n";
    }

    appendToOutput("[Security] Markdown report exported: " + outFile + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ExportSecurityCsvReport() {
    const auto all = collectSecurityFindings();
    const std::string outFile = chooseSavePath(m_hwndMain, "Export Security CSV",
                                               "rawrxd_security_report.csv",
                                               "CSV Files\0*.csv\0All Files\0*.*\0", "csv");
    if (outFile.empty()) return;
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        appendToOutput("[Security] Failed to open CSV output path.\n", "Output", OutputSeverity::Error);
        return;
    }

    out << "source,severity,code,ruleId,message,path,line,column\n";
    for (const auto& p : all) {
        out << csvEscape(p.source) << ","
            << p.severity << ","
            << csvEscape(p.code) << ","
            << csvEscape(p.ruleId) << ","
            << csvEscape(p.message) << ","
            << csvEscape(p.path) << ","
            << p.line << ","
            << p.column << "\n";
    }

    appendToOutput("[Security] CSV report exported: " + outFile + "\n", "Output", OutputSeverity::Info);
}
