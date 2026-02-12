// ============================================================================
// Win32IDE_TelemetryDashboard.cpp — Tier 5 Gap #46: Telemetry Dashboard
// ============================================================================
//
// PURPOSE:
//   Extends the existing Win32IDE_TelemetryPanel with a local visual dashboard
//   that displays logged telemetry events in a browsable, filterable ListView.
//   Shows event name, timestamp, category, payload size, and latency.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>

// ============================================================================
// Telemetry event model
// ============================================================================

struct TelemetryEvent {
    uint64_t    id;
    std::string timestamp;
    std::string eventName;
    std::string category;     // "ui", "perf", "error", "agent", "debug"
    std::string payload;      // JSON snippet
    double      latencyMs;
    bool        exported;
};

static std::vector<TelemetryEvent> s_telemetryEvents;
static std::mutex s_telemetryMutex;
static uint64_t s_telemetryNextId = 1;

static HWND s_hwndTelDashboard   = nullptr;
static HWND s_hwndTelListView    = nullptr;
static HWND s_hwndTelFilterCombo = nullptr;
static bool s_telDashboardClassRegistered = false;
static const wchar_t* TEL_DASHBOARD_CLASS = L"RawrXD_TelemetryDashboard";

// ============================================================================
// Add telemetry event (called from various IDE subsystems)
// ============================================================================

static void addTelemetryEvent(const std::string& name, const std::string& category,
                               const std::string& payload, double latencyMs) {
    TelemetryEvent evt;
    evt.id = s_telemetryNextId++;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf;
    localtime_s(&tmBuf, &time);
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    char fullTime[48];
    snprintf(fullTime, sizeof(fullTime), "%s.%03d", timeBuf, (int)ms.count());
    evt.timestamp = fullTime;

    evt.eventName = name;
    evt.category  = category;
    evt.payload   = payload;
    evt.latencyMs = latencyMs;
    evt.exported  = false;

    std::lock_guard<std::mutex> lock(s_telemetryMutex);
    s_telemetryEvents.push_back(evt);

    // Cap at 10000 events
    if (s_telemetryEvents.size() > 10000) {
        s_telemetryEvents.erase(s_telemetryEvents.begin());
    }
}

// ============================================================================
// ListView helpers
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

static int TD_InsertColumnW(HWND hwnd, int col, int width, const wchar_t* text) {
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt  = LVCFMT_LEFT;
    lvc.cx   = width;
    lvc.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
}

