// feature_registry_panel.cpp - Enterprise Feature Registry Display Panel Implementation
// Win32 native rendering of feature matrix with license tier gating
// Uses V2 API: EnterpriseLicenseV2, FeatureFlagsRuntime, LicenseEnforcer

#include "feature_registry_panel.h"
#include "../../include/license_enforcement.h"
#include "../../include/feature_flags_runtime.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <commdlg.h>

using namespace RawrXD::License;
using namespace RawrXD::Flags;
using namespace RawrXD::Enforce;

bool FeatureRegistryPanel::s_classRegistered = false;

// ============================================================================
// Construction
// ============================================================================
FeatureRegistryPanel::FeatureRegistryPanel() = default;

FeatureRegistryPanel::~FeatureRegistryPanel() {
    destroy();
}

// ============================================================================
// Window Class Registration & Creation
// ============================================================================
bool FeatureRegistryPanel::create(HWND parent, int x, int y, int width, int height) {
    m_parent = parent;

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = PANEL_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(m_colors.background);
        if (!RegisterClassExW(&wc)) return false;
        s_classRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, PANEL_CLASS, L"Enterprise Features",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        x, y, width, height,
        parent, nullptr, GetModuleHandle(nullptr), this
    );

    if (!m_hwnd) return false;

    refreshFeatures();
    startLiveRefresh(500);  // Refresh license state every 500ms
    return true;
}

void FeatureRegistryPanel::destroy() {
    stopLiveRefresh();  // Clean up timer before destroying window
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void FeatureRegistryPanel::show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
void FeatureRegistryPanel::hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

void FeatureRegistryPanel::resize(int x, int y, int width, int height) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, width, height, TRUE);
}

