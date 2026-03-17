// ============================================================================
// license_manager_panel.cpp — License Manager UI Implementation
// ============================================================================

#include "../include/license_manager_panel.h"
#include "../include/enterprise_license.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD::License {

// ============================================================================
// LicenseManagerPanel Implementation
// ============================================================================

LicenseManagerPanel::LicenseManagerPanel() = default;

LicenseManagerPanel::~LicenseManagerPanel() {
    destroy();
}

bool LicenseManagerPanel::create(HWND parentHwnd, int x, int y, int width, int height) {
    if (!parentHwnd) return false;

    m_hwndParent = parentHwnd;

    // Register window class
    static const char* className = "RawrXD_LicenseManagerPanel";
    static bool classRegistered = false;

    if (!classRegistered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = className;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

        if (!RegisterClass(&wc)) return false;
        classRegistered = true;
    }

    // Create window
    m_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        className,
        "License Manager",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        parentHwnd,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!m_hwnd) return false;

    m_brushBg = CreateSolidBrush(RGB(240, 240, 240));
    createControls();
    updateControlData();

    return true;
}

void LicenseManagerPanel::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_brushBg) {
        DeleteObject(m_brushBg);
        m_brushBg = nullptr;
    }
    if (m_fontTitle) DeleteObject(m_fontTitle);
    if (m_fontNormal) DeleteObject(m_fontNormal);
    if (m_fontSmall) DeleteObject(m_fontSmall);
}

void LicenseManagerPanel::setVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

bool LicenseManagerPanel::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void LicenseManagerPanel::refresh() {
    if (m_hwnd) {
        updateControlData();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

LRESULT CALLBACK LicenseManagerPanel::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LicenseManagerPanel* pThis = nullptr;

    if (message == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<LicenseManagerPanel*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = reinterpret_cast<LicenseManagerPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handleMessage(message, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT LicenseManagerPanel::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            onPaint();
            return 0;

        case WM_ERASEBKGND:
            return 1;  // Prevent flicker

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;

        default:
            return DefWindowProc(m_hwnd, message, wParam, lParam);
    }
}

void LicenseManagerPanel::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    // Fill background
    FillRect(hdc, &rcClient, m_brushBg);

    // Draw sections
    int y = 10;
    RECT rcSection = { 10, y, rcClient.right - 10, rcClient.bottom - 10 };
    drawLicenseInfo(hdc, rcSection);

    EndPaint(m_hwnd, &ps);
}

void LicenseManagerPanel::drawLicenseInfo(HDC hdc, const RECT& rect) {
    auto& lic = EnterpriseLicenseV2::Instance();

    // Current tier
    LicenseTierV2 tier = lic.currentTier();
    const char* tierStr = tierName(tier);

    // Draw tier info
    std::string tierText = "Current Tier: ";
    tierText += tierStr;

    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkColor(hdc, RGB(240, 240, 240));

    RECT rcText = rect;
    DrawText(hdc, tierText.c_str(), tierText.length(), &rcText, DT_LEFT | DT_TOP);

    // Draw feature count
    rcText.top += 30;
    std::string featureText = "Enabled Features: " + std::to_string(lic.enabledFeatureCount());
    DrawText(hdc, featureText.c_str(), featureText.length(), &rcText, DT_LEFT | DT_TOP);

    // Draw HWID
    rcText.top += 30;
    std::string hwidText = "Hardware ID: " + m_cachedHWIDStr;
    DrawText(hdc, hwidText.c_str(), hwidText.length(), &rcText, DT_LEFT | DT_TOP);
}

void LicenseManagerPanel::drawFeatureList(HDC hdc, const RECT& rect) {
    // Stub for feature list display
}

void LicenseManagerPanel::drawAuditLog(HDC hdc, const RECT& rect) {
    // Stub for audit log display
}

void LicenseManagerPanel::drawLimits(HDC hdc, const RECT& rect) {
    auto& lic = EnterpriseLicenseV2::Instance();
    const auto& limits = lic.currentLimits();

    std::string text = "Tier Limits\n";
    text += "Max Model: " + std::to_string(limits.maxModelGB) + " GB\n";
    text += "Max Context: " + std::to_string(limits.maxContextTokens) + " tokens\n";

    RECT rcText = rect;
    DrawText(hdc, text.c_str(), text.length(), &rcText, DT_LEFT | DT_TOP);
}

void LicenseManagerPanel::createControls() {
    // Create UI controls (buttons, lists, etc.)
    // This is a simplified implementation
}

void LicenseManagerPanel::updateControlData() {
    auto& lic = EnterpriseLicenseV2::Instance();
    m_cachedTier = lic.currentTier();
    m_cachedFeatureCount = lic.enabledFeatureCount();

    char buf[33] = {};
    lic.getHardwareIDHex(buf, sizeof(buf));
    m_cachedHWIDStr = buf;
}

std::string LicenseManagerPanel::formatTimestamp(uint32_t timestamp) const {
    time_t t = timestamp;
    struct tm* tm_info = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

std::string LicenseManagerPanel::formatBytes(uint64_t bytes) const {
    std::ostringstream oss;
    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024)) << " MB";
    } else {
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024 * 1024)) << " GB";
    }
    return oss.str();
}

