// ============================================================================
// Win32IDE_AuditDashboard.cpp — Phase 31: Audit Dashboard UI
// ============================================================================
//
// PURPOSE:
//   Win32IDE integration for the IDE Self-Audit & Verification System.
//   Provides:
//     1. Audit system initialization during IDE startup
//     2. Command routing for IDM_AUDIT_* commands (9500 range)
//     3. ListView-based audit dashboard window
//     4. Full audit runner (stub detection + menu wire check + component tests)
//     5. Report generation and export
//     6. Quick-stats status bar integration
//
// UI:
//   Creates a modeless dialog with a WC_LISTVIEW showing all registered
//   features, their status, stub detection result, menu wiring, and
//   runtime test outcome. Color-coded rows (green=complete, yellow=partial,
//   red=stub/broken).
//
// PATTERN:   No exceptions. PatchResult-compatible returns where applicable.
// THREADING: All UI calls on main thread (STA).
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

// This file uses Wide (W-suffix) Win32 APIs throughout.
// The project builds as ANSI (no global UNICODE define), so we use explicit
// W-suffix message constants (LVM_SETITEMTEXTW, LVM_INSERTITEMW, etc.)
// instead of relying on the ListView_* macros which resolve to ANSI.

#include "Win32IDE.h"
#include "../../include/feature_registry.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <commctrl.h>
#include <commdlg.h>     // GetSaveFileNameW
#include <shlobj.h>
#include <windowsx.h>

// Link against common controls v6
#pragma comment(lib, "comctl32.lib")

// ---------------------------------------------------------------------------
// Explicit Wide ListView helpers for ANSI build
// ---------------------------------------------------------------------------
// The project is built without UNICODE, so ListView_SetItemText etc. resolve
// to their ANSI variants.  This file uses wchar_t throughout, so we send the
// W messages directly.
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW  (LVM_FIRST + 77)
#endif
#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif

static inline int LV_InsertItemW(HWND hwnd, const LVITEMW* item) {
    return static_cast<int>(SendMessageW(hwnd, LVM_INSERTITEMW, 0,
                                          reinterpret_cast<LPARAM>(item)));
}
static inline void LV_SetItemTextW(HWND hwnd, int iItem, int iSubItem,
                                    LPWSTR pszText) {
    LVITEMW lvi{};
    lvi.iSubItem = iSubItem;
    lvi.pszText = pszText;
    SendMessageW(hwnd, LVM_SETITEMTEXTW, static_cast<WPARAM>(iItem),
                 reinterpret_cast<LPARAM>(&lvi));
}
static inline int LV_InsertColumnW(HWND hwnd, int iCol, const LVCOLUMNW* col) {
    return static_cast<int>(SendMessageW(hwnd, LVM_INSERTCOLUMNW,
                                          static_cast<WPARAM>(iCol),
                                          reinterpret_cast<LPARAM>(col)));
}
// ---------------------------------------------------------------------------

// Forward declarations from menu_auditor.cpp
namespace RawrXD { namespace Audit {
    bool verifyCommandInMenu(HMENU hMenu, int commandId);
    std::string buildMenuBreadcrumb(HMENU hMenu, int commandId);
    std::string getMenuWiringReport(HMENU hMenu);
    std::vector<std::string> findOrphanedCommands(HMENU hMenu);
    std::vector<int> findUnregisteredMenuItems(HMENU hMenu);
} }

// ============================================================================
// CONSTANTS
// ============================================================================
static const wchar_t* AUDIT_WINDOW_CLASS = L"RawrXD_AuditDashboard";
static const int AUDIT_LISTVIEW_ID = 11001;
static const int AUDIT_BTN_RUN_ID = 11002;
static const int AUDIT_BTN_EXPORT_ID = 11003;
static const int AUDIT_BTN_REFRESH_ID = 11004;
static const int AUDIT_BTN_CLOSE_ID = 11005;
static const int AUDIT_STATUS_ID = 11006;

