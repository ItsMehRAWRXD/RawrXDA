// ============================================================================
// Win32IDE_ReferencesPanel.cpp — Feature 16: Find All References UI
// Tree-structured panel with file grouping, context lines, and navigation
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF REFS_BG              = RGB(37, 37, 38);
static const COLORREF REFS_HEADER_BG       = RGB(30, 30, 30);
static const COLORREF REFS_TEXT            = RGB(204, 204, 204);
static const COLORREF REFS_FILE_TEXT       = RGB(229, 192, 123);
static const COLORREF REFS_LINE_NUM        = RGB(110, 110, 110);
static const COLORREF REFS_MATCH_BG        = RGB(81, 69, 31);
static const COLORREF REFS_MATCH_TEXT      = RGB(255, 231, 146);
static const COLORREF REFS_BORDER          = RGB(60, 60, 60);
static const COLORREF REFS_SELECTED_BG     = RGB(4, 57, 94);
static const COLORREF REFS_HOVER_BG        = RGB(45, 45, 46);
static const COLORREF REFS_COUNT_TEXT      = RGB(100, 185, 255);
static const COLORREF REFS_COLLAPSE_ICON   = RGB(180, 180, 180);

// Registered class
static bool s_refsClassRegistered = false;
static const char* REFS_CLASS_NAME = "RawrXD_ReferencesPanel";

static void ensureRefsClassRegistered(HINSTANCE hInst) {
    if (s_refsClassRegistered) return;
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(REFS_BG);
    wc.lpszClassName = REFS_CLASS_NAME;
    RegisterClassExA(&wc);
    s_refsClassRegistered = true;
}

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initReferencesPanel() {
    m_refsState = ReferencePanelState{};
    m_refsState.hFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    ensureRefsClassRegistered(GetModuleHandle(NULL));
    LOG_INFO("[ReferencesPanel] Initialized");
}

void Win32IDE::shutdownReferencesPanel() {
    closeReferencesPanel();
    if (m_refsState.hFont) { DeleteObject(m_refsState.hFont); m_refsState.hFont = nullptr; }
    LOG_INFO("[ReferencesPanel] Shutdown");
}

// ── Show References ────────────────────────────────────────────────────────
void Win32IDE::showReferences(const std::string& symbolName,
                              const std::vector<ReferenceLocation>& refs) {
    closeReferencesPanel();
    if (refs.empty()) {
        appendToOutput("No references found for '" + symbolName + "'", "Output", OutputSeverity::Info);
        return;
    }

    m_refsState.symbolName = symbolName;
    m_refsState.totalCount = (int)refs.size();
    m_refsState.visible = true;

    // Group by file
    std::map<std::string, std::vector<ReferenceLocation>> grouped;
    for (auto& ref : refs) {
        grouped[ref.filePath].push_back(ref);
    }
    m_refsState.groups.clear();
    for (auto& [filePath, fileRefs] : grouped) {
        ReferenceGroup group;
        group.filePath = filePath;
        group.refs = fileRefs;
        group.expanded = true;
        m_refsState.groups.push_back(group);
    }

    // Create panel at bottom of editor area (peek view style)
    RECT rcClient;
    GetClientRect(m_hwndMain, &rcClient);
    int panelH = dpiScale(250);
    int panelX = m_sidebarVisible ? m_sidebarWidth + 48 : 48;
    int panelW = rcClient.right - panelX;
    int panelY = rcClient.bottom - panelH - 22; // above status bar

    m_refsState.hwndPanel = CreateWindowExA(WS_EX_CLIENTEDGE,
        REFS_CLASS_NAME, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        panelX, panelY, panelW, panelH, m_hwndMain,
        (HMENU)(UINT_PTR)IDC_REFS_PANEL, GetModuleHandle(NULL), NULL);
    SetPropA(m_refsState.hwndPanel, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_refsState.hwndPanel, GWLP_WNDPROC, (LONG_PTR)ReferencesPanelProc);

    // Header with count
    int headerH = dpiScale(28);
    m_refsState.hwndCountLabel = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, panelW - dpiScale(32), headerH, m_refsState.hwndPanel,
        (HMENU)(UINT_PTR)IDC_REFS_COUNT_LABEL, GetModuleHandle(NULL), NULL);
    SetPropA(m_refsState.hwndCountLabel, "IDE_PTR", (HANDLE)this);

    // Close button
    CreateWindowExA(0, "BUTTON", "X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelW - dpiScale(30), dpiScale(2), dpiScale(26), dpiScale(22),
        m_refsState.hwndPanel,
        (HMENU)(UINT_PTR)IDC_REFS_CLOSE_BTN, GetModuleHandle(NULL), NULL);

    // TreeView for results
    m_refsState.hwndTree = CreateWindowExA(0, WC_TREEVIEWA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_LINESATROOT |
        TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
        0, headerH, panelW, panelH - headerH, m_refsState.hwndPanel,
        (HMENU)(UINT_PTR)IDC_REFS_TREE, GetModuleHandle(NULL), NULL);

    // Dark mode styling
    TreeView_SetBkColor(m_refsState.hwndTree, REFS_BG);
    TreeView_SetTextColor(m_refsState.hwndTree, REFS_TEXT);
    SendMessageA(m_refsState.hwndTree, WM_SETFONT, (WPARAM)m_refsState.hFont, TRUE);

    populateReferencesTree();
    LOG_INFO("[ReferencesPanel] Showing " + std::to_string(m_refsState.totalCount) +
             " references for '" + symbolName + "'");
}

