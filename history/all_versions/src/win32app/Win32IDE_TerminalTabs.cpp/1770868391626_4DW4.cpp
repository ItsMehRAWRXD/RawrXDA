// ============================================================================
// Win32IDE_TerminalTabs.cpp — Feature 12: Integrated Terminal Tabs
// Multi-tab terminal with profile switching (PowerShell, CMD, Git Bash, WSL)
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF TERMTAB_BG           = RGB(37, 37, 38);
static const COLORREF TERMTAB_ACTIVE_BG    = RGB(30, 30, 30);
static const COLORREF TERMTAB_HOVER_BG     = RGB(45, 45, 46);
static const COLORREF TERMTAB_TEXT         = RGB(204, 204, 204);
static const COLORREF TERMTAB_ACTIVE_TEXT  = RGB(255, 255, 255);
static const COLORREF TERMTAB_BORDER       = RGB(60, 60, 60);
static const COLORREF TERMTAB_ADD_COLOR    = RGB(100, 200, 100);
static const COLORREF TERMINAL_OUTPUT_BG   = RGB(1, 36, 86);   // PowerShell blue

// ── Profile definitions ────────────────────────────────────────────────────
struct TerminalProfile {
    const char* name;
    const char* executable;
    const char* args;
    COLORREF bgColor;
};

static const TerminalProfile PROFILES[] = {
    { "PowerShell", "powershell.exe", "-NoLogo", RGB(1, 36, 86) },
    { "CMD",        "cmd.exe",        "/K",      RGB(12, 12, 12) },
    { "Git Bash",   "C:\\Program Files\\Git\\bin\\bash.exe", "--login", RGB(16, 16, 16) },
    { "WSL",        "wsl.exe",        "",        RGB(16, 16, 16) },
};
static const int NUM_PROFILES = sizeof(PROFILES) / sizeof(PROFILES[0]);

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initTerminalTabs() {
    m_terminalTabs.clear();
    m_activeTerminalTab = -1;
    LOG_INFO("[TerminalTabs] Initialized");
}

void Win32IDE::shutdownTerminalTabs() {
    for (auto& tab : m_terminalTabs) {
        if (tab.hwndContent) {
            DestroyWindow(tab.hwndContent);
            tab.hwndContent = nullptr;
        }
        if (tab.hProcess) {
            TerminateProcess(tab.hProcess, 0);
            CloseHandle(tab.hProcess);
            tab.hProcess = nullptr;
        }
        if (tab.hWritePipe) { CloseHandle(tab.hWritePipe); tab.hWritePipe = nullptr; }
        if (tab.hReadPipe)  { CloseHandle(tab.hReadPipe);  tab.hReadPipe = nullptr; }
    }
    m_terminalTabs.clear();
    m_activeTerminalTab = -1;
    if (m_hwndTermTabBar) {
        DestroyWindow(m_hwndTermTabBar);
        m_hwndTermTabBar = nullptr;
    }
    LOG_INFO("[TerminalTabs] Shutdown");
}

// ── Tab Bar Creation ───────────────────────────────────────────────────────
void Win32IDE::createTerminalTabBar(HWND hwndParent) {
    if (m_hwndTermTabBar) return;

    RECT rcParent;
    GetClientRect(hwndParent, &rcParent);
    int tabBarH = dpiScale(30);

    m_hwndTermTabBar = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, rcParent.right, tabBarH, hwndParent,
        (HMENU)(UINT_PTR)IDC_TERMTAB_BAR, GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndTermTabBar, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndTermTabBar, GWLP_WNDPROC, (LONG_PTR)TermTabBarProc);

    // Add button (+)
    int addBtnW = dpiScale(28), addBtnH = dpiScale(22);
    CreateWindowExA(0, "BUTTON", "+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dpiScale(4), dpiScale(4), addBtnW, addBtnH, m_hwndTermTabBar,
        (HMENU)(UINT_PTR)IDC_TERMTAB_ADD_BTN, GetModuleHandle(NULL), NULL);

    // Profile dropdown (COMBOBOX)
    int cbX = dpiScale(36);
    HWND hCombo = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        cbX, dpiScale(3), dpiScale(110), dpiScale(200), m_hwndTermTabBar,
        (HMENU)(UINT_PTR)IDC_TERMTAB_PROFILE, GetModuleHandle(NULL), NULL);
    for (int i = 0; i < NUM_PROFILES; i++) {
        SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)PROFILES[i].name);
    }
    SendMessageA(hCombo, CB_SETCURSEL, 0, 0);

    LOG_INFO("[TerminalTabs] Tab bar created");
}

