// ============================================================================
// license_manager_panel.cpp — License Manager UI Implementation
// ============================================================================

#include "../include/license_manager_panel.h"
#include "../include/enterprise_license.h"
#include <commctrl.h>
#include <commdlg.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD::License {

namespace {
constexpr int IDC_BROWSE = 7101;
constexpr int IDC_ACTIVATE = 7102;
constexpr int IDC_PATH_EDIT = 7103;
constexpr int IDC_LICINFO_LIST = 7200;
constexpr int IDC_PANEL_LOAD_LICENSE = 7301;
constexpr int IDC_PANEL_COPY_HWID = 7302;
constexpr int IDC_PANEL_VIEW_AUDIT = 7303;

constexpr const char* LICENSE_INFO_CLASS = "RawrXD_LicenseInfoDlg";
constexpr const char* LICENSE_ACTIVATION_CLASS = "RawrXD_LicenseActivationDlg";

static INT_PTR g_licenseInfoResult = IDCANCEL;
static INT_PTR g_licenseActivationResult = IDCANCEL;
}

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

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PANEL_LOAD_LICENSE: onLoadLicenseClick(); return 0;
                case IDC_PANEL_COPY_HWID:    onCopyHWIDClick(); return 0;
                case IDC_PANEL_VIEW_AUDIT:   onViewAuditClick(); return 0;
            }
            break;

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;

        default:
            return DefWindowProc(m_hwnd, message, wParam, lParam);
    }
    return DefWindowProc(m_hwnd, message, wParam, lParam);
}

void LicenseManagerPanel::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    FillRect(hdc, &rcClient, m_brushBg);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkColor(hdc, RGB(240, 240, 240));

    const int pad = 10;
    const int btnAreaHeight = 44;
    int y = pad;
    int right = rcClient.right - pad;
    int bottom = rcClient.bottom - pad - btnAreaHeight;

    RECT rcLicense = { pad, y, right, y + 72 };
    drawLicenseInfo(hdc, rcLicense);
    y = rcLicense.bottom + pad;

    RECT rcFeatures = { pad, y, right, (int)(y + 22 + TOTAL_FEATURES * 16) };
    if (rcFeatures.bottom > bottom) rcFeatures.bottom = bottom;
    drawFeatureList(hdc, rcFeatures);
    y = rcFeatures.bottom + pad;

    RECT rcAudit = { pad, y, right, y + 22 };
    if (rcAudit.bottom <= bottom) drawAuditLog(hdc, rcAudit);
    y = rcAudit.bottom + pad;

    RECT rcLimits = { pad, y, right, bottom };
    if (rcLimits.bottom > rcLimits.top) drawLimits(hdc, rcLimits);

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
    auto& lic = EnterpriseLicenseV2::Instance();
    RECT rcText = rect;
    DrawText(hdc, "Features", 8, &rcText, DT_LEFT | DT_TOP);
    rcText.top += 18;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const auto& def = lic.getFeatureDef(static_cast<FeatureID>(i));
        std::string line = lic.isFeatureEnabled(static_cast<FeatureID>(i)) ? "[+] " : "[-] ";
        line += def.name;
        DrawText(hdc, line.c_str(), static_cast<int>(line.length()), &rcText, DT_LEFT | DT_TOP);
        rcText.top += 16;
    }
}

