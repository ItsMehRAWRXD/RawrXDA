// ============================================================================
// Win32IDE_RenamePreview.cpp — Feature 17: Rename Refactoring Preview
// Diff preview before applying rename, with per-change checkboxes
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <fstream>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF RENAME_BG            = RGB(37, 37, 38);
static const COLORREF RENAME_HEADER_BG     = RGB(30, 30, 30);
static const COLORREF RENAME_TEXT          = RGB(204, 204, 204);
static const COLORREF RENAME_OLD_BG        = RGB(72, 30, 30);
static const COLORREF RENAME_NEW_BG        = RGB(35, 61, 37);
static const COLORREF RENAME_OLD_TEXT      = RGB(255, 100, 100);
static const COLORREF RENAME_NEW_TEXT      = RGB(100, 255, 100);
static const COLORREF RENAME_FILE_TEXT     = RGB(229, 192, 123);
static const COLORREF RENAME_LINE_NUM      = RGB(110, 110, 110);
static const COLORREF RENAME_CONTEXT       = RGB(150, 150, 150);
static const COLORREF RENAME_BORDER        = RGB(60, 60, 60);
static const COLORREF RENAME_APPLY_BG      = RGB(14, 99, 156);
static const COLORREF RENAME_CANCEL_BG     = RGB(90, 30, 30);
static const COLORREF RENAME_CHECKBOX      = RGB(0, 122, 204);

// Registered class
static bool s_renameClassRegistered = false;
static const char* RENAME_CLASS_NAME = "RawrXD_RenamePreview";

static void ensureRenameClassRegistered(HINSTANCE hInst) {
    if (s_renameClassRegistered) return;
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RENAME_BG);
    wc.lpszClassName = RENAME_CLASS_NAME;
    RegisterClassExA(&wc);
    s_renameClassRegistered = true;
}

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initRenamePreview() {
    m_renamePreview = RenamePreviewState{};
    m_renamePreview.hFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    ensureRenameClassRegistered(GetModuleHandle(NULL));
    LOG_INFO("[RenamePreview] Initialized");
}

void Win32IDE::shutdownRenamePreview() {
    closeRenamePreview();
    if (m_renamePreview.hFont) { DeleteObject(m_renamePreview.hFont); m_renamePreview.hFont = nullptr; }
    LOG_INFO("[RenamePreview] Shutdown");
}

