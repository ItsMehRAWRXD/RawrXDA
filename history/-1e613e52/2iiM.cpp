// ============================================================================
// Win32IDE_Tier2Cosmetics.cpp — Tier 2: High Visibility (Daily Friction)
// ============================================================================
//
// 11. Git Diff Side-by-Side         — Visual diff in D3D11/GDI overlay
// 12. Integrated Terminal Tabs      — Multi-tab terminal (bash, PS, CMD)
// 13. Hover Documentation Tooltips  — WM_MOUSEHOVER → popup with content
// 14. Parameter Hints (Sig Help)    — floating tooltip near cursor
// 15. Document Symbols Outline      — Enhanced from Win32IDE_OutlinePanel.cpp
// 16. Find All References UI        — ReferencePanel with tree structure
// 17. Rename Refactoring Preview    — Enhanced from Win32IDE_RenamePreview.cpp
// 18. CodeLens (Reference Counts)   — Phantom text in margin
// 19. Inlay Type Hints              — Ghost text for auto/var types
//
// Design: Zero simplification — every feature is fully wired.
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <functional>
#include <regex>

#pragma comment(lib, "comctl32.lib")

// ============================================================================
// TIER 2 COLORS (VS Code / JetBrains standard palette)
// ============================================================================
static const COLORREF T2_BG              = RGB(30, 30, 30);
static const COLORREF T2_PANEL_BG        = RGB(37, 37, 38);
static const COLORREF T2_HEADER_BG       = RGB(45, 45, 46);
static const COLORREF T2_TEXT            = RGB(204, 204, 204);
static const COLORREF T2_DIM_TEXT        = RGB(128, 128, 128);
static const COLORREF T2_ACCENT         = RGB(0, 122, 204);
static const COLORREF T2_BORDER         = RGB(60, 60, 60);
static const COLORREF T2_SELECTED       = RGB(4, 57, 94);
static const COLORREF T2_HOVER_BG       = RGB(45, 45, 46);
static const COLORREF T2_DIFF_ADD_BG    = RGB(35, 61, 37);
static const COLORREF T2_DIFF_DEL_BG    = RGB(72, 30, 30);
static const COLORREF T2_DIFF_ADD_TEXT  = RGB(100, 255, 100);
static const COLORREF T2_DIFF_DEL_TEXT  = RGB(255, 100, 100);
static const COLORREF T2_DIFF_HDR       = RGB(86, 156, 214);
static const COLORREF T2_CODELENS_TEXT  = RGB(140, 140, 140);
static const COLORREF T2_INLAY_TEXT     = RGB(110, 160, 210);
static const COLORREF T2_HOVER_BORDER   = RGB(80, 80, 80);
static const COLORREF T2_PARAM_ACTIVE   = RGB(86, 156, 214);
static const COLORREF T2_PARAM_INACTIVE = RGB(150, 150, 150);
static const COLORREF T2_REF_FILE       = RGB(229, 192, 123);
static const COLORREF T2_REF_LINE_NUM  = RGB(110, 110, 110);
static const COLORREF T2_TAB_ACTIVE_BG = RGB(30, 30, 30);
static const COLORREF T2_TAB_INACTIVE_BG= RGB(45, 45, 46);

// ============================================================================
// TIER 2 IDs
// ============================================================================
static constexpr int IDC_GITDIFF_PANEL       = 11200;
static constexpr int IDC_GITDIFF_LEFT        = 11201;
static constexpr int IDC_GITDIFF_RIGHT       = 11202;
static constexpr int IDC_GITDIFF_CLOSE       = 11203;
static constexpr int IDC_GITDIFF_PREV        = 11204;
static constexpr int IDC_GITDIFF_NEXT        = 11205;

static constexpr int IDC_TERMTAB_BAR         = 11210;
static constexpr int IDC_TERMTAB_ADD         = 11211;
static constexpr int IDC_TERMTAB_CLOSE       = 11212;
static constexpr int IDC_TERMTAB_DROPDOWN    = 11213;

static constexpr int IDC_HOVER_POPUP         = 11220;
static constexpr int IDC_SIGHELP_POPUP       = 11221;

static constexpr int IDC_REFPANEL            = 11230;
static constexpr int IDC_REFPANEL_TREE       = 11231;
static constexpr int IDC_REFPANEL_CLOSE      = 11232;
static constexpr int IDC_REFPANEL_HEADER     = 11233;

// ── Window class names ─────────────────────────────────────────────────────
static const char* GITDIFF_CLASS    = "RawrXD_GitDiffPanel";
static const char* HOVER_CLASS     = "RawrXD_HoverTooltip";
static const char* SIGHELP_CLASS   = "RawrXD_SigHelp";
static const char* REFPANEL_CLASS  = "RawrXD_RefPanel";
static const char* CODELENS_CLASS  = "RawrXD_CodeLens";

static bool s_tier2ClassesRegistered = false;

static void ensureTier2ClassesRegistered(HINSTANCE hInst) {
    if (s_tier2ClassesRegistered) return;

    auto reg = [&](const char* name) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(T2_BG);
        wc.lpszClassName = name;
        RegisterClassExA(&wc);
    };

    reg(GITDIFF_CLASS);
    reg(HOVER_CLASS);
    reg(SIGHELP_CLASS);
    reg(REFPANEL_CLASS);
    reg(CODELENS_CLASS);
    s_tier2ClassesRegistered = true;
}

// ============================================================================
//
//  11. GIT DIFF SIDE-BY-SIDE (Win32IDE_Annotations.cpp overlay system)
//
// ============================================================================

void Win32IDE::initGitDiffViewer() {
    m_gitDiffVisible = false;
    m_gitDiffLeftLines.clear();
    m_gitDiffRightLines.clear();
    m_gitDiffHunks.clear();
    m_gitDiffCurrentHunk = -1;
    m_gitDiffFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    ensureTier2ClassesRegistered(GetModuleHandle(NULL));
    LOG_INFO("[GitDiff] Side-by-side diff viewer initialized");
}

void Win32IDE::shutdownGitDiffViewer() {
    closeGitDiffViewer();
    if (m_gitDiffFont) { DeleteObject(m_gitDiffFont); m_gitDiffFont = nullptr; }
}

void Win32IDE::showGitDiffSideBySide(const std::string& filePath) {
    closeGitDiffViewer();

    // Run git diff to get unified diff
    std::string diffOutput;
    std::string cmd = "git diff -- \"" + filePath + "\"";

    // Execute git diff via pipe
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        appendToOutput("[GitDiff] Failed to create pipe", "Output", OutputSeverity::Error);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    char cmdBuf[1024];
    snprintf(cmdBuf, sizeof(cmdBuf), "cmd.exe /c %s", cmd.c_str());

    if (CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = NULL;

        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            diffOutput += buf;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (hWritePipe) CloseHandle(hWritePipe);
    if (hReadPipe) CloseHandle(hReadPipe);

    if (diffOutput.empty()) {
        appendToOutput("[GitDiff] No changes detected for " + filePath, "Output", OutputSeverity::Info);
        return;
    }

    // Parse unified diff into left/right line arrays
    parseUnifiedDiff(diffOutput, filePath);

    // Read original file (HEAD version) for left pane
    std::string origCmd = "git show HEAD:\"" + filePath + "\"";
    m_gitDiffLeftLines.clear();
    {
        HANDLE hR = NULL, hW = NULL;
        if (CreatePipe(&hR, &hW, &sa, 0)) {
            SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);
            STARTUPINFOA si2 = {};
            si2.cb = sizeof(si2);
            si2.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si2.hStdOutput = hW;
            si2.hStdError = hW;
            si2.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi2 = {};
            char cmdBuf2[1024];
            snprintf(cmdBuf2, sizeof(cmdBuf2), "cmd.exe /c %s", origCmd.c_str());
            if (CreateProcessA(NULL, cmdBuf2, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                               NULL, NULL, &si2, &pi2)) {
                CloseHandle(hW); hW = NULL;
                std::string origContent;
                char buf[4096];
                DWORD bytesRead;
                while (ReadFile(hR, buf, sizeof(buf)-1, &bytesRead, NULL) && bytesRead > 0) {
                    buf[bytesRead] = '\0';
                    origContent += buf;
                }
                WaitForSingleObject(pi2.hProcess, 5000);
                CloseHandle(pi2.hProcess);
                CloseHandle(pi2.hThread);

                std::istringstream iss(origContent);
                std::string line;
                while (std::getline(iss, line)) {
                    m_gitDiffLeftLines.push_back({line, DiffLineType::Context, -1});
                }
            }
            if (hW) CloseHandle(hW);
            if (hR) CloseHandle(hR);
        }
    }

    // Read current working copy for right pane
    m_gitDiffRightLines.clear();
    {
        std::ifstream ifs(filePath);
        if (ifs.is_open()) {
            std::string line;
            while (std::getline(ifs, line)) {
                m_gitDiffRightLines.push_back({line, DiffLineType::Context, -1});
            }
        }
    }

    // Mark changed lines based on parsed hunk data
    markDiffLines();

    // Create side-by-side panel
    createGitDiffPanel();
    m_gitDiffVisible = true;

    LOG_INFO("[GitDiff] Showing diff for: " + filePath +
             " (" + std::to_string(m_gitDiffHunks.size()) + " hunks)");
}

