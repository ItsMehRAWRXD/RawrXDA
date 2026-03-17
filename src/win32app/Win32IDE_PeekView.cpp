// =============================================================================
// Win32IDE Peek Definition / References Overlay — VS Code Parity
// =============================================================================
// Implements the inline peek view widget that VS Code shows on Alt+F12/Shift+F12
// A bordered mini-editor embedded within the main editor at cursor position,
// showing definition(s) or reference(s) with file tabs and navigation.
// =============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <filesystem>

// ─── Peek View Constants ─────────────────────────────────────────────────────
static constexpr int PEEK_HEIGHT = 260;
static constexpr int PEEK_HEADER_HEIGHT = 28;
static constexpr int PEEK_TAB_HEIGHT = 24;
static constexpr int PEEK_BORDER = 2;
static constexpr int PEEK_MAX_CONTEXT_LINES = 15;
static constexpr UINT WM_PEEK_CLOSE = WM_APP + 450;
static constexpr UINT WM_PEEK_NAVIGATE = WM_APP + 451;

// ─── Colors (VS Code Peek View dark theme) ───────────────────────────────────
static constexpr COLORREF PEEK_BG          = RGB(30, 30, 30);
static constexpr COLORREF PEEK_HEADER_BG   = RGB(55, 55, 55);
static constexpr COLORREF PEEK_BORDER_CLR  = RGB(0, 122, 204);  // VS Code blue
static constexpr COLORREF PEEK_TAB_ACTIVE  = RGB(30, 30, 30);
static constexpr COLORREF PEEK_TAB_INACTIVE = RGB(45, 45, 45);
static constexpr COLORREF PEEK_TEXT_CLR    = RGB(212, 212, 212);
static constexpr COLORREF PEEK_PATH_CLR   = RGB(156, 156, 156);
static constexpr COLORREF PEEK_LINE_CLR   = RGB(86, 156, 214);  // blue for line numbers
static constexpr COLORREF PEEK_HIGHLIGHT  = RGB(255, 210, 0);   // yellow match highlight
static constexpr COLORREF PEEK_GUTTER_BG  = RGB(37, 37, 38);

// ─── Peek Location: use Win32IDE::PeekLocation from header ──────────────────
using PeekLocation = Win32IDE::PeekLocation;

// ─── Peek State (stored per-IDE) ─────────────────────────────────────────────
struct PeekViewState {
    Win32IDE* owner = nullptr;
    HWND hwndOverlay = nullptr;
    HWND hwndContent = nullptr;      // RichEdit for content display
    HWND hwndFileList = nullptr;     // Listbox for file tabs
    HWND hwndCloseBtn = nullptr;
    std::vector<PeekLocation> locations;
    int activeIndex = 0;
    int anchorLine = 0;     // editor line where peek is anchored
    bool isVisible = false;
    std::string symbolName;
    HFONT hFont = nullptr;
    HFONT hFontBold = nullptr;
};

static PeekViewState g_peekState;

// ─── Forward declarations ────────────────────────────────────────────────────
static LRESULT CALLBACK PeekOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void renderPeekContent(const PeekLocation& loc);
static std::vector<std::string> readFileLines(const std::string& path, int centerLine, int contextRadius);

// =============================================================================
// Public API: showPeekDefinition / showPeekReferences
// =============================================================================