// ── Show Preview ───────────────────────────────────────────────────────────
void Win32IDE::showRenamePreview(const std::string& oldName, const std::string& newName,
                                 const std::vector<RenameChange>& changes) {
    closeRenamePreview();

    if (changes.empty()) {
        appendToOutput("No occurrences to rename", "Output", OutputSeverity::Warning);
        return;
    }

    m_renamePreview.oldName = oldName;
    m_renamePreview.newName = newName;
    m_renamePreview.changes = changes;
    m_renamePreview.visible = true;

    // Create preview panel (center overlay)
    RECT rcClient;
    GetClientRect(m_hwndMain, &rcClient);
    int panelW = dpiScale(600);
    int panelH = dpiScale(400);
    int panelX = (rcClient.right - panelW) / 2;
    int panelY = (rcClient.bottom - panelH) / 2;

    m_renamePreview.hwndPanel = CreateWindowExA(WS_EX_CLIENTEDGE,
        RENAME_CLASS_NAME, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        panelX, panelY, panelW, panelH, m_hwndMain,
        (HMENU)(UINT_PTR)IDC_RENAME_PREVIEW, GetModuleHandle(NULL), NULL);
    SetPropA(m_renamePreview.hwndPanel, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_renamePreview.hwndPanel, GWLP_WNDPROC, (LONG_PTR)RenamePreviewProc);

    int headerH = dpiScale(40);
    int buttonH = dpiScale(36);
    int listH = panelH - headerH - buttonH;

    // Header: "Rename 'oldName' → 'newName'"
    // (drawn in WM_PAINT)

    // Rename input field
    HWND hInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", newName.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        dpiScale(180), dpiScale(8), dpiScale(200), dpiScale(22),
        m_renamePreview.hwndPanel,
        (HMENU)(UINT_PTR)IDC_RENAME_INPUT, GetModuleHandle(NULL), NULL);
    SendMessageA(hInput, WM_SETFONT, (WPARAM)m_renamePreview.hFont, TRUE);

    // Change list with checkboxes (ListView)
    m_renamePreview.hwndList = CreateWindowExA(0, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, headerH, panelW, listH, m_renamePreview.hwndPanel,
        (HMENU)(UINT_PTR)IDC_RENAME_CHECKLIST, GetModuleHandle(NULL), NULL);

    // Extended styles with checkboxes
    ListView_SetExtendedListViewStyle(m_renamePreview.hwndList,
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Dark colors
    ListView_SetBkColor(m_renamePreview.hwndList, RENAME_BG);
    ListView_SetTextBkColor(m_renamePreview.hwndList, RENAME_BG);
    ListView_SetTextColor(m_renamePreview.hwndList, RENAME_TEXT);
    SendMessageA(m_renamePreview.hwndList, WM_SETFONT, (WPARAM)m_renamePreview.hFont, TRUE);

    // Columns
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;

    col.pszText = (char*)"File";
    col.cx = dpiScale(180);
    col.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_renamePreview.hwndList, 0, &col);

    col.pszText = (char*)"Line";
    col.cx = dpiScale(50);
    ListView_InsertColumn(m_renamePreview.hwndList, 1, &col);

    col.pszText = (char*)"Context";
    col.cx = dpiScale(340);
    ListView_InsertColumn(m_renamePreview.hwndList, 2, &col);

    // Populate changes
    for (int i = 0; i < (int)m_renamePreview.changes.size(); i++) {
        auto& change = m_renamePreview.changes[i];

        // Extract filename
        std::string fileName = change.filePath;
        size_t lastSlash = fileName.find_last_of("\\/");
        if (lastSlash != std::string::npos) fileName = fileName.substr(lastSlash + 1);

        LVITEMA lvi = {};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (char*)fileName.c_str();
        lvi.lParam = (LPARAM)i;
        ListView_InsertItem(m_renamePreview.hwndList, &lvi);

        // Line number
        char lineStr[16];
        snprintf(lineStr, sizeof(lineStr), "%d", change.line + 1);
        ListView_SetItemText(m_renamePreview.hwndList, i, 1, lineStr);

        // Context with highlighted old → new
        std::string context = change.contextLine;
        size_t pos = context.find(change.oldText);
        if (pos != std::string::npos) {
            context = context.substr(0, pos) + "[" + change.oldText + " → " +
                      change.newText + "]" + context.substr(pos + change.oldText.size());
        }
        ListView_SetItemText(m_renamePreview.hwndList, i, 2, (char*)context.c_str());

        // Check by default
        if (change.selected)
            ListView_SetCheckState(m_renamePreview.hwndList, i, TRUE);
    }

    // Buttons at bottom
    int btnW = dpiScale(100);
    int btnGap = dpiScale(12);
    int btnY = panelH - buttonH + dpiScale(6);

    CreateWindowExA(0, "BUTTON", "Apply Rename",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - 2 * btnW - btnGap - dpiScale(12), btnY, btnW, dpiScale(26),
        m_renamePreview.hwndPanel,
        (HMENU)(UINT_PTR)IDC_RENAME_APPLY_BTN, GetModuleHandle(NULL), NULL);

    CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - btnW - dpiScale(12), btnY, btnW, dpiScale(26),
        m_renamePreview.hwndPanel,
        (HMENU)(UINT_PTR)IDC_RENAME_CANCEL_BTN, GetModuleHandle(NULL), NULL);

    SetFocus(m_renamePreview.hwndPanel);
    LOG_INFO("[RenamePreview] Showing " + std::to_string(changes.size()) +
             " changes: " + oldName + " → " + newName);
}