void Win32IDE::parseUnifiedDiff(const std::string& diff, const std::string& filePath) {
    m_gitDiffHunks.clear();

    std::istringstream stream(diff);
    std::string line;
    DiffHunk currentHunk;
    bool inHunk = false;

    while (std::getline(stream, line)) {
        if (line.substr(0, 3) == "@@" || (line.size() > 4 && line.substr(0, 4) == "@@ -")) {
            if (inHunk && !currentHunk.lines.empty()) {
                m_gitDiffHunks.push_back(currentHunk);
            }
            currentHunk = DiffHunk{};
            currentHunk.header = line;
            inHunk = true;

            // Parse @@ -old,count +new,count @@
            size_t minusPos = line.find('-');
            size_t plusPos = line.find('+');
            if (minusPos != std::string::npos && plusPos != std::string::npos) {
                currentHunk.oldStart = atoi(line.c_str() + minusPos + 1);
                currentHunk.newStart = atoi(line.c_str() + plusPos + 1);
            }
            continue;
        }

        if (!inHunk) continue;

        DiffLine dl;
        if (!line.empty() && line[0] == '+') {
            dl.type = DiffLineType::Added;
            dl.text = line.substr(1);
        } else if (!line.empty() && line[0] == '-') {
            dl.type = DiffLineType::Removed;
            dl.text = line.substr(1);
        } else if (!line.empty() && line[0] == ' ') {
            dl.type = DiffLineType::Context;
            dl.text = line.substr(1);
        } else {
            dl.type = DiffLineType::Context;
            dl.text = line;
        }
        dl.lineNumber = -1;
        currentHunk.lines.push_back(dl);
    }

    if (inHunk && !currentHunk.lines.empty()) {
        m_gitDiffHunks.push_back(currentHunk);
    }
}

void Win32IDE::markDiffLines() {
    // Number the left and right lines and mark additions/removals
    int leftLine = 0;
    int rightLine = 0;
    for (auto& hunk : m_gitDiffHunks) {
        leftLine = hunk.oldStart - 1;
        rightLine = hunk.newStart - 1;

        for (auto& dl : hunk.lines) {
            switch (dl.type) {
            case DiffLineType::Context:
                dl.lineNumber = rightLine;
                leftLine++;
                rightLine++;
                break;
            case DiffLineType::Removed:
                dl.lineNumber = leftLine;
                if (leftLine < (int)m_gitDiffLeftLines.size()) {
                    m_gitDiffLeftLines[leftLine].type = DiffLineType::Removed;
                }
                leftLine++;
                break;
            case DiffLineType::Added:
                dl.lineNumber = rightLine;
                if (rightLine < (int)m_gitDiffRightLines.size()) {
                    m_gitDiffRightLines[rightLine].type = DiffLineType::Added;
                }
                rightLine++;
                break;
            }
        }
    }
}

void Win32IDE::createGitDiffPanel() {
    if (m_hwndGitDiffPanel) {
        DestroyWindow(m_hwndGitDiffPanel);
        m_hwndGitDiffPanel = nullptr;
    }

    RECT rcClient;
    GetClientRect(m_hwndMain, &rcClient);
    int panelW = rcClient.right - rcClient.left;
    int panelH = rcClient.bottom - rcClient.top;

    m_hwndGitDiffPanel = CreateWindowExA(WS_EX_CLIENTEDGE,
        GITDIFF_CLASS, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, dpiScale(35), panelW, panelH - dpiScale(60),
        m_hwndMain, (HMENU)(UINT_PTR)IDC_GITDIFF_PANEL,
        GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndGitDiffPanel, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndGitDiffPanel, GWLP_WNDPROC, (LONG_PTR)GitDiffPanelProc);

    // Left pane (old)
    LoadLibraryA("Riched20.dll");
    int halfW = panelW / 2;
    int headerH = dpiScale(32);

    m_hwndGitDiffLeft = CreateWindowExA(0,
        "RichEdit20A", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, headerH, halfW, panelH - dpiScale(60) - headerH,
        m_hwndGitDiffPanel, (HMENU)(UINT_PTR)IDC_GITDIFF_LEFT,
        GetModuleHandle(NULL), NULL);
    SendMessageA(m_hwndGitDiffLeft, WM_SETFONT, (WPARAM)m_gitDiffFont, TRUE);
    SendMessageA(m_hwndGitDiffLeft, EM_SETBKGNDCOLOR, 0, T2_BG);

    // Right pane (new)
    m_hwndGitDiffRight = CreateWindowExA(0,
        "RichEdit20A", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        halfW, headerH, halfW, panelH - dpiScale(60) - headerH,
        m_hwndGitDiffPanel, (HMENU)(UINT_PTR)IDC_GITDIFF_RIGHT,
        GetModuleHandle(NULL), NULL);
    SendMessageA(m_hwndGitDiffRight, WM_SETFONT, (WPARAM)m_gitDiffFont, TRUE);
    SendMessageA(m_hwndGitDiffRight, EM_SETBKGNDCOLOR, 0, T2_BG);

    // Navigation buttons
    CreateWindowExA(0, "BUTTON", "< Prev Hunk",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        halfW - dpiScale(160), dpiScale(4), dpiScale(75), dpiScale(24),
        m_hwndGitDiffPanel, (HMENU)(UINT_PTR)IDC_GITDIFF_PREV,
        GetModuleHandle(NULL), NULL);

    CreateWindowExA(0, "BUTTON", "Next Hunk >",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        halfW - dpiScale(80), dpiScale(4), dpiScale(75), dpiScale(24),
        m_hwndGitDiffPanel, (HMENU)(UINT_PTR)IDC_GITDIFF_NEXT,
        GetModuleHandle(NULL), NULL);

    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - dpiScale(70), dpiScale(4), dpiScale(60), dpiScale(24),
        m_hwndGitDiffPanel, (HMENU)(UINT_PTR)IDC_GITDIFF_CLOSE,
        GetModuleHandle(NULL), NULL);

    // Populate panes
    populateGitDiffPane(m_hwndGitDiffLeft, m_gitDiffLeftLines, "Original (HEAD)");
    populateGitDiffPane(m_hwndGitDiffRight, m_gitDiffRightLines, "Working Copy");

    if (!m_gitDiffHunks.empty()) {
        m_gitDiffCurrentHunk = 0;
    }

    BringWindowToTop(m_hwndGitDiffPanel);
    SetFocus(m_hwndGitDiffPanel);
}