static int TD_InsertItemW(HWND hwnd, int row, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.mask     = LVIF_TEXT;
    lvi.iItem    = row;
    lvi.iSubItem = 0;
    lvi.pszText  = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

static void TD_SetItemTextW(HWND hwnd, int row, int col, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.iSubItem = col;
    lvi.pszText  = const_cast<LPWSTR>(text);
    SendMessageW(hwnd, LVM_SETITEMTEXTW, row, (LPARAM)&lvi);
}

// ============================================================================
// Refresh dashboard
// ============================================================================

static void refreshTelDashboard(const std::string& categoryFilter = "") {
    if (!s_hwndTelListView) return;
    SendMessageW(s_hwndTelListView, LVM_DELETEALLITEMS, 0, 0);

    std::lock_guard<std::mutex> lock(s_telemetryMutex);
    int row = 0;

    // Show most recent first
    for (int i = (int)s_telemetryEvents.size() - 1; i >= 0 && row < 500; --i) {
        auto& evt = s_telemetryEvents[i];

        if (!categoryFilter.empty() && categoryFilter != "All" &&
            evt.category != categoryFilter)
            continue;

        wchar_t buf[256];

        // Col 0: ID
        swprintf(buf, 256, L"%llu", (unsigned long long)evt.id);
        TD_InsertItemW(s_hwndTelListView, row, buf);

        // Col 1: Timestamp
        MultiByteToWideChar(CP_UTF8, 0, evt.timestamp.c_str(), -1, buf, 255);
        TD_SetItemTextW(s_hwndTelListView, row, 1, buf);

        // Col 2: Event Name
        MultiByteToWideChar(CP_UTF8, 0, evt.eventName.c_str(), -1, buf, 255);
        TD_SetItemTextW(s_hwndTelListView, row, 2, buf);

        // Col 3: Category
        MultiByteToWideChar(CP_UTF8, 0, evt.category.c_str(), -1, buf, 255);
        TD_SetItemTextW(s_hwndTelListView, row, 3, buf);

        // Col 4: Latency
        swprintf(buf, 256, L"%.2f ms", evt.latencyMs);
        TD_SetItemTextW(s_hwndTelListView, row, 4, buf);

        // Col 5: Payload size
        swprintf(buf, 256, L"%zu B", evt.payload.size());
        TD_SetItemTextW(s_hwndTelListView, row, 5, buf);

        ++row;
    }
}

// ============================================================================
// Custom Draw
// ============================================================================

static LRESULT handleTelCustomDraw(LPNMLVCUSTOMDRAW lpcd) {
    switch (lpcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            // Color by category via item text
            wchar_t catText[64] = {};
            LVITEMW lvi{};
            lvi.mask       = LVIF_TEXT;
            lvi.iItem      = (int)lpcd->nmcd.dwItemSpec;
            lvi.iSubItem   = 3;  // Category column
            lvi.pszText    = catText;
            lvi.cchTextMax = 63;
            SendMessageW(lpcd->nmcd.hdr.hwndFrom, LVM_GETITEMTEXTW,
                         lpcd->nmcd.dwItemSpec, (LPARAM)&lvi);

            if (wcscmp(catText, L"error") == 0) {
                lpcd->clrText = RGB(255, 80, 80);
            } else if (wcscmp(catText, L"perf") == 0) {
                lpcd->clrText = RGB(80, 200, 255);
            } else if (wcscmp(catText, L"agent") == 0) {
                lpcd->clrText = RGB(180, 130, 255);
            } else if (wcscmp(catText, L"ui") == 0) {
                lpcd->clrText = RGB(80, 200, 120);
            } else {
                lpcd->clrText = RGB(220, 220, 220);
            }
            lpcd->clrTextBk = RGB(30, 30, 30);
            return CDRF_DODEFAULT;
        }
        default: return CDRF_DODEFAULT;
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

#define IDC_TD_FILTER  6001
#define IDC_TD_CLEAR   6002
#define IDC_TD_EXPORT  6003

static LRESULT CALLBACK telDashboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Category filter combo
        s_hwndTelFilterCombo = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 7, 120, 200, hwnd, (HMENU)IDC_TD_FILTER,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"All");
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"ui");
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"perf");
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"error");
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"agent");
        SendMessageW(s_hwndTelFilterCombo, CB_ADDSTRING, 0, (LPARAM)L"debug");
        SendMessageW(s_hwndTelFilterCombo, CB_SETCURSEL, 0, 0);

        CreateWindowExW(0, L"BUTTON", L"Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, 5, 60, 28, hwnd, (HMENU)IDC_TD_CLEAR,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Export",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            205, 5, 60, 28, hwnd, (HMENU)IDC_TD_EXPORT,
            GetModuleHandleW(nullptr), nullptr);

        // ListView
        s_hwndTelListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
            0, 38, 0, 0, hwnd, (HMENU)6010,
            GetModuleHandleW(nullptr), nullptr);

        if (s_hwndTelListView) {
            SendMessageW(s_hwndTelListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
            SendMessageW(s_hwndTelListView, LVM_SETBKCOLOR,     0, RGB(30, 30, 30));
            SendMessageW(s_hwndTelListView, LVM_SETTEXTBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(s_hwndTelListView, LVM_SETTEXTCOLOR,   0, RGB(220, 220, 220));

            TD_InsertColumnW(s_hwndTelListView, 0, 50,  L"ID");
            TD_InsertColumnW(s_hwndTelListView, 1, 100, L"Timestamp");
            TD_InsertColumnW(s_hwndTelListView, 2, 200, L"Event");
            TD_InsertColumnW(s_hwndTelListView, 3, 80,  L"Category");
            TD_InsertColumnW(s_hwndTelListView, 4, 80,  L"Latency");
            TD_InsertColumnW(s_hwndTelListView, 5, 70,  L"Size");

            refreshTelDashboard();
        }
        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (s_hwndTelListView)
            MoveWindow(s_hwndTelListView, 0, 38, rc.right, rc.bottom - 38, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_TD_FILTER && HIWORD(wParam) == CBN_SELCHANGE) {
            wchar_t catW[32] = {};
            int sel = (int)SendMessageW(s_hwndTelFilterCombo, CB_GETCURSEL, 0, 0);
            SendMessageW(s_hwndTelFilterCombo, CB_GETLBTEXT, sel, (LPARAM)catW);
            char catA[32] = {};
            WideCharToMultiByte(CP_UTF8, 0, catW, -1, catA, 31, nullptr, nullptr);
            refreshTelDashboard(catA);
        } else if (wmId == IDC_TD_CLEAR) {
            std::lock_guard<std::mutex> lock(s_telemetryMutex);
            s_telemetryEvents.clear();
            refreshTelDashboard();
        } else if (wmId == IDC_TD_EXPORT) {
            // Export to file
            std::lock_guard<std::mutex> lock(s_telemetryMutex);
            std::ofstream fout("telemetry_dashboard_export.json");
            if (fout.is_open()) {
                fout << "[\n";
                for (size_t i = 0; i < s_telemetryEvents.size(); ++i) {
                    auto& e = s_telemetryEvents[i];
                    fout << "  {\"id\":" << e.id
                         << ",\"time\":\"" << e.timestamp
                         << "\",\"event\":\"" << e.eventName
                         << "\",\"cat\":\"" << e.category
                         << "\",\"latency\":" << e.latencyMs
                         << ",\"size\":" << e.payload.size() << "}";
                    if (i + 1 < s_telemetryEvents.size()) fout << ",";
                    fout << "\n";
                }
                fout << "]\n";
                fout.close();
            }
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh && nmh->code == NM_CUSTOMDRAW) {
            return handleTelCustomDraw((LPNMLVCUSTOMDRAW)lParam);
        }
        break;
    }

    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        s_hwndTelDashboard   = nullptr;
        s_hwndTelListView    = nullptr;
        s_hwndTelFilterCombo = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool ensureTelDashboardClass() {
    if (s_telDashboardClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = telDashboardWndProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName  = TEL_DASHBOARD_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_telDashboardClassRegistered = true;
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initTelemetryDashboard() {
    if (m_telemetryDashboardInitialized) return;

    // Seed some sample telemetry events
    addTelemetryEvent("ide.startup",         "perf",  "{\"phase\":\"init\"}", 245.7);
    addTelemetryEvent("editor.open",         "ui",    "{\"file\":\"main.cpp\"}", 12.3);
    addTelemetryEvent("lsp.handshake",       "perf",  "{\"server\":\"clangd\"}", 85.6);
    addTelemetryEvent("agent.query",         "agent", "{\"model\":\"gpt-4\"}", 1230.0);
    addTelemetryEvent("debug.breakpoint",    "debug", "{\"line\":42}", 0.5);
    addTelemetryEvent("compile.error",       "error", "{\"code\":\"C2664\"}", 0.1);
    addTelemetryEvent("hotpatch.apply",      "perf",  "{\"layer\":\"memory\"}", 3.2);
    addTelemetryEvent("extension.load",      "ui",    "{\"ext\":\"cpp-tools\"}", 45.0);

    OutputDebugStringA("[TelDashboard] Tier 5 — Telemetry Dashboard initialized.\n");
    m_telemetryDashboardInitialized = true;
    appendToOutput("[TelDashboard] Local telemetry dashboard ready.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleTelemetryDashboardCommand(int commandId) {
    if (!m_telemetryDashboardInitialized) initTelemetryDashboard();
    switch (commandId) {
        case IDM_TELDASH_SHOW:    cmdTelDashShow();    return true;
        case IDM_TELDASH_LOG:     cmdTelDashLog();     return true;
        case IDM_TELDASH_CLEAR:   cmdTelDashClear();   return true;
        case IDM_TELDASH_EXPORT:  cmdTelDashExport();  return true;
        case IDM_TELDASH_STATS:   cmdTelDashStats();   return true;
        default: return false;
    }
}

// ============================================================================
// Show Dashboard Window
// ============================================================================

void Win32IDE::cmdTelDashShow() {
    if (s_hwndTelDashboard && IsWindow(s_hwndTelDashboard)) {
        SetForegroundWindow(s_hwndTelDashboard);
        return;
    }

    if (!ensureTelDashboardClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register telemetry dashboard class.",
                    L"Dashboard Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndTelDashboard = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        TEL_DASHBOARD_CLASS,
        L"RawrXD — Telemetry Dashboard",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 450,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndTelDashboard) {
        ShowWindow(s_hwndTelDashboard, SW_SHOW);
        UpdateWindow(s_hwndTelDashboard);
    }
}

// ============================================================================
// Log a manual telemetry event
// ============================================================================

void Win32IDE::cmdTelDashLog() {
    addTelemetryEvent("manual.log", "ui", "{\"source\":\"command\"}", 0.0);
    refreshTelDashboard();
    appendToOutput("[TelDashboard] Manual event logged.\n");
}

// ============================================================================
// Clear all events
// ============================================================================

void Win32IDE::cmdTelDashClear() {
    {
        std::lock_guard<std::mutex> lock(s_telemetryMutex);
        s_telemetryEvents.clear();
    }
    refreshTelDashboard();
    appendToOutput("[TelDashboard] All telemetry events cleared.\n");
}

// ============================================================================
// Export events
// ============================================================================

void Win32IDE::cmdTelDashExport() {
    std::lock_guard<std::mutex> lock(s_telemetryMutex);

    std::ofstream fout("telemetry_export.json");
    if (!fout.is_open()) {
        appendToOutput("[TelDashboard] ERROR: Failed to write export file.\n");
        return;
    }

    fout << "[\n";
    for (size_t i = 0; i < s_telemetryEvents.size(); ++i) {
        auto& e = s_telemetryEvents[i];
        fout << "  {\"id\":" << e.id
             << ",\"time\":\"" << e.timestamp
             << "\",\"event\":\"" << e.eventName
             << "\",\"category\":\"" << e.category
             << "\",\"latencyMs\":" << e.latencyMs
             << ",\"payloadSize\":" << e.payload.size() << "}";
        if (i + 1 < s_telemetryEvents.size()) fout << ",";
        fout << "\n";
    }
    fout << "]\n";
    fout.close();

    appendToOutput("[TelDashboard] Exported " + std::to_string(s_telemetryEvents.size()) +
                   " events to telemetry_export.json\n");
}

// ============================================================================
// Statistics summary
// ============================================================================

void Win32IDE::cmdTelDashStats() {
    std::lock_guard<std::mutex> lock(s_telemetryMutex);

    int total = (int)s_telemetryEvents.size();
    double avgLatency = 0;
    std::unordered_map<std::string, int> catCounts;

    for (auto& e : s_telemetryEvents) {
        avgLatency += e.latencyMs;
        catCounts[e.category]++;
    }
    if (total > 0) avgLatency /= total;

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║             TELEMETRY DASHBOARD STATISTICS                 ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total Events:      " << std::setw(10) << total
        << "                          ║\n"
        << "║  Avg Latency:       " << std::setw(10) << std::fixed << std::setprecision(2)
        << avgLatency << " ms                     ║\n";

    for (auto& [cat, count] : catCounts) {
        char line[128];
        snprintf(line, sizeof(line), "║  %-18s  %10d                          ║\n",
                 cat.c_str(), count);
        oss << line;
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}
