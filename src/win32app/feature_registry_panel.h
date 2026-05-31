// feature_registry_panel.h - Enterprise Feature Registry Display Panel
// Win32 native panel showing all features, their tier, lock status, and implementation state
// Integrates with EnterpriseLicenseV2 for real-time gating display
// VSU Effects: Uses Adobe RGBa color space for professional color accuracy

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "../../include/enterprise_license.h"
#include "../../include/RawrXD_ColorSpace.h"

using namespace RawrXD::License;
using namespace RawrXD::ColorSpace;

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
    void startLiveRefresh(int intervalMs = 1000);  // Refresh license status every N ms
    void stopLiveRefresh();

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

    /** Set callback to route print* output to IDE output panel. When set, used instead of std::cout. */
    static void setConsoleOutputCallback(std::function<void(const std::string&)> cb);

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
    
    // Live refresh timer
    UINT_PTR m_refreshTimerId = 0;
    static constexpr UINT_PTR REFRESH_TIMER_ID = 1001;

    // Colors - Adobe RGBa VSU Effects
    struct {
        AdobeRGBa background     = VSU::Acrylic::PanelTint;
        AdobeRGBa headerBg       = VSU::Acrylic::DarkBase;
        AdobeRGBa rowBg          = AdobeRGBa(0.14f, 0.14f, 0.14f, 1.00f);
        AdobeRGBa rowAltBg       = AdobeRGBa(0.15f, 0.15f, 0.15f, 1.00f);
        AdobeRGBa selectedBg     = VSU::Accents::BlueDark;
        AdobeRGBa textNormal     = AdobeRGBa(0.86f, 0.86f, 0.86f, 1.00f);
        AdobeRGBa textDim        = AdobeRGBa(0.55f, 0.55f, 0.55f, 1.00f);
        AdobeRGBa unlockedColor  = VSU::Accents::Success;
        AdobeRGBa lockedColor    = VSU::Accents::Error;
        AdobeRGBa implColor      = VSU::Accents::Blue;
        AdobeRGBa missingColor   = VSU::Accents::Warning;
        AdobeRGBa tierCommunity  = AdobeRGBa(0.59f, 0.59f, 0.59f, 1.00f);
        AdobeRGBa tierPro        = AdobeRGBa(0.31f, 0.71f, 0.90f, 1.00f);
        AdobeRGBa tierEnterprise = VSU::Accents::Warning;
        AdobeRGBa tierSovereign  = AdobeRGBa(0.78f, 0.31f, 0.78f, 1.00f);
    } m_colors;

    static constexpr const wchar_t* PANEL_CLASS = L"RawrXD_FeatureRegistryPanel";
    static bool s_classRegistered;
    static std::function<void(const std::string&)> s_consoleOutputCb;

    static void output(const std::string& s);  // Uses s_consoleOutputCb when set, else std::cout
};

inline void FeatureRegistryPanel::setConsoleOutputCallback(std::function<void(const std::string&)> cb) {
    s_consoleOutputCb = std::move(cb);
}
