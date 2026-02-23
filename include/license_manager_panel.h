// ============================================================================
// license_manager_panel.h — IDE License Management UI Panel
// ============================================================================
// Win32 native panel for license activation, status display, and feature audit.
// Shows active license tier, enabled features, expiry information, and audit log.
//
// Part of Phase 4: License Manager UI
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>

namespace RawrXD::License {

// ============================================================================
// License Manager Panel — Win32 Native UI
// ============================================================================
class LicenseManagerPanel {
public:
    // ── Lifecycle ──
    LicenseManagerPanel();
    ~LicenseManagerPanel();

    /// Create the panel window
    /// @param parentHwnd Parent window handle
    /// @param x, y, width, height Window position and size
    /// @return true if creation successful
    bool create(HWND parentHwnd, int x, int y, int width, int height);

    /// Destroy the panel window
    void destroy();

    /// Get the panel window handle
    HWND getHwnd() const { return m_hwnd; }

    /// Show/hide the panel
    void setVisible(bool visible);

    /// Refresh the display (call after license change)
    void refresh();

    // ── State ──
    bool isCreated() const { return m_hwnd != nullptr; }
    bool isVisible() const;

private:
    // ── Windows message handling ──
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // ── Rendering ──
    void onPaint();
    void drawLicenseInfo(HDC hdc, const RECT& rect);
    void drawFeatureList(HDC hdc, const RECT& rect);
    void drawAuditLog(HDC hdc, const RECT& rect);
    void drawLimits(HDC hdc, const RECT& rect);

    // ── UI Controls ──
    void createControls();
    void updateControlData();
    void onLoadLicenseClick();
    void onCopyHWIDClick();
    void onViewAuditClick();

    // ── Helper Methods ──
    std::string formatTimestamp(uint32_t timestamp) const;
    std::string formatBytes(uint64_t bytes) const;
    std::string getTierColor(LicenseTierV2 tier) const;

    // ── Members ──
    HWND        m_hwnd = nullptr;
    HWND        m_hwndParent = nullptr;
    HBRUSH      m_brushBg = nullptr;
    HFONT       m_fontTitle = nullptr;
    HFONT       m_fontNormal = nullptr;
    HFONT       m_fontSmall = nullptr;

    // UI State
    int         m_scrollOffset = 0;
    int         m_selectedTab = 0;  // 0=Info, 1=Features, 2=Audit, 3=Limits

    // Cached data
    LicenseTierV2    m_cachedTier;
    uint32_t         m_cachedFeatureCount = 0;
    std::string      m_cachedHWIDStr;
};

// ============================================================================
// License Info Dialog — Modal dialog for detailed license info
// ============================================================================
class LicenseInfoDialog {
public:
    /// Show the dialog
    /// @param parentHwnd Parent window
    /// @return IDOK or IDCANCEL
    static INT_PTR show(HWND parentHwnd);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Dialog initialization
    static void onInitDialog(HWND hwnd);
    static void onCancel(HWND hwnd);
    static void displayLicenseDetails(HWND hwndList);
    static void displayFeatureTable(HWND hwndList);
    static void displayAuditTrail(HWND hwndList);
};

// ============================================================================
// License Activation Dialog — Load and activate a new license
// ============================================================================
class LicenseActivationDialog {
public:
    /// Show the dialog
    /// @param parentHwnd Parent window
    /// @return IDOK (license loaded) or IDCANCEL
    static INT_PTR show(HWND parentHwnd);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    static void onInitDialog(HWND hwnd);
    static void onBrowseClick(HWND hwnd);
    static void onActivateClick(HWND hwnd);
    static void onCancel(HWND hwnd);
};

// ============================================================================
// Feature Audit Report — JSON export of feature status
// ============================================================================
class FeatureAuditReport {
public:
    /// Generate a JSON report of all features
    /// @return JSON string
    static std::string generateJSON();

    /// Generate a human-readable text report
    /// @return Formatted text
    static std::string generateText();

    /// Export to file
    /// @param path File path
    /// @return true if successful
    static bool exportToFile(const char* path);
};

// ============================================================================
// Compliance Report — For enterprise audit purposes
// ============================================================================
class ComplianceReport {
public:
    /// Generate SIEM-compatible audit log
    /// @return SIEM format string
    static std::string generateSIEM();

    /// Generate SOC2 compliance report
    /// @return Compliance report text
    static std::string generateSOC2();

    /// Export to CSV
    /// @param path File path
    /// @return true if successful
    static bool exportToCSV(const char* path);
};

}  // namespace RawrXD::License
