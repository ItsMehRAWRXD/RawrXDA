// ============================================================================
// Win32IDE_DiffView.cpp — Feature 11: Git Diff Side-by-Side Viewer
// Renders unified/split diff with color-coded hunks, navigation, inline toggle
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <fstream>

// ── Colors matching VS Code dark diff theme ────────────────────────────────
static const COLORREF DIFF_BG              = RGB(30, 30, 30);
static const COLORREF DIFF_ADDED_BG        = RGB(35, 61, 37);
static const COLORREF DIFF_REMOVED_BG      = RGB(72, 30, 30);
static const COLORREF DIFF_MODIFIED_BG     = RGB(52, 52, 28);
static const COLORREF DIFF_CONTEXT_BG      = RGB(30, 30, 30);
static const COLORREF DIFF_ADDED_GUTTER    = RGB(72, 156, 72);
static const COLORREF DIFF_REMOVED_GUTTER  = RGB(200, 72, 72);
static const COLORREF DIFF_TEXT_COLOR      = RGB(212, 212, 212);
static const COLORREF DIFF_LINE_NUM_COLOR  = RGB(110, 110, 110);
static const COLORREF DIFF_TOOLBAR_BG      = RGB(37, 37, 38);
static const COLORREF DIFF_SEPARATOR       = RGB(60, 60, 60);
static const COLORREF DIFF_HUNK_HIGHLIGHT  = RGB(38, 79, 120);

// ── Diff computation (Myers-like LCS) ──────────────────────────────────────
static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// Simple LCS-based diff
struct DiffOp {
    enum Type { Equal, Insert, Delete } type;
    int leftLine;
    int rightLine;
};

static std::vector<DiffOp> computeLCS(const std::vector<std::string>& a,
                                       const std::vector<std::string>& b) {
    int m = (int)a.size();
    int n = (int)b.size();

    // For very large files, use a simplified approach
    if (m > 10000 || n > 10000) {
        std::vector<DiffOp> ops;
        int i = 0, j = 0;
        while (i < m && j < n) {
            if (a[i] == b[j]) {
                ops.push_back({DiffOp::Equal, i, j});
                i++; j++;
            } else {
                ops.push_back({DiffOp::Delete, i, -1});
                i++;
            }
        }
        while (i < m) { ops.push_back({DiffOp::Delete, i++, -1}); }
        while (j < n) { ops.push_back({DiffOp::Insert, -1, j++}); }
        return ops;
    }

    // LCS table
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (a[i-1] == b[j-1])
                dp[i][j] = dp[i-1][j-1] + 1;
            else
                dp[i][j] = (std::max)(dp[i-1][j], dp[i][j-1]);
        }
    }

    // Backtrack to produce diff ops
    std::vector<DiffOp> ops;
    int i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i-1] == b[j-1]) {
            ops.push_back({DiffOp::Equal, i-1, j-1});
            i--; j--;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            ops.push_back({DiffOp::Insert, -1, j-1});
            j--;
        } else {
            ops.push_back({DiffOp::Delete, i-1, -1});
            i--;
        }
    }
    std::reverse(ops.begin(), ops.end());
    return ops;
}

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initDiffView() {
    m_diffState = DiffViewState{};
    m_diffState.hFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    LOG_INFO("[DiffView] Initialized");
}

void Win32IDE::shutdownDiffView() {
    closeDiffView();
    if (m_diffState.hFont) {
        DeleteObject(m_diffState.hFont);
        m_diffState.hFont = nullptr;
    }
    LOG_INFO("[DiffView] Shutdown");
}

