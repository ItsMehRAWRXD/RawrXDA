// ============================================================================
// Win32IDE_Gauntlet.cpp — Phase 32: Gauntlet UI & Command Integration
// ============================================================================
//
// PURPOSE:
//   Win32IDE integration for the Final Gauntlet runtime verification.
//   Provides:
//     1. Command routing for IDM_GAUNTLET_* commands (9600 range)
//     2. ListView-based results window (pass/fail/time per test)
//     3. Console-output summary appended to the Output panel
//     4. Exportable text report
//
// UI:
//   Creates a modeless dialog with a WC_LISTVIEW showing all 10 gauntlet
//   tests, their status, error detail, and elapsed time. Color-coded rows:
//     green  = PASS
//     red    = FAIL
//
// PATTERN:   No exceptions. PatchResult-compatible returns.
// THREADING: All UI calls on main thread (STA).
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "final_gauntlet.h"

#include <commctrl.h>
#include <sstream>
#include <fstream>
#include <cstdio>

// ============================================================================
// Wide-suffix ListView helpers (project builds ANSI, explicit W calls)
// ============================================================================

#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif

#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif

#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif

static int LVG_InsertColumnW(HWND hwnd, int col, int width, const wchar_t* text) {
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = width;
    lvc.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
}

static int LVG_InsertItemW(HWND hwnd, int row, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = row;
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

static void LVG_SetItemTextW(HWND hwnd, int row, int col, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.iSubItem = col;
    lvi.pszText = const_cast<LPWSTR>(text);
    SendMessageW(hwnd, LVM_SETITEMTEXTW, row, (LPARAM)&lvi);
}

// ============================================================================
// Gauntlet Window Class & State
// ============================================================================

static const wchar_t* GAUNTLET_WINDOW_CLASS = L"RawrXD_GauntletWindow";
static bool s_gauntletClassRegistered = false;

struct GauntletWindowState {
    Win32IDE*       ide;
    HWND            hwndListView;
    GauntletSummary summary;
};

static LRESULT CALLBACK gauntletWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool ensureGauntletWindowClass() {
    if (s_gauntletClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = gauntletWndProc;
    wc.cbWndExtra = sizeof(void*);
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = GAUNTLET_WINDOW_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_gauntletClassRegistered = true;
    return true;
}

// ============================================================================
// Populate the ListView with gauntlet results
// ============================================================================

static void populateGauntletListView(HWND hwndLV, const GauntletSummary& summary) {
    SendMessageW(hwndLV, LVM_DELETEALLITEMS, 0, 0);

    for (int i = 0; i < summary.totalTests; ++i) {
        const GauntletResult& r = summary.results[i];

        // Column 0: Test #
        wchar_t num[16];
        swprintf(num, 16, L"%d", i + 1);
        LVG_InsertItemW(hwndLV, i, num);

        // Column 1: Test Name
        wchar_t name[128] = {0};
        if (r.testName) {
            MultiByteToWideChar(CP_UTF8, 0, r.testName, -1, name, 127);
        }
        LVG_SetItemTextW(hwndLV, i, 1, name);

        // Column 2: Status
        LVG_SetItemTextW(hwndLV, i, 2, r.passed ? L"PASS ✓" : L"FAIL ✗");

        // Column 3: Detail
        wchar_t detail[512] = {0};
        if (r.detail) {
            MultiByteToWideChar(CP_UTF8, 0, r.detail, -1, detail, 511);
        }
        LVG_SetItemTextW(hwndLV, i, 3, detail);

        // Column 4: Time (ms)
        wchar_t time[32];
        swprintf(time, 32, L"%.2f ms", r.elapsedMs);
        LVG_SetItemTextW(hwndLV, i, 4, time);

        // Column 5: Error Code
        wchar_t errCode[16];
        swprintf(errCode, 16, L"%d", r.errorCode);
        LVG_SetItemTextW(hwndLV, i, 5, errCode);
    }
}

// ============================================================================
// Custom Draw (color-coded rows)
// ============================================================================

static LRESULT handleGauntletCustomDraw(LPNMLVCUSTOMDRAW lpcd, const GauntletSummary& summary) {
    switch (lpcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: {
        int item = (int)lpcd->nmcd.dwItemSpec;
        if (item >= 0 && item < summary.totalTests) {
            if (summary.results[item].passed) {
                lpcd->clrText = RGB(50, 205, 50);      // Lime green
                lpcd->clrTextBk = RGB(20, 40, 20);     // Dark green bg
            } else {
                lpcd->clrText = RGB(255, 80, 80);      // Red
                lpcd->clrTextBk = RGB(50, 20, 20);     // Dark red bg
            }
        }
        return CDRF_DODEFAULT;
    }

    default:
        return CDRF_DODEFAULT;
    }
}

// ============================================================================
// Gauntlet Window Procedure
// ============================================================================

static LRESULT CALLBACK gauntletWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GauntletWindowState* state = (GauntletWindowState*)GetWindowLongPtrW(hwnd, 0);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        state = (GauntletWindowState*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, 0, (LONG_PTR)state);

        // Create ListView
        HWND hwndLV = CreateWindowExW(
            0, WC_LISTVIEWW,  L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
            0, 0, 0, 0,
            hwnd, (HMENU)1001,
            GetModuleHandleW(nullptr), nullptr);

        if (hwndLV) {
            // Enable full-row select and grid lines
            SendMessageW(hwndLV, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            // Set dark colors
            SendMessageW(hwndLV, LVM_SETBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(hwndLV, LVM_SETTEXTBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(hwndLV, LVM_SETTEXTCOLOR, 0, RGB(220, 220, 220));

            // Insert columns
            LVG_InsertColumnW(hwndLV, 0, 40,  L"#");
            LVG_InsertColumnW(hwndLV, 1, 200, L"Test Name");
            LVG_InsertColumnW(hwndLV, 2, 80,  L"Status");
            LVG_InsertColumnW(hwndLV, 3, 340, L"Detail");
            LVG_InsertColumnW(hwndLV, 4, 80,  L"Time");
            LVG_InsertColumnW(hwndLV, 5, 60,  L"Error");

            state->hwndListView = hwndLV;
        }
        return 0;
    }

    case WM_SIZE: {
        if (state && state->hwndListView) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Reserve bottom 40px for status bar area
            MoveWindow(state->hwndListView, 0, 0, rc.right, rc.bottom - 40, TRUE);
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh && nmh->code == NM_CUSTOMDRAW && state) {
            LPNMLVCUSTOMDRAW lpcd = (LPNMLVCUSTOMDRAW)lParam;
            return handleGauntletCustomDraw(lpcd, state->summary);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Draw status bar at bottom
        RECT statusRect = { 0, rc.bottom - 40, rc.right, rc.bottom };
        HBRUSH hbr = CreateSolidBrush(RGB(25, 25, 25));
        FillRect(hdc, &statusRect, hbr);
        DeleteObject(hbr);

        if (state) {
            SetBkMode(hdc, TRANSPARENT);

            wchar_t statusText[256];
            if (state->summary.allPassed) {
                SetTextColor(hdc, RGB(50, 205, 50));
                swprintf(statusText, 256,
                         L"✓ ALL %d TESTS PASSED (%.2f ms total) — Ready for Phase 33 Packaging",
                         state->summary.totalTests,
                         state->summary.totalElapsedMs);
            } else {
                SetTextColor(hdc, RGB(255, 80, 80));
                swprintf(statusText, 256,
                         L"✗ %d/%d FAILED (%.2f ms total) — Fix failures before packaging",
                         state->summary.failed,
                         state->summary.totalTests,
                         state->summary.totalElapsedMs);
            }

            HFONT hFont = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);

            RECT textRect = { 10, rc.bottom - 35, rc.right - 10, rc.bottom - 5 };
            DrawTextW(hdc, statusText, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOld);
            DeleteObject(hFont);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY: {
        if (state) {
            // IDE will detect window closure via IsWindow() check
            // (m_hwndGauntletWindow becomes invalid when DestroyWindow completes)
            delete state;
            SetWindowLongPtrW(hwnd, 0, 0);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   // Prevent flicker (WM_PAINT handles background)
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdGauntletRun() {
    OutputDebugStringA("[Phase 32] Running Final Gauntlet...\n");

    // Run all 10 tests
    GauntletSummary summary = runFinalGauntlet();

    // Append summary to output panel
    std::ostringstream oss;
    oss << "\n";
    oss << "╔══════════════════════════════════════════════════════════════╗\n";
    oss << "║            PHASE 32: THE FINAL GAUNTLET                    ║\n";
    oss << "╚══════════════════════════════════════════════════════════════╝\n\n";

    for (int i = 0; i < summary.totalTests; ++i) {
        const GauntletResult& r = summary.results[i];
        char line[512];
        snprintf(line, sizeof(line), "  [%s] Test %2d: %-30s (%.2f ms)\n",
                 r.passed ? "PASS" : "FAIL",
                 i + 1,
                 r.testName ? r.testName : "???",
                 r.elapsedMs);
        oss << line;
        if (!r.passed && r.detail) {
            snprintf(line, sizeof(line), "           → %s (err %d)\n",
                     r.detail, r.errorCode);
            oss << line;
        }
    }

    oss << "\n  ─────────────────────────────────────────────────────\n";
    char finalLine[256];
    snprintf(finalLine, sizeof(finalLine),
             "  RESULT: %d/%d passed | %d failed | %.2f ms total\n",
             summary.passed, summary.totalTests,
             summary.failed, summary.totalElapsedMs);
    oss << finalLine;

    if (summary.allPassed) {
        oss << "  STATUS: ✓ ALL GREEN — Ready for Phase 33 Packaging\n";
    } else {
        oss << "  STATUS: ✗ FAILURES DETECTED — Fix before packaging\n";
    }
    oss << "\n";

    appendToOutput(oss.str());

    // Show in gauntlet window
    cmdGauntletShowResults(summary);
}

void Win32IDE::cmdGauntletShowResults(const GauntletSummary& summary) {
    // If already open, destroy and recreate with fresh results
    if (m_hwndGauntletWindow && IsWindow(m_hwndGauntletWindow)) {
        DestroyWindow(m_hwndGauntletWindow);
        m_hwndGauntletWindow = nullptr;
    }

    if (!ensureGauntletWindowClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register gauntlet window class.",
                     L"Gauntlet Error", MB_OK | MB_ICONERROR);
        return;
    }

    GauntletWindowState* state = new GauntletWindowState();
    state->ide = this;
    state->summary = summary;

    m_hwndGauntletWindow = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        GAUNTLET_WINDOW_CLASS,
        L"RawrXD — Phase 32: The Final Gauntlet",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 860, 480,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr),
        state);

    if (m_hwndGauntletWindow) {
        // Populate the ListView
        populateGauntletListView(state->hwndListView, summary);
        ShowWindow(m_hwndGauntletWindow, SW_SHOW);
        UpdateWindow(m_hwndGauntletWindow);
    }
}

void Win32IDE::cmdGauntletExport() {
    OutputDebugStringA("[Phase 32] Running gauntlet and exporting report...\n");

    GauntletSummary summary = runFinalGauntlet();

    // Build text report
    std::ostringstream oss;
    oss << "=====================================\n";
    oss << "RawrXD Phase 32: Final Gauntlet Report\n";
    oss << "=====================================\n\n";

    for (int i = 0; i < summary.totalTests; ++i) {
        const GauntletResult& r = summary.results[i];
        char line[512];
        snprintf(line, sizeof(line),
                 "Test %2d [%s]: %-30s  %.2f ms  err=%d\n",
                 i + 1,
                 r.passed ? "PASS" : "FAIL",
                 r.testName ? r.testName : "???",
                 r.elapsedMs,
                 r.errorCode);
        oss << line;
        if (!r.passed && r.detail) {
            oss << "  Detail: " << r.detail << "\n";
        }
    }

    oss << "\nSummary: " << summary.passed << "/" << summary.totalTests
        << " passed, " << summary.failed << " failed, "
        << summary.totalElapsedMs << " ms total\n";
    oss << "Verdict: " << (summary.allPassed ? "ALL GREEN" : "FAILURES DETECTED") << "\n";

    // Write to file
    const char* reportPath = "gauntlet_report.txt";
    std::ofstream fout(reportPath);
    if (fout.is_open()) {
        fout << oss.str();
        fout.close();
        std::string msg = "[Phase 32] Report exported to: " + std::string(reportPath) + "\n";
        appendToOutput(msg);
        MessageBoxA(m_hwndMain, "Gauntlet report exported to gauntlet_report.txt",
                    "Export Complete", MB_OK | MB_ICONINFORMATION);
    } else {
        appendToOutput("[Phase 32] ERROR: Failed to write gauntlet report\n");
        MessageBoxA(m_hwndMain, "Failed to write gauntlet report.",
                    "Export Error", MB_OK | MB_ICONERROR);
    }
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleGauntletCommand(int commandId) {
    switch (commandId) {
        case IDM_GAUNTLET_RUN:     cmdGauntletRun();    return true;
        case IDM_GAUNTLET_EXPORT:  cmdGauntletExport(); return true;
        default: return false;
    }
}
