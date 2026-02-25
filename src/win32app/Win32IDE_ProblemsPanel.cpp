// Win32IDE_ProblemsPanel.cpp — Unified Problems Panel (P0)
// Aggregates: LSP, SAST, SCA, Secrets, Build. Reads from ProblemsAggregator.
#include "Win32IDE.h"
#include "core/problems_aggregator.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <string>

#pragma comment(lib, "comctl32.lib")

namespace {

WNDPROC g_origProblemsPanelProc = nullptr;
HWND g_hwndProblemsMain = nullptr;

// The project builds as ANSI (no global UNICODE define). The ListView_* helper
// macros in <commctrl.h> therefore resolve to ANSI variants (LV_ITEM/LPSTR),
// which breaks when we pass wchar_t text. Use explicit wide (W) ListView
// messages instead.
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
static inline void LV_SetItemTextW(HWND hwnd, int iItem, int iSubItem, LPWSTR pszText) {
    LVITEMW lvi{};
    lvi.iSubItem = iSubItem;
    lvi.pszText = pszText;
    SendMessageW(hwnd, LVM_SETITEMTEXTW, static_cast<WPARAM>(iItem),
        reinterpret_cast<LPARAM>(&lvi));
}
static inline int LV_InsertColumnW(HWND hwnd, int iCol, const LVCOLUMNW* col) {
    return static_cast<int>(SendMessageW(hwnd, LVM_INSERTCOLUMNW,
        static_cast<WPARAM>(iCol), reinterpret_cast<LPARAM>(col)));
}

LRESULT CALLBACK ProblemsPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND && g_hwndProblemsMain) {
        SendMessageW(g_hwndProblemsMain, WM_COMMAND, wParam, lParam);
        return 0;
    }
    return CallWindowProcW(g_origProblemsPanelProc, hWnd, msg, wParam, lParam);
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring out(static_cast<size_t>(wlen), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], wlen);
    return out;
}

Win32IDE* g_pProblemsIDE = nullptr;
// Note: g_pMainIDE is declared extern in Win32IDE.h — do not re-declare here

LRESULT CALLBACK ProblemsListSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_LBUTTONDBLCLK && g_pProblemsIDE) {
        LVHITTESTINFO hit = {};
        hit.pt.x = GET_X_LPARAM(lParam);
        hit.pt.y = GET_Y_LPARAM(lParam);
        int index = ListView_HitTest(hWnd, &hit);
        if (index >= 0)
            g_pProblemsIDE->onProblemsItemActivate(index);
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

} // namespace

void Win32IDE::initProblemsPanel() {
    if (m_problemsPanelInitialized) return;

    g_pProblemsIDE = this;
    ::g_pMainIDE = this;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(m_hwndMain, GWLP_HINSTANCE);

    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    int w = rc.right - rc.left;
    int h = 220;

    m_hwndProblemsPanel = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        0, 0, w, h, m_hwndMain, (HMENU)IDC_PROBLEMS_PANEL, hInst, nullptr);
    if (!m_hwndProblemsPanel) return;

    int yToolbar = 2;
    int listTop = 28;

    CreateWindowW(L"BUTTON", L"Errors",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        5, yToolbar, 55, 20, m_hwndProblemsPanel, (HMENU)IDC_PROBLEMS_SHOW_ERRORS, hInst, nullptr);
    SendMessageW(GetDlgItem(m_hwndProblemsPanel, IDC_PROBLEMS_SHOW_ERRORS), BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowW(L"BUTTON", L"Warnings",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        65, yToolbar, 65, 20, m_hwndProblemsPanel, (HMENU)IDC_PROBLEMS_SHOW_WARNINGS, hInst, nullptr);
    SendMessageW(GetDlgItem(m_hwndProblemsPanel, IDC_PROBLEMS_SHOW_WARNINGS), BM_SETCHECK, BST_CHECKED, 0);

    m_hwndProblemsFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        140, yToolbar, 180, 20, m_hwndProblemsPanel, (HMENU)IDC_PROBLEMS_FILTER, hInst, nullptr);

    CreateWindowW(L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 70, yToolbar, 65, 20, m_hwndProblemsPanel, (HMENU)IDC_PROBLEMS_CLEAR, hInst, nullptr);

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    m_hwndProblemsListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        0, listTop, w, h - listTop, m_hwndProblemsPanel, (HMENU)IDC_PROBLEMS_LISTVIEW, hInst, nullptr);
    if (!m_hwndProblemsListView) return;

    ListView_SetExtendedListViewStyle(m_hwndProblemsListView,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    struct { const wchar_t* text; int width; } cols[] = {
        { L"Severity", 72 },
        { L"Source", 52 },
        { L"Code", 72 },
        { L"Message", 320 },
        { L"File", 200 },
        { L"Line", 44 },
    };
    for (int i = 0; i < 6; i++) {
        col.pszText = (LPWSTR)cols[i].text;
        col.cx = cols[i].width;
        col.iSubItem = i;
        LV_InsertColumnW(m_hwndProblemsListView, i, &col);
    }

    SetWindowSubclass(m_hwndProblemsListView, ProblemsListSubclassProc, 0, 0);

    g_hwndProblemsMain = m_hwndMain;
    g_origProblemsPanelProc = (WNDPROC)SetWindowLongPtr(m_hwndProblemsPanel, GWLP_WNDPROC, (LONG_PTR)ProblemsPanelProc);

    m_problemsPanelInitialized = true;
    refreshProblemsView();
}