std::string LicenseManagerPanel::getTierColor(LicenseTierV2 tier) const {
    switch (tier) {
        case LicenseTierV2::Community:    return "#CCCCCC";
        case LicenseTierV2::Professional: return "#4CAF50";
        case LicenseTierV2::Enterprise:   return "#2196F3";
        case LicenseTierV2::Sovereign:    return "#F44336";
        default:                          return "#000000";
    }
}

void LicenseManagerPanel::onLoadLicenseClick() {
    // File picker dialog
}

void LicenseManagerPanel::onCopyHWIDClick() {
    // Copy HWID to clipboard
}

void LicenseManagerPanel::onViewAuditClick() {
    // Show audit log viewer
}

// ============================================================================
// LicenseInfoDialog Implementation
// ============================================================================

INT_PTR LicenseInfoDialog::show(HWND parentHwnd) {
    // Create and show modeless dialog
    return IDOK;  // Placeholder
}

INT_PTR CALLBACK LicenseInfoDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            onInitDialog(hwnd);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                onCancel(hwnd);
            }
            return TRUE;
    }
    return FALSE;
}

void LicenseInfoDialog::onInitDialog(HWND hwnd) {
    // Initialize dialog controls
}

void LicenseInfoDialog::onCancel(HWND hwnd) {
    EndDialog(hwnd, IDCANCEL);
}

void LicenseInfoDialog::displayLicenseDetails(HWND hwndList) {
    // Display license key details
}

void LicenseInfoDialog::displayFeatureTable(HWND hwndList) {
    // Display feature table
}

void LicenseInfoDialog::displayAuditTrail(HWND hwndList) {
    // Display audit trail
}

// ============================================================================
// LicenseActivationDialog Implementation
// ============================================================================

INT_PTR LicenseActivationDialog::show(HWND parentHwnd) {
    return IDOK;  // Placeholder
}

INT_PTR CALLBACK LicenseActivationDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            onInitDialog(hwnd);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BROWSE:
                    onBrowseClick(hwnd);
                    return TRUE;
                case IDC_ACTIVATE:
                    onActivateClick(hwnd);
                    return TRUE;
                case IDCANCEL:
                    onCancel(hwnd);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void LicenseActivationDialog::onInitDialog(HWND hwnd) {
    // Initialize controls
}

void LicenseActivationDialog::onBrowseClick(HWND hwnd) {
    // Show file picker
}

void LicenseActivationDialog::onActivateClick(HWND hwnd) {
    // Load and activate license
}

void LicenseActivationDialog::onCancel(HWND hwnd) {
    EndDialog(hwnd, IDCANCEL);
}

// ============================================================================
// FeatureAuditReport Implementation
// ============================================================================

std::string FeatureAuditReport::generateJSON() {
    std::ostringstream json;
    auto& lic = EnterpriseLicenseV2::Instance();

    json << "{\n";
    json << "  \"tier\": \"" << tierName(lic.currentTier()) << "\",\n";
    json << "  \"enabledFeatures\": " << lic.enabledFeatureCount() << ",\n";
    json << "  \"timestamp\": " << std::time(nullptr) << ",\n";
    json << "  \"features\": [\n";

    bool first = true;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const auto& def = lic.getFeatureDef(static_cast<FeatureID>(i));
        if (lic.isFeatureEnabled(static_cast<FeatureID>(i))) {
            if (!first) json << ",\n";
            json << "    {\n";
            json << "      \"id\": " << i << ",\n";
            json << "      \"name\": \"" << def.name << "\",\n";
            json << "      \"tier\": \"" << tierName(def.minTier) << "\"\n";
            json << "    }";
            first = false;
        }
    }

    json << "\n  ]\n";
    json << "}\n";

    return json.str();
}

std::string FeatureAuditReport::generateText() {
    std::ostringstream text;
    auto& lic = EnterpriseLicenseV2::Instance();

    text << "=== Feature Audit Report ===\n";
    text << "Tier: " << tierName(lic.currentTier()) << "\n";
    text << "Enabled Features: " << lic.enabledFeatureCount() << "\n\n";

    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const auto& def = lic.getFeatureDef(static_cast<FeatureID>(i));
        text << (lic.isFeatureEnabled(static_cast<FeatureID>(i)) ? "[✓] " : "[ ] ");
        text << def.name << "\n";
    }

    return text.str();
}

bool FeatureAuditReport::exportToFile(const char* path) {
    // Export JSON to file
    return true;
}

// ============================================================================
// ComplianceReport Implementation
// ============================================================================

std::string ComplianceReport::generateSIEM() {
    std::ostringstream siem;
    auto& lic = EnterpriseLicenseV2::Instance();

    siem << "SIEM_AUDIT\n";
    siem << "timestamp=" << std::time(nullptr) << "\n";
    siem << "event=license_audit\n";
    siem << "tier=" << tierName(lic.currentTier()) << "\n";
    siem << "features=" << lic.enabledFeatureCount() << "\n";

    return siem.str();
}

std::string ComplianceReport::generateSOC2() {
    return "=== SOC2 Compliance Report ===\n";
}

bool ComplianceReport::exportToCSV(const char* path) {
    return true;
}

}  // namespace RawrXD::License