// ============================================================================
// Data — uses V2 API: g_FeatureManifest + EnterpriseLicenseV2
// ============================================================================
void FeatureRegistryPanel::refreshFeatures() {
    auto& lic = EnterpriseLicenseV2::Instance();
    auto& flags = FeatureFlagsRuntime::Instance();

    m_displayItems.clear();
    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        FeatureDisplayItem item;
        item.featureId = def.id;
        item.name = def.name ? def.name : "";
        item.description = def.description ? def.description : "";
        item.tierName = tierName(def.minTier);
        item.sourceFile = def.sourceFile ? def.sourceFile : "";
        item.requiredTier = def.minTier;
        item.unlocked = flags.isEnabled(def.id);
        item.implemented = def.implemented;
        item.wired = def.wiredToUI;
        item.tested = def.tested;
        m_displayItems.push_back(item);
    }

    buildDisplayList();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FeatureRegistryPanel::refreshLicenseStatus() {
    auto& flags = FeatureFlagsRuntime::Instance();
    for (auto& item : m_displayItems) {
        item.unlocked = flags.isEnabled(item.featureId);
    }
    buildDisplayList();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FeatureRegistryPanel::startLiveRefresh(int intervalMs) {
    if (m_hwnd && !m_refreshTimerId) {
        m_refreshTimerId = SetTimer(m_hwnd, REFRESH_TIMER_ID, intervalMs, nullptr);
    }
}

void FeatureRegistryPanel::stopLiveRefresh() {
    if (m_hwnd && m_refreshTimerId) {
        KillTimer(m_hwnd, m_refreshTimerId);
        m_refreshTimerId = 0;
    }
}

void FeatureRegistryPanel::setFilter(FilterMode mode) {
    m_filter = mode;
    buildDisplayList();
    m_scrollOffset = 0;
    m_selectedIndex = -1;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FeatureRegistryPanel::buildDisplayList() {
    m_filteredItems.clear();
    for (const auto& item : m_displayItems) {
        bool pass = false;
        switch (m_filter) {
            case FilterMode::All:           pass = true; break;
            case FilterMode::Unlocked:      pass = item.unlocked; break;
            case FilterMode::Locked:        pass = !item.unlocked; break;
            case FilterMode::Implemented:   pass = item.implemented; break;
            case FilterMode::Missing:       pass = !item.implemented; break;
            case FilterMode::Community:     pass = item.requiredTier == LicenseTierV2::Community; break;
            case FilterMode::Professional:  pass = item.requiredTier == LicenseTierV2::Professional; break;
            case FilterMode::Enterprise:    pass = item.requiredTier == LicenseTierV2::Enterprise; break;
            case FilterMode::Sovereign:     pass = item.requiredTier == LicenseTierV2::Sovereign; break;
        }
        if (pass) m_filteredItems.push_back(item);
    }
}

// ============================================================================
// Painting
// ============================================================================
void FeatureRegistryPanel::paint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(m_colors.background);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    RECT area = clientRect;
    paintHeader(memDC, area);
    paintFeatureList(memDC, area);
    paintStatusBar(memDC, area);

    // Blit
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void FeatureRegistryPanel::paintHeader(HDC hdc, RECT& area) {
    auto& lic = EnterpriseLicenseV2::Instance();

    RECT headerRect = { area.left, area.top, area.right, area.top + 60 };
    HBRUSH hdrBrush = CreateSolidBrush(m_colors.headerBg);
    FillRect(hdc, &headerRect, hdrBrush);
    DeleteObject(hdrBrush);

    SetBkMode(hdc, TRANSPARENT);

    // Title
    HFONT titleFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, titleFont);
    SetTextColor(hdc, m_colors.textNormal);

    std::wstring title = L"RawrXD Enterprise Feature Registry";
    TextOutW(hdc, area.left + 12, area.top + 6, title.c_str(), (int)title.size());

    // License status line
    HFONT smallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, smallFont);

    const char* tn = tierName(lic.currentTier());
    uint32_t enabled = lic.enabledFeatureCount();
    std::wstringstream wss;
    wss << L"Tier: ";
    for (const char* p = tn; *p; ++p) wss << (wchar_t)*p;
    wss << L"  |  Enabled: " << enabled << L" / " << TOTAL_FEATURES;
    std::wstring wsummary = wss.str();

    COLORREF tierColor = m_colors.tierCommunity;
    switch (lic.currentTier()) {
        case LicenseTierV2::Professional: tierColor = m_colors.tierPro; break;
        case LicenseTierV2::Enterprise:   tierColor = m_colors.tierEnterprise; break;
        case LicenseTierV2::Sovereign:    tierColor = m_colors.tierSovereign; break;
        default: break;
    }
    SetTextColor(hdc, tierColor);
    TextOutW(hdc, area.left + 12, area.top + 30, wsummary.c_str(), (int)wsummary.size());

    // Feature count on right
    std::wstring countStr = L"Showing: " + std::to_wstring(m_filteredItems.size()) +
                            L" / " + std::to_wstring(m_displayItems.size());
    SetTextColor(hdc, m_colors.textDim);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, countStr.c_str(), (int)countStr.size(), &textSize);
    TextOutW(hdc, area.right - textSize.cx - 12, area.top + 30, countStr.c_str(), (int)countStr.size());

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(smallFont);

    // Column headers
    RECT colRect = { area.left, area.top + 48, area.right, area.top + 60 };
    HBRUSH colBrush = CreateSolidBrush(RGB(45, 45, 45));
    FillRect(hdc, &colRect, colBrush);
    DeleteObject(colBrush);

    HFONT colFont = CreateFontW(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, colFont);
    SetTextColor(hdc, m_colors.textDim);

    TextOutW(hdc, area.left + 12,  colRect.top, L"STATUS", 6);
    TextOutW(hdc, area.left + 90,  colRect.top, L"FEATURE", 7);
    TextOutW(hdc, area.left + 380, colRect.top, L"TIER", 4);
    TextOutW(hdc, area.left + 470, colRect.top, L"IMPL", 4);
    TextOutW(hdc, area.left + 520, colRect.top, L"WIRED", 5);
    TextOutW(hdc, area.left + 575, colRect.top, L"TEST", 4);

    SelectObject(hdc, oldFont);
    DeleteObject(colFont);

    area.top += 60;
}