void Win32IDE::refreshProblemsView() {
    if (!m_hwndProblemsListView) return;

    using namespace RawrXD;
    auto& agg = ProblemsAggregator::instance();
    std::vector<ProblemEntry> all = agg.getProblems("", "");

    m_problemsViewCache.clear();
    for (const auto& p : all) {
        if (p.severity == 1 && !m_problemsShowErrors) continue;
        if (p.severity == 2 && !m_problemsShowWarnings) continue;
        if (p.severity >= 3 && !m_problemsShowInfo) continue;

        wchar_t filterBuf[256] = {};
        if (m_hwndProblemsFilter)
            GetWindowTextW(m_hwndProblemsFilter, filterBuf, 256);
        if (filterBuf[0]) {
            std::wstring f(filterBuf);
            std::wstring msg = utf8ToWide(p.message);
            std::transform(f.begin(), f.end(), f.begin(), ::towlower);
            std::transform(msg.begin(), msg.end(), msg.begin(), ::towlower);
            if (msg.find(f) == std::wstring::npos) continue;
        }
        m_problemsViewCache.push_back(p);
    }

    ListView_DeleteAllItems(m_hwndProblemsListView);

    const wchar_t* sevStr[] = { L"", L"Error", L"Warning", L"Info", L"Hint" };
    for (size_t i = 0; i < m_problemsViewCache.size(); i++) {
        const auto& p = m_problemsViewCache[i];
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = (int)i;
        item.lParam = (LPARAM)i;

        int sev = (p.severity >= 1 && p.severity <= 4) ? p.severity : 2;
        item.pszText = (LPWSTR)sevStr[sev];
        LV_InsertItemW(m_hwndProblemsListView, &item);

        { auto ws = utf8ToWide(p.source);  LV_SetItemTextW(m_hwndProblemsListView, (int)i, 1, (LPWSTR)ws.c_str()); }
        { auto ws = utf8ToWide(p.code);    LV_SetItemTextW(m_hwndProblemsListView, (int)i, 2, (LPWSTR)ws.c_str()); }
        { auto ws = utf8ToWide(p.message); LV_SetItemTextW(m_hwndProblemsListView, (int)i, 3, (LPWSTR)ws.c_str()); }
        { auto ws = utf8ToWide(p.path);    LV_SetItemTextW(m_hwndProblemsListView, (int)i, 4, (LPWSTR)ws.c_str()); }

        wchar_t lineBuf[16];
        lineBuf[0] = L'\0';
        if (p.line > 0) _itow_s(p.line, lineBuf, 10);
        LV_SetItemTextW(m_hwndProblemsListView, (int)i, 5, lineBuf);
    }

    if (m_hwndStatusBar) {
        int err = 0, warn = 0, other = 0;
        auto raw = agg.getProblems("", "");
        for (const auto& e : raw) {
            if (e.severity == 1) err++;
            else if (e.severity == 2) warn++;
            else other++;
        }
        wchar_t buf[128];
        swprintf_s(buf, L"Problems: %zu (%d errors, %d warnings, %d others)",
                   raw.size(), err, warn, other);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
    }
}

void Win32IDE::onProblemsItemActivate(int index) {
    if (index < 0 || index >= (int)m_problemsViewCache.size()) return;
    const auto& p = m_problemsViewCache[index];
    if (p.path.empty()) return;
    navigateToFileLine(p.path, p.line > 0 ? (uint32_t)p.line : 1);
}

void Win32IDE::handleProblemsCommand(int commandId) {
    switch (commandId) {
        case IDC_PROBLEMS_SHOW_ERRORS:
            m_problemsShowErrors = (SendMessageW(GetDlgItem(m_hwndProblemsPanel, IDC_PROBLEMS_SHOW_ERRORS), BM_GETCHECK, 0, 0) == BST_CHECKED);
            refreshProblemsView();
            break;
        case IDC_PROBLEMS_SHOW_WARNINGS:
            m_problemsShowWarnings = (SendMessageW(GetDlgItem(m_hwndProblemsPanel, IDC_PROBLEMS_SHOW_WARNINGS), BM_GETCHECK, 0, 0) == BST_CHECKED);
            refreshProblemsView();
            break;
        case IDC_PROBLEMS_CLEAR:
            RawrXD::ProblemsAggregator::instance().clear("");
            refreshProblemsView();
            break;
        case IDC_PROBLEMS_FILTER:
            refreshProblemsView();
            break;
        default:
            break;
    }
}