// ============================================================================
// AUDIT DASHBOARD STATE (file-scope, owned by Win32IDE via HWND)
// ============================================================================
struct AuditDashboardState {
    HWND hwndListView = nullptr;
    HWND hwndStatusBar = nullptr;
    HWND hwndBtnRun = nullptr;
    HWND hwndBtnExport = nullptr;
    HWND hwndBtnRefresh = nullptr;
    HWND hwndBtnClose = nullptr;
    Win32IDE* ide = nullptr;
    bool auditRun = false;
};

// Store per-window state via GWLP_USERDATA
static AuditDashboardState* getAuditState(HWND hwnd) {
    return reinterpret_cast<AuditDashboardState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// ============================================================================
// LISTVIEW POPULATION
// ============================================================================
static void populateAuditListView(AuditDashboardState* state) {
    if (!state || !state->hwndListView) return;

    // Clear existing items
    ListView_DeleteAllItems(state->hwndListView);

    auto features = FeatureRegistry::instance().getAllFeatures();

    for (size_t i = 0; i < features.size(); ++i) {
        const FeatureEntry& f = features[i];

        // Column 0: Feature Name
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.lParam = static_cast<LPARAM>(i);

        wchar_t nameBuf[128] = {};
        if (f.name) {
            MultiByteToWideChar(CP_UTF8, 0, f.name, -1, nameBuf, 127);
        }
        lvi.pszText = nameBuf;
        int idx = LV_InsertItemW(state->hwndListView, &lvi);
        if (idx < 0) continue;

        // Column 1: Category
        wchar_t catBuf[32] = {};
        MultiByteToWideChar(CP_UTF8, 0, featureCategoryToString(f.category), -1, catBuf, 31);
        LV_SetItemTextW(state->hwndListView, idx, 1, catBuf);

        // Column 2: Status
        wchar_t statusBuf[32] = {};
        MultiByteToWideChar(CP_UTF8, 0, implStatusToString(f.status), -1, statusBuf, 31);
        LV_SetItemTextW(state->hwndListView, idx, 2, statusBuf);

        // Column 3: Phase
        wchar_t phaseBuf[32] = {};
        if (f.phase) {
            MultiByteToWideChar(CP_UTF8, 0, f.phase, -1, phaseBuf, 31);
        } else {
            wcscpy_s(phaseBuf, L"—");
        }
        LV_SetItemTextW(state->hwndListView, idx, 3, phaseBuf);

        // Column 4: Menu Wired
        LV_SetItemTextW(state->hwndListView, idx, 4,
                         const_cast<LPWSTR>(f.menuWired ? L"YES" : L"NO"));

        // Column 5: IDM
        wchar_t idmBuf[16] = {};
        if (f.commandId != 0) {
            _snwprintf_s(idmBuf, 15, L"%d", f.commandId);
        } else {
            wcscpy_s(idmBuf, L"—");
        }
        LV_SetItemTextW(state->hwndListView, idx, 5, idmBuf);

        // Column 6: Stub?
        LV_SetItemTextW(state->hwndListView, idx, 6,
                         const_cast<LPWSTR>(f.stubDetected ? L"STUB" : L"OK"));

        // Column 7: Runtime Test
        LV_SetItemTextW(state->hwndListView, idx, 7,
                         const_cast<LPWSTR>(f.runtimeTested ? L"PASS" : L"—"));
    }

    // Update status bar
    if (state->hwndStatusBar) {
        float pct = FeatureRegistry::instance().getCompletionPercentage();
        size_t total = FeatureRegistry::instance().getFeatureCount();
        size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
        size_t complete = FeatureRegistry::instance().getCountByStatus(ImplStatus::Complete);

        wchar_t statusBuf[256];
        _snwprintf_s(statusBuf, 255,
                      L"Features: %zu  |  Complete: %zu  |  Stubs: %zu  |  Completion: %.1f%%",
                      total, complete, stubs, pct * 100.0f);
        SetWindowTextW(state->hwndStatusBar, statusBuf);
    }
}

// ============================================================================
// CUSTOM DRAW — Color-coded rows based on status
// ============================================================================
static LRESULT handleCustomDraw(LPNMLVCUSTOMDRAW lpcd) {
    switch (lpcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT: {
            // Get the feature index from lParam
            size_t idx = static_cast<size_t>(lpcd->nmcd.lItemlParam);
            auto features = FeatureRegistry::instance().getAllFeatures();
            if (idx < features.size()) {
                const FeatureEntry& f = features[idx];
                switch (f.status) {
                    case ImplStatus::Complete:
                        lpcd->clrText = RGB(0, 128, 0);     // Green
                        lpcd->clrTextBk = RGB(240, 255, 240);
                        break;
                    case ImplStatus::Partial:
                        lpcd->clrText = RGB(180, 140, 0);   // Amber
                        lpcd->clrTextBk = RGB(255, 255, 230);
                        break;
                    case ImplStatus::Stub:
                        lpcd->clrText = RGB(200, 0, 0);     // Red
                        lpcd->clrTextBk = RGB(255, 235, 235);
                        break;
                    case ImplStatus::Broken:
                        lpcd->clrText = RGB(255, 255, 255);
                        lpcd->clrTextBk = RGB(180, 0, 0);   // Dark red bg
                        break;
                    case ImplStatus::Untested:
                        lpcd->clrText = RGB(100, 100, 100);  // Gray
                        lpcd->clrTextBk = RGB(245, 245, 245);
                        break;
                    case ImplStatus::Deprecated:
                        lpcd->clrText = RGB(150, 150, 150);  // Light gray
                        lpcd->clrTextBk = RGB(240, 240, 240);
                        break;
                    default:
                        break;
                }

                // Override: if stub was detected at runtime, force red text
                if (f.stubDetected) {
                    lpcd->clrText = RGB(200, 0, 0);
                }
            }
            return CDRF_DODEFAULT;
        }

        default:
            return CDRF_DODEFAULT;
    }
}

// ============================================================================
// EXPORT REPORT TO FILE
// ============================================================================
static void exportAuditReport(HWND hwndParent) {
    std::string report = FeatureRegistry::instance().generateReport();

    // Get save filename
    wchar_t filePath[MAX_PATH] = L"RawrXD_AuditReport.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwndParent;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) {
        return;  // User cancelled
    }

    // Write report
    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwndParent, L"Failed to create report file.",
                     L"Audit Export Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, report.c_str(), static_cast<DWORD>(report.size()), &written, nullptr);
    CloseHandle(hFile);

    wchar_t msg[MAX_PATH + 64];
    _snwprintf_s(msg, MAX_PATH + 63, L"Report exported to:\n%s", filePath);
    MessageBoxW(hwndParent, msg, L"Audit Export", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================
static LRESULT CALLBACK auditDashboardWndProc(HWND hwnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            AuditDashboardState* state =
                reinterpret_cast<AuditDashboardState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            // Get client area
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            // Create ListView
            state->hwndListView = CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER |
                LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                0, 0, w, h - 70,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_LISTVIEW_ID)),
                GetModuleHandleW(nullptr), nullptr);

            // Extended ListView styles
            ListView_SetExtendedListViewStyle(state->hwndListView,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            // Add columns
            struct ColDef { const wchar_t* name; int width; };
            ColDef columns[] = {
                { L"Feature",   200 },
                { L"Category",  100 },
                { L"Status",     80 },
                { L"Phase",      70 },
                { L"Menu",       50 },
                { L"IDM",        50 },
                { L"Stub",       50 },
                { L"Test",       50 },
            };

            for (int c = 0; c < 8; ++c) {
                LVCOLUMNW lvc{};
                lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                lvc.fmt = LVCFMT_LEFT;
                lvc.cx = columns[c].width;
                lvc.pszText = const_cast<LPWSTR>(columns[c].name);
                LV_InsertColumnW(state->hwndListView, c, &lvc);
            }

            // Create buttons
            int btnY = h - 60;
            int btnW = 100;
            int btnH = 30;
            int gap = 10;

            state->hwndBtnRun = CreateWindowExW(0, L"BUTTON", L"Run Audit",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                gap, btnY, btnW, btnH,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_RUN_ID)),
                GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                gap + btnW + gap, btnY, btnW, btnH,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_REFRESH_ID)),
                GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnExport = CreateWindowExW(0, L"BUTTON", L"Export Report",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                gap + (btnW + gap) * 2, btnY, btnW + 20, btnH,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_EXPORT_ID)),
                GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnClose = CreateWindowExW(0, L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                w - btnW - gap, btnY, btnW, btnH,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_CLOSE_ID)),
                GetModuleHandleW(nullptr), nullptr);

            // Status bar (bottom strip)
            state->hwndStatusBar = CreateWindowExW(0, L"STATIC", L"Ready — click 'Run Audit' to begin.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                gap, btnY + btnH + 5, w - gap * 2, 20,
                hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_STATUS_ID)),
                GetModuleHandleW(nullptr), nullptr);

            // Populate with current registry data
            populateAuditListView(state);

            return 0;
        }

        case WM_SIZE: {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state) break;

            int w = LOWORD(lParam);
            int h = HIWORD(lParam);

            // Resize ListView
            if (state->hwndListView) {
                MoveWindow(state->hwndListView, 0, 0, w, h - 70, TRUE);
            }

            // Reposition buttons
            int btnY = h - 60;
            int btnW = 100;
            int btnH = 30;
            int gap = 10;

            if (state->hwndBtnRun)
                MoveWindow(state->hwndBtnRun, gap, btnY, btnW, btnH, TRUE);
            if (state->hwndBtnRefresh)
                MoveWindow(state->hwndBtnRefresh, gap + btnW + gap, btnY, btnW, btnH, TRUE);
            if (state->hwndBtnExport)
                MoveWindow(state->hwndBtnExport, gap + (btnW + gap) * 2, btnY, btnW + 20, btnH, TRUE);
            if (state->hwndBtnClose)
                MoveWindow(state->hwndBtnClose, w - btnW - gap, btnY, btnW, btnH, TRUE);
            if (state->hwndStatusBar)
                MoveWindow(state->hwndStatusBar, gap, btnY + btnH + 5, w - gap * 2, 20, TRUE);

            return 0;
        }

        case WM_COMMAND: {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state) break;

            int id = LOWORD(wParam);

            if (id == AUDIT_BTN_RUN_ID) {
                // Run full audit with auto-discovery refresh
                SetWindowTextW(state->hwndStatusBar, L"Running auto-discovery...");
                UpdateWindow(state->hwndStatusBar);

                // Re-run auto-discovery to pick up any runtime changes
                if (state->ide) {
                    AutoDiscoveryEngine::instance().discoverAll(state->ide->getMainWindow());
                }

                SetWindowTextW(state->hwndStatusBar, L"Running stub detection...");
                UpdateWindow(state->hwndStatusBar);

                FeatureRegistry::instance().detectStubs();

                SetWindowTextW(state->hwndStatusBar, L"Verifying menu wiring...");
                UpdateWindow(state->hwndStatusBar);

                if (state->ide) {
                    HMENU hMenu = GetMenu(state->ide->getMainWindow());
                    if (hMenu) {
                        FeatureRegistry::instance().verifyMenuWiring(hMenu);
                    }
                }

                SetWindowTextW(state->hwndStatusBar, L"Running component tests...");
                UpdateWindow(state->hwndStatusBar);

                FeatureRegistry::instance().runComponentTests();

                state->auditRun = true;

                // Refresh display
                populateAuditListView(state);

                SetWindowTextW(state->hwndStatusBar, L"Audit complete (auto-discovery refreshed).");
                return 0;
            }

            if (id == AUDIT_BTN_REFRESH_ID) {
                populateAuditListView(state);
                return 0;
            }

            if (id == AUDIT_BTN_EXPORT_ID) {
                exportAuditReport(hwnd);
                return 0;
            }

            if (id == AUDIT_BTN_CLOSE_ID) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_NOTIFY: {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state) break;

            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == AUDIT_LISTVIEW_ID &&
                nmhdr->code == NM_CUSTOMDRAW) {
                LPNMLVCUSTOMDRAW lpcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
                LRESULT result = handleCustomDraw(lpcd);
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, result);
                return result;
            }
            break;
        }

        case WM_DESTROY: {
            AuditDashboardState* state = getAuditState(hwnd);
            if (state) {
                // Notify IDE that dashboard is closed
                if (state->ide) {
                    // IDE will detect via m_hwndAuditDashboard == nullptr check
                }
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// REGISTER WINDOW CLASS (called once)
// ============================================================================
static bool s_auditClassRegistered = false;

static bool ensureAuditWindowClass() {
    if (s_auditClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = auditDashboardWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = AUDIT_WINDOW_CLASS;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (RegisterClassExW(&wc)) {
        s_auditClassRegistered = true;
        return true;
    }

    // May already be registered
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        s_auditClassRegistered = true;
        return true;
    }

    return false;
}

// ============================================================================
// Win32IDE INTEGRATION — Command Handlers
// ============================================================================

void Win32IDE::initAuditSystem() {
    if (m_auditInitialized) return;

    OutputDebugStringA("[Phase 31] Initializing audit system with auto-discovery...\n");

    // Run the auto-discovery engine — this populates the FeatureRegistry
    // with ALL known IDM_* commands, checks menu wiring, detects stubs,
    // and auto-classifies status. Zero manual RAW_REGISTER_FEATURE() needed.
    AutoDiscoveryEngine::instance().discoverAll(m_hwndMain);

    m_auditInitialized = true;

    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf),
             "[Phase 31] Audit system initialized: %zu features discovered, "
             "%.1f%% completion.\n",
             FeatureRegistry::instance().getFeatureCount(),
             FeatureRegistry::instance().getCompletionPercentage() * 100.0f);
    OutputDebugStringA(logBuf);
}

