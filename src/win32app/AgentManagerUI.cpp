// AgentManagerUI.cpp — Win32 multi-agent dashboard panel
// A dockable child window with a SysListView32 agent table and a log edit.

#include "AgentManagerUI.h"
#include <commctrl.h>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {

AgentManagerUI& AgentManagerUI::instance() {
    static AgentManagerUI s_instance;
    return s_instance;
}

bool AgentManagerUI::create(HWND hwndParent, int x, int y, int w, int h) {
    if (m_hwndPanel) return true;
    if (!hwndParent) return false;

    // Ensure Common Controls are initialized.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    // Create container panel (static parent for layout).
    m_hwndPanel = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, w, h,
        hwndParent, nullptr,
        (HINSTANCE)GetWindowLongPtrW(hwndParent, GWLP_HINSTANCE),
        nullptr);
    if (!m_hwndPanel) return false;

    const int btnH    = 28;
    const int logH    = 120;
    const int listH   = h - logH - btnH - 12;

    // SysListView32 for agent table.
    m_hwndList = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        0, 0, w, listH,
        m_hwndPanel, (HMENU)(intptr_t)ID_LIST,
        (HINSTANCE)GetWindowLongPtrW(hwndParent, GWLP_HINSTANCE),
        nullptr);

    if (m_hwndList) {
        ListView_SetExtendedListViewStyle(m_hwndList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // Columns: ID | Name | Model | Status | Tools | Tokens
        auto addCol = [&](int i, const wchar_t* text, int cx) {
            LVCOLUMNW col{};
            col.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.iSubItem = i;
            col.pszText = const_cast<wchar_t*>(text);
            col.cx      = cx;
            ListView_InsertColumn(m_hwndList, i, &col);
        };
        addCol(0, L"ID",     45);
        addCol(1, L"Name",   90);
        addCol(2, L"Model", 140);
        addCol(3, L"Status", 70);
        addCol(4, L"Tools",  55);
        addCol(5, L"Tokens", 75);
    }

    // Buttons.
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwndParent, GWLP_HINSTANCE);
    m_hwndBtnStart = CreateWindowW(L"BUTTON", L"Start Agent",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, listH + 4, 110, btnH,
        m_hwndPanel, (HMENU)(intptr_t)ID_BTN_START, hInst, nullptr);

    m_hwndBtnStop = CreateWindowW(L"BUTTON", L"Stop Agent",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        116, listH + 4, 110, btnH,
        m_hwndPanel, (HMENU)(intptr_t)ID_BTN_STOP, hInst, nullptr);

    // Log edit (multiline, read-only, scrollable).
    m_hwndLog = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, listH + btnH + 8, w, logH,
        m_hwndPanel, (HMENU)(intptr_t)ID_LOG, hInst, nullptr);

    if (m_hwndLog) {
        SendMessageW(m_hwndLog, EM_LIMITTEXT, 65536, 0);
        HFONT hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
        SendMessageW(m_hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    return true;
}

void AgentManagerUI::destroy() {
    if (m_hwndPanel) {
        DestroyWindow(m_hwndPanel);
        m_hwndPanel    = nullptr;
        m_hwndList     = nullptr;
        m_hwndLog      = nullptr;
        m_hwndBtnStart = nullptr;
        m_hwndBtnStop  = nullptr;
    }
}

void AgentManagerUI::refresh(const std::vector<AgentRecord>& agents) {
    if (!m_hwndList) return;
    ListView_DeleteAllItems(m_hwndList);
    populateListView(agents);
}

void AgentManagerUI::populateListView(const std::vector<AgentRecord>& agents) {
    if (!m_hwndList) return;

    int row = 0;
    for (const auto& a : agents) {
        wchar_t wbuf[256];

        LVITEMW item{};
        item.mask    = LVIF_TEXT;
        item.iItem   = row;
        item.iSubItem = 0;
        swprintf_s(wbuf, L"%u", a.id);
        item.pszText = wbuf;
        ListView_InsertItem(m_hwndList, &item);

        auto setCol = [&](int col, const std::string& s) {
            // Convert narrow to wide for ListView
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuf, 256);
            ListView_SetItemText(m_hwndList, row, col, wbuf);
        };
        setCol(1, a.name);
        setCol(2, a.model);
        setCol(3, a.status);

        swprintf_s(wbuf, L"%u", a.toolCalls);
        ListView_SetItemText(m_hwndList, row, 4, wbuf);
        swprintf_s(wbuf, L"%llu", (unsigned long long)a.tokensTotal);
        ListView_SetItemText(m_hwndList, row, 5, wbuf);

        ++row;
    }
}

void AgentManagerUI::appendLog(const std::string& line) {
    if (!m_hwndLog) return;
    // Append to end of edit control.
    int len = GetWindowTextLengthA(m_hwndLog);
    SendMessageA(m_hwndLog, EM_SETSEL, len, len);
    std::string entry = line + "\r\n";
    SendMessageA(m_hwndLog, EM_REPLACESEL, FALSE, (LPARAM)entry.c_str());
}

bool AgentManagerUI::handleCommand(WPARAM wParam, LPARAM /*lParam*/) {
    int id = LOWORD(wParam);
    if (id == ID_BTN_START) {
        appendLog("[AgentManager] Start Agent requested");
        return true;
    }
    if (id == ID_BTN_STOP) {
        appendLog("[AgentManager] Stop Agent requested");
        return true;
    }
    return false;
}

void AgentManagerUI::resize(int x, int y, int w, int h) {
    if (!m_hwndPanel) return;

    MoveWindow(m_hwndPanel, x, y, w, h, TRUE);

    const int btnH  = 28;
    const int logH  = 120;
    const int listH = h - logH - btnH - 12;

    if (m_hwndList)     MoveWindow(m_hwndList,     0, 0,         w,  listH, TRUE);
    if (m_hwndBtnStart) MoveWindow(m_hwndBtnStart, 0, listH + 4, 110, btnH, TRUE);
    if (m_hwndBtnStop)  MoveWindow(m_hwndBtnStop,  116, listH + 4, 110, btnH, TRUE);
    if (m_hwndLog)      MoveWindow(m_hwndLog,      0, listH + btnH + 8, w, logH, TRUE);
}

} // namespace RawrXD