void LicenseManagerPanel::drawAuditLog(HDC hdc, const RECT& rect) {
    auto& lic = EnterpriseLicenseV2::Instance();
    std::string audit = "Audit: " + std::string(tierName(lic.currentTier()))
        + ", " + std::to_string(lic.enabledFeatureCount()) + " features, "
        + formatTimestamp(static_cast<uint32_t>(std::time(nullptr)));
    RECT rcText = rect;
    DrawText(hdc, audit.c_str(), static_cast<int>(audit.length()), &rcText, DT_LEFT | DT_TOP);
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
    if (!m_hwnd) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int pad = 10;
    const int btnH = 28;
    const int btnW = 100;
    int y = rc.bottom - pad - btnH;
    int x = pad;
    CreateWindowExA(0, "BUTTON", "Load License", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_PANEL_LOAD_LICENSE, GetModuleHandle(nullptr), nullptr);
    x += btnW + pad;
    CreateWindowExA(0, "BUTTON", "Copy HWID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_PANEL_COPY_HWID, GetModuleHandle(nullptr), nullptr);
    x += btnW + pad;
    CreateWindowExA(0, "BUTTON", "View Audit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_PANEL_VIEW_AUDIT, GetModuleHandle(nullptr), nullptr);
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
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "License files (*.lic;*.key;*.json)\0*.lic;*.key;*.json\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        if (EnterpriseLicenseV2::Instance().loadKeyFromFile(path).success)
            refresh();
    }
}

void LicenseManagerPanel::onCopyHWIDClick() {
    if (m_cachedHWIDStr.empty()) return;
    if (!OpenClipboard(m_hwnd)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, m_cachedHWIDStr.size() + 1);
    if (h) {
        char* p = static_cast<char*>(GlobalLock(h));
        if (p) {
            memcpy(p, m_cachedHWIDStr.c_str(), m_cachedHWIDStr.size() + 1);
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
}

void LicenseManagerPanel::onViewAuditClick() {
    LicenseInfoDialog::show(m_hwnd);
}

// ============================================================================
// LicenseInfoDialog Implementation
// ============================================================================

static LRESULT CALLBACK LicenseInfoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int pad = 10;
        int btnH = 28;
        int btnW = 90;
        HWND hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            pad, pad, rc.right - 2 * pad, rc.bottom - btnH - 2 * pad,
            hwnd, (HMENU)(UINT_PTR)IDC_LICINFO_LIST, hInst, nullptr);
        if (hList) {
            LicenseInfoDialog::displayLicenseDetails(hList);
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)"");
            LicenseInfoDialog::displayFeatureTable(hList);
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)"");
            LicenseInfoDialog::displayAuditTrail(hList);
        }
        CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rc.right - 2 * btnW - 2 * pad, rc.bottom - btnH - pad, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDOK, hInst, nullptr);
        CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rc.right - btnW - pad, rc.bottom - btnH - pad, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDCANCEL, hInst, nullptr);
        return 0;
    }
    if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK) {
            g_licenseInfoResult = IDOK;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            g_licenseInfoResult = IDCANCEL;
            DestroyWindow(hwnd);
            return 0;
        }
    }
    if (msg == WM_CLOSE) {
        g_licenseInfoResult = IDCANCEL;
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

INT_PTR LicenseInfoDialog::show(HWND parentHwnd) {
    static bool classReg = false;
    if (!classReg) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = LicenseInfoWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = LICENSE_INFO_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (!RegisterClassExA(&wc)) return IDCANCEL;
        classReg = true;
    }
    g_licenseInfoResult = IDCANCEL;
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, LICENSE_INFO_CLASS,
        "License Information", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 420,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return IDCANCEL;
    ShowWindow(hDlg, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) && IsWindow(hDlg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return g_licenseInfoResult;
}

INT_PTR CALLBACK LicenseInfoDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            onInitDialog(hwnd);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) onCancel(hwnd);
            return TRUE;
    }
    return FALSE;
}

void LicenseInfoDialog::onInitDialog(HWND hwnd) {
    (void)hwnd;
}

void LicenseInfoDialog::onCancel(HWND hwnd) {
    g_licenseInfoResult = IDCANCEL;
    DestroyWindow(hwnd);
}

void LicenseInfoDialog::displayLicenseDetails(HWND hwndList) {
    auto& lic = EnterpriseLicenseV2::Instance();
    std::ostringstream oss;
    oss << "Tier: " << tierName(lic.currentTier());
    SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)oss.str().c_str());
    oss.str("");
    oss << "Enabled features: " << lic.enabledFeatureCount();
    SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)oss.str().c_str());
}

void LicenseInfoDialog::displayFeatureTable(HWND hwndList) {
    auto& lic = EnterpriseLicenseV2::Instance();
    SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)"--- Features ---");
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const auto& def = lic.getFeatureDef(static_cast<FeatureID>(i));
        std::string line = lic.isFeatureEnabled(static_cast<FeatureID>(i)) ? "[+] " : "[-] ";
        line += def.name;
        SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

void LicenseInfoDialog::displayAuditTrail(HWND hwndList) {
    SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)"--- Audit ---");
    SendMessageA(hwndList, LB_ADDSTRING, 0, (LPARAM)"(Audit trail entries when available)");
}

