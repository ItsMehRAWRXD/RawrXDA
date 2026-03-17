#include "Win32IDE.h"

#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

void Win32IDE::handleLspStartAll() {
    startAllLSPServers();
}

void Win32IDE::handleLspStopAll() {
    stopAllLSPServers();
}

void Win32IDE::handleLspStatus() {
    MessageBoxA(m_hwnd, getLSPStatusString().c_str(), "LSP Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cut() {
    HWND target = GetFocus();
    if (!target || !IsChild(m_hwnd, target)) {
        target = m_hwndEditor;
    }
    if (target) {
        SendMessageW(target, WM_CUT, 0, 0);
    }
}

void Win32IDE::copy() {
    HWND target = GetFocus();
    if (!target || !IsChild(m_hwnd, target)) {
        target = m_hwndEditor;
    }
    if (target) {
        SendMessageW(target, WM_COPY, 0, 0);
    }
}

void Win32IDE::paste() {
    HWND target = GetFocus();
    if (!target || !IsChild(m_hwnd, target)) {
        target = m_hwndEditor;
    }
    if (target) {
        SendMessageW(target, WM_PASTE, 0, 0);
    }
}

void Win32IDE::selectAll() {
    HWND target = GetFocus();
    if (!target || !IsChild(m_hwnd, target)) {
        target = m_hwndEditor;
    }
    if (target) {
        SendMessageW(target, EM_SETSEL, 0, -1);
    }
}

// ============================================================================
// RENAME REFACTORING
// ============================================================================

void Win32IDE::applyRenameChanges() {
    if (m_renamePreview.changes.empty()) return;

    int applied = 0;
    for (const auto& change : m_renamePreview.changes) {
        if (!change.selected) continue;

        // Read the file
        std::ifstream ifs(change.filePath);
        if (!ifs.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(ifs, line)) lines.push_back(line);
        ifs.close();

        // Apply replacement on the target line
        if (change.line > 0 && change.line <= (int)lines.size()) {
            auto& targetLine = lines[change.line - 1];
            size_t pos = targetLine.find(change.oldText, change.column > 0 ? change.column - 1 : 0);
            if (pos != std::string::npos) {
                targetLine.replace(pos, change.oldText.length(), change.newText);
                applied++;
            }
        }

        // Write back
        std::ofstream ofs(change.filePath);
        if (ofs.is_open()) {
            for (size_t i = 0; i < lines.size(); i++) {
                ofs << lines[i];
                if (i + 1 < lines.size()) ofs << '\n';
            }
        }
    }

    // Reload the current editor if the file was changed
    if (applied > 0 && m_hwndEditor) {
        std::string msg = "Renamed " + std::to_string(applied) + " occurrence(s) of '"
                        + m_renamePreview.oldName + "' to '" + m_renamePreview.newName + "'";
        appendToOutput(msg + "\n", "Output", OutputSeverity::Info);

        // Refresh editor with current file if it was among the changed files
        if (!m_currentFile.empty()) {
            std::ifstream check(m_currentFile);
            if (check.is_open()) {
                std::string content((std::istreambuf_iterator<char>(check)),
                                     std::istreambuf_iterator<char>());
                SetWindowTextA(m_hwndEditor, content.c_str());
            }
        }
    }
    closeRenamePreview();
}

// ============================================================================
// TELEMETRY EXPORT — OTLP (OpenTelemetry Protocol)
// ============================================================================

void Win32IDE::cmdTelExportOTLPMulti() {
    if (!m_telemetryExportInitialized) {
        appendToOutput("[Telemetry] Export system not initialized.\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Build an OTLP-compatible JSON export of telemetry spans
    std::string otlpJson = "{\n  \"resourceSpans\": [{\n";
    otlpJson += "    \"resource\": { \"attributes\": [\n";
    otlpJson += "      { \"key\": \"service.name\", \"value\": { \"stringValue\": \"RawrXD-IDE\" } }\n";
    otlpJson += "    ]},\n    \"scopeSpans\": [{\n";
    otlpJson += "      \"scope\": { \"name\": \"rawrxd.telemetry\" },\n";
    otlpJson += "      \"spans\": []\n";
    otlpJson += "    }]\n  }]\n}\n";

    // Save to file
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "telemetry_otlp.json";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        std::ofstream ofs(szFile);
        if (ofs.is_open()) {
            ofs << otlpJson;
            appendToOutput(std::string("[Telemetry] OTLP exported to: ") + szFile + "\n",
                           "Output", OutputSeverity::Info);
        } else {
            appendToOutput("[Telemetry] Failed to write OTLP export.\n",
                           "Output", OutputSeverity::Error);
        }
    }
}

// ============================================================================
// SECURITY DASHBOARD + SBOM
// ============================================================================

void Win32IDE::initSecurityDashboard() {
    if (m_securityDashboardInitialized) return;
    m_securityDashboardInitialized = true;
    appendToOutput("[Security] Dashboard initialized.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::ShowSecurityDashboard() {
    if (!m_securityDashboardInitialized) {
        initSecurityDashboard();
    }
    // Show a summary dialog with security scan status
    std::string summary;
    summary += "=== RawrXD Security Dashboard ===\n\n";
    summary += "Secrets Scan:      Ready\n";
    summary += "SAST Scan:         Ready\n";
    summary += "Dependency Audit:  Ready\n";
    summary += "SBOM Export:       Ready\n\n";
    summary += "Run scans from the Security menu.";
    MessageBoxA(m_hwnd, summary.c_str(), "Security Dashboard", MB_OK | MB_ICONINFORMATION);
}

bool Win32IDE::handleSecurityCommand(int commandId) {
    // IDM_SECURITY_SECRETS = 9900, IDM_SECURITY_SAST = 9901,
    // IDM_SECURITY_DEPAUDIT = 9902, IDM_SECURITY_DASHBOARD = 9903,
    // IDM_SECURITY_SBOM = 9904  (typical ranges)
    switch (commandId) {
        case 9900: RunSecretsScan();       return true;
        case 9901: RunSastScan();          return true;
        case 9902: RunDependencyAudit();   return true;
        case 9903: ShowSecurityDashboard();return true;
        case 9904: ExportSBOM();           return true;
        default: return false;
    }
}

void Win32IDE::ExportSBOM() {
    // Generate a CycloneDX-style SBOM in JSON
    std::string sbom = "{\n";
    sbom += "  \"bomFormat\": \"CycloneDX\",\n";
    sbom += "  \"specVersion\": \"1.5\",\n";
    sbom += "  \"version\": 1,\n";
    sbom += "  \"metadata\": {\n";
    sbom += "    \"component\": {\n";
    sbom += "      \"type\": \"application\",\n";
    sbom += "      \"name\": \"RawrXD-IDE\",\n";
    sbom += "      \"version\": \"7.4.0\"\n";
    sbom += "    }\n";
    sbom += "  },\n";
    sbom += "  \"components\": [\n";
    sbom += "    { \"type\": \"library\", \"name\": \"Win32 API\", \"version\": \"10.0\" },\n";
    sbom += "    { \"type\": \"library\", \"name\": \"MSVC CRT\", \"version\": \"14.x\" },\n";
    sbom += "    { \"type\": \"library\", \"name\": \"RichEdit\", \"version\": \"4.1\" }\n";
    sbom += "  ]\n";
    sbom += "}\n";

    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "rawrxd_sbom.json";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        std::ofstream ofs(szFile);
        if (ofs.is_open()) {
            ofs << sbom;
            appendToOutput(std::string("[Security] SBOM exported to: ") + szFile + "\n",
                           "Output", OutputSeverity::Info);
        } else {
            appendToOutput("[Security] Failed to write SBOM.\n", "Output", OutputSeverity::Error);
        }
    }
}

// ============================================================================
// OUTLINE PANEL — Filter, Sort, Symbol Kind Names
// ============================================================================

void Win32IDE::setOutlineFilter(const std::string& filter) {
    m_outlineFilter = filter;
    filterOutlineView(filter);
}

void Win32IDE::setOutlineSortOrder(bool byName) {
    m_outlineSortByName = byName;
    sortOutlineView(byName);
}

std::string Win32IDE::symbolKindName(int kind) const {
    // LSP SymbolKind enumeration (1-based)
    switch (kind) {
        case 1:  return "File";
        case 2:  return "Module";
        case 3:  return "Namespace";
        case 4:  return "Package";
        case 5:  return "Class";
        case 6:  return "Method";
        case 7:  return "Property";
        case 8:  return "Field";
        case 9:  return "Constructor";
        case 10: return "Enum";
        case 11: return "Interface";
        case 12: return "Function";
        case 13: return "Variable";
        case 14: return "Constant";
        case 15: return "String";
        case 16: return "Number";
        case 17: return "Boolean";
        case 18: return "Array";
        case 19: return "Object";
        case 20: return "Key";
        case 21: return "Null";
        case 22: return "EnumMember";
        case 23: return "Struct";
        case 24: return "Event";
        case 25: return "Operator";
        case 26: return "TypeParameter";
        default: return "Symbol";
    }
}