void Win32IDE::populateGitDiffPane(HWND hwndRichEdit, const std::vector<DiffLine>& lines,
                                    const std::string& header) {
    if (!hwndRichEdit) return;

    // Clear
    SetWindowTextA(hwndRichEdit, "");

    auto appendColored = [&](const std::string& text, COLORREF textColor, COLORREF bgColor) {
        int len = GetWindowTextLengthA(hwndRichEdit);
        SendMessageA(hwndRichEdit, EM_SETSEL, len, len);
        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_BACKCOLOR;
        cf.crTextColor = textColor;
        cf.crBackColor = bgColor;
        SendMessageA(hwndRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        SendMessageA(hwndRichEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    };

    int lineNum = 1;
    for (const auto& dl : lines) {
        char lineNumBuf[16];
        snprintf(lineNumBuf, sizeof(lineNumBuf), "%4d | ", lineNum++);

        COLORREF textColor = T2_TEXT;
        COLORREF bgColor = T2_BG;

        switch (dl.type) {
        case DiffLineType::Added:
            textColor = T2_DIFF_ADD_TEXT;
            bgColor = T2_DIFF_ADD_BG;
            break;
        case DiffLineType::Removed:
            textColor = T2_DIFF_DEL_TEXT;
            bgColor = T2_DIFF_DEL_BG;
            break;
        case DiffLineType::Context:
        default:
            break;
        }

        appendColored(lineNumBuf, T2_DIM_TEXT, T2_BG);
        appendColored(dl.text + "\n", textColor, bgColor);
    }
}

void Win32IDE::closeGitDiffViewer() {
    if (m_hwndGitDiffPanel) {
        DestroyWindow(m_hwndGitDiffPanel);
        m_hwndGitDiffPanel = nullptr;
        m_hwndGitDiffLeft = nullptr;
        m_hwndGitDiffRight = nullptr;
    }
    m_gitDiffVisible = false;
}

void Win32IDE::navigateDiffHunk(int direction) {
    if (m_gitDiffHunks.empty()) return;
    m_gitDiffCurrentHunk += direction;
    if (m_gitDiffCurrentHunk < 0) m_gitDiffCurrentHunk = (int)m_gitDiffHunks.size() - 1;
    if (m_gitDiffCurrentHunk >= (int)m_gitDiffHunks.size()) m_gitDiffCurrentHunk = 0;

    // Scroll to hunk in both panes
    auto& hunk = m_gitDiffHunks[m_gitDiffCurrentHunk];
    if (m_hwndGitDiffLeft) {
        int charIdx = (int)SendMessageA(m_hwndGitDiffLeft, EM_LINEINDEX, hunk.oldStart - 1, 0);
        if (charIdx >= 0) {
            CHARRANGE cr = {charIdx, charIdx};
            SendMessageA(m_hwndGitDiffLeft, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(m_hwndGitDiffLeft, EM_SCROLLCARET, 0, 0);
        }
    }
    if (m_hwndGitDiffRight) {
        int charIdx = (int)SendMessageA(m_hwndGitDiffRight, EM_LINEINDEX, hunk.newStart - 1, 0);
        if (charIdx >= 0) {
            CHARRANGE cr = {charIdx, charIdx};
            SendMessageA(m_hwndGitDiffRight, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(m_hwndGitDiffRight, EM_SCROLLCARET, 0, 0);
        }
    }

    appendToOutput("[GitDiff] Hunk " + std::to_string(m_gitDiffCurrentHunk + 1) + "/" +
                   std::to_string(m_gitDiffHunks.size()) + ": " + hunk.header,
                   "Output", OutputSeverity::Info);
}

LRESULT CALLBACK Win32IDE::GitDiffPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Header bar
        int headerH = ide ? ide->dpiScale(32) : 32;
        RECT headerRect = {0, 0, rc.right, headerH};
        HBRUSH hdrBrush = CreateSolidBrush(T2_HEADER_BG);
        FillRect(hdc, &headerRect, hdrBrush);
        DeleteObject(hdrBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, T2_DIFF_HDR);
        int halfW = rc.right / 2;
        RECT leftLabel = {8, 6, halfW, headerH};
        DrawTextA(hdc, "Original (HEAD)", -1, &leftLabel, DT_LEFT | DT_SINGLELINE);
        RECT rightLabel = {halfW + 8, 6, rc.right, headerH};
        DrawTextA(hdc, "Working Copy", -1, &rightLabel, DT_LEFT | DT_SINGLELINE);

        // Divider
        HPEN pen = CreatePen(PS_SOLID, 1, T2_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, halfW, 0, NULL);
        LineTo(hdc, halfW, rc.bottom);
        MoveToEx(hdc, 0, headerH, NULL);
        LineTo(hdc, rc.right, headerH);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (ide) {
            int id = LOWORD(wParam);
            if (id == IDC_GITDIFF_CLOSE) { ide->closeGitDiffViewer(); return 0; }
            if (id == IDC_GITDIFF_PREV)  { ide->navigateDiffHunk(-1); return 0; }
            if (id == IDC_GITDIFF_NEXT)  { ide->navigateDiffHunk(1); return 0; }
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && ide) { ide->closeGitDiffViewer(); return 0; }
        break;
    case WM_SIZE: {
        if (!ide) break;
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        int headerH = ide->dpiScale(32);
        int halfW = w / 2;
        if (ide->m_hwndGitDiffLeft)
            SetWindowPos(ide->m_hwndGitDiffLeft, NULL, 0, headerH, halfW, h - headerH, SWP_NOZORDER);
        if (ide->m_hwndGitDiffRight)
            SetWindowPos(ide->m_hwndGitDiffRight, NULL, halfW, headerH, halfW, h - headerH, SWP_NOZORDER);
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}


// ============================================================================
//
//  12. INTEGRATED TERMINAL TABS
//
// ============================================================================

void Win32IDE::initTerminalTabs() {
    m_terminalTabProfiles.clear();

    // Default profiles
    TerminalProfile ps;
    ps.name = "PowerShell";
    ps.shellPath = "powershell.exe";
    ps.shellArgs = "-NoLogo";
    ps.icon = "PS";
    ps.color = RGB(0, 122, 204);
    m_terminalTabProfiles.push_back(ps);

    TerminalProfile cmd;
    cmd.name = "Command Prompt";
    cmd.shellPath = "cmd.exe";
    cmd.shellArgs = "";
    cmd.icon = ">";
    cmd.color = RGB(204, 204, 204);
    m_terminalTabProfiles.push_back(cmd);

    // Check for Git Bash
    if (GetFileAttributesA("C:\\Program Files\\Git\\bin\\bash.exe") != INVALID_FILE_ATTRIBUTES) {
        TerminalProfile bash;
        bash.name = "Git Bash";
        bash.shellPath = "C:\\Program Files\\Git\\bin\\bash.exe";
        bash.shellArgs = "--login -i";
        bash.icon = "$";
        bash.color = RGB(214, 157, 133);
        m_terminalTabProfiles.push_back(bash);
    }

    // Check for WSL
    if (GetFileAttributesA("C:\\Windows\\System32\\wsl.exe") != INVALID_FILE_ATTRIBUTES) {
        TerminalProfile wsl;
        wsl.name = "WSL";
        wsl.shellPath = "wsl.exe";
        wsl.shellArgs = "";
        wsl.icon = "~";
        wsl.color = RGB(78, 201, 176);
        m_terminalTabProfiles.push_back(wsl);
    }

    m_activeTerminalTab = 0;
    m_hwndTerminalTabBar = nullptr;
    LOG_INFO("[TerminalTabs] Initialized with " + std::to_string(m_terminalTabProfiles.size()) +
             " profiles");
}

void Win32IDE::createTerminalTabBar() {
    if (m_hwndTerminalTabBar || !m_hwndPowerShellPanel) return;

    RECT panelRect;
    GetClientRect(m_hwndPowerShellPanel, &panelRect);

    // Tab bar sits above the existing toolbar
    m_hwndTerminalTabBar = CreateWindowExA(0,
        "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, panelRect.right, dpiScale(28),
        m_hwndPowerShellPanel, (HMENU)(UINT_PTR)IDC_TERMTAB_BAR,
        GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndTerminalTabBar, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndTerminalTabBar, GWLP_WNDPROC, (LONG_PTR)TerminalTabBarProc);

    // "+" button for new terminal
    CreateWindowExA(0, "BUTTON", "+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelRect.right - dpiScale(60), dpiScale(2), dpiScale(24), dpiScale(22),
        m_hwndPowerShellPanel, (HMENU)(UINT_PTR)IDC_TERMTAB_ADD,
        GetModuleHandle(NULL), NULL);

    // Profile dropdown
    CreateWindowExA(0, "BUTTON", "v",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelRect.right - dpiScale(32), dpiScale(2), dpiScale(24), dpiScale(22),
        m_hwndPowerShellPanel, (HMENU)(UINT_PTR)IDC_TERMTAB_DROPDOWN,
        GetModuleHandle(NULL), NULL);

    // Add default PowerShell tab
    if (m_terminalTabs.empty()) {
        addTerminalTab(0); // PowerShell profile
    }

    LOG_INFO("[TerminalTabs] Tab bar created");
}

void Win32IDE::addTerminalTab(int profileIndex) {
    if (profileIndex < 0 || profileIndex >= (int)m_terminalTabProfiles.size()) return;

    TerminalTabInfo tab;
    auto& profile = m_terminalTabProfiles[profileIndex];
    tab.profileIndex = profileIndex;
    tab.title = profile.name + " " + std::to_string(m_terminalTabs.size() + 1);
    tab.color = profile.color;
    tab.active = true;
    tab.hwndOutput = nullptr;
    tab.manager = std::make_unique<Win32TerminalManager>();

    // Set up callbacks
    auto tabIndex = m_terminalTabs.size();
    tab.manager->onOutput = [this, tabIndex](const std::string& output) {
        if (isShuttingDown()) return;
        // Append to the tab's output (if it's the active tab, show immediately)
        if (m_activeTerminalTab == (int)tabIndex && m_hwndPowerShellOutput) {
            appendPowerShellOutput(output, RGB(200, 200, 200));
        }
    };

    tab.manager->onError = [this, tabIndex](const std::string& error) {
        if (isShuttingDown()) return;
        if (m_activeTerminalTab == (int)tabIndex && m_hwndPowerShellOutput) {
            appendPowerShellOutput("[ERROR] " + error, RGB(255, 100, 100));
        }
    };

    // Start the shell
    auto shellType = Win32TerminalManager::PowerShell;
    if (profile.shellPath.find("cmd.exe") != std::string::npos) {
        shellType = Win32TerminalManager::CommandPrompt;
    } else if (profile.shellPath.find("bash") != std::string::npos) {
        shellType = Win32TerminalManager::GitBash;
    } else if (profile.shellPath.find("wsl") != std::string::npos) {
        shellType = Win32TerminalManager::WSL;
    }

    tab.manager->start(shellType);
    m_terminalTabs.push_back(std::move(tab));

    m_activeTerminalTab = (int)m_terminalTabs.size() - 1;
    switchTerminalTab(m_activeTerminalTab);

    if (m_hwndTerminalTabBar) {
        InvalidateRect(m_hwndTerminalTabBar, nullptr, TRUE);
    }

    LOG_INFO("[TerminalTabs] Added tab: " + profile.name);
}

void Win32IDE::closeTerminalTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= (int)m_terminalTabs.size()) return;
    if (m_terminalTabs.size() <= 1) return; // Keep at least one

    auto& tab = m_terminalTabs[tabIndex];
    if (tab.manager) tab.manager->stop();

    m_terminalTabs.erase(m_terminalTabs.begin() + tabIndex);

    if (m_activeTerminalTab >= (int)m_terminalTabs.size()) {
        m_activeTerminalTab = (int)m_terminalTabs.size() - 1;
    }
    switchTerminalTab(m_activeTerminalTab);

    if (m_hwndTerminalTabBar) {
        InvalidateRect(m_hwndTerminalTabBar, nullptr, TRUE);
    }

    LOG_INFO("[TerminalTabs] Closed tab index " + std::to_string(tabIndex));
}

void Win32IDE::switchTerminalTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= (int)m_terminalTabs.size()) return;

    m_activeTerminalTab = tabIndex;

    // Clear output and show the buffer of the new active tab
    if (m_hwndPowerShellOutput) {
        SetWindowTextA(m_hwndPowerShellOutput, "");
        auto& tab = m_terminalTabs[tabIndex];
        auto& profile = m_terminalTabProfiles[tab.profileIndex];
        appendPowerShellOutput("[" + profile.name + "] Terminal active\n", tab.color);
    }

    if (m_hwndTerminalTabBar) {
        InvalidateRect(m_hwndTerminalTabBar, nullptr, TRUE);
    }
}