// ── Open / Close ───────────────────────────────────────────────────────────
void Win32IDE::openDiffView(const std::string& leftContent, const std::string& rightContent,
                            const std::string& leftTitle, const std::string& rightTitle) {
    if (m_hwndDiffPanel) closeDiffView();

    m_diffState.visible = true;
    m_diffState.leftTitle = leftTitle;
    m_diffState.rightTitle = rightTitle;
    m_diffState.leftLines = splitLines(leftContent);
    m_diffState.rightLines = splitLines(rightContent);
    m_diffState.scrollPos = 0;
    m_diffState.currentHunk = -1;
    m_diffState.inlineMode = false;

    computeDiffHunks();

    // Create diff panel overlaying the editor area
    RECT rcClient;
    GetClientRect(m_hwndMain, &rcClient);
    int panelX = m_sidebarVisible ? m_sidebarWidth + 48 : 48;
    int panelW = rcClient.right - panelX;
    int panelH = rcClient.bottom - 22; // leave status bar

    m_hwndDiffPanel = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | SS_OWNERDRAW,
        panelX, 0, panelW, panelH, m_hwndMain,
        (HMENU)(UINT_PTR)IDC_DIFF_PANEL, GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndDiffPanel, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hwndDiffPanel, GWLP_WNDPROC, (LONG_PTR)DiffPanelProc);

    // Toolbar at top
    int toolbarH = dpiScale(32);
    m_hwndDiffToolbar = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, panelW, toolbarH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_TOOLBAR, GetModuleHandle(NULL), NULL);

    // Navigation buttons
    int btnW = dpiScale(28), btnH = dpiScale(24);
    int bx = dpiScale(8);
    CreateWindowExA(0, "BUTTON", "\xE2\x96\xB2", // ▲ prev
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        bx, dpiScale(4), btnW, btnH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_PREV_BTN, GetModuleHandle(NULL), NULL);
    bx += btnW + dpiScale(4);
    CreateWindowExA(0, "BUTTON", "\xE2\x96\xBC", // ▼ next
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        bx, dpiScale(4), btnW, btnH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_NEXT_BTN, GetModuleHandle(NULL), NULL);
    bx += btnW + dpiScale(12);
    CreateWindowExA(0, "BUTTON", "Inline",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        bx, dpiScale(4), dpiScale(48), btnH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_INLINE_BTN, GetModuleHandle(NULL), NULL);

    // Close button at right
    CreateWindowExA(0, "BUTTON", "X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - dpiScale(32), dpiScale(4), btnW, btnH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_CLOSE_BTN, GetModuleHandle(NULL), NULL);

    // Left and right panes
    int paneTop = toolbarH;
    int paneH = panelH - toolbarH;
    int halfW = panelW / 2;

    m_hwndDiffLeft = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, paneTop, halfW, paneH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_LEFT, GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndDiffLeft, "IDE_PTR", (HANDLE)this);

    m_hwndDiffRight = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        halfW, paneTop, panelW - halfW, paneH, m_hwndDiffPanel,
        (HMENU)(UINT_PTR)IDC_DIFF_RIGHT, GetModuleHandle(NULL), NULL);
    SetPropA(m_hwndDiffRight, "IDE_PTR", (HANDLE)this);

    InvalidateRect(m_hwndDiffPanel, NULL, TRUE);
    LOG_INFO("[DiffView] Opened: " + leftTitle + " <-> " + rightTitle);
}

void Win32IDE::closeDiffView() {
    if (m_hwndDiffPanel) {
        DestroyWindow(m_hwndDiffPanel);
        m_hwndDiffPanel = nullptr;
        m_hwndDiffLeft = nullptr;
        m_hwndDiffRight = nullptr;
        m_hwndDiffToolbar = nullptr;
    }
    m_diffState.visible = false;
    m_diffState.hunks.clear();
    m_diffState.leftLines.clear();
    m_diffState.rightLines.clear();
    LOG_INFO("[DiffView] Closed");
}

// ── Diff Computation ───────────────────────────────────────────────────────
void Win32IDE::computeDiffHunks() {
    m_diffState.hunks.clear();
    auto ops = computeLCS(m_diffState.leftLines, m_diffState.rightLines);

    DiffViewHunk current;
    bool inHunk = false;
    int contextBefore = 3;

    for (size_t i = 0; i < ops.size(); i++) {
        auto& op = ops[i];
        if (op.type == DiffOp::Equal) {
            if (inHunk) {
                m_diffState.hunks.push_back(current);
                current = DiffViewHunk{};
                inHunk = false;
            }
        } else {
            if (!inHunk) {
                current = DiffViewHunk{};
                current.leftStart = (op.leftLine >= 0) ? op.leftLine : 0;
                current.rightStart = (op.rightLine >= 0) ? op.rightLine : 0;
                current.leftCount = 0;
                current.rightCount = 0;
                inHunk = true;
            }
            if (op.type == DiffOp::Delete) {
                current.leftCount++;
                current.type = DiffViewHunk::Removed;
                if (op.leftLine >= 0 && op.leftLine < (int)m_diffState.leftLines.size())
                    current.leftText += m_diffState.leftLines[op.leftLine] + "\n";
            } else if (op.type == DiffOp::Insert) {
                current.rightCount++;
                current.type = DiffViewHunk::Added;
                if (op.rightLine >= 0 && op.rightLine < (int)m_diffState.rightLines.size())
                    current.rightText += m_diffState.rightLines[op.rightLine] + "\n";
            }
            if (current.leftCount > 0 && current.rightCount > 0)
                current.type = DiffViewHunk::Modified;
        }
    }
    if (inHunk) m_diffState.hunks.push_back(current);
    LOG_INFO("[DiffView] Computed " + std::to_string(m_diffState.hunks.size()) + " hunks");
}

