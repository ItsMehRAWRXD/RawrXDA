// ============================================================================
// feature_registry_panel.cpp — Win32 Enterprise Feature Matrix Display Panel
// ============================================================================
// Implementation of the feature registry panel Win32 UI component.
// Renders a scrollable grid showing all 55+ features with tier,
// implementation status, UI wiring, and license state.
//
// PATTERN:   No exceptions. Pure Win32 + GDI.
// THREADING: UI thread only.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "feature_registry_panel.h"
#include "enterprise_license.h"

#include <cstdio>
#include <cstring>

using namespace RawrXD::License;

// ============================================================================
// Window Class Registration Name
// ============================================================================
#ifdef _WIN32
static const char* const WC_FEATURE_PANEL = "RawrXD_FeatureRegistryPanel";
static bool s_classRegistered = false;
#endif

// ============================================================================
// Constructor / Destructor
// ============================================================================
FeatureRegistryPanel::FeatureRegistryPanel() = default;

FeatureRegistryPanel::~FeatureRegistryPanel() {
#ifdef _WIN32
    destroy();
#endif
}

// ============================================================================
// Win32 Lifecycle
// ============================================================================
#ifdef _WIN32

bool FeatureRegistryPanel::create(HWND hParent, int x, int y, int w, int h) {
    if (m_hwnd) return true; // Already created

    m_hParent = hParent;

    // Register window class once
    if (!s_classRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = WC_FEATURE_PANEL;
        wc.cbWndExtra = sizeof(void*);

        if (!RegisterClassExA(&wc)) return false;
        s_classRegistered = true;
    }

    // Create fonts
    m_hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          ANSI_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH,
                          "Consolas");
    m_hBoldFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              ANSI_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH,
                              "Consolas");

    // Create window
    m_hwnd = CreateWindowExA(
        0, WC_FEATURE_PANEL, "Enterprise Feature Registry",
        WS_CHILD | WS_VSCROLL | WS_BORDER,
        x, y, w, h,
        hParent, nullptr, GetModuleHandleA(nullptr), this);

    if (!m_hwnd) return false;

    m_visible = false;
    refresh();
    return true;
}

void FeatureRegistryPanel::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hBoldFont) {
        DeleteObject(m_hBoldFont);
        m_hBoldFont = nullptr;
    }
}

void FeatureRegistryPanel::resize(int x, int y, int w, int h) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, w, h, TRUE);
    }
}