void FeatureRegistryPanel::paintFeatureList(HDC hdc, RECT& area) {
    RECT listArea = { area.left, area.top, area.right, area.bottom - 30 };

    HFONT rowFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
    HGDIOBJ oldFont = SelectObject(hdc, rowFont);

    int y = listArea.top - m_scrollOffset;
    for (int i = 0; i < (int)m_filteredItems.size(); i++) {
        if (y + m_rowHeight < listArea.top) { y += m_rowHeight; continue; }
        if (y > listArea.bottom) break;

        RECT rowRect = { listArea.left, y, listArea.right, y + m_rowHeight };
        paintFeatureRow(hdc, m_filteredItems[i], rowRect, i == m_selectedIndex);
        y += m_rowHeight;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(rowFont);

    area.bottom -= 30; // Reserve for status bar
}

void FeatureRegistryPanel::paintFeatureRow(HDC hdc, const FeatureDisplayItem& item,
                                            RECT& row, bool selected) {
    // Row background
    COLORREF bgColor = selected ? m_colors.selectedBg :
                       ((row.top / m_rowHeight) % 2 == 0 ? m_colors.rowBg : m_colors.rowAltBg);
    HBRUSH rowBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &row, rowBrush);
    DeleteObject(rowBrush);

    SetBkMode(hdc, TRANSPARENT);
    int y = row.top + 5;

    // Lock status indicator
    SetTextColor(hdc, item.unlocked ? m_colors.unlockedColor : m_colors.lockedColor);
    std::wstring lockStr = item.unlocked ? L"[OPEN]" : L"[LOCK]";
    TextOutW(hdc, row.left + 12, y, lockStr.c_str(), (int)lockStr.size());

    // Feature name
    SetTextColor(hdc, item.unlocked ? m_colors.textNormal : m_colors.textDim);
    std::wstring wname(item.name.begin(), item.name.end());
    TextOutW(hdc, row.left + 90, y, wname.c_str(), (int)wname.size());

    // Tier
    COLORREF tierColor = m_colors.tierCommunity;
    switch (item.requiredTier) {
        case LicenseTierV2::Professional: tierColor = m_colors.tierPro; break;
        case LicenseTierV2::Enterprise:   tierColor = m_colors.tierEnterprise; break;
        case LicenseTierV2::Sovereign:    tierColor = m_colors.tierSovereign; break;
        default: break;
    }
    SetTextColor(hdc, tierColor);
    std::wstring wtier(item.tierName.begin(), item.tierName.end());
    TextOutW(hdc, row.left + 380, y, wtier.c_str(), (int)wtier.size());

    // Implementation status badges
    SetTextColor(hdc, item.implemented ? m_colors.implColor : m_colors.missingColor);
    TextOutW(hdc, row.left + 470, y, item.implemented ? L"YES" : L"NO", item.implemented ? 3 : 2);

    SetTextColor(hdc, item.wired ? m_colors.implColor : m_colors.missingColor);
    TextOutW(hdc, row.left + 520, y, item.wired ? L"YES" : L"NO", item.wired ? 3 : 2);

    SetTextColor(hdc, item.tested ? m_colors.unlockedColor : m_colors.textDim);
    TextOutW(hdc, row.left + 575, y, item.tested ? L"YES" : L"---", 3);
}

void FeatureRegistryPanel::paintStatusBar(HDC hdc, RECT& area) {
    RECT statusRect = { area.left, area.bottom, area.right, area.bottom + 30 };
    HBRUSH statusBrush = CreateSolidBrush(m_colors.headerBg);
    FillRect(hdc, &statusRect, statusBrush);
    DeleteObject(statusBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT statusFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, statusFont);
    SetTextColor(hdc, m_colors.textDim);

    // Count stats
    int impl = 0, miss = 0, unlocked = 0, locked = 0;
    for (const auto& item : m_displayItems) {
        if (item.implemented) impl++; else miss++;
        if (item.unlocked) unlocked++; else locked++;
    }

    std::wstring statusText = L"Implemented: " + std::to_wstring(impl) +
                              L"  |  Missing: " + std::to_wstring(miss) +
                              L"  |  Unlocked: " + std::to_wstring(unlocked) +
                              L"  |  Locked: " + std::to_wstring(locked);
    TextOutW(hdc, statusRect.left + 12, statusRect.top + 8, statusText.c_str(), (int)statusText.size());

    SelectObject(hdc, oldFont);
    DeleteObject(statusFont);
}

// ============================================================================
// Window Procedure
// ============================================================================
LRESULT CALLBACK FeatureRegistryPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FeatureRegistryPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = static_cast<FeatureRegistryPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
    } else {
        panel = reinterpret_cast<FeatureRegistryPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!panel) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            panel->paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            panel->m_scrollOffset -= delta / 2;
            int maxScroll = (int)panel->m_filteredItems.size() * panel->m_rowHeight;
            RECT rc;
            GetClientRect(hwnd, &rc);
            maxScroll -= (rc.bottom - 90); // header + status
            if (panel->m_scrollOffset < 0) panel->m_scrollOffset = 0;
            if (panel->m_scrollOffset > maxScroll) panel->m_scrollOffset = maxScroll;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
            int listTop = 60;
            int idx = (y - listTop + panel->m_scrollOffset) / panel->m_rowHeight;
            if (idx >= 0 && idx < (int)panel->m_filteredItems.size()) {
                panel->m_selectedIndex = idx;
                if (panel->m_featureSelectedCb) {
                    panel->m_featureSelectedCb(panel->m_filteredItems[idx].featureId);
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_UP && panel->m_selectedIndex > 0) {
                panel->m_selectedIndex--;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wParam == VK_DOWN && panel->m_selectedIndex < (int)panel->m_filteredItems.size() - 1) {
                panel->m_selectedIndex++;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == REFRESH_TIMER_ID && panel) {
                panel->refreshLicenseStatus();  // Live refresh license state
            }
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Console Output (for non-GUI / CLI contexts)
// ============================================================================
void FeatureRegistryPanel::printFeatureTable() {
    auto& lic = EnterpriseLicenseV2::Instance();

    std::cout << "\n";
    std::cout << std::left << std::setw(6) << "ID"
              << std::setw(40) << "Feature"
              << std::setw(15) << "Tier"
              << std::setw(8)  << "Enabled"
              << std::setw(8)  << "Impl"
              << std::setw(8)  << "Wired"
              << std::setw(8)  << "Tested"
              << "\n";
    std::cout << std::string(93, '-') << "\n";

    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        bool enabled = lic.isFeatureEnabled(def.id);
        std::cout << std::left << std::setw(6) << i
                  << std::setw(40) << (def.name ? def.name : "?")
                  << std::setw(15) << tierName(def.minTier)
                  << std::setw(8)  << (enabled ? "YES" : "no")
                  << std::setw(8)  << (def.implemented ? "YES" : "no")
                  << std::setw(8)  << (def.wiredToUI ? "YES" : "no")
                  << std::setw(8)  << (def.tested ? "YES" : "---")
                  << "\n";
    }
    std::cout << std::endl;
}