// ── Navigation ─────────────────────────────────────────────────────────────
void Win32IDE::diffNavigateHunk(int direction) {
    if (m_diffState.hunks.empty()) return;
    m_diffState.currentHunk += direction;
    if (m_diffState.currentHunk < 0) m_diffState.currentHunk = 0;
    if (m_diffState.currentHunk >= (int)m_diffState.hunks.size())
        m_diffState.currentHunk = (int)m_diffState.hunks.size() - 1;

    auto& hunk = m_diffState.hunks[m_diffState.currentHunk];
    m_diffState.scrollPos = (std::max)(0, hunk.leftStart - 3);

    if (m_hwndDiffLeft) InvalidateRect(m_hwndDiffLeft, NULL, TRUE);
    if (m_hwndDiffRight) InvalidateRect(m_hwndDiffRight, NULL, TRUE);
}

void Win32IDE::toggleDiffInlineMode() {
    m_diffState.inlineMode = !m_diffState.inlineMode;
    if (m_hwndDiffPanel) InvalidateRect(m_hwndDiffPanel, NULL, TRUE);
}

// ── Render ─────────────────────────────────────────────────────────────────
void Win32IDE::renderDiffPanel(HWND hwnd, HDC hdc, bool isRight) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    // Background
    HBRUSH bgBrush = CreateSolidBrush(DIFF_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HFONT oldFont = (HFONT)SelectObject(hdc, m_diffState.hFont);
    SetBkMode(hdc, TRANSPARENT);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineH = tm.tmHeight + 2;
    int gutterW = dpiScale(50);
    int visibleLines = h / lineH;

    auto& lines = isRight ? m_diffState.rightLines : m_diffState.leftLines;
    int startLine = m_diffState.scrollPos;
    int endLine = (std::min)(startLine + visibleLines, (int)lines.size());

    // Title bar
    RECT titleRect = {0, 0, w, lineH + 4};
    HBRUSH titleBrush = CreateSolidBrush(DIFF_TOOLBAR_BG);
    FillRect(hdc, &titleRect, titleBrush);
    DeleteObject(titleBrush);
    SetTextColor(hdc, DIFF_TEXT_COLOR);
    std::string title = isRight ? m_diffState.rightTitle : m_diffState.leftTitle;
    TextOutA(hdc, gutterW + 4, 2, title.c_str(), (int)title.size());

    // Draw lines
    for (int i = startLine; i < endLine; i++) {
        int y = (i - startLine + 1) * lineH + 4;
        RECT lineRect = {0, y, w, y + lineH};

        // Check if this line is in a diff hunk
        COLORREF lineBg = DIFF_BG;
        COLORREF gutterColor = DIFF_LINE_NUM_COLOR;
        for (auto& hunk : m_diffState.hunks) {
            int hStart = isRight ? hunk.rightStart : hunk.leftStart;
            int hCount = isRight ? hunk.rightCount : hunk.leftCount;
            if (i >= hStart && i < hStart + hCount) {
                if (hunk.type == DiffViewHunk::Added && isRight) {
                    lineBg = DIFF_ADDED_BG;
                    gutterColor = DIFF_ADDED_GUTTER;
                } else if (hunk.type == DiffViewHunk::Removed && !isRight) {
                    lineBg = DIFF_REMOVED_BG;
                    gutterColor = DIFF_REMOVED_GUTTER;
                } else if (hunk.type == DiffViewHunk::Modified) {
                    lineBg = isRight ? DIFF_ADDED_BG : DIFF_REMOVED_BG;
                    gutterColor = isRight ? DIFF_ADDED_GUTTER : DIFF_REMOVED_GUTTER;
                }
                break;
            }
        }

        // Highlight current hunk
        if (m_diffState.currentHunk >= 0 && m_diffState.currentHunk < (int)m_diffState.hunks.size()) {
            auto& ch = m_diffState.hunks[m_diffState.currentHunk];
            int hs = isRight ? ch.rightStart : ch.leftStart;
            int hc = isRight ? ch.rightCount : ch.leftCount;
            if (i >= hs && i < hs + hc) {
                lineBg = DIFF_HUNK_HIGHLIGHT;
            }
        }

        HBRUSH lbr = CreateSolidBrush(lineBg);
        FillRect(hdc, &lineRect, lbr);
        DeleteObject(lbr);

        // Gutter line number
        char lineNum[16];
        snprintf(lineNum, sizeof(lineNum), "%4d", i + 1);
        SetTextColor(hdc, gutterColor);
        TextOutA(hdc, 4, y, lineNum, (int)strlen(lineNum));

        // Gutter separator
        HPEN sepPen = CreatePen(PS_SOLID, 1, DIFF_SEPARATOR);
        HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
        MoveToEx(hdc, gutterW, y, NULL);
        LineTo(hdc, gutterW, y + lineH);
        SelectObject(hdc, oldPen);
        DeleteObject(sepPen);

        // Line text
        if (i < (int)lines.size()) {
            SetTextColor(hdc, DIFF_TEXT_COLOR);
            TextOutA(hdc, gutterW + 8, y, lines[i].c_str(), (int)lines[i].size());
        }
    }

    SelectObject(hdc, oldFont);
}