// Helper: get word under the cursor in the editor
static std::string peekGetWordUnderCursor(HWND hwndEditor) {
    if (!hwndEditor) return "";
    CHARRANGE cr;
    SendMessage(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    int pos = cr.cpMax;
    int lineIdx = (int)SendMessage(hwndEditor, EM_LINEFROMCHAR, pos, 0);
    int lineStart = (int)SendMessage(hwndEditor, EM_LINEINDEX, lineIdx, 0);
    char buf[4096] = {0};
    *(WORD*)buf = sizeof(buf) - 2;
    int lineLen = (int)SendMessageA(hwndEditor, EM_GETLINE, lineIdx, (LPARAM)buf);
    buf[lineLen] = '\0';
    int col = pos - lineStart;
    int ws = col;
    while (ws > 0 && (std::isalnum(buf[ws-1]) || buf[ws-1] == '_')) ws--;
    int we = col;
    while (we < lineLen && (std::isalnum(buf[we]) || buf[we] == '_')) we++;
    if (ws >= we) return "";
    return std::string(buf + ws, we - ws);
}

void Win32IDE::showPeekDefinition()
{
    if (!m_hwndEditor) return;

    // Get current word under cursor
    std::string symbol = peekGetWordUnderCursor(m_hwndEditor);
    if (symbol.empty()) {
        appendToOutput("Peek: No symbol under cursor\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Gather definition locations (from symbol indexer, LSP, or local scan)
    std::vector<PeekLocation> locations;

    // Strategy 1: Use LSP textDocument/definition if available
    if (m_lspInitialized) {
        // LSP definitions handled via lspGotoDefinition inside Win32IDE_LSPClient.cpp
        // For peek, we use the local scanner which is faster and doesn't block
    }

    // Strategy 2: Fallback to local symbol scan
    if (locations.empty()) {
        locations = scanForDefinitions(symbol);
    }

    if (locations.empty()) {
        appendToOutput("Peek: No definition found for '" + symbol + "'\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Load context lines for each location
    for (auto& loc : locations) {
        loc.contextLines = readFileLines(loc.filePath, loc.line, PEEK_MAX_CONTEXT_LINES / 2);
        loc.contextStartLine = std::max(1, loc.line - PEEK_MAX_CONTEXT_LINES / 2);
    }

    showPeekOverlay(symbol, locations, true);
}

void Win32IDE::showPeekReferences()
{
    if (!m_hwndEditor) return;

    std::string symbol = peekGetWordUnderCursor(m_hwndEditor);
    if (symbol.empty()) return;

    std::vector<PeekLocation> locations;

    // LSP references: defer to local scanner for non-blocking peek

    // Fallback: grep-style workspace scan
    if (locations.empty()) {
        locations = scanForReferences(symbol);
    }

    if (locations.empty()) {
        appendToOutput("Peek: No references found for '" + symbol + "'\n", "Output", OutputSeverity::Warning);
        return;
    }

    for (auto& loc : locations) {
        loc.contextLines = readFileLines(loc.filePath, loc.line, PEEK_MAX_CONTEXT_LINES / 2);
        loc.contextStartLine = std::max(1, loc.line - PEEK_MAX_CONTEXT_LINES / 2);
    }

    showPeekOverlay(symbol, locations, false);
}

// =============================================================================
// showPeekOverlay — Creates the inline peek widget
// =============================================================================

void Win32IDE::showPeekOverlay(const std::string& symbol,
                                const std::vector<PeekLocation>& locations,
                                bool isDefinition)
{
    // Close any existing peek
    closePeekView();

    if (locations.empty()) return;

    g_peekState.symbolName = symbol;
    g_peekState.locations.assign(locations.begin(), locations.end());
    g_peekState.activeIndex = 0;
    g_peekState.owner = this;

    // Determine where to anchor: get cursor position in editor client coords
    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    POINTL pt;
    SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, cr.cpMin);

    // Get line number for anchor
    int lineIndex = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMin, 0);
    g_peekState.anchorLine = lineIndex;

    // Calculate peek overlay position: below the current line
    RECT editorRect;
    GetClientRect(m_hwndEditor, &editorRect);

    // Get line height
    HDC hdc = GetDC(m_hwndEditor);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    ReleaseDC(m_hwndEditor, hdc);

    int peekY = pt.y + lineHeight + 4;
    int peekW = editorRect.right - editorRect.left;
    int peekH = PEEK_HEIGHT;

    // Don't overflow editor bottom
    if (peekY + peekH > editorRect.bottom) {
        peekY = std::max(0, (int)(pt.y - peekH - 4));
    }

    // Create fonts
    if (!g_peekState.hFont) {
        g_peekState.hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    }
    if (!g_peekState.hFontBold) {
        g_peekState.hFontBold = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    }

    // Register peek overlay window class
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = PeekOverlayProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "RawrXD_PeekOverlay";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(PEEK_BG);
        RegisterClassA(&wc);
        classRegistered = true;
    }

    // Create overlay as child of editor
    g_peekState.hwndOverlay = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "RawrXD_PeekOverlay", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, peekY, peekW, peekH,
        m_hwndEditor, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!g_peekState.hwndOverlay) return;

    // ─── File list (left side, like VS Code peek sidebar) ────────────────
    int fileListWidth = 220;
    g_peekState.hwndFileList = CreateWindowExA(
        0, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, PEEK_HEADER_HEIGHT, fileListWidth, peekH - PEEK_HEADER_HEIGHT,
        g_peekState.hwndOverlay, (HMENU)101, GetModuleHandle(nullptr), nullptr);
    SendMessage(g_peekState.hwndFileList, WM_SETFONT, (WPARAM)g_peekState.hFontBold, TRUE);

    // Populate file list
    for (size_t i = 0; i < locations.size(); ++i) {
        std::string label = std::filesystem::path(locations[i].filePath).filename().string()
                          + ":" + std::to_string(locations[i].line);
        SendMessageA(g_peekState.hwndFileList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    }
    SendMessage(g_peekState.hwndFileList, LB_SETCURSEL, 0, 0);

    // ─── Content area (right side, RichEdit) ─────────────────────────────
    g_peekState.hwndContent = CreateWindowExA(
        0, RICHEDIT_CLASSA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        fileListWidth + PEEK_BORDER, PEEK_HEADER_HEIGHT,
        peekW - fileListWidth - PEEK_BORDER * 2, peekH - PEEK_HEADER_HEIGHT,
        g_peekState.hwndOverlay, (HMENU)102, GetModuleHandle(nullptr), nullptr);
    SendMessage(g_peekState.hwndContent, WM_SETFONT, (WPARAM)g_peekState.hFont, TRUE);

    // Set dark theme on rich edit
    SendMessage(g_peekState.hwndContent, EM_SETBKGNDCOLOR, 0, PEEK_BG);
    CHARFORMAT2A cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = PEEK_TEXT_CLR;
    cf.yHeight = 180; // 9pt
    strcpy_s(cf.szFaceName, "Consolas");
    SendMessageA(g_peekState.hwndContent, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // ─── Close button ────────────────────────────────────────────────────
    g_peekState.hwndCloseBtn = CreateWindowExA(
        0, "BUTTON", "x",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        peekW - 28, 2, 24, 22,
        g_peekState.hwndOverlay, (HMENU)103, GetModuleHandle(nullptr), nullptr);

    // Render the first location
    renderPeekContent(locations[0]);

    g_peekState.isVisible = true;
    SetFocus(g_peekState.hwndContent);

    appendToOutput("[Peek] Opened for '" + symbol + "' with " + std::to_string(locations.size()) + " locations\n", "Output", OutputSeverity::Info);
}

// =============================================================================
// closePeekView
// =============================================================================

void Win32IDE::closePeekView()
{
    if (g_peekState.hwndOverlay) {
        DestroyWindow(g_peekState.hwndOverlay);
        g_peekState.hwndOverlay = nullptr;
        g_peekState.hwndContent = nullptr;
        g_peekState.hwndFileList = nullptr;
        g_peekState.hwndCloseBtn = nullptr;
    }
    g_peekState.isVisible = false;
    g_peekState.owner = nullptr;
    g_peekState.locations.clear();

    if (m_hwndEditor) SetFocus(m_hwndEditor);
}

// =============================================================================
// navigatePeekLocation — Jump to a location and optionally close peek
// =============================================================================

void Win32IDE::navigatePeekLocation(int index)
{
    bool closeAfter = true;
    if (index < 0 || index >= (int)g_peekState.locations.size()) return;

    auto& loc = g_peekState.locations[index];

    // Open file if different from current
    if (loc.filePath != m_currentFile) {
        openFile(loc.filePath);
    }

    // Go to line
    gotoLine(loc.line);

    if (closeAfter) closePeekView();
}

// =============================================================================
// Peek Overlay Window Proc
// =============================================================================

static LRESULT CALLBACK PeekOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Draw header bar
        RECT headerRect = { 0, 0, rc.right, PEEK_HEADER_HEIGHT };
        HBRUSH hdrBrush = CreateSolidBrush(PEEK_HEADER_BG);
        FillRect(hdc, &headerRect, hdrBrush);
        DeleteObject(hdrBrush);

        // Draw header text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, PEEK_TEXT_CLR);
        HFONT hOld = (HFONT)SelectObject(hdc, g_peekState.hFontBold);

        std::string headerText;
        if (!g_peekState.locations.empty()) {
            auto& loc = g_peekState.locations[g_peekState.activeIndex];
            std::string fname = std::filesystem::path(loc.filePath).filename().string();
            headerText = g_peekState.symbolName + " — " + fname + ":" + std::to_string(loc.line);
        } else {
            headerText = g_peekState.symbolName;
        }

        RECT textRect = { 8, 4, rc.right - 32, PEEK_HEADER_HEIGHT };
        DrawTextA(hdc, headerText.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, hOld);

        // Draw blue border (VS Code peek view style)
        HPEN pen = CreatePen(PS_SOLID, PEEK_BORDER, PEEK_BORDER_CLR);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);

        // Draw top blue accent line
        HBRUSH accentBrush = CreateSolidBrush(PEEK_BORDER_CLR);
        RECT accentRect = { 0, 0, rc.right, 3 };
        FillRect(hdc, &accentRect, accentBrush);
        DeleteObject(accentBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id == 103) {
            // Close button
            auto* ide = g_peekState.owner;
            if (ide) ide->closePeekView();
            return 0;
        }

        if (id == 101 && notify == LBN_SELCHANGE) {
            // File list selection changed
            int sel = (int)SendMessage(g_peekState.hwndFileList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_peekState.locations.size()) {
                g_peekState.activeIndex = sel;
                renderPeekContent(g_peekState.locations[sel]);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        if (id == 101 && notify == LBN_DBLCLK) {
            // Double-click in file list → navigate to location
            int sel = (int)SendMessage(g_peekState.hwndFileList, LB_GETCURSEL, 0, 0);
            auto* ide = g_peekState.owner;
            if (ide) ide->navigatePeekLocation(sel);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            auto* ide = g_peekState.owner;
            if (ide) ide->closePeekView();
            return 0;
        }
        if (wParam == VK_RETURN) {
            auto* ide = g_peekState.owner;
            if (ide) ide->navigatePeekLocation(g_peekState.activeIndex);
            return 0;
        }
        if (wParam == VK_F2 || (wParam == VK_DOWN && GetKeyState(VK_MENU) < 0)) {
            // Next location
            if (g_peekState.activeIndex + 1 < (int)g_peekState.locations.size()) {
                g_peekState.activeIndex++;
                SendMessage(g_peekState.hwndFileList, LB_SETCURSEL, g_peekState.activeIndex, 0);
                renderPeekContent(g_peekState.locations[g_peekState.activeIndex]);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        if (wParam == VK_F2 && GetKeyState(VK_SHIFT) < 0) {
            // Previous location
            if (g_peekState.activeIndex > 0) {
                g_peekState.activeIndex--;
                SendMessage(g_peekState.hwndFileList, LB_SETCURSEL, g_peekState.activeIndex, 0);
                renderPeekContent(g_peekState.locations[g_peekState.activeIndex]);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        break;

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, PEEK_TAB_INACTIVE);
        SetTextColor(hdc, PEEK_TEXT_CLR);
        static HBRUSH lbBrush = CreateSolidBrush(PEEK_TAB_INACTIVE);
        return (LRESULT)lbBrush;
    }

    case WM_DESTROY:
        g_peekState.isVisible = false;
        break;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// =============================================================================
// renderPeekContent — Renders source context into the peek RichEdit
// =============================================================================

static void renderPeekContent(const PeekLocation& loc)
{
    if (!g_peekState.hwndContent) return;

    // Clear existing content
    SendMessageA(g_peekState.hwndContent, WM_SETTEXT, 0, (LPARAM)"");

    // Build display text with line numbers
    std::stringstream ss;
    int startLine = loc.contextStartLine;

    for (size_t i = 0; i < loc.contextLines.size(); ++i) {
        int lineNum = startLine + (int)i;
        char lineNumStr[16];
        snprintf(lineNumStr, sizeof(lineNumStr), "%4d  ", lineNum);
        ss << lineNumStr << loc.contextLines[i] << "\r\n";
    }

    std::string text = ss.str();
    SendMessageA(g_peekState.hwndContent, EM_SETSEL, 0, -1);
    SendMessageA(g_peekState.hwndContent, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

    // Highlight the target line
    if (loc.line >= startLine) {
        int targetIdx = loc.line - startLine;
        // Find line position in the RichEdit
        int lineStart = (int)SendMessage(g_peekState.hwndContent, EM_LINEINDEX, targetIdx, 0);
        int lineLen = (int)SendMessage(g_peekState.hwndContent, EM_LINELENGTH, lineStart, 0);

        if (lineStart >= 0) {
            // Set entire target line background to highlight
            CHARRANGE cr = { lineStart, lineStart + lineLen };
            SendMessage(g_peekState.hwndContent, EM_EXSETSEL, 0, (LPARAM)&cr);

            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_BACKCOLOR;
            cf.crBackColor = RGB(50, 50, 20); // subtle yellow highlight
            SendMessageA(g_peekState.hwndContent, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

            // Reset selection
            CHARRANGE crEnd = { 0, 0 };
            SendMessage(g_peekState.hwndContent, EM_EXSETSEL, 0, (LPARAM)&crEnd);
        }
    }

    // Scroll to make target line visible (center it)
    int targetScrollLine = std::max(0, loc.line - startLine - 3);
    SendMessage(g_peekState.hwndContent, EM_LINESCROLL, 0, targetScrollLine);
}

// =============================================================================
// readFileLines — Reads lines around a center point from a file
// =============================================================================

static std::vector<std::string> readFileLines(const std::string& path, int centerLine, int contextRadius)
{
    std::vector<std::string> result;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return result;

    std::string line;
    int lineNum = 0;
    int startLine = std::max(1, centerLine - contextRadius);
    int endLine = centerLine + contextRadius;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (lineNum >= startLine && lineNum <= endLine) {
            result.push_back(line);
        }
        if (lineNum > endLine) break;
    }

    return result;
}

// =============================================================================
// scanForDefinitions — Local fallback: regex scan for symbol definitions
// =============================================================================

std::vector<PeekLocation> Win32IDE::scanForDefinitions(const std::string& symbol)
{
    std::vector<PeekLocation> results;
    if (symbol.empty()) return results;

    // Scan the current workspace directory
    std::string wsRoot = m_currentDirectory.empty() ? "." : m_currentDirectory;

    // Pattern: look for function/class/struct/variable definitions
    // Match: "type symbol(" or "class symbol" or "struct symbol" or "#define symbol"
    auto matchDefinition = [&](const std::string& line) -> bool {
        // Function definition: return_type symbol(
        size_t pos = line.find(symbol);
        if (pos == std::string::npos) return false;
        // Check it's a word boundary
        if (pos > 0 && (std::isalnum(line[pos - 1]) || line[pos - 1] == '_')) return false;
        size_t endPos = pos + symbol.size();
        if (endPos < line.size() && (std::isalnum(line[endPos]) || line[endPos] == '_')) return false;
        // Check for definition patterns
        if (endPos < line.size() && line[endPos] == '(') return true;   // function
        if (pos > 0) {
            // class/struct/enum before it
            std::string before = line.substr(0, pos);
            if (before.find("class ") != std::string::npos) return true;
            if (before.find("struct ") != std::string::npos) return true;
            if (before.find("enum ") != std::string::npos) return true;
            if (before.find("#define ") != std::string::npos) return true;
        }
        return false;
    };

    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 wsRoot, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".c" && ext != ".cxx") continue;
            auto pathStr = entry.path().string();
            if (pathStr.find("\\.git\\") != std::string::npos || pathStr.find("\\build\\") != std::string::npos) continue;

            std::ifstream ifs(pathStr);
            if (!ifs.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(ifs, line)) {
                ++lineNum;
                if (matchDefinition(line)) {
                    PeekLocation loc;
                    loc.filePath = pathStr;
                    loc.line = lineNum;
                    loc.col = (int)line.find(symbol);
                    loc.endCol = loc.col + (int)symbol.size();
                    loc.preview = line;
                    results.push_back(std::move(loc));

                    if (results.size() >= 20) return results; // cap results
                }
            }
        }
    } catch (...) {}

    return results;
}

// =============================================================================
// scanForReferences — Local fallback: word-boundary grep for symbol
// =============================================================================

std::vector<PeekLocation> Win32IDE::scanForReferences(const std::string& symbol)
{
    std::vector<PeekLocation> results;
    if (symbol.empty()) return results;

    std::string wsRoot = m_currentDirectory.empty() ? "." : m_currentDirectory;

    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 wsRoot, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".c" &&
                ext != ".cxx" && ext != ".py" && ext != ".js" && ext != ".ts") continue;
            auto pathStr = entry.path().string();
            if (pathStr.find("\\.git\\") != std::string::npos || pathStr.find("\\build\\") != std::string::npos) continue;

            std::ifstream ifs(pathStr);
            if (!ifs.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(ifs, line)) {
                ++lineNum;
                size_t pos = 0;
                while ((pos = line.find(symbol, pos)) != std::string::npos) {
                    // Word boundary check
                    bool leftOk = (pos == 0 || !std::isalnum(line[pos - 1]) && line[pos - 1] != '_');
                    size_t ep = pos + symbol.size();
                    bool rightOk = (ep >= line.size() || !std::isalnum(line[ep]) && line[ep] != '_');

                    if (leftOk && rightOk) {
                        PeekLocation loc;
                        loc.filePath = pathStr;
                        loc.line = lineNum;
                        loc.col = (int)pos;
                        loc.endCol = (int)(pos + symbol.size());
                        loc.preview = line;
                        results.push_back(std::move(loc));
                        break; // one per line
                    }
                    pos = ep;
                }

                if (results.size() >= 50) return results;
            }
        }
    } catch (...) {}

    return results;
}

// =============================================================================
// LSP Definition/Reference helpers — use built-in LSP via sendLSPRequest
// =============================================================================
// Note: The main peek flow already uses scanForDefinitions/scanForReferences
// as the primary strategy. These LSP helpers are available for future enhancement
// when LSP servers provide faster/more accurate results.