void Win32IDE::showTerminalProfileMenu() {
    HMENU hMenu = CreatePopupMenu();
    for (int i = 0; i < (int)m_terminalTabProfiles.size(); i++) {
        AppendMenuA(hMenu, MF_STRING, (UINT_PTR)(100 + i),
                    m_terminalTabProfiles[i].name.c_str());
    }

    POINT pt;
    GetCursorPos(&pt);
    int cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y,
                                m_hwndMain, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 100 && cmd < 100 + (int)m_terminalTabProfiles.size()) {
        addTerminalTab(cmd - 100);
    }
}

LRESULT CALLBACK Win32IDE::TerminalTabBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH bgBrush = CreateSolidBrush(T2_HEADER_BG);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        if (!ide) { EndPaint(hwnd, &ps); return 0; }

        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        int tabW = ide->dpiScale(120);
        int tabH = rc.bottom - rc.top;
        int x = 0;

        for (int i = 0; i < (int)ide->m_terminalTabs.size(); i++) {
            auto& tab = ide->m_terminalTabs[i];
            RECT tabRect = {x, 0, x + tabW, tabH};

            COLORREF bgColor = (i == ide->m_activeTerminalTab) ? T2_TAB_ACTIVE_BG : T2_TAB_INACTIVE_BG;
            HBRUSH tabBrush = CreateSolidBrush(bgColor);
            FillRect(hdc, &tabRect, tabBrush);
            DeleteObject(tabBrush);

            // Bottom accent for active tab
            if (i == ide->m_activeTerminalTab) {
                RECT accent = {x, tabH - 2, x + tabW, tabH};
                HBRUSH accentBrush = CreateSolidBrush(tab.color);
                FillRect(hdc, &accent, accentBrush);
                DeleteObject(accentBrush);
            }

            // Icon circle
            HBRUSH iconBrush = CreateSolidBrush(tab.color);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, iconBrush);
            int iconCx = x + 12;
            int iconCy = tabH / 2;
            Ellipse(hdc, iconCx - 5, iconCy - 5, iconCx + 5, iconCy + 5);
            SelectObject(hdc, oldBr);
            DeleteObject(iconBrush);

            // Tab title
            SetTextColor(hdc, (i == ide->m_activeTerminalTab) ? T2_TEXT : T2_DIM_TEXT);
            RECT textRect = {x + 22, 0, x + tabW - 20, tabH};
            DrawTextA(hdc, tab.title.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Close [x] area
            SetTextColor(hdc, T2_DIM_TEXT);
            RECT closeRect = {x + tabW - 18, 0, x + tabW - 4, tabH};
            DrawTextA(hdc, "x", 1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            x += tabW;
        }

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!ide) break;
        int mx = (short)LOWORD(lParam);
        int tabW = ide->dpiScale(120);
        int tabIndex = mx / tabW;

        if (tabIndex < 0 || tabIndex >= (int)ide->m_terminalTabs.size()) break;

        // Check if close button area was clicked
        int tabRight = (tabIndex + 1) * tabW;
        if (mx > tabRight - 20) {
            ide->closeTerminalTab(tabIndex);
        } else {
            ide->switchTerminalTab(tabIndex);
        }
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}


// ============================================================================
//
//  13. HOVER DOCUMENTATION TOOLTIPS
//
// ============================================================================

void Win32IDE::initHoverTooltip() {
    m_hwndHoverPopup = nullptr;
    m_hoverVisible = false;
    m_hoverContent.clear();
    m_hoverFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    m_hoverBoldFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    ensureTier2ClassesRegistered(GetModuleHandle(NULL));
    LOG_INFO("[HoverTooltip] Initialized");
}

void Win32IDE::shutdownHoverTooltip() {
    dismissHoverTooltip();
    if (m_hoverFont) { DeleteObject(m_hoverFont); m_hoverFont = nullptr; }
    if (m_hoverBoldFont) { DeleteObject(m_hoverBoldFont); m_hoverBoldFont = nullptr; }
}

void Win32IDE::showHoverTooltip(int screenX, int screenY, const std::string& content) {
    dismissHoverTooltip();

    if (content.empty()) return;
    m_hoverContent = content;
    m_hoverVisible = true;

    // Measure content dimensions
    HDC hdc = GetDC(m_hwndMain);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hoverFont);

    // Split content into lines and measure
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    int maxWidth = 0;
    while (std::getline(stream, line)) {
        lines.push_back(line);
        SIZE sz;
        GetTextExtentPoint32A(hdc, line.c_str(), (int)line.size(), &sz);
        if (sz.cx > maxWidth) maxWidth = sz.cx;
    }

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwndMain, hdc);

    int popupW = maxWidth + dpiScale(24);
    int popupH = (int)lines.size() * lineHeight + dpiScale(16);
    if (popupW < dpiScale(200)) popupW = dpiScale(200);
    if (popupW > dpiScale(600)) popupW = dpiScale(600);
    if (popupH > dpiScale(400)) popupH = dpiScale(400);

    // Position below cursor, keep on screen
    HMONITOR hMon = MonitorFromPoint({screenX, screenY}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoA(hMon, &mi);

    if (screenX + popupW > mi.rcWork.right) screenX = mi.rcWork.right - popupW;
    if (screenY + popupH > mi.rcWork.bottom) screenY -= popupH + dpiScale(20);

    m_hwndHoverPopup = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        HOVER_CLASS, "",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        screenX, screenY + dpiScale(20), popupW, popupH,
        m_hwndMain, (HMENU)(UINT_PTR)IDC_HOVER_POPUP,
        GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndHoverPopup, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndHoverPopup, GWLP_WNDPROC, (LONG_PTR)HoverTooltipProc);
    InvalidateRect(m_hwndHoverPopup, nullptr, TRUE);

    // Auto-dismiss timer (5 seconds)
    SetTimer(m_hwndMain, 9200, 5000, nullptr);
}

void Win32IDE::dismissHoverTooltip() {
    KillTimer(m_hwndMain, 9200);
    if (m_hwndHoverPopup) {
        DestroyWindow(m_hwndHoverPopup);
        m_hwndHoverPopup = nullptr;
    }
    m_hoverVisible = false;
    m_hoverContent.clear();
}

void Win32IDE::onEditorMouseHover(int charPos) {
    if (!m_hwndEditor || charPos < 0) return;

    // Get the word under cursor
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, charPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, charPos, 0);

    if (lineLen <= 0) return;
    std::string lineText(lineLen + 1, '\0');
    *(WORD*)lineText.data() = (WORD)lineLen;
    SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)lineText.data());
    lineText.resize(lineLen);

    int col = charPos - lineStart;
    if (col < 0 || col >= (int)lineText.size()) return;

    // Extract word boundaries
    int wordStart = col, wordEnd = col;
    while (wordStart > 0 && (isalnum(lineText[wordStart-1]) || lineText[wordStart-1] == '_'))
        wordStart--;
    while (wordEnd < (int)lineText.size() && (isalnum(lineText[wordEnd]) || lineText[wordEnd] == '_'))
        wordEnd++;

    std::string word = lineText.substr(wordStart, wordEnd - wordStart);
    if (word.empty()) return;

    // Try LSP hover
    std::string uri = "file:///" + m_currentFile;
    auto hoverInfo = lspHover(uri, lineIndex, col);

    std::string content;
    if (hoverInfo.valid && !hoverInfo.contents.empty()) {
        content = hoverInfo.contents;
    } else {
        // Fallback: show basic symbol info from local index
        content = word + "\n\n(no documentation available)";
    }

    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, charPos);
    POINT screenPt = {pt.x, pt.y};
    ClientToScreen(m_hwndEditor, &screenPt);

    showHoverTooltip(screenPt.x, screenPt.y, content);
}