// ── Git diff for current file ──────────────────────────────────────────────
void Win32IDE::openGitDiffForCurrentFile() {
    if (m_currentFile.empty()) {
        appendToOutput("No file open for diff", "Output", OutputSeverity::Warning);
        return;
    }

    // Read current file content
    std::ifstream currentStream(m_currentFile);
    std::string currentContent((std::istreambuf_iterator<char>(currentStream)),
                                std::istreambuf_iterator<char>());

    // Get git HEAD version via `git show HEAD:<file>`
    std::string gitCmd = "git show HEAD:\"" + m_currentFile + "\" 2>nul";
    FILE* pipe = _popen(gitCmd.c_str(), "r");
    std::string headContent;
    if (pipe) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            headContent += buffer;
        }
        _pclose(pipe);
    }

    if (headContent.empty()) {
        appendToOutput("No git history for file or not in a git repo", "Output", OutputSeverity::Warning);
        return;
    }

    openDiffView(headContent, currentContent, "HEAD: " + m_currentFile, "Working: " + m_currentFile);
}

// ── Window Proc ────────────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::DiffPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(DIFF_TOOLBAR_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        if (!ide) break;
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        int id = (int)dis->CtlID;
        if (id == IDC_DIFF_LEFT || id == IDC_DIFF_RIGHT) {
            ide->renderDiffPanel(dis->hwndItem, dis->hDC, id == IDC_DIFF_RIGHT);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        if (!ide) break;
        int id = LOWORD(wParam);
        if (id == IDC_DIFF_CLOSE_BTN)       ide->closeDiffView();
        else if (id == IDC_DIFF_PREV_BTN)   ide->diffNavigateHunk(-1);
        else if (id == IDC_DIFF_NEXT_BTN)   ide->diffNavigateHunk(1);
        else if (id == IDC_DIFF_INLINE_BTN) ide->toggleDiffInlineMode();
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!ide) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        ide->m_diffState.scrollPos -= (delta / 120) * 3;
        if (ide->m_diffState.scrollPos < 0) ide->m_diffState.scrollPos = 0;
        if (ide->m_hwndDiffLeft) InvalidateRect(ide->m_hwndDiffLeft, NULL, TRUE);
        if (ide->m_hwndDiffRight) InvalidateRect(ide->m_hwndDiffRight, NULL, TRUE);
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