void FeatureRegistryPanel::printLicenseStatus() {
    auto& lic = EnterpriseLicenseV2::Instance();
    std::cout << "  Tier:                " << tierName(lic.currentTier()) << "\n";
    std::cout << "  Enabled Features:    " << lic.enabledFeatureCount() << " / " << TOTAL_FEATURES << "\n";
    std::cout << "  Implemented:         " << lic.countImplemented() << " / " << TOTAL_FEATURES << "\n";
    std::cout << "  Wired to UI:         " << lic.countWiredToUI() << " / " << TOTAL_FEATURES << "\n";
    std::cout << "  Tested:              " << lic.countTested() << " / " << TOTAL_FEATURES << "\n";
    std::cout << "  Audit Entries:       " << lic.getAuditEntryCount() << " / " << lic.MAX_AUDIT_ENTRIES << "\n";
    char hwid[64] = {};
    lic.getHardwareIDHex(hwid, sizeof(hwid));
    std::cout << "  HWID:                " << hwid << "\n";
    std::cout << std::endl;
}

void FeatureRegistryPanel::printAuditReport() {
    auto& lic = EnterpriseLicenseV2::Instance();
    size_t count = lic.getAuditEntryCount();
    const LicenseAuditEntry* entries = lic.getAuditEntries();

    std::cout << "\n  AUDIT TRAIL  (" << count << " entries)\n";
    std::cout << "  " << std::string(60, '-') << "\n";

    size_t show = count > 50 ? 50 : count;
    for (size_t i = count - show; i < count; i++) {
        const auto& e = entries[i % lic.MAX_AUDIT_ENTRIES];
        std::cout << "  [" << std::setw(4) << i << "] "
                  << (e.granted ? "ALLOW" : "DENY ") << "  "
                  << featureName(e.feature) << "  by " << (e.caller ? e.caller : "?")
                  << "\n";
    }
    std::cout << std::endl;
}

// ============================================================================
// License Activation Dialog — File Browse + Activate
// ============================================================================
void FeatureRegistryPanel::showActivationDialog() {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"License Files (*.key;*.lic;*.txt)\0*.key;*.lic;*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Select RawrXD License Key File";

    if (GetOpenFileNameW(&ofn)) {
        char path[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, path, MAX_PATH, nullptr, nullptr);

        auto& lic = EnterpriseLicenseV2::Instance();
        auto result = lic.loadKeyFromFile(path);

        if (result.success) {
            FeatureFlagsRuntime::Instance().refreshFromLicense();
            refreshFeatures();

            std::wstringstream msg;
            msg << L"License activated!\n\nTier: ";
            const char* tn = tierName(lic.currentTier());
            for (const char* p = tn; *p; ++p) msg << (wchar_t)*p;
            msg << L"\nEnabled: " << lic.enabledFeatureCount() << L" / " << TOTAL_FEATURES;
            std::wstring ws = msg.str();
            MessageBoxW(m_hwnd, ws.c_str(), L"License Activated", MB_OK | MB_ICONINFORMATION);
        } else {
            std::wstring ws = L"Activation failed: ";
            if (result.detail) {
                for (const char* p = result.detail; *p; ++p) ws += (wchar_t)*p;
            }
            MessageBoxW(m_hwnd, ws.c_str(), L"Activation Failed", MB_OK | MB_ICONERROR);
        }
    }
}