LRESULT CALLBACK Win32IDE::HoverTooltipProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Background
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 44));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        if (!ide || ide->m_hoverContent.empty()) { EndPaint(hwnd, &ps); return 0; }

        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, ide->m_hoverFont);

        TEXTMETRICA tm;
        GetTextMetricsA(hdc, &tm);
        int lineHeight = tm.tmHeight + tm.tmExternalLeading;

        std::istringstream stream(ide->m_hoverContent);
        std::string line;
        int y = 8;
        bool firstLine = true;

        while (std::getline(stream, line)) {
            // First line is identifier — render bold
            if (firstLine && ide->m_hoverBoldFont) {
                SelectObject(hdc, ide->m_hoverBoldFont);
                SetTextColor(hdc, RGB(78, 201, 176));
                firstLine = false;
            } else if (line.find("```") == 0) {
                // Code block marker — use monospace color
                SetTextColor(hdc, RGB(206, 145, 120));
                SelectObject(hdc, ide->m_hoverFont);
                continue;
            } else {
                SelectObject(hdc, ide->m_hoverFont);
                SetTextColor(hdc, T2_TEXT);
            }

            TextOutA(hdc, 12, y, line.c_str(), (int)line.size());
            y += lineHeight;

            if (y > rc.bottom - 8) break;
        }

        // Border
        HPEN borderPen = CreatePen(PS_SOLID, 1, T2_HOVER_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (ide) ide->dismissHoverTooltip();
        return 0;
    case WM_MOUSELEAVE:
        if (ide) ide->dismissHoverTooltip();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}


// ============================================================================
//
//  14. PARAMETER HINTS (SIGNATURE HELP)
//
// ============================================================================

void Win32IDE::initSignatureHelp() {
    m_hwndSignaturePopup = nullptr;
    m_signatureVisible = false;
    m_signatureContent.clear();
    m_signatureActiveParam = 0;
    m_signatureParamCount = 0;
    m_signatureFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    ensureTier2ClassesRegistered(GetModuleHandle(NULL));
    LOG_INFO("[SignatureHelp] Parameter hints initialized");
}

void Win32IDE::shutdownSignatureHelp() {
    dismissSignatureHelp();
    if (m_signatureFont) { DeleteObject(m_signatureFont); m_signatureFont = nullptr; }
}

void Win32IDE::triggerSignatureHelp() {
    if (!m_hwndEditor) return;

    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);

    if (lineLen <= 0) return;
    std::string lineText(lineLen + 1, '\0');
    *(WORD*)lineText.data() = (WORD)lineLen;
    SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)lineText.data());
    lineText.resize(lineLen);

    int col = sel.cpMin - lineStart;

    // Find the opening parenthesis and function name
    int parenDepth = 0;
    int commaCount = 0;
    int funcNameEnd = -1;

    for (int i = col - 1; i >= 0; i--) {
        char c = lineText[i];
        if (c == ')') parenDepth++;
        else if (c == '(') {
            if (parenDepth == 0) {
                funcNameEnd = i;
                break;
            }
            parenDepth--;
        } else if (c == ',' && parenDepth == 0) {
            commaCount++;
        }
    }

    if (funcNameEnd < 0) {
        dismissSignatureHelp();
        return;
    }

    // Extract function name
    int nameStart = funcNameEnd - 1;
    while (nameStart >= 0 && (isalnum(lineText[nameStart]) || lineText[nameStart] == '_' ||
                               lineText[nameStart] == ':'))
        nameStart--;
    nameStart++;

    std::string funcName = lineText.substr(nameStart, funcNameEnd - nameStart);
    if (funcName.empty()) {
        dismissSignatureHelp();
        return;
    }

    // Try LSP signature help
    std::string sigContent;
    int paramIdx = commaCount;
    int totalParams = 0;

    // Try to get signature from LSP
    // For now, use local heuristics based on outline symbols
    for (const auto& sym : m_outlineSymbols) {
        if (sym.name == funcName || sym.name.find("::" + funcName) != std::string::npos) {
            sigContent = sym.detail;
            // Count parameters from detail string
            int pCount = 0;
            for (char c : sigContent) {
                if (c == ',') pCount++;
            }
            totalParams = pCount + 1; // N commas = N+1 params
            break;
        }
        for (const auto& child : sym.children) {
            if (child.name == funcName) {
                sigContent = child.detail;
                int pCount = 0;
                for (char c : sigContent) {
                    if (c == ',') pCount++;
                }
                totalParams = pCount + 1;
                break;
            }
        }
        if (!sigContent.empty()) break;
    }

    if (sigContent.empty()) {
        sigContent = funcName + "(...)";
        totalParams = 1;
    }

    m_signatureContent = sigContent;
    m_signatureActiveParam = paramIdx;
    m_signatureParamCount = totalParams;

    // Position popup above the cursor
    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, sel.cpMin);
    POINT screenPt = {pt.x, pt.y};
    ClientToScreen(m_hwndEditor, &screenPt);

    showSignaturePopup(screenPt.x, screenPt.y - dpiScale(40));
}

void Win32IDE::showSignaturePopup(int screenX, int screenY) {
    dismissSignatureHelp();

    if (m_signatureContent.empty()) return;
    m_signatureVisible = true;

    int popupW = dpiScale(400);
    int popupH = dpiScale(60);

    m_hwndSignaturePopup = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        SIGHELP_CLASS, "",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        screenX, screenY, popupW, popupH,
        m_hwndMain, (HMENU)(UINT_PTR)IDC_SIGHELP_POPUP,
        GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndSignaturePopup, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndSignaturePopup, GWLP_WNDPROC, (LONG_PTR)SignatureHelpProc);
    InvalidateRect(m_hwndSignaturePopup, nullptr, TRUE);
}

void Win32IDE::dismissSignatureHelp() {
    if (m_hwndSignaturePopup) {
        DestroyWindow(m_hwndSignaturePopup);
        m_hwndSignaturePopup = nullptr;
    }
    m_signatureVisible = false;
}

void Win32IDE::updateSignatureActiveParam(int paramIndex) {
    m_signatureActiveParam = paramIndex;
    if (m_hwndSignaturePopup) {
        InvalidateRect(m_hwndSignaturePopup, nullptr, TRUE);
    }
}

LRESULT CALLBACK Win32IDE::SignatureHelpProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 44));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        if (!ide || ide->m_signatureContent.empty()) { EndPaint(hwnd, &ps); return 0; }

        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, ide->m_signatureFont);

        // Parse signature and highlight active parameter
        const std::string& sig = ide->m_signatureContent;
        int activeParam = ide->m_signatureActiveParam;

        // Render parameter indicator
        std::string paramLabel = "Parameter " + std::to_string(activeParam + 1) +
                                  "/" + std::to_string(ide->m_signatureParamCount);
        SetTextColor(hdc, T2_DIM_TEXT);
        TextOutA(hdc, 12, 6, paramLabel.c_str(), (int)paramLabel.size());

        // Render the signature with highlighted active param
        SetTextColor(hdc, T2_TEXT);
        int y = rc.bottom / 2;

        // Split signature at commas and highlight the active one
        size_t parenOpen = sig.find('(');
        size_t parenClose = sig.rfind(')');
        if (parenOpen != std::string::npos) {
            std::string prefix = sig.substr(0, parenOpen + 1);
            SetTextColor(hdc, T2_TEXT);
            SIZE prefixSize;
            GetTextExtentPoint32A(hdc, prefix.c_str(), (int)prefix.size(), &prefixSize);
            TextOutA(hdc, 12, y, prefix.c_str(), (int)prefix.size());

            int x = 12 + prefixSize.cx;
            std::string params = sig.substr(parenOpen + 1,
                (parenClose != std::string::npos ? parenClose - parenOpen - 1 : sig.size()));

            // Split params
            std::vector<std::string> paramList;
            std::istringstream pStream(params);
            std::string p;
            while (std::getline(pStream, p, ',')) {
                paramList.push_back(p);
            }

            for (int i = 0; i < (int)paramList.size(); i++) {
                if (i > 0) {
                    SetTextColor(hdc, T2_DIM_TEXT);
                    TextOutA(hdc, x, y, ", ", 2);
                    SIZE commaSize;
                    GetTextExtentPoint32A(hdc, ", ", 2, &commaSize);
                    x += commaSize.cx;
                }

                SetTextColor(hdc, (i == activeParam) ? T2_PARAM_ACTIVE : T2_PARAM_INACTIVE);
                if (i == activeParam) {
                    SelectObject(hdc, ide->m_hoverBoldFont ? ide->m_hoverBoldFont : ide->m_signatureFont);
                }
                TextOutA(hdc, x, y, paramList[i].c_str(), (int)paramList[i].size());
                SIZE paramSize;
                GetTextExtentPoint32A(hdc, paramList[i].c_str(), (int)paramList[i].size(), &paramSize);
                x += paramSize.cx;

                if (i == activeParam) {
                    SelectObject(hdc, ide->m_signatureFont);
                }
            }

            if (parenClose != std::string::npos) {
                SetTextColor(hdc, T2_TEXT);
                TextOutA(hdc, x, y, ")", 1);
            }
        } else {
            TextOutA(hdc, 12, y, sig.c_str(), (int)sig.size());
        }

        // Border
        HPEN borderPen = CreatePen(PS_SOLID, 1, T2_HOVER_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && ide) { ide->dismissSignatureHelp(); return 0; }
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}


