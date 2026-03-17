// ============================================================================
// Win32IDE_GUILayoutHotpatch.cpp — IDE GUI layout auditor and hotpatch
// ============================================================================
// Architecture: C++20 | Win32 Unicode (W) | No Qt | No exceptions
// Allows the image viewer (Vision) to view the IDE GUI and auto-correct
// overlapping, not displaying, or bugged layout by:
//   - Capturing a screenshot of the main window
//   - Auditing key child RECTs (overlaps, zero-size, off-screen, visibility)
//   - Hotpatching via forced onSize() re-layout
//   - Optionally showing a viewer dialog with screenshot and report.
// ============================================================================

#include "Win32IDE.h"
#include "multimodal/vision_encoder.h"
#include "logging/logger.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace {

// UTF-8 (std::string) to UTF-16 (std::wstring) for Unicode Win32 APIs
static std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) == 0)
        return {};
    return out;
}

} // namespace

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif

#ifdef _WIN32
#pragma comment(lib, "gdi32.lib")
#endif

namespace {

struct LayoutEntry {
    std::string name;
    HWND        hwnd = nullptr;
    RECT        rect = {};
    bool        visible = false;
    std::string issues;
};

static RawrXD::Multimodal::VisionEncoder& getVisionEncoder() {
    static RawrXD::Multimodal::VisionEncoder s_encoder;
    return s_encoder;
}

// Collect RECT in main window client coordinates.
static void getChildRectInMain(HWND hMain, HWND hChild, RECT& out) {
    if (!hMain || !hChild || !IsWindow(hChild)) {
        SetRectEmpty(&out);
        return;
    }
    RECT cr;
    if (!GetWindowRect(hChild, &cr)) {
        SetRectEmpty(&out);
        return;
    }
    POINT pt[] = { { cr.left, cr.top }, { cr.right, cr.bottom } };
    MapWindowPoints(HWND_DESKTOP, hMain, pt, 2);
    out.left   = pt[0].x;
    out.top    = pt[0].y;
    out.right  = pt[1].x;
    out.bottom = pt[1].y;
}

static bool rectsOverlap(const RECT& a, const RECT& b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

static int rectArea(const RECT& r) {
    return (r.right - r.left) * (r.bottom - r.top);
}

} // namespace

// ============================================================================
// Audit IDE layout: collect key controls, detect overlaps/zero-size/invisible
// ============================================================================
bool Win32IDE::auditIDEGUILayout(std::string& reportOut) {
    reportOut.clear();
    std::vector<LayoutEntry> entries;
    HWND hMain = getMainWindow();
    if (!hMain || !IsWindow(hMain)) {
        reportOut = "Main window not available.";
        return true;
    }

    RECT mainClient = {};
    GetClientRect(hMain, &mainClient);
    const int mainW = mainClient.right - mainClient.left;
    const int mainH = mainClient.bottom - mainClient.top;

    auto add = [&](const char* name, HWND hwnd) {
        LayoutEntry e;
        e.name = name;
        e.hwnd = hwnd;
        e.visible = hwnd && IsWindowVisible(hwnd);
        getChildRectInMain(hMain, hwnd, e.rect);
        entries.push_back(e);
    };

    add("Toolbar",      m_hwndToolbar);
    add("ActivityBar",  m_hwndActivityBar);
    add("Sidebar",      m_hwndSidebar);
    add("TabBar",       m_hwndTabBar);
    add("Breadcrumbs",  m_hwndBreadcrumbs);
    add("LineNumbers",  m_hwndLineNumbers);
    add("Editor",       m_hwndEditor);
    add("Minimap",      m_hwndMinimap);
    add("StatusBar",    m_hwndStatusBar);
    add("OutputTabs",   m_hwndOutputTabs);
    if (m_hwndFileExplorer) add("FileExplorer", m_hwndFileExplorer);

    // Detect issues
    for (size_t i = 0; i < entries.size(); ++i) {
        LayoutEntry& e = entries[i];
        if (!e.hwnd) continue;

        int w = e.rect.right - e.rect.left;
        int h = e.rect.bottom - e.rect.top;

        if (w <= 0 || h <= 0)
            e.issues += " zero-size;";
        if (e.rect.right < 0 || e.rect.bottom < 0 || e.rect.left > mainW || e.rect.top > mainH)
            e.issues += " off-screen;";
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].hwnd) continue;
        for (size_t j = i + 1; j < entries.size(); ++j) {
            if (!entries[j].hwnd) continue;
            if (rectArea(entries[i].rect) <= 0 || rectArea(entries[j].rect) <= 0) continue;
            if (rectsOverlap(entries[i].rect, entries[j].rect)) {
                entries[i].issues += " overlaps-" + entries[j].name + ";";
                entries[j].issues += " overlaps-" + entries[i].name + ";";
            }
        }
    }

    // Build report
    bool hadIssues = false;
    for (const auto& e : entries) {
        if (e.hwnd && !e.issues.empty()) {
            hadIssues = true;
            reportOut += e.name + ": " + e.issues + "\n";
        }
    }
    if (reportOut.empty())
        reportOut = "No layout issues detected.";

    return hadIssues;
}