// ── Tab Operations ─────────────────────────────────────────────────────────
void Win32IDE::createTerminalTab(const std::string& profile) {
    TerminalTabInfo tab;
    tab.profile = profile;
    tab.title = profile + " " + std::to_string(m_terminalTabs.size() + 1);
    tab.active = false;

    // Find parent (PowerShell panel or main window)
    HWND hwndParent = m_hwndPowerShellPanel ? m_hwndPowerShellPanel : m_hwndMain;
    if (!m_hwndTermTabBar) {
        createTerminalTabBar(hwndParent);
    }

    RECT rcParent;
    GetClientRect(hwndParent, &rcParent);
    int tabBarH = dpiScale(30);

    // Determine background color
    COLORREF bgColor = RGB(12, 12, 12);
    for (int i = 0; i < NUM_PROFILES; i++) {
        if (profile == PROFILES[i].name) {
            bgColor = PROFILES[i].bgColor;
            break;
        }
    }

    // Create RichEdit output pane for this tab
    tab.hwndContent = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, tabBarH, rcParent.right, rcParent.bottom - tabBarH - dpiScale(28),
        hwndParent,
        (HMENU)(UINT_PTR)(IDC_TERMTAB_FIRST + (int)m_terminalTabs.size()),
        GetModuleHandle(NULL), NULL);

    // Style the output
    SendMessageA(tab.hwndContent, EM_SETBKGNDCOLOR, 0, (LPARAM)bgColor);
    HFONT termFont = CreateFontA(-dpiScale(14), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessageA(tab.hwndContent, WM_SETFONT, (WPARAM)termFont, TRUE);

    CHARFORMATA cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(204, 204, 204);
    SendMessageA(tab.hwndContent, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Create a tab button in the tab bar
    int tabIndex = (int)m_terminalTabs.size();
    int tabW = dpiScale(120);
    int tabX = dpiScale(150) + tabIndex * (tabW + dpiScale(2));
    tab.hwndTabButton = CreateWindowExA(0, "BUTTON", tab.title.c_str(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        tabX, dpiScale(2), tabW, dpiScale(26), m_hwndTermTabBar,
        (HMENU)(UINT_PTR)(IDC_TERMTAB_FIRST + tabIndex),
        GetModuleHandle(NULL), NULL);

    m_terminalTabs.push_back(tab);
    switchTerminalTab(tabIndex);

    // Append welcome message
    std::string welcome = "[" + profile + "] Terminal ready.\r\n";
    CHARRANGEA cr = {-1, -1};
    SendMessageA(tab.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessageA(tab.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)welcome.c_str());

    LOG_INFO("[TerminalTabs] Created tab: " + tab.title);
}

void Win32IDE::closeTerminalTab(int index) {
    if (index < 0 || index >= (int)m_terminalTabs.size()) return;
    auto& tab = m_terminalTabs[index];

    if (tab.hwndContent) { DestroyWindow(tab.hwndContent); tab.hwndContent = nullptr; }
    if (tab.hwndTabButton) { DestroyWindow(tab.hwndTabButton); tab.hwndTabButton = nullptr; }
    if (tab.hProcess) { TerminateProcess(tab.hProcess, 0); CloseHandle(tab.hProcess); tab.hProcess = nullptr; }
    if (tab.hWritePipe) { CloseHandle(tab.hWritePipe); tab.hWritePipe = nullptr; }
    if (tab.hReadPipe) { CloseHandle(tab.hReadPipe); tab.hReadPipe = nullptr; }

    m_terminalTabs.erase(m_terminalTabs.begin() + index);

    // Switch to previous tab if available
    if (m_activeTerminalTab >= (int)m_terminalTabs.size())
        m_activeTerminalTab = (int)m_terminalTabs.size() - 1;
    if (m_activeTerminalTab >= 0)
        switchTerminalTab(m_activeTerminalTab);

    updateTerminalTabBar();
    LOG_INFO("[TerminalTabs] Closed tab " + std::to_string(index));
}

void Win32IDE::switchTerminalTab(int index) {
    if (index < 0 || index >= (int)m_terminalTabs.size()) return;

    // Hide all tabs
    for (int i = 0; i < (int)m_terminalTabs.size(); i++) {
        m_terminalTabs[i].active = (i == index);
        if (m_terminalTabs[i].hwndContent) {
            ShowWindow(m_terminalTabs[i].hwndContent, (i == index) ? SW_SHOW : SW_HIDE);
        }
    }
    m_activeTerminalTab = index;
    updateTerminalTabBar();
    LOG_INFO("[TerminalTabs] Switched to tab " + std::to_string(index));
}

void Win32IDE::renameTerminalTab(int index, const std::string& name) {
    if (index < 0 || index >= (int)m_terminalTabs.size()) return;
    m_terminalTabs[index].title = name;
    if (m_terminalTabs[index].hwndTabButton)
        SetWindowTextA(m_terminalTabs[index].hwndTabButton, name.c_str());
    updateTerminalTabBar();
}

void Win32IDE::updateTerminalTabBar() {
    if (!m_hwndTermTabBar) return;

    // Reposition tab buttons
    int tabW = dpiScale(120);
    int x = dpiScale(150);
    for (int i = 0; i < (int)m_terminalTabs.size(); i++) {
        if (m_terminalTabs[i].hwndTabButton) {
            MoveWindow(m_terminalTabs[i].hwndTabButton, x, dpiScale(2), tabW, dpiScale(26), TRUE);
            InvalidateRect(m_terminalTabs[i].hwndTabButton, NULL, TRUE);
        }
        x += tabW + dpiScale(2);
    }
    InvalidateRect(m_hwndTermTabBar, NULL, TRUE);
}

// ── Tab Bar Window Proc ────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::TermTabBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(TERMTAB_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        // Bottom border
        HPEN pen = CreatePen(PS_SOLID, 1, TERMTAB_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, rc.bottom - 1, NULL);
        LineTo(hdc, rc.right, rc.bottom - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        if (!ide) break;
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        int tabId = (int)dis->CtlID - IDC_TERMTAB_FIRST;
        if (tabId >= 0 && tabId < (int)ide->m_terminalTabs.size()) {
            auto& tab = ide->m_terminalTabs[tabId];
            COLORREF bgCol = tab.active ? TERMTAB_ACTIVE_BG : TERMTAB_BG;
            COLORREF txtCol = tab.active ? TERMTAB_ACTIVE_TEXT : TERMTAB_TEXT;

            HBRUSH br = CreateSolidBrush(bgCol);
            FillRect(dis->hDC, &dis->rcItem, br);
            DeleteObject(br);

            // Active indicator (top line)
            if (tab.active) {
                HPEN ap = CreatePen(PS_SOLID, 2, RGB(0, 122, 204));
                HPEN op = (HPEN)SelectObject(dis->hDC, ap);
                MoveToEx(dis->hDC, dis->rcItem.left, dis->rcItem.top, NULL);
                LineTo(dis->hDC, dis->rcItem.right, dis->rcItem.top);
                SelectObject(dis->hDC, op);
                DeleteObject(ap);
            }

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, txtCol);
            RECT textRect = dis->rcItem;
            textRect.left += 8;
            textRect.right -= 20; // leave room for close icon
            DrawTextA(dis->hDC, tab.title.c_str(), -1, &textRect,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

            // Close icon (x)
            RECT closeRect = {dis->rcItem.right - 18, dis->rcItem.top + 4,
                              dis->rcItem.right - 4, dis->rcItem.bottom - 4};
            SetTextColor(dis->hDC, RGB(160, 160, 160));
            DrawTextA(dis->hDC, "x", 1, &closeRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        if (!ide) break;
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);
        if (id == IDC_TERMTAB_ADD_BTN) {
            // Get selected profile
            HWND hCombo = GetDlgItem(hwnd, IDC_TERMTAB_PROFILE);
            int sel = (int)SendMessageA(hCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < NUM_PROFILES)
                ide->createTerminalTab(PROFILES[sel].name);
            else
                ide->createTerminalTab("PowerShell");
            return 0;
        }
        // Tab button click → switch
        int tabId = id - IDC_TERMTAB_FIRST;
        if (tabId >= 0 && tabId < (int)ide->m_terminalTabs.size()) {
            ide->switchTerminalTab(tabId);
            return 0;
        }
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