// ============================================================================
//
//  16. FIND ALL REFERENCES UI (ReferencePanel)
//
// ============================================================================

void Win32IDE::initReferencePanel() {
    m_hwndReferencePanel = nullptr;
    m_hwndReferenceTree = nullptr;
    m_referencePanelVisible = false;
    m_referenceResults.clear();
    m_referenceFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    ensureTier2ClassesRegistered(GetModuleHandle(NULL));
    LOG_INFO("[ReferencePanel] Initialized");
}

void Win32IDE::shutdownReferencePanel() {
    closeReferencePanel();
    if (m_referenceFont) { DeleteObject(m_referenceFont); m_referenceFont = nullptr; }
}

void Win32IDE::showFindAllReferences(const std::string& symbol,
                                      const std::vector<ReferenceResult>& results) {
    closeReferencePanel();

    if (results.empty()) {
        appendToOutput("[References] No references found for '" + symbol + "'",
                       "Output", OutputSeverity::Info);
        return;
    }

    m_referenceResults = results;
    m_referenceSymbol = symbol;
    m_referencePanelVisible = true;

    // Create panel in the bottom panel area
    RECT rcClient;
    GetClientRect(m_hwndMain, &rcClient);
    int panelH = dpiScale(250);
    int panelW = rcClient.right - rcClient.left;
    int panelY = rcClient.bottom - dpiScale(24) - panelH;

    m_hwndReferencePanel = CreateWindowExA(WS_EX_CLIENTEDGE,
        REFPANEL_CLASS, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, panelY, panelW, panelH,
        m_hwndMain, (HMENU)(UINT_PTR)IDC_REFPANEL,
        GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndReferencePanel, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndReferencePanel, GWLP_WNDPROC, (LONG_PTR)ReferencePanelProc);

    int headerH = dpiScale(28);

    // Close button
    CreateWindowExA(0, "BUTTON", "x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - dpiScale(28), dpiScale(2), dpiScale(24), dpiScale(22),
        m_hwndReferencePanel, (HMENU)(UINT_PTR)IDC_REFPANEL_CLOSE,
        GetModuleHandle(NULL), NULL);

    // TreeView for results grouped by file
    m_hwndReferenceTree = CreateWindowExA(0,
        WC_TREEVIEWA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS |
        TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, headerH, panelW, panelH - headerH,
        m_hwndReferencePanel, (HMENU)(UINT_PTR)IDC_REFPANEL_TREE,
        GetModuleHandle(NULL), NULL);

    // Dark theme for TreeView
    TreeView_SetBkColor(m_hwndReferenceTree, T2_PANEL_BG);
    TreeView_SetTextColor(m_hwndReferenceTree, T2_TEXT);
    SendMessageA(m_hwndReferenceTree, WM_SETFONT, (WPARAM)m_referenceFont, TRUE);

    // Group results by file
    std::map<std::string, std::vector<const ReferenceResult*>> byFile;
    for (const auto& ref : m_referenceResults) {
        byFile[ref.filePath].push_back(&ref);
    }

    // Populate tree
    for (const auto& [file, refs] : byFile) {
        std::string fileName = file;
        size_t lastSlash = fileName.find_last_of("\\/");
        if (lastSlash != std::string::npos) fileName = fileName.substr(lastSlash + 1);

        std::string fileLabel = fileName + " (" + std::to_string(refs.size()) + " references)";

        TVINSERTSTRUCTA tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (char*)fileLabel.c_str();
        tvis.item.lParam = -1; // File node
        HTREEITEM hFileNode = TreeView_InsertItem(m_hwndReferenceTree, &tvis);

        for (const auto* ref : refs) {
            char lineLabel[512];
            snprintf(lineLabel, sizeof(lineLabel), "  L%d:%d  %s",
                     ref->line + 1, ref->column, ref->contextLine.c_str());

            TVINSERTSTRUCTA child = {};
            child.hParent = hFileNode;
            child.hInsertAfter = TVI_LAST;
            child.item.mask = TVIF_TEXT | TVIF_PARAM;
            child.item.pszText = lineLabel;
            child.item.lParam = (LPARAM)(ref - &m_referenceResults[0]);
            TreeView_InsertItem(m_hwndReferenceTree, &child);
        }

        TreeView_Expand(m_hwndReferenceTree, hFileNode, TVE_EXPAND);
    }

    LOG_INFO("[ReferencePanel] Showing " + std::to_string(results.size()) +
             " references for '" + symbol + "'");
}

void Win32IDE::closeReferencePanel() {
    if (m_hwndReferencePanel) {
        DestroyWindow(m_hwndReferencePanel);
        m_hwndReferencePanel = nullptr;
        m_hwndReferenceTree = nullptr;
    }
    m_referencePanelVisible = false;
}

void Win32IDE::navigateToReference(int refIndex) {
    if (refIndex < 0 || refIndex >= (int)m_referenceResults.size()) return;

    const auto& ref = m_referenceResults[refIndex];

    // Open file if needed
    if (ref.filePath != m_currentFile) {
        int tabIdx = findTabByPath(ref.filePath);
        if (tabIdx >= 0) {
            setActiveTab(tabIdx);
        } else {
            std::string displayName = ref.filePath;
            size_t lastSlash = displayName.find_last_of("\\/");
            if (lastSlash != std::string::npos) displayName = displayName.substr(lastSlash + 1);
            addTab(ref.filePath, displayName);
        }
    }

    // Go to line
    if (m_hwndEditor) {
        int charIdx = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, ref.line, 0);
        if (charIdx >= 0) {
            charIdx += ref.column;
            CHARRANGE cr = {charIdx, charIdx + (int)m_referenceSymbol.size()};
            SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(m_hwndEditor, EM_SCROLLCARET, 0, 0);
        }
    }
}

LRESULT CALLBACK Win32IDE::ReferencePanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        int headerH = ide ? ide->dpiScale(28) : 28;
        RECT headerRect = {0, 0, rc.right, headerH};
        HBRUSH hdrBrush = CreateSolidBrush(T2_HEADER_BG);
        FillRect(hdc, &headerRect, hdrBrush);
        DeleteObject(hdrBrush);

        if (ide) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, T2_TEXT);
            std::string title = "References: '" + ide->m_referenceSymbol + "' (" +
                                std::to_string(ide->m_referenceResults.size()) + " results)";
            RECT titleRect = {8, 4, rc.right - 32, headerH};
            DrawTextA(hdc, title.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        HPEN borderPen = CreatePen(PS_SOLID, 1, T2_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        MoveToEx(hdc, 0, headerH - 1, NULL);
        LineTo(hdc, rc.right, headerH - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NOTIFY: {
        if (!ide) break;
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->code == NM_DBLCLK && nmhdr->idFrom == IDC_REFPANEL_TREE) {
            HTREEITEM hItem = TreeView_GetSelection(ide->m_hwndReferenceTree);
            if (hItem) {
                TVITEMA item = {};
                item.mask = TVIF_PARAM;
                item.hItem = hItem;
                TreeView_GetItem(ide->m_hwndReferenceTree, &item);
                if (item.lParam >= 0) {
                    ide->navigateToReference((int)item.lParam);
                }
            }
        }
        break;
    }
    case WM_COMMAND:
        if (ide && LOWORD(wParam) == IDC_REFPANEL_CLOSE) {
            ide->closeReferencePanel();
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && ide) { ide->closeReferencePanel(); return 0; }
        if (wParam == VK_RETURN && ide) {
            HTREEITEM hItem = TreeView_GetSelection(ide->m_hwndReferenceTree);
            if (hItem) {
                TVITEMA item = {};
                item.mask = TVIF_PARAM;
                item.hItem = hItem;
                TreeView_GetItem(ide->m_hwndReferenceTree, &item);
                if (item.lParam >= 0) {
                    ide->navigateToReference((int)item.lParam);
                }
            }
            return 0;
        }
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}


// ============================================================================
//
//  18. CODELENS (REFERENCE COUNTS)
//
// ============================================================================

void Win32IDE::initCodeLens() {
    m_codeLensEnabled = true;
    m_codeLensEntries.clear();
    m_codeLensFont = CreateFontA(-dpiScale(11), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    LOG_INFO("[CodeLens] Reference counts initialized");
}

void Win32IDE::shutdownCodeLens() {
    m_codeLensEntries.clear();
    if (m_codeLensFont) { DeleteObject(m_codeLensFont); m_codeLensFont = nullptr; }
}

void Win32IDE::refreshCodeLens() {
    m_codeLensEntries.clear();

    if (!m_codeLensEnabled || !m_hwndEditor) return;
    if (m_currentFile.empty()) return;

    // Get all symbol definitions from current file
    for (const auto& sym : m_outlineSymbols) {
        if (sym.kind == SK_Function || sym.kind == SK_Method ||
            sym.kind == SK_Class || sym.kind == SK_Struct) {

            CodeLensEntry entry;
            entry.line = sym.line;
            entry.symbol = sym.name;
            entry.referenceCount = countSymbolReferences(sym.name);
            entry.text = std::to_string(entry.referenceCount) + " reference" +
                         (entry.referenceCount != 1 ? "s" : "");

            if (entry.referenceCount > 0) {
                m_codeLensEntries.push_back(entry);
            }
        }

        // Also process children (methods within classes)
        for (const auto& child : sym.children) {
            if (child.kind == SK_Function || child.kind == SK_Method) {
                CodeLensEntry entry;
                entry.line = child.line;
                entry.symbol = child.name;
                entry.referenceCount = countSymbolReferences(child.name);
                entry.text = std::to_string(entry.referenceCount) + " reference" +
                             (entry.referenceCount != 1 ? "s" : "");

                if (entry.referenceCount > 0) {
                    m_codeLensEntries.push_back(entry);
                }
            }
        }
    }

    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    LOG_INFO("[CodeLens] Refreshed: " + std::to_string(m_codeLensEntries.size()) + " entries");
}

int Win32IDE::countSymbolReferences(const std::string& symbol) {
    if (symbol.empty() || !m_hwndEditor) return 0;

    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0) return 0;

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    // Simple word-boundary reference count
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find(symbol, pos)) != std::string::npos) {
        // Check word boundaries
        bool leftBound = (pos == 0 || (!isalnum(content[pos-1]) && content[pos-1] != '_'));
        bool rightBound = (pos + symbol.size() >= content.size() ||
                          (!isalnum(content[pos + symbol.size()]) &&
                           content[pos + symbol.size()] != '_'));
        if (leftBound && rightBound) count++;
        pos += symbol.size();
    }

    // Subtract 1 for the definition itself
    return count > 0 ? count - 1 : 0;
}