// ============================================================================
// WndProc
// ============================================================================
LRESULT CALLBACK FeatureRegistryPanel::WndProc(HWND hwnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam) {
    FeatureRegistryPanel* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = static_cast<FeatureRegistryPanel*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, 0, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<FeatureRegistryPanel*>(GetWindowLongPtrA(hwnd, 0));
    }

    switch (msg) {
    case WM_PAINT: {
        if (!self) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        self->onPaint(hdc, ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_VSCROLL: {
        if (!self) break;
        SCROLLINFO si = { sizeof(si), SIF_ALL };
        GetScrollInfo(hwnd, SB_VERT, &si);
        int oldPos = si.nPos;

        switch (LOWORD(wParam)) {
            case SB_LINEUP:    si.nPos -= 20; break;
            case SB_LINEDOWN:  si.nPos += 20; break;
            case SB_PAGEUP:    si.nPos -= si.nPage; break;
            case SB_PAGEDOWN:  si.nPos += si.nPage; break;
            case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
        }

        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);

        if (si.nPos != oldPos) {
            self->m_scrollY = si.nPos;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (!self) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        self->m_scrollY -= delta / 4;
        if (self->m_scrollY < 0) self->m_scrollY = 0;

        SCROLLINFO si = { sizeof(si), SIF_POS };
        si.nPos = self->m_scrollY;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Paint — Feature Grid
// ============================================================================
void FeatureRegistryPanel::onPaint(HDC hdc, const RECT& rc) {
    (void)rc;

    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);

    int y = 10 - m_scrollY;
    int colWidths[] = { 30, 220, 100, 50, 50, 50 };
    int numCols = 6;

    // Title
    SelectObject(hdc, m_hBoldFont);
    SetTextColor(hdc, RGB(0, 200, 255));
    TextOutA(hdc, 10, y, "Enterprise License V2 — Feature Registry", 42);
    y += 24;

    // Tier summary
    auto& lic = EnterpriseLicenseV2::Instance();
    char summary[256];
    snprintf(summary, sizeof(summary),
             "Tier: %s | Enabled: %u/%u | Impl: %u | UI: %u",
             tierName(lic.currentTier()),
             lic.enabledFeatureCount(), TOTAL_FEATURES,
             lic.countImplemented(), lic.countWiredToUI());
    SetTextColor(hdc, RGB(200, 200, 200));
    SelectObject(hdc, m_hFont);
    TextOutA(hdc, 10, y, summary, (int)strlen(summary));
    y += 24;

    // Column headers
    SelectObject(hdc, m_hBoldFont);
    SetTextColor(hdc, RGB(180, 180, 180));
    int cx = 10;
    const char* headers[] = { "ID", "Feature Name", "Min Tier", "Impl", "UI", "Lic" };
    for (int c = 0; c < numCols; ++c) {
        TextOutA(hdc, cx, y, headers[c], (int)strlen(headers[c]));
        cx += colWidths[c];
    }
    y += 20;

    // Separator
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, 10, y, nullptr);
    LineTo(hdc, clientRect.right - 10, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    y += 4;

    // Feature rows grouped by tier
    SelectObject(hdc, m_hFont);
    m_displayedCount = 0;

    struct TierGroup {
        LicenseTierV2 tier;
        const char* name;
        COLORREF color;
    };

    TierGroup groups[] = {
        { LicenseTierV2::Community,    "Community (Free)",   RGB(100, 200, 100) },
        { LicenseTierV2::Professional, "Professional",       RGB(100, 150, 255) },
        { LicenseTierV2::Enterprise,   "Enterprise",         RGB(255, 180, 50)  },
        { LicenseTierV2::Sovereign,    "Sovereign",          RGB(255, 80, 80)   },
    };

    for (auto& group : groups) {
        if (m_tierFilter < 4 &&
            static_cast<uint32_t>(group.tier) != m_tierFilter) continue;

        // Tier header
        paintTierHeader(hdc, y, group.name, group.color);
        y += 22;

        for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
            if (g_FeatureManifest[i].minTier != group.tier) continue;
            if (y > clientRect.bottom + m_scrollY) break;

            if (y >= -20) { // Only paint visible rows
                paintFeatureRow(hdc, y, i, colWidths, numCols);
            }
            y += 18;
            m_displayedCount++;
        }

        y += 8;
    }

    // Update scroll range
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = y + m_scrollY;
    si.nPage = clientRect.bottom - clientRect.top;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    SelectObject(hdc, oldFont);
}

void FeatureRegistryPanel::paintTierHeader(HDC hdc, int y,
                                            const char* name, COLORREF color) {
    SelectObject(hdc, m_hBoldFont);
    SetTextColor(hdc, color);
    TextOutA(hdc, 10, y, name, (int)strlen(name));
    SelectObject(hdc, m_hFont);
}

void FeatureRegistryPanel::paintFeatureRow(HDC hdc, int y, uint32_t featureIdx,
                                            int colWidths[], int numCols) {
    (void)numCols;
    const FeatureDefV2& f = g_FeatureManifest[featureIdx];
    auto& lic = EnterpriseLicenseV2::Instance();
    bool licensed = lic.isFeatureLicensed(f.id);

    int cx = 10;
    char buf[16];

    // ID column
    snprintf(buf, sizeof(buf), "%u", featureIdx);
    SetTextColor(hdc, RGB(120, 120, 120));
    TextOutA(hdc, cx, y, buf, (int)strlen(buf));
    cx += colWidths[0];

    // Name
    SetTextColor(hdc, f.implemented ? RGB(220, 220, 220) : RGB(100, 100, 100));
    TextOutA(hdc, cx, y, f.name, (int)strlen(f.name));
    cx += colWidths[1];

    // Tier
    SetTextColor(hdc, RGB(160, 160, 160));
    TextOutA(hdc, cx, y, tierName(f.minTier), (int)strlen(tierName(f.minTier)));
    cx += colWidths[2];

    // Implemented
    const char* implStr = f.implemented ? "[Y]" : "[ ]";
    SetTextColor(hdc, f.implemented ? RGB(100, 200, 100) : RGB(200, 80, 80));
    TextOutA(hdc, cx, y, implStr, 3);
    cx += colWidths[3];

    // UI wired
    const char* uiStr = f.wiredToUI ? "[Y]" : "[ ]";
    SetTextColor(hdc, f.wiredToUI ? RGB(100, 200, 100) : RGB(200, 80, 80));
    TextOutA(hdc, cx, y, uiStr, 3);
    cx += colWidths[4];

    // Licensed
    const char* licStr = licensed ? "[Y]" : "[ ]";
    SetTextColor(hdc, licensed ? RGB(100, 255, 100) : RGB(150, 150, 150));
    TextOutA(hdc, cx, y, licStr, 3);
}

#endif // _WIN32

// ============================================================================
// Content Refresh
// ============================================================================
void FeatureRegistryPanel::refresh() {
    m_displayedCount = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (m_tierFilter >= 4 ||
            static_cast<uint32_t>(g_FeatureManifest[i].minTier) == m_tierFilter) {
            m_displayedCount++;
        }
    }

#ifdef _WIN32
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
#endif
}

// ============================================================================
// Tier Filter
// ============================================================================
void FeatureRegistryPanel::setTierFilter(LicenseTierV2 tier) {
    m_tierFilter = static_cast<uint32_t>(tier);
    refresh();
}

// ============================================================================
// Text Report — for non-GUI / console output
// ============================================================================
int FeatureRegistryPanel::generateTextReport(char* buf, size_t bufLen) const {
    if (!buf || bufLen == 0) return 0;

    auto& lic = EnterpriseLicenseV2::Instance();
    int written = 0;

    written += snprintf(buf + written, bufLen - written,
        "=== Enterprise License V2 Feature Registry ===\n"
        "Tier: %s | Enabled: %u/%u | Impl: %u | UI: %u | Tested: %u\n\n",
        tierName(lic.currentTier()),
        lic.enabledFeatureCount(), TOTAL_FEATURES,
        lic.countImplemented(), lic.countWiredToUI(), lic.countTested());

    written += snprintf(buf + written, bufLen - written,
        "%-4s %-30s %-14s %-5s %-5s %-5s\n",
        "ID", "Feature", "Min Tier", "Impl", "UI", "Lic");
    written += snprintf(buf + written, bufLen - written,
        "%-4s %-30s %-14s %-5s %-5s %-5s\n",
        "----", "------------------------------", "--------------",
        "-----", "-----", "-----");

    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const FeatureDefV2& f = g_FeatureManifest[i];
        bool licensed = lic.isFeatureLicensed(f.id);

        if (static_cast<size_t>(written) >= bufLen - 100) break;

        written += snprintf(buf + written, bufLen - written,
            "%-4u %-30s %-14s %-5s %-5s %-5s\n",
            i, f.name, tierName(f.minTier),
            f.implemented ? "[Y]" : "[ ]",
            f.wiredToUI   ? "[Y]" : "[ ]",
            licensed      ? "[Y]" : "[ ]");
    }

    return written;
}

void FeatureRegistryPanel::printToConsole() const {
    char buf[8192];
    int len = generateTextReport(buf, sizeof(buf));
    if (len > 0) {
        fwrite(buf, 1, static_cast<size_t>(len), stdout);
    }
}