bool Win32IDE::handleAuditCommand(int commandId) {
    switch (commandId) {
        case IDM_AUDIT_SHOW_DASHBOARD:  cmdAuditShowDashboard();  return true;
        case IDM_AUDIT_RUN_FULL:        cmdAuditRunFull();        return true;
        case IDM_AUDIT_DETECT_STUBS:    cmdAuditDetectStubs();    return true;
        case IDM_AUDIT_CHECK_MENUS:     cmdAuditCheckMenus();     return true;
        case IDM_AUDIT_RUN_TESTS:       cmdAuditRunTests();       return true;
        case IDM_AUDIT_EXPORT_REPORT:   cmdAuditExportReport();   return true;
        case IDM_AUDIT_QUICK_STATS:     cmdAuditQuickStats();     return true;
        default: return false;
    }
}

void Win32IDE::cmdAuditShowDashboard() {
    // If already open, bring to front
    if (m_hwndAuditDashboard && IsWindow(m_hwndAuditDashboard)) {
        SetForegroundWindow(m_hwndAuditDashboard);
        return;
    }

    if (!ensureAuditWindowClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register audit window class.",
                     L"Audit Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Create state
    AuditDashboardState* state = new AuditDashboardState();
    state->ide = this;

    // Create window
    m_hwndAuditDashboard = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        AUDIT_WINDOW_CLASS,
        L"RawrXD — IDE Audit Dashboard (Phase 31)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr),
        state);

    if (m_hwndAuditDashboard) {
        ShowWindow(m_hwndAuditDashboard, SW_SHOW);
        UpdateWindow(m_hwndAuditDashboard);
    }
}