void Win32IDE::closeReferencesPanel() {
    if (m_refsState.hwndPanel) {
        DestroyWindow(m_refsState.hwndPanel);
        m_refsState.hwndPanel = nullptr;
        m_refsState.hwndTree = nullptr;
        m_refsState.hwndCountLabel = nullptr;
    }
    m_refsState.visible = false;
    m_refsState.groups.clear();
}

// ── Populate Tree ──────────────────────────────────────────────────────────
void Win32IDE::populateReferencesTree() {
    if (!m_refsState.hwndTree) return;
    TreeView_DeleteAllItems(m_refsState.hwndTree);

    for (int gi = 0; gi < (int)m_refsState.groups.size(); gi++) {
        auto& group = m_refsState.groups[gi];

        // Extract filename from path
        std::string fileName = group.filePath;
        size_t lastSlash = fileName.find_last_of("\\/");
        if (lastSlash != std::string::npos) fileName = fileName.substr(lastSlash + 1);

        // File group header: "filename.cpp (N references)"
        std::string headerText = fileName + "  (" + std::to_string(group.refs.size()) + " references)";

        TVINSERTSTRUCTA tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (char*)headerText.c_str();
        tvis.item.lParam = (LPARAM)(-gi - 1); // Negative = group index

        HTREEITEM hGroup = TreeView_InsertItem(m_refsState.hwndTree, &tvis);

        // Individual references
        for (int ri = 0; ri < (int)group.refs.size(); ri++) {
            auto& ref = group.refs[ri];
            char refText[512];
            snprintf(refText, sizeof(refText), "  %4d:%d  %s",
                     ref.line + 1, ref.column + 1, ref.lineText.c_str());

            TVINSERTSTRUCTA childTvis = {};
            childTvis.hParent = hGroup;
            childTvis.hInsertAfter = TVI_LAST;
            childTvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            childTvis.item.pszText = refText;
            childTvis.item.lParam = (LPARAM)((gi << 16) | ri); // Encode group + ref index

            TreeView_InsertItem(m_refsState.hwndTree, &childTvis);
        }

        if (group.expanded)
            TreeView_Expand(m_refsState.hwndTree, hGroup, TVE_EXPAND);
    }
}

// ── Navigate to Reference ──────────────────────────────────────────────────
void Win32IDE::navigateToReference(int groupIndex, int refIndex) {
    if (groupIndex < 0 || groupIndex >= (int)m_refsState.groups.size()) return;
    auto& group = m_refsState.groups[groupIndex];

    if (refIndex < 0 || refIndex >= (int)group.refs.size()) return;
    auto& ref = group.refs[refIndex];

    // Open file if different
    if (ref.filePath != m_currentFile) {
        // Use existing file opening mechanism
        openFile(ref.filePath);
    }

    // Go to line
    if (m_hwndEditor) {
        int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, ref.line, 0);
        int charPos = lineIndex + ref.column;
        CHARRANGE cr = {charPos, charPos + (int)ref.matchText.size()};
        SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessageA(m_hwndEditor, EM_SCROLLCARET, 0, 0);
        SetFocus(m_hwndEditor);
    }
}

// ── Render ─────────────────────────────────────────────────────────────────
void Win32IDE::renderReferencesPanel(HDC hdc, RECT rc) {
    // Background
    HBRUSH bgBrush = CreateSolidBrush(REFS_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Header
    RECT headerRect = {rc.left, rc.top, rc.right, rc.top + dpiScale(28)};
    HBRUSH headerBrush = CreateSolidBrush(REFS_HEADER_BG);
    FillRect(hdc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_refsState.hFont);

    // Header text
    std::string headerText = "'" + m_refsState.symbolName + "' — " +
                             std::to_string(m_refsState.totalCount) + " references in " +
                             std::to_string(m_refsState.groups.size()) + " files";
    SetTextColor(hdc, REFS_COUNT_TEXT);
    TextOutA(hdc, rc.left + 8, rc.top + 6, headerText.c_str(), (int)headerText.size());

    SelectObject(hdc, oldFont);
}

// ── Window Proc ────────────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::ReferencesPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ide) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ide->renderReferencesPanel(hdc, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        if (!ide) break;
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if ((int)dis->CtlID == IDC_REFS_COUNT_LABEL) {
            HBRUSH bg = CreateSolidBrush(REFS_HEADER_BG);
            FillRect(dis->hDC, &dis->rcItem, bg);
            DeleteObject(bg);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, REFS_COUNT_TEXT);
            std::string text = "'" + ide->m_refsState.symbolName + "' — " +
                std::to_string(ide->m_refsState.totalCount) + " references";
            RECT textRect = dis->rcItem;
            textRect.left += 8;
            DrawTextA(dis->hDC, text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        if (!ide) break;
        if (LOWORD(wParam) == IDC_REFS_CLOSE_BTN) {
            ide->closeReferencesPanel();
            return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        if (!ide) break;
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->idFrom == IDC_REFS_TREE) {
            if (nmhdr->code == NM_DBLCLK || nmhdr->code == NM_RETURN) {
                HTREEITEM hSel = TreeView_GetSelection(ide->m_refsState.hwndTree);
                if (hSel) {
                    TVITEMA item = {};
                    item.mask = TVIF_PARAM;
                    item.hItem = hSel;
                    TreeView_GetItem(ide->m_refsState.hwndTree, &item);
                    LPARAM param = item.lParam;

                    if (param >= 0) {
                        int groupIdx = (int)(param >> 16);
                        int refIdx = (int)(param & 0xFFFF);
                        ide->navigateToReference(groupIdx, refIdx);
                    }
                }
                return 0;
            }
        }
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
