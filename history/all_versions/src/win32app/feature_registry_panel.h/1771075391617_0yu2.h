// feature_registry_panel.h - Enterprise Feature Registry Display Panel
// Win32 native panel showing all features, their tier, lock status, and implementation state
// Integrates with EnterpriseLicenseV2 for real-time gating display

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "../../include/enterprise_license.h"

using namespace RawrXD::License;

// ============================================================================
// Feature Display Item — UI-ready version of FeatureDefV2
// ============================================================================
struct FeatureDisplayItem {
    FeatureID featureId;
    std::string name;
    std::string description;
    std::string tierName;
    std::string sourceFile;
    LicenseTierV2 requiredTier;
    bool unlocked;
    bool implemented;
    bool wired;
    bool tested;
};

// ============================================================================
// Feature Registry Panel — Win32 Native
// ============================================================================
class FeatureRegistryPanel {
public:
    FeatureRegistryPanel();
    ~FeatureRegistryPanel();

    // Panel lifecycle
    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    void show();
    void hide();
    void resize(int x, int y, int width, int height);

    // Data refresh
    void refreshFeatures();
    void refreshLicenseStatus();

    // Filter modes
    enum class FilterMode {
        All,
        Unlocked,
        Locked,
        Implemented,
        Missing,
        Community,
        Professional,
        Enterprise,
        Sovereign
    };
    void setFilter(FilterMode mode);
    FilterMode getFilter() const { return m_filter; }

    // License activation from panel
    void showActivationDialog();
    void showLicenseCreatorDialog();

    // Rendering
    void paint(HDC hdc);

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Callbacks
    using FeatureSelectedCallback = std::function<void(FeatureID)>;
    void onFeatureSelected(FeatureSelectedCallback cb) { m_featureSelectedCb = cb; }

    // Console display (for non-GUI contexts)
    static void printFeatureTable();
    static void printLicenseStatus();
    static void printAuditReport();
    static void printEnforcementStatus();
    static void printGapsReport();
    static void printFullDashboard();

private:
    void buildDisplayList();
    void paintHeader(HDC hdc, RECT& area);
    void paintFeatureList(HDC hdc, RECT& area);
    void paintStatusBar(HDC hdc, RECT& area);
    void paintFeatureRow(HDC hdc, const FeatureDisplayItem& item, RECT& row, bool selected);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    std::vector<FeatureDisplayItem> m_displayItems;
    std::vector<FeatureDisplayItem> m_filteredItems;
    FilterMode m_filter = FilterMode::All;
    int m_scrollOffset = 0;
    int m_selectedIndex = -1;
    int m_rowHeight = 28;
    FeatureSelectedCallback m_featureSelectedCb;

    // Colors
    struct {
        COLORREF background     = RGB(30, 30, 30);
        COLORREF headerBg       = RGB(40, 40, 40);
        COLORREF rowBg          = RGB(35, 35, 35);
        COLORREF rowAltBg       = RGB(38, 38, 38);
        COLORREF selectedBg     = RGB(50, 80, 120);
        COLORREF textNormal     = RGB(220, 220, 220);
        COLORREF textDim        = RGB(140, 140, 140);
        COLORREF unlockedColor  = RGB(80, 200, 80);
        COLORREF lockedColor    = RGB(200, 80, 80);
        COLORREF implColor      = RGB(80, 160, 230);
        COLORREF missingColor   = RGB(200, 160, 40);
        COLORREF tierCommunity  = RGB(150, 150, 150);
        COLORREF tierPro        = RGB(80, 180, 230);
        COLORREF tierEnterprise = RGB(200, 160, 40);
        COLORREF tierSovereign  = RGB(200, 80, 200);
    } m_colors;

    static constexpr const wchar_t* PANEL_CLASS = L"RawrXD_FeatureRegistryPanel";
    static bool s_classRegistered;
};
