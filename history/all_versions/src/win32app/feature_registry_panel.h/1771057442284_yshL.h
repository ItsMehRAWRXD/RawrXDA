// ============================================================================
// feature_registry_panel.h — Win32 Enterprise Feature Matrix Display Panel
// ============================================================================
// Embeddable Win32 panel that renders the 55+ feature license grid,
// tier comparison matrix, and enforcement status. Integrates into the
// Win32IDE sidebar as a "License" view.
//
// PATTERN:   No exceptions. No Qt. Pure Win32 + GDI.
// THREADING: Panel methods called from UI thread only.
// ============================================================================

#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdint>

// Forward declarations
namespace RawrXD::License {
    enum class LicenseTierV2 : uint32_t;
    enum class FeatureID : uint32_t;
}

// ============================================================================
// Feature Registry Panel — Win32 UI Component
// ============================================================================
class FeatureRegistryPanel {
public:
    FeatureRegistryPanel();
    ~FeatureRegistryPanel();

    // Non-copyable
    FeatureRegistryPanel(const FeatureRegistryPanel&) = delete;
    FeatureRegistryPanel& operator=(const FeatureRegistryPanel&) = delete;

    // ── Lifecycle ──
#ifdef _WIN32
    bool create(HWND hParent, int x, int y, int w, int h);
    void destroy();
    HWND hwnd() const { return m_hwnd; }
    void show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
    void hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }
    void resize(int x, int y, int w, int h);
#endif

    // ── Content ──
    void refresh();
    bool isVisible() const { return m_visible; }

    // ── Feature Display ──
    uint32_t getDisplayedFeatureCount() const { return m_displayedCount; }

    /// Set tier filter (COUNT = show all)
    void setTierFilter(RawrXD::License::LicenseTierV2 tier);

    /// Toggle implementation status display
    void showImplementationStatus(bool show) { m_showImplStatus = show; }

    /// Toggle audit trail display
    void showAuditTrail(bool show) { m_showAudit = show; }

    // ── Text Report (for non-GUI contexts) ──
    /// Generate formatted text report of the feature matrix
    /// Writes to buf (up to bufLen chars), returns chars written
    int generateTextReport(char* buf, size_t bufLen) const;

    /// Print feature matrix to stdout
    void printToConsole() const;

private:
#ifdef _WIN32
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);
    void onPaint(HDC hdc, const RECT& rc);
    void paintFeatureRow(HDC hdc, int y, uint32_t featureIdx,
                         int colWidths[], int numCols);
    void paintTierHeader(HDC hdc, int y, const char* tierName, COLORREF color);

    HWND    m_hwnd = nullptr;
    HWND    m_hParent = nullptr;
    HFONT   m_hFont = nullptr;
    HFONT   m_hBoldFont = nullptr;
    int     m_scrollY = 0;
#endif

    bool        m_visible = false;
    bool        m_showImplStatus = true;
    bool        m_showAudit = false;
    uint32_t    m_displayedCount = 0;
    uint32_t    m_tierFilter = 4; // 4 = COUNT = show all
};
