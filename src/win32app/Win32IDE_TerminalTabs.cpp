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

// Forward: reader thread
static DWORD WINAPI TerminalReaderThread(LPVOID param);

void Win32IDE::createTerminalTab(const std::string& profile) {
    TerminalTabInfo tab;
    tab.profile = profile;
    tab.title = profile + " " + std::to_string(m_terminalTabs.size() + 1);
    tab.active = false;
    tab.hProcess = nullptr;
    tab.hWritePipe = nullptr;
    tab.hReadPipe = nullptr;

    // Find parent (PowerShell panel or main window)
    HWND hwndParent = m_hwndPowerShellPanel ? m_hwndPowerShellPanel : m_hwndMain;
    if (!m_hwndTermTabBar) {
        createTerminalTabBar(hwndParent);
    }

    RECT rcParent;
    GetClientRect(hwndParent, &rcParent);
    int tabBarH = dpiScale(30);
    int inputH  = dpiScale(24);

    // Determine background color and executable
    COLORREF bgColor = RGB(12, 12, 12);
    const char* shellExe  = "powershell.exe";
    const char* shellArgs = "-NoLogo";
    for (int i = 0; i < NUM_PROFILES; i++) {
        if (profile == PROFILES[i].name) {
            bgColor  = PROFILES[i].bgColor;
            shellExe  = PROFILES[i].executable;
            shellArgs = PROFILES[i].args;
            break;
        }
    }

    // ── Create stdout/stdin pipes for the shell process ────────────────────
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinRead  = nullptr, hStdinWrite  = nullptr;
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;

    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) ||
        !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        LOG_ERROR("[TerminalTabs] Failed to create pipes");
        if (hStdinRead)  CloseHandle(hStdinRead);
        if (hStdinWrite) CloseHandle(hStdinWrite);
        // Fall through — create tab without process
    }

    // Ensure our ends are not inherited
    if (hStdinWrite) SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    if (hStdoutRead) SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // ── Spawn the shell process ────────────────────────────────────────────
    bool processStarted = false;
    if (hStdinRead && hStdoutWrite) {
        std::string cmdLine = std::string("\"") + shellExe + "\"";
        if (shellArgs && shellArgs[0]) {
            cmdLine += " ";
            cmdLine += shellArgs;
        }

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput  = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError  = hStdoutWrite;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        char cmdBuf[2048] = {};
        strncpy_s(cmdBuf, cmdLine.c_str(), _TRUNCATE);

        if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            tab.hProcess   = pi.hProcess;
            tab.hWritePipe = hStdinWrite;
            tab.hReadPipe  = hStdoutRead;
            CloseHandle(pi.hThread);
            processStarted = true;
            LOG_INFO("[TerminalTabs] Spawned shell: " + cmdLine + " (pid=" + std::to_string(pi.dwProcessId) + ")");
        } else {
            LOG_ERROR("[TerminalTabs] CreateProcess failed for: " + cmdLine + " err=" + std::to_string(GetLastError()));
        }

        // Close child-side pipe handles (we keep our side)
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
    }

    // ── Create RichEdit output pane ────────────────────────────────────────
    int contentH = rcParent.bottom - tabBarH - inputH - 4;
    tab.hwndContent = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, tabBarH, rcParent.right, contentH,
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

    // Set large text limit
    SendMessageA(tab.hwndContent, EM_EXLIMITTEXT, 0, 1024 * 1024);

    // ── Create input edit (writable, for typing commands) ──────────────────
    tab.hwndInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        0, tabBarH + contentH + 2, rcParent.right, inputH,
        hwndParent, nullptr, GetModuleHandle(NULL), NULL);

    // Style input line
    HFONT inputFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessageA(tab.hwndInput, WM_SETFONT, (WPARAM)inputFont, TRUE);

    // Subclass input to capture Enter key
    SetPropA(tab.hwndInput, "IDE_PTR", (HANDLE)this);
    SetPropA(tab.hwndInput, "TAB_IDX", (HANDLE)(LONG_PTR)m_terminalTabs.size());
    tab.origInputProc = (WNDPROC)SetWindowLongPtrA(tab.hwndInput, GWLP_WNDPROC,
        (LONG_PTR)TerminalInputSubclassProc);

    // Create tab button in tab bar
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

    // ── Start reader thread to pipe stdout → RichEdit ──────────────────────
    if (processStarted && tab.hReadPipe) {
        struct ReaderContext {
            HANDLE hReadPipe;
            HWND   hwndOutput;
        };
        auto* ctx = new ReaderContext{ tab.hReadPipe, tab.hwndContent };
        HANDLE hThread = CreateThread(nullptr, 0, TerminalReaderThread, ctx, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    } else {
        // No process — show info message
        std::string msg = "[" + profile + "] Shell not found or failed to start.\r\n"
                         "Executable: " + std::string(shellExe) + "\r\n"
                         "Type commands below — they will be executed via CreateProcess.\r\n";
        CHARRANGEA cr = {-1, -1};
        SendMessageA(tab.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessageA(tab.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
    }

    LOG_INFO("[TerminalTabs] Created tab: " + tab.title +
             (processStarted ? " (live shell)" : " (no process)"));
}

// ── Reader thread: reads shell stdout and appends to RichEdit ──────────────
static DWORD WINAPI TerminalReaderThread(LPVOID param) {
    struct ReaderContext {
        HANDLE hReadPipe;
        HWND   hwndOutput;
    };
    auto* ctx = (ReaderContext*)param;
    HANDLE hPipe = ctx->hReadPipe;
    HWND hwndOut = ctx->hwndOutput;
    delete ctx;

    char buffer[4096];
    DWORD bytesRead;
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

    while (ReadFile(hPipe, buffer, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
        const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
        buffer[safeBytes] = '\0';

        // Post append to UI thread via SendMessage (thread-safe for RichEdit)
        if (IsWindow(hwndOut)) {
            // Select end
            CHARRANGEA cr = {-1, -1};
            SendMessageA(hwndOut, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(hwndOut, EM_REPLACESEL, FALSE, (LPARAM)buffer);
            // Auto-scroll to bottom
            SendMessageA(hwndOut, WM_VSCROLL, SB_BOTTOM, 0);
        }
    }

    return 0;
}

// ── Input subclass proc: captures Enter to send command to shell stdin ──────
LRESULT CALLBACK Win32IDE::TerminalInputSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    int tabIdx = (int)(LONG_PTR)GetPropA(hwnd, "TAB_IDX");
    WNDPROC origProc = nullptr;

    if (ide && tabIdx >= 0 && tabIdx < (int)ide->m_terminalTabs.size()) {
        origProc = ide->m_terminalTabs[tabIdx].origInputProc;
    }

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        if (ide && tabIdx >= 0 && tabIdx < (int)ide->m_terminalTabs.size()) {
            auto& tab = ide->m_terminalTabs[tabIdx];

            // Get command text
            char cmd[4096] = {};
            GetWindowTextA(hwnd, cmd, sizeof(cmd));
            SetWindowTextA(hwnd, "");

            std::string command(cmd);
            command += "\r\n";

            if (tab.hWritePipe) {
                // Write to shell stdin pipe
                DWORD written;
                WriteFile(tab.hWritePipe, command.c_str(), (DWORD)command.size(), &written, nullptr);
            } else {
                // No live shell — execute via CreateProcess and capture output
                std::string fullCmd = "cmd.exe /C " + std::string(cmd);
                char cmdBuf[4096] = {};
                strncpy_s(cmdBuf, fullCmd.c_str(), _TRUNCATE);

                SECURITY_ATTRIBUTES sa = {};
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;

                HANDLE hReadP = nullptr, hWriteP = nullptr;
                CreatePipe(&hReadP, &hWriteP, &sa, 0);
                SetHandleInformation(hReadP, HANDLE_FLAG_INHERIT, 0);

                STARTUPINFOA si = {};
                si.cb = sizeof(si);
                si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                si.hStdOutput = hWriteP;
                si.hStdError = hWriteP;
                si.wShowWindow = SW_HIDE;

                PROCESS_INFORMATION pi = {};
                if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(hWriteP);

                    // Echo command
                    std::string echo = "> " + std::string(cmd) + "\r\n";
                    CHARRANGEA cr = {-1, -1};
                    SendMessageA(tab.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);
                    SendMessageA(tab.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)echo.c_str());

                    char buf[4096];
                    DWORD bytesRead;
                    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buf) - 1);
                    while (ReadFile(hReadP, buf, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
                        const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                        buf[safeBytes] = '\0';
                        cr = {-1, -1};
                        SendMessageA(tab.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);
                        SendMessageA(tab.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)buf);
                    }
                    SendMessageA(tab.hwndContent, WM_VSCROLL, SB_BOTTOM, 0);

                    WaitForSingleObject(pi.hProcess, 5000);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                } else {
                    if (hWriteP) CloseHandle(hWriteP);
                    std::string err = "[Error] Failed to execute: " + std::string(cmd) + "\r\n";
                    CHARRANGEA cr = {-1, -1};
                    SendMessageA(tab.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);
                    SendMessageA(tab.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)err.c_str());
                }
                if (hReadP) CloseHandle(hReadP);
            }
        }
        return 0;
    }

    // History: Up/Down arrows for command history (basic)
    if (msg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN)) {
        // TODO: implement command history ring buffer
        return 0;
    }

    return origProc ? CallWindowProcA(origProc, hwnd, msg, wParam, lParam)
                    : DefWindowProcA(hwnd, msg, wParam, lParam);
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