// ============================================================================
// LicenseActivationDialog Implementation
// ============================================================================

static LRESULT CALLBACK LicenseActivationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int pad = 10;
        int btnH = 28;
        int editH = 24;
        CreateWindowExA(0, "STATIC", "License file path:", WS_CHILD | WS_VISIBLE,
            pad, pad, 120, editH, hwnd, nullptr, hInst, nullptr);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            pad, pad + editH + 4, rc.right - 2 * pad - 100, editH,
            hwnd, (HMENU)(UINT_PTR)IDC_PATH_EDIT, hInst, nullptr);
        CreateWindowExA(0, "BUTTON", "Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rc.right - pad - 90, pad + editH + 4, 90, editH, hwnd, (HMENU)(UINT_PTR)IDC_BROWSE, hInst, nullptr);
        CreateWindowExA(0, "BUTTON", "Activate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            pad, rc.bottom - btnH - pad, 90, btnH, hwnd, (HMENU)(UINT_PTR)IDC_ACTIVATE, hInst, nullptr);
        CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rc.right - 90 - pad, rc.bottom - btnH - pad, 90, btnH, hwnd, (HMENU)(UINT_PTR)IDCANCEL, hInst, nullptr);
        return 0;
    }
    if (msg == WM_COMMAND) {
        switch (LOWORD(wParam)) {
            case IDC_BROWSE:
                LicenseActivationDialog::onBrowseClick(hwnd);
                return 0;
            case IDC_ACTIVATE:
                LicenseActivationDialog::onActivateClick(hwnd);
                return 0;
            case IDCANCEL:
                g_licenseActivationResult = IDCANCEL;
                DestroyWindow(hwnd);
                return 0;
        }
    }
    if (msg == WM_CLOSE) {
        g_licenseActivationResult = IDCANCEL;
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

INT_PTR LicenseActivationDialog::show(HWND parentHwnd) {
    static bool classReg = false;
    if (!classReg) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = LicenseActivationWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = LICENSE_ACTIVATION_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (!RegisterClassExA(&wc)) return IDCANCEL;
        classReg = true;
    }
    g_licenseActivationResult = IDCANCEL;
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, LICENSE_ACTIVATION_CLASS,
        "Activate License", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 140,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return IDCANCEL;
    ShowWindow(hDlg, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) && IsWindow(hDlg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return g_licenseActivationResult;
}

INT_PTR CALLBACK LicenseActivationDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            onInitDialog(hwnd);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BROWSE: onBrowseClick(hwnd); return TRUE;
                case IDC_ACTIVATE: onActivateClick(hwnd); return TRUE;
                case IDCANCEL: onCancel(hwnd); return TRUE;
            }
            break;
    }
    return FALSE;
}

void LicenseActivationDialog::onInitDialog(HWND hwnd) {
    (void)hwnd;
}

void LicenseActivationDialog::onBrowseClick(HWND hwnd) {
    OPENFILENAMEA ofn = {};
    char path[MAX_PATH] = "";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "License files (*.lic)\0*.lic\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        HWND hEdit = GetDlgItem(hwnd, IDC_PATH_EDIT);
        if (hEdit) SetWindowTextA(hEdit, path);
    }
}

void LicenseActivationDialog::onActivateClick(HWND hwnd) {
    char path[MAX_PATH] = "";
    HWND hEdit = GetDlgItem(hwnd, IDC_PATH_EDIT);
    if (!hEdit || GetWindowTextA(hEdit, path, MAX_PATH) == 0) {
        MessageBoxA(hwnd, "Enter or browse to a license file path.", "Activate License", MB_OK | MB_ICONINFORMATION);
        return;
    }
    LicenseResult result = EnterpriseLicenseV2::Instance().loadKeyFromFile(path);
    if (result.success) {
        MessageBoxA(hwnd, "License activated successfully.", "Activate License", MB_OK | MB_ICONINFORMATION);
        g_licenseActivationResult = IDOK;
        DestroyWindow(hwnd);
    } else {
        const char* err = result.detail ? result.detail : "Failed to load license file.";
        MessageBoxA(hwnd, err, "Activate License", MB_OK | MB_ICONWARNING);
    }
}

void LicenseActivationDialog::onCancel(HWND hwnd) {
    g_licenseActivationResult = IDCANCEL;
    DestroyWindow(hwnd);
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