void Win32IDE::closeRenamePreview() {
    if (m_renamePreview.hwndPanel) {
        DestroyWindow(m_renamePreview.hwndPanel);
        m_renamePreview.hwndPanel = nullptr;
        m_renamePreview.hwndList = nullptr;
    }
    m_renamePreview.visible = false;
    m_renamePreview.changes.clear();
}

// ── Apply Selected Renames ─────────────────────────────────────────────────
void Win32IDE::applySelectedRenames() {
    if (!m_renamePreview.hwndList) return;

    // Update selections from checkboxes
    for (int i = 0; i < (int)m_renamePreview.changes.size(); i++) {
        m_renamePreview.changes[i].selected =
            ListView_GetCheckState(m_renamePreview.hwndList, i) != 0;
    }

    // Get potentially updated new name from input field
    HWND hInput = GetDlgItem(m_renamePreview.hwndPanel, IDC_RENAME_INPUT);
    if (hInput) {
        char buf[256] = {};
        GetWindowTextA(hInput, buf, sizeof(buf));
        if (buf[0]) m_renamePreview.newName = buf;
    }

    // Group changes by file, apply in reverse order (to preserve line numbers)
    std::map<std::string, std::vector<RenameChange*>> byFile;
    for (auto& change : m_renamePreview.changes) {
        if (change.selected) {
            change.newText = m_renamePreview.newName;
            byFile[change.filePath].push_back(&change);
        }
    }

    int applied = 0;
    for (auto& [filePath, changes] : byFile) {
        // Sort by line descending, then column descending
        std::sort(changes.begin(), changes.end(),
            [](const RenameChange* a, const RenameChange* b) {
                if (a->line != b->line) return a->line > b->line;
                return a->column > b->column;
            });

        // Read file
        std::ifstream inFile(filePath);
        if (!inFile.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(inFile)),
                             std::istreambuf_iterator<char>());
        inFile.close();

        // Apply replacements (reverse order preserves positions)
        std::vector<std::string> lines;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);

        for (auto* change : changes) {
            if (change->line >= 0 && change->line < (int)lines.size()) {
                auto& ln = lines[change->line];
                size_t pos = ln.find(change->oldText, change->column);
                if (pos != std::string::npos) {
                    ln.replace(pos, change->oldText.size(), change->newText);
                    applied++;
                }
            }
        }

        // Write back
        std::ofstream outFile(filePath);
        if (outFile.is_open()) {
            for (size_t i = 0; i < lines.size(); i++) {
                outFile << lines[i];
                if (i < lines.size() - 1) outFile << '\n';
            }
            outFile.close();
        }
    }

    appendToOutput("Rename complete: " + std::to_string(applied) + " occurrences of '" +
                   m_renamePreview.oldName + "' → '" + m_renamePreview.newName + "'",
                   "Output", OutputSeverity::Info);

    // Reload current file if affected
    if (byFile.count(m_currentFile)) {
        // Trigger reload
        std::ifstream reloadStream(m_currentFile);
        if (reloadStream.is_open()) {
            std::string newContent((std::istreambuf_iterator<char>(reloadStream)),
                                    std::istreambuf_iterator<char>());
            SetWindowTextA(m_hwndEditor, newContent.c_str());
        }
    }

    closeRenamePreview();
}