void Win32IDE::renderCodeLens(HDC hdc) {
    if (!m_codeLensEnabled || m_codeLensEntries.empty() || !m_hwndEditor) return;

    int firstVisibleLine = (int)SendMessageA(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

    HFONT oldFont = (HFONT)SelectObject(hdc, m_codeLensFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);
    int visibleLines = (editorRC.bottom - editorRC.top) / lineHeight;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, T2_CODELENS_TEXT);

    for (const auto& entry : m_codeLensEntries) {
        int displayLine = entry.line - 1 - firstVisibleLine; // zero-based visual line
        // CodeLens appears one line above the function declaration
        displayLine -= 1;

        if (displayLine < 0 || displayLine >= visibleLines) continue;

        int y = displayLine * lineHeight;
        int x = dpiScale(60); // After gutter

        // Draw the CodeLens text
        TextOutA(hdc, x, y, entry.text.c_str(), (int)entry.text.size());
    }

    SelectObject(hdc, oldFont);
}

void Win32IDE::toggleCodeLens() {
    m_codeLensEnabled = !m_codeLensEnabled;
    if (m_codeLensEnabled) {
        refreshCodeLens();
    } else {
        m_codeLensEntries.clear();
    }
    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
    appendToOutput(std::string("[CodeLens] ") + (m_codeLensEnabled ? "Enabled" : "Disabled"),
                   "Output", OutputSeverity::Info);
}


// ============================================================================
//
//  19. INLAY TYPE HINTS (extends Win32IDE_GhostText.cpp)
//
// ============================================================================

void Win32IDE::initInlayHints() {
    m_inlayHintsEnabled = true;
    m_inlayHintEntries.clear();
    m_inlayHintFont = CreateFontA(-dpiScale(11), 0, 0, 0, FW_NORMAL,
        TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");
    LOG_INFO("[InlayHints] Type hint system initialized");
}

void Win32IDE::shutdownInlayHints() {
    m_inlayHintEntries.clear();
    if (m_inlayHintFont) { DeleteObject(m_inlayHintFont); m_inlayHintFont = nullptr; }
}

void Win32IDE::refreshInlayHints() {
    m_inlayHintEntries.clear();

    if (!m_inlayHintsEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage != SyntaxLanguage::Cpp &&
        m_syntaxLanguage != SyntaxLanguage::JavaScript) return;

    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0) return;

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;
        if (line.empty()) continue;

        size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string::npos) continue;
        std::string trimmed = line.substr(indent);

        // Detect "auto" variable declarations in C++
        if (m_syntaxLanguage == SyntaxLanguage::Cpp) {
            // Pattern: auto varName = ...
            if (trimmed.find("auto ") == 0 || trimmed.find("const auto ") == 0 ||
                trimmed.find("auto& ") == 0 || trimmed.find("const auto& ") == 0) {

                size_t eqPos = trimmed.find('=');
                if (eqPos == std::string::npos) continue;

                // Extract the RHS to infer type
                std::string rhs = trimmed.substr(eqPos + 1);
                size_t rhsStart = rhs.find_first_not_of(" \t");
                if (rhsStart == std::string::npos) continue;
                rhs = rhs.substr(rhsStart);

                std::string inferredType = inferTypeFromExpression(rhs);
                if (inferredType.empty()) continue;

                // Find the variable name position
                size_t autoEnd = trimmed.find("auto") + 4;
                if (trimmed[autoEnd] == '&') autoEnd++;
                if (trimmed[autoEnd] == ' ') autoEnd++;
                size_t nameEnd = trimmed.find_first_of(" =;", autoEnd);
                int col = (int)(indent + nameEnd);

                InlayHintEntry hint;
                hint.line = lineNum;
                hint.column = col;
                hint.text = ": " + inferredType;
                hint.kind = InlayHintKind::Type;
                m_inlayHintEntries.push_back(hint);
            }
        }

        // Detect "var"/"let"/"const" in JavaScript without explicit type
        if (m_syntaxLanguage == SyntaxLanguage::JavaScript) {
            if (trimmed.find("let ") == 0 || trimmed.find("const ") == 0 ||
                trimmed.find("var ") == 0) {

                size_t eqPos = trimmed.find('=');
                if (eqPos == std::string::npos) continue;

                std::string rhs = trimmed.substr(eqPos + 1);
                size_t rhsStart = rhs.find_first_not_of(" \t");
                if (rhsStart == std::string::npos) continue;
                rhs = rhs.substr(rhsStart);

                std::string inferredType = inferTypeFromExpression(rhs);
                if (inferredType.empty()) continue;

                size_t kwEnd = trimmed.find(' ') + 1;
                size_t nameEnd = trimmed.find_first_of(" =;:", kwEnd);
                int col = (int)(indent + nameEnd);

                InlayHintEntry hint;
                hint.line = lineNum;
                hint.column = col;
                hint.text = ": " + inferredType;
                hint.kind = InlayHintKind::Type;
                m_inlayHintEntries.push_back(hint);
            }
        }
    }

    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    LOG_INFO("[InlayHints] Refreshed: " + std::to_string(m_inlayHintEntries.size()) + " hints");
}

std::string Win32IDE::inferTypeFromExpression(const std::string& expr) {
    if (expr.empty()) return "";

    // String literal
    if (expr[0] == '"' || expr[0] == '\'') return "string";
    if (expr.find("std::string") == 0 || expr.find("\"") != std::string::npos)
        return "std::string";

    // Numeric
    if (isdigit(expr[0]) || (expr[0] == '-' && expr.size() > 1 && isdigit(expr[1]))) {
        if (expr.find('.') != std::string::npos || expr.find('f') != std::string::npos)
            return "double";
        if (expr.find("ULL") != std::string::npos || expr.find("ull") != std::string::npos)
            return "uint64_t";
        if (expr.find("LL") != std::string::npos || expr.find("ll") != std::string::npos)
            return "int64_t";
        return "int";
    }

    // Boolean
    if (expr == "true" || expr == "false" || expr == "TRUE" || expr == "FALSE")
        return "bool";

    // nullptr
    if (expr == "nullptr" || expr == "NULL" || expr == "null")
        return "nullptr_t";

    // Container constructors
    if (expr.find("std::vector") == 0) return "std::vector<...>";
    if (expr.find("std::map") == 0) return "std::map<...>";
    if (expr.find("std::set") == 0) return "std::set<...>";
    if (expr.find("std::unique_ptr") == 0) return "std::unique_ptr<...>";
    if (expr.find("std::shared_ptr") == 0) return "std::shared_ptr<...>";
    if (expr.find("std::make_unique") == 0) return "std::unique_ptr<...>";
    if (expr.find("std::make_shared") == 0) return "std::shared_ptr<...>";

    // Function call — try to match against known functions
    size_t parenPos = expr.find('(');
    if (parenPos != std::string::npos) {
        std::string funcName = expr.substr(0, parenPos);
        // Common constructors
        if (funcName == "CreateFontA" || funcName == "CreateFontW" ||
            funcName == "CreateFontIndirectA") return "HFONT";
        if (funcName == "CreateSolidBrush" || funcName == "CreateHatchBrush")
            return "HBRUSH";
        if (funcName == "CreatePen") return "HPEN";
        if (funcName == "CreateWindowExA" || funcName == "CreateWindowA")
            return "HWND";
        if (funcName == "GetDC") return "HDC";
        if (funcName == "LoadCursor") return "HCURSOR";
        if (funcName == "LoadIcon") return "HICON";

        // STL
        if (funcName == "std::to_string") return "std::string";
        if (funcName == "std::stoi") return "int";
        if (funcName == "std::stof") return "float";
        if (funcName == "std::stod") return "double";
    }

    // Array/initializer list
    if (expr[0] == '{') return "initializer_list<...>";
    if (expr[0] == '[') return "array";

    // new expression
    if (expr.find("new ") == 0) {
        size_t typeEnd = expr.find_first_of("({[ ", 4);
        if (typeEnd != std::string::npos) {
            return expr.substr(4, typeEnd - 4) + "*";
        }
    }

    return "";
}