void FeatureRegistryPanel::showLicenseCreatorDialog() {
    MessageBoxW(m_hwnd,
        L"Use the RawrXD-LicenseCreatorV2 CLI tool to generate license keys.\n\n"
        L"Example:\n"
        L"  RawrXD-LicenseCreatorV2 --create --tier enterprise --days 365\n"
        L"  RawrXD-LicenseCreatorV2 --validate <key-file>\n"
        L"  RawrXD-LicenseCreatorV2 --dev-unlock\n"
        L"  RawrXD-LicenseCreatorV2 --list\n"
        L"\nOr use the unified creator:\n"
        L"  RawrXD-LicenseUnified --dashboard\n"
        L"  RawrXD-LicenseUnified --wiring-status",
        L"License Creator", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Enforcement Status Console Display
// ============================================================================
void FeatureRegistryPanel::printEnforcementStatus() {
    auto& enf = LicenseEnforcer::Instance();
    char buf[4096] = {};
    int len = enf.generateReport(buf, sizeof(buf));
    if (len > 0) {
        std::cout << buf << std::endl;
    } else {
        std::cout << "  (Enforcement not yet initialized)" << std::endl;
    }
}

// ============================================================================
// Gaps Report Console Display
// ============================================================================
void FeatureRegistryPanel::printGapsReport() {
    std::cout << "\n========================================"
              << "========================================\n";
    std::cout << "  FEATURE GAPS REPORT\n";
    std::cout << "========================================"
              << "========================================\n\n";

    int notImpl = 0, notWired = 0, notTested = 0;

    std::cout << "  NOT IMPLEMENTED:\n";
    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        if (!def.implemented) {
            std::cout << "    [X] " << (def.name ? def.name : "?")
                      << "  (" << (def.sourceFile ? def.sourceFile : "?") << ")\n";
            notImpl++;
        }
    }
    if (notImpl == 0) std::cout << "    (none)\n";

    std::cout << "\n  NOT WIRED TO ENFORCEMENT:\n";
    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        if (def.implemented && !def.wiredToUI) {
            std::cout << "    [~] " << (def.name ? def.name : "?")
                      << "  (" << (def.sourceFile ? def.sourceFile : "?") << ")\n";
            notWired++;
        }
    }
    if (notWired == 0) std::cout << "    (none)\n";

    std::cout << "\n  NOT TESTED:\n";
    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        if (def.implemented && !def.tested) {
            std::cout << "    [?] " << (def.name ? def.name : "?")
                      << "  (" << (def.sourceFile ? def.sourceFile : "?") << ")\n";
            notTested++;
        }
    }
    if (notTested == 0) std::cout << "    (none)\n";

    std::cout << "\n  Summary: " << notImpl << " not implemented, "
              << notWired << " not wired, " << notTested << " not tested\n";
    std::cout << "  Total features: " << TOTAL_FEATURES << "\n\n";
}

// ============================================================================
// Full Dashboard — Combined console report of everything
// ============================================================================
void FeatureRegistryPanel::printFullDashboard() {
    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "         RawrXD Enterprise License Dashboard v2.0\n";
    std::cout << "=================================================================\n";
    std::cout << "\n";

    // Section 1: License Status
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  LICENSE STATUS\n";
    std::cout << "-----------------------------------------------------------------\n";
    printLicenseStatus();
    std::cout << "\n";

    // Section 2: Feature Matrix
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  FEATURE MATRIX\n";
    std::cout << "-----------------------------------------------------------------\n";
    printFeatureTable();
    std::cout << "\n";

    // Section 3: Enforcement Status
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  ENFORCEMENT GATES\n";
    std::cout << "-----------------------------------------------------------------\n";
    printEnforcementStatus();
    std::cout << "\n";

    // Section 4: Gaps
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  GAPS & COVERAGE\n";
    std::cout << "-----------------------------------------------------------------\n";
    printGapsReport();

    // Section 5: Runtime Feature Flags
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  RUNTIME FEATURE FLAGS\n";
    std::cout << "-----------------------------------------------------------------\n";
    auto& ff = FeatureFlagsRuntime::Instance();
    std::cout << "  Enabled: " << ff.enabledCount()
              << "  Overridden: " << ff.overriddenCount()
              << "  Events: " << ff.getEventCount()
              << "\n";

    // Section 6: Machine ID
    std::cout << "-----------------------------------------------------------------\n";
    auto& lic = EnterpriseLicenseV2::Instance();
    char hwid[64] = {};
    lic.getHardwareIDHex(hwid, sizeof(hwid));
    std::cout << "  Machine Fingerprint: " << hwid << "\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << std::endl;
}