void Win32IDE::cmdAuditRunFull() {
    OutputDebugStringA("[Phase 31] Running full audit with auto-discovery...\n");

    // 0. Run auto-discovery (re-scans everything fresh)
    AutoDiscoveryEngine::instance().discoverAll(m_hwndMain);

    // 1. Detect stubs
    FeatureRegistry::instance().detectStubs();

    // 2. Verify menu wiring
    HMENU hMenu = GetMenu(m_hwndMain);
    if (hMenu) {
        FeatureRegistry::instance().verifyMenuWiring(hMenu);
    }

    // 3. Run component tests
    FeatureRegistry::instance().runComponentTests();

    // 4. Show results in dashboard
    cmdAuditShowDashboard();
}

void Win32IDE::cmdAuditDetectStubs() {
    FeatureRegistry::instance().detectStubs();

    size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
    size_t total = FeatureRegistry::instance().getFeatureCount();

    wchar_t msg[256];
    _snwprintf_s(msg, 255,
                  L"Stub Detection Complete\n\n"
                  L"Total Features: %zu\n"
                  L"Stubs Found: %zu",
                  total, stubs);
    MessageBoxW(m_hwndMain, msg, L"Stub Detection", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditCheckMenus() {
    HMENU hMenu = GetMenu(m_hwndMain);
    if (!hMenu) {
        MessageBoxW(m_hwndMain, L"No menu bar found.", L"Menu Audit", MB_OK | MB_ICONWARNING);
        return;
    }

    FeatureRegistry::instance().verifyMenuWiring(hMenu);

    std::string report = RawrXD::Audit::getMenuWiringReport(hMenu);

    // Show in a message box (truncated if too long)
    if (report.size() > 4000) {
        report = report.substr(0, 4000) + "\n\n... (truncated, use Export for full report)";
    }

    wchar_t wReport[4200] = {};
    MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, wReport, 4199);
    MessageBoxW(m_hwndMain, wReport, L"Menu Wiring Audit", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditRunTests() {
    FeatureRegistry::instance().runComponentTests();

    auto features = FeatureRegistry::instance().getAllFeatures();
    int passed = 0, failed = 0, noTest = 0;
    for (const auto& f : features) {
        if (f.runtimeTested) passed++;
        else noTest++;
    }

    wchar_t msg[256];
    _snwprintf_s(msg, 255,
                  L"Component Tests Complete\n\n"
                  L"Passed: %d\n"
                  L"No Test: %d",
                  passed, noTest);
    MessageBoxW(m_hwndMain, msg, L"Component Tests", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditExportReport() {
    std::string report = FeatureRegistry::instance().generateReport();

    // Append menu wiring report
    HMENU hMenu = GetMenu(m_hwndMain);
    if (hMenu) {
        report += "\n\n";
        report += RawrXD::Audit::getMenuWiringReport(hMenu);
    }

    // Save to file
    wchar_t filePath[MAX_PATH] = L"RawrXD_AuditReport.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) return;

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(m_hwndMain, L"Failed to create report file.",
                     L"Export Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, report.c_str(), static_cast<DWORD>(report.size()), &written, nullptr);
    CloseHandle(hFile);

    wchar_t msg[MAX_PATH + 64];
    _snwprintf_s(msg, MAX_PATH + 63, L"Report exported to:\n%s", filePath);
    MessageBoxW(m_hwndMain, msg, L"Audit Export", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditQuickStats() {
    float pct = FeatureRegistry::instance().getCompletionPercentage();
    size_t total = FeatureRegistry::instance().getFeatureCount();
    size_t complete = FeatureRegistry::instance().getCountByStatus(ImplStatus::Complete);
    size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
    size_t partial = FeatureRegistry::instance().getCountByStatus(ImplStatus::Partial);
    size_t broken = FeatureRegistry::instance().getCountByStatus(ImplStatus::Broken);

    wchar_t msg[512];
    _snwprintf_s(msg, 511,
                  L"RawrXD IDE — Quick Stats\n\n"
                  L"Registered Features: %zu\n"
                  L"Complete:  %zu\n"
                  L"Partial:   %zu\n"
                  L"Stubs:     %zu\n"
                  L"Broken:    %zu\n\n"
                  L"Overall Completion: %.1f%%",
                  total, complete, partial, stubs, broken, pct * 100.0f);
    MessageBoxW(m_hwndMain, msg, L"IDE Quick Stats", MB_OK | MB_ICONINFORMATION);
}