// ── Render Item ────────────────────────────────────────────────────────────
void Win32IDE::renderRenamePreviewItem(HDC hdc, RECT itemRect, const RenameChange& change) {
    HBRUSH bg = CreateSolidBrush(RENAME_BG);
    FillRect(hdc, &itemRect, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_renamePreview.hFont);

    int x = itemRect.left + 8;
    int y = itemRect.top + 2;

    // Checkbox indicator
    RECT checkRect = {itemRect.left + 2, y + 2, itemRect.left + 16, y + 16};
    if (change.selected) {
        HBRUSH checkBrush = CreateSolidBrush(RENAME_CHECKBOX);
        FillRect(hdc, &checkRect, checkBrush);
        DeleteObject(checkBrush);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextA(hdc, "v", 1, &checkRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    } else {
        FrameRect(hdc, &checkRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
    }
    x += 20;

    // Context line with highlighted change
    std::string ctx = change.contextLine;
    size_t pos = ctx.find(change.oldText);
    if (pos != std::string::npos) {
        // Before match
        std::string before = ctx.substr(0, pos);
        SetTextColor(hdc, RENAME_CONTEXT);
        TextOutA(hdc, x, y, before.c_str(), (int)before.size());
        SIZE beforeSize;
        GetTextExtentPoint32A(hdc, before.c_str(), (int)before.size(), &beforeSize);
        x += beforeSize.cx;

        // Old text (strikethrough red)
        SetTextColor(hdc, RENAME_OLD_TEXT);
        SetBkColor(hdc, RENAME_OLD_BG);
        SetBkMode(hdc, OPAQUE);
        TextOutA(hdc, x, y, change.oldText.c_str(), (int)change.oldText.size());
        SIZE oldSize;
        GetTextExtentPoint32A(hdc, change.oldText.c_str(), (int)change.oldText.size(), &oldSize);
        x += oldSize.cx;

        // Arrow
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(180, 180, 180));
        TextOutA(hdc, x, y, " -> ", 4);
        SIZE arrowSize;
        GetTextExtentPoint32A(hdc, " -> ", 4, &arrowSize);
        x += arrowSize.cx;

        // New text (green)
        SetTextColor(hdc, RENAME_NEW_TEXT);
        SetBkColor(hdc, RENAME_NEW_BG);
        SetBkMode(hdc, OPAQUE);
        TextOutA(hdc, x, y, change.newText.c_str(), (int)change.newText.size());
        SIZE newSize;
        GetTextExtentPoint32A(hdc, change.newText.c_str(), (int)change.newText.size(), &newSize);
        x += newSize.cx;

        // After match
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RENAME_CONTEXT);
        std::string after = ctx.substr(pos + change.oldText.size());
        TextOutA(hdc, x, y, after.c_str(), (int)after.size());
    } else {
        SetTextColor(hdc, RENAME_TEXT);
        TextOutA(hdc, x, y, ctx.c_str(), (int)ctx.size());
    }

    SelectObject(hdc, oldFont);
}

// ── Window Proc ────────────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::RenamePreviewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Background
        HBRUSH bg = CreateSolidBrush(RENAME_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // Header
        int headerH = ide ? ide->dpiScale(40) : 40;
        RECT headerRect = {0, 0, rc.right, headerH};
        HBRUSH headerBg = CreateSolidBrush(RENAME_HEADER_BG);
        FillRect(hdc, &headerRect, headerBg);
        DeleteObject(headerBg);

        if (ide) {
            SetBkMode(hdc, TRANSPARENT);
            HFONT oldFont = (HFONT)SelectObject(hdc, ide->m_renamePreview.hFont);

            // Title
            SetTextColor(hdc, RENAME_TEXT);
            std::string title = "Rename '" + ide->m_renamePreview.oldName + "'  ->  ";
            TextOutA(hdc, 8, 12, title.c_str(), (int)title.size());

            SelectObject(hdc, oldFont);
        }

        // Bottom border
        HPEN borderPen = CreatePen(PS_SOLID, 1, RENAME_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        MoveToEx(hdc, 0, headerH - 1, NULL);
        LineTo(hdc, rc.right, headerH - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        if (!ide) break;
        int id = LOWORD(wParam);
        if (id == IDC_RENAME_APPLY_BTN) {
            ide->applySelectedRenames();
            return 0;
        }
        if (id == IDC_RENAME_CANCEL_BTN) {
            ide->closeRenamePreview();
            return 0;
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && ide) {
            ide->closeRenamePreview();
            return 0;
        }
        if (wParam == VK_RETURN && ide) {
            ide->applySelectedRenames();
            return 0;
        }
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