// ============================================================================
// Hotpatch: force re-layout so overlapping/off-screen controls are corrected
// ============================================================================
void Win32IDE::hotpatchIDELayout() {
    HWND hMain = getMainWindow();
    if (!hMain || !IsWindow(hMain)) return;
    RECT rc;
    if (!GetClientRect(hMain, &rc)) return;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    onSize(w, h);
    InvalidateRect(hMain, nullptr, TRUE);
    UpdateWindow(hMain);
}

// ============================================================================
// Viewer window: show screenshot and report (hotpatch result) — Unicode (W)
// ============================================================================
static const wchar_t VIEWER_CLASS[] = L"RawrXDGUIImageViewer";
static bool s_viewerClassRegistered = false;

static LRESULT CALLBACK ViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wParam) == IDOK) {
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND createViewerDialog(HWND parent, const std::vector<uint8_t>& bmpData, const std::string& report) {
    if (!s_viewerClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ViewerWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = VIEWER_CLASS;
        wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)(uintptr_t)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (!RegisterClassExW(&wc)) return nullptr;
        s_viewerClassRegistered = true;
    }

    const std::wstring reportW = utf8ToWide(report);
    const int dlgW = 720;
    const int dlgH = 560;
    int x = 100, y = 100;
    HINSTANCE hInst = GetModuleHandle(nullptr);
    HWND hDlg = CreateWindowExW(0, VIEWER_CLASS, L"IDE GUI — View & Hotpatch Result",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                 x, y, dlgW, dlgH, parent, nullptr, hInst, nullptr);
    if (!hDlg) return nullptr;

    CreateWindowExW(0, L"STATIC", nullptr,
                    WS_CHILD | WS_VISIBLE | SS_BITMAP,
                    10, 10, 400, 300, hDlg, (HMENU)1000, hInst, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", reportW.c_str(),
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    420, 10, 280, 300, hDlg, (HMENU)1001, hInst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Close",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    dlgW / 2 - 40, dlgH - 44, 80, 28, hDlg, (HMENU)IDOK, hInst, nullptr);

    if (!bmpData.empty() && bmpData.size() > 54) {
        BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(bmpData.data() + 14);
        int w = bih->biWidth;
        int h = abs(bih->biHeight);
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
        SelectObject(hdcMem, hBmp);
        StretchDIBits(hdcMem, 0, 0, w, h, 0, 0, w, h,
                     bmpData.data() + 14 + sizeof(BITMAPINFOHEADER),
                     (BITMAPINFO*)(bmpData.data() + 14),
                     DIB_RGB_COLORS, SRCCOPY);
        SendDlgItemMessage(hDlg, 1000, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    ShowWindow(hDlg, SW_SHOW);
    return hDlg;
}

// ============================================================================
// Command: capture IDE, audit, hotpatch if needed, show viewer
// ============================================================================
void Win32IDE::cmdVisionViewIDEGUIAndHotpatch() {
    HWND hMain = getMainWindow();
    if (!hMain || !IsWindow(hMain)) {
        MessageBoxW(hMain, L"Main window not available.", L"IDE GUI Hotpatch", MB_ICONWARNING);
        return;
    }

    RECT wr = {};
    if (!GetWindowRect(hMain, &wr)) {
        MessageBoxW(hMain, L"Could not get window rect.", L"IDE GUI Hotpatch", MB_ICONERROR);
        return;
    }
    int capW = wr.right - wr.left;
    int capH = wr.bottom - wr.top;
    if (capW <= 0 || capH <= 0) {
        MessageBoxW(hMain, L"Invalid window size.", L"IDE GUI Hotpatch", MB_ICONWARNING);
        return;
    }

    auto& encoder = getVisionEncoder();
    auto result = encoder.CaptureScreenshot(wr.left, wr.top, capW, capH);
    if (!result.success) {
        MessageBoxW(hMain, utf8ToWide(result.error).c_str(), L"Screenshot Error", MB_ICONERROR);
        return;
    }

    std::string report;
    bool hadIssues = auditIDEGUILayout(report);

    if (hadIssues) {
        hotpatchIDELayout();
        report = "[Auto-corrected]\n" + report;
        RAWRXD_LOG_INFO("IDE GUI hotpatch applied: " << report);
    }

    if (result.image.rawBytes.empty()) {
        MessageBoxW(hMain, utf8ToWide(report).c_str(), L"IDE GUI Layout", MB_OK);
        return;
    }

    createViewerDialog(hMain, result.image.rawBytes, report);
}