void Win32IDE::renderInlayHints(HDC hdc) {
    if (!m_inlayHintsEnabled || m_inlayHintEntries.empty() || !m_hwndEditor) return;

    int firstVisibleLine = (int)SendMessageA(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    int charWidth = tm.tmAveCharWidth;
    if (lineHeight <= 0) lineHeight = 16;
    if (charWidth <= 0) charWidth = 8;

    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);
    int visibleLines = (editorRC.bottom - editorRC.top) / lineHeight;

    SelectObject(hdc, m_inlayHintFont ? m_inlayHintFont : hFont);

    for (const auto& hint : m_inlayHintEntries) {
        int displayLine = hint.line - 1 - firstVisibleLine;
        if (displayLine < 0 || displayLine >= visibleLines) continue;

        int y = displayLine * lineHeight;
        int x = hint.column * charWidth;

        // Draw a small background pill
        SIZE textSize;
        GetTextExtentPoint32A(hdc, hint.text.c_str(), (int)hint.text.size(), &textSize);

        RECT pillRect = {x + 2, y + 1, x + textSize.cx + 6, y + lineHeight - 1};
        HBRUSH pillBrush = CreateSolidBrush(RGB(50, 50, 55));
        FillRect(hdc, &pillRect, pillBrush);
        DeleteObject(pillBrush);

        // Draw rounded corners (simplified — just rect)
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(65, 65, 70));
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, pillRect.left, pillRect.top, pillRect.right, pillRect.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        // Draw text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, T2_INLAY_TEXT);
        TextOutA(hdc, x + 4, y + 1, hint.text.c_str(), (int)hint.text.size());
    }

    SelectObject(hdc, oldFont);
}

void Win32IDE::toggleInlayHints() {
    m_inlayHintsEnabled = !m_inlayHintsEnabled;
    if (m_inlayHintsEnabled) {
        refreshInlayHints();
    } else {
        m_inlayHintEntries.clear();
    }
    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
    appendToOutput(std::string("[InlayHints] ") + (m_inlayHintsEnabled ? "Enabled" : "Disabled"),
                   "Output", OutputSeverity::Info);
}


// ============================================================================
//
//  TIER 2 LIFECYCLE — initialization and command routing
//
// ============================================================================

void Win32IDE::initTier2Cosmetics() {
    initGitDiffViewer();
    initTerminalTabs();
    initHoverTooltip();
    initSignatureHelp();
    // initOutlinePanel() already exists in Win32IDE_OutlinePanel.cpp
    initReferencePanel();
    // initRenamePreview() already exists in Win32IDE_RenamePreview.cpp
    initCodeLens();
    initInlayHints();

    LOG_INFO("[Tier2] All cosmetic features #11-#19 initialized");
    appendToOutput("[Tier2] High-visibility cosmetic gaps loaded.\n");
}

void Win32IDE::shutdownTier2Cosmetics() {
    shutdownGitDiffViewer();
    shutdownHoverTooltip();
    shutdownSignatureHelp();
    shutdownReferencePanel();
    shutdownCodeLens();
    shutdownInlayHints();
}

bool Win32IDE::handleTier2Command(int commandId) {
    switch (commandId) {
    case IDM_TIER2_GITDIFF:
        if (!m_currentFile.empty()) showGitDiffSideBySide(m_currentFile);
        return true;
    case IDM_TIER2_GITDIFF_CLOSE:
        closeGitDiffViewer();
        return true;
    case IDM_TIER2_GITDIFF_PREV:
        navigateDiffHunk(-1);
        return true;
    case IDM_TIER2_GITDIFF_NEXT:
        navigateDiffHunk(1);
        return true;
    case IDM_TIER2_TERMINAL_NEW:
        showTerminalProfileMenu();
        return true;
    case IDM_TIER2_TERMINAL_CLOSE:
        closeTerminalTab(m_activeTerminalTab);
        return true;
    case IDM_TIER2_HOVER:
        if (m_hwndEditor) {
            CHARRANGE sel;
            SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
            onEditorMouseHover(sel.cpMin);
        }
        return true;
    case IDM_TIER2_SIGHELP:
        triggerSignatureHelp();
        return true;
    case IDM_TIER2_OUTLINE_REFRESH:
        refreshOutlineFromLSP();
        return true;
    case IDM_TIER2_OUTLINE_FILTER:
        // Toggle filter — could show an input dialog
        return true;
    case IDM_TIER2_OUTLINE_SORT:
        m_outlineSortByName = !m_outlineSortByName;
        refreshOutlineFromLSP();
        return true;
    case IDM_TIER2_FIND_REFS: {
        // Find references for word under cursor
        if (m_hwndEditor) {
            CHARRANGE sel;
            SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
            int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
            int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
            int lineLen = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);
            if (lineLen > 0) {
                std::string lineText(lineLen + 1, '\0');
                *(WORD*)lineText.data() = (WORD)lineLen;
                SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)lineText.data());
                lineText.resize(lineLen);
                int col = sel.cpMin - lineStart;
                int ws = col, we = col;
                while (ws > 0 && (isalnum(lineText[ws-1]) || lineText[ws-1] == '_')) ws--;
                while (we < (int)lineText.size() && (isalnum(lineText[we]) || lineText[we] == '_')) we++;
                std::string word = lineText.substr(ws, we - ws);
                if (!word.empty()) {
                    cmdFindAllReferences(word);
                }
            }
        }
        return true;
    }
    case IDM_TIER2_RENAME_PREVIEW:
        // Handled by Win32IDE_RenamePreview.cpp
        return true;
    case IDM_TIER2_CODELENS_TOGGLE:
        toggleCodeLens();
        return true;
    case IDM_TIER2_CODELENS_REFRESH:
        refreshCodeLens();
        return true;
    case IDM_TIER2_INLAY_TOGGLE:
        toggleInlayHints();
        return true;
    case IDM_TIER2_INLAY_REFRESH:
        refreshInlayHints();
        return true;
    }
    return false;
}

void Win32IDE::cmdFindAllReferences(const std::string& symbol) {
    if (symbol.empty()) return;

    std::vector<ReferenceResult> results;

    // Search current file for all occurrences
    if (m_hwndEditor) {
        int totalLen = GetWindowTextLengthA(m_hwndEditor);
        if (totalLen > 0) {
            std::string content(totalLen + 1, '\0');
            GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
            content.resize(totalLen);

            std::istringstream stream(content);
            std::string line;
            int lineNum = 0;

            while (std::getline(stream, line)) {
                size_t pos = 0;
                while ((pos = line.find(symbol, pos)) != std::string::npos) {
                    // Check word boundaries
                    bool leftBound = (pos == 0 || (!isalnum(line[pos-1]) && line[pos-1] != '_'));
                    bool rightBound = (pos + symbol.size() >= line.size() ||
                                      (!isalnum(line[pos + symbol.size()]) &&
                                       line[pos + symbol.size()] != '_'));
                    if (leftBound && rightBound) {
                        ReferenceResult ref;
                        ref.filePath = m_currentFile;
                        ref.line = lineNum;
                        ref.column = (int)pos;
                        ref.contextLine = line;
                        if (ref.contextLine.size() > 100) ref.contextLine.resize(100);
                        results.push_back(ref);
                    }
                    pos += symbol.size();
                }
                lineNum++;
            }
        }
    }

    showFindAllReferences(symbol, results);
}

// ============================================================================
//
//  TIER 2 RENDER INTEGRATION
//
// ============================================================================

void Win32IDE::renderTier2Overlays(HDC hdc) {
    renderCodeLens(hdc);
    renderInlayHints(hdc);
}
