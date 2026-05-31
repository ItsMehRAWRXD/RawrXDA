// ============================================================================
// Win32IDE_CodeCompletion.cpp — IntelliSense Code Completion Popup
// ============================================================================
// Dropdown completion popup for the code editor, triggered by Ctrl+Space
// or typing trigger characters (., ->, ::).  Displays HybridCompletionItem
// results from LSP + AI + keyword sources with kind icons.
//
// Architecture:
//   - WS_POPUP window with embedded ListView (report mode, single column)
//   - Detail text in a separate tooltip/label below the list
//   - Keyboard: Up/Down to navigate, Tab/Enter to accept, Esc to dismiss
//   - Auto-position below the editor caret
//   - Async: request runs on background thread → PostMessage to UI
//
// Integration:
//   - Editor subclass intercepts WM_CHAR / WM_KEYDOWN for triggers
//   - requestHybridCompletion() backend provides items
//   - insertCompletionText() replaces the partial prefix with the selection
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <thread>

// Window IDs
#define IDC_COMPLETION_LIST  9801
#define IDC_COMPLETION_DETAIL 9802
#define WM_COMPLETION_READY (WM_USER + 0x501)

// ============================================================================
// Completion Popup State (file-static)
// ============================================================================
static struct CompletionPopupState {
    HWND hwndPopup        = nullptr;
    HWND hwndList         = nullptr;
    HWND hwndDetail       = nullptr;
    bool visible          = false;
    int  selectedIndex    = 0;
    std::string prefix;                 // The partial word being completed
    int prefixStartChar   = 0;          // Char index in the editor where prefix starts
    std::vector<Win32IDE::HybridCompletionItem> items;
    std::vector<Win32IDE::HybridCompletionItem> filteredItems;
} s_popup;

// ============================================================================
// Kind → Display Icon Character (Unicode symbols for kind column)
// ============================================================================
static const wchar_t* kindIcon(const std::string& source, float confidence) {
    // Source-based icon (simple approach using Unicode)
    if (source == "lsp")
        return L"\x25CB";   // ○ LSP
    if (source == "ai")
        return L"\x2605";   // ★ AI
    if (source == "asm")
        return L"\x25A0";   // ■ ASM
    return L"\x25CF";       // ● Other
}

// ============================================================================
// Filter items by prefix (case-insensitive substring match)
// ============================================================================
static void filterItems(const std::string& prefix) {
    s_popup.filteredItems.clear();
    if (prefix.empty()) {
        s_popup.filteredItems = s_popup.items;
        return;
    }

    std::string lowerPrefix = prefix;
    for (auto& c : lowerPrefix) c = (char)std::tolower((unsigned char)c);

    for (const auto& item : s_popup.items) {
        std::string lowerLabel = item.label;
        for (auto& c : lowerLabel) c = (char)std::tolower((unsigned char)c);

        // Substring match (handles fuzzy-like behavior for short prefixes)
        if (lowerLabel.find(lowerPrefix) != std::string::npos) {
            s_popup.filteredItems.push_back(item);
        }
    }

    // Sort: exact prefix match first, then by confidence descending
    std::stable_sort(s_popup.filteredItems.begin(), s_popup.filteredItems.end(),
        [&](const Win32IDE::HybridCompletionItem& a, const Win32IDE::HybridCompletionItem& b) {
            std::string la = a.label, lb = b.label;
            for (auto& c : la) c = (char)std::tolower((unsigned char)c);
            for (auto& c : lb) c = (char)std::tolower((unsigned char)c);
            bool aPrefix = (la.find(lowerPrefix) == 0);
            bool bPrefix = (lb.find(lowerPrefix) == 0);
            if (aPrefix != bPrefix) return aPrefix;
            return a.confidence > b.confidence;
        });
}

// ============================================================================
// Populate the ListView with current filtered items
// ============================================================================
static void populateListView() {
    if (!s_popup.hwndList) return;
    SendMessage(s_popup.hwndList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(s_popup.hwndList);

    for (int i = 0; i < (int)s_popup.filteredItems.size() && i < 50; ++i) {
        const auto& item = s_popup.filteredItems[i];

        // Build display string: icon + label + detail
        std::wstring display;
        display += kindIcon(item.source, item.confidence);
        display += L" ";

        // Convert label to wide
        int needed = MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, nullptr, 0);
        if (needed > 0) {
            std::wstring wlabel(needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, &wlabel[0], needed);
            wlabel.resize(needed - 1); // remove null
            display += wlabel;
        }

        if (!item.detail.empty()) {
            display += L"  \x2014 ";  // — em dash separator
            int nd = MultiByteToWideChar(CP_UTF8, 0, item.detail.c_str(), -1, nullptr, 0);
            if (nd > 0) {
                std::wstring wd(nd, 0);
                MultiByteToWideChar(CP_UTF8, 0, item.detail.c_str(), -1, &wd[0], nd);
                wd.resize(nd - 1);
                display += wd;
            }
        }

        LVITEMW lvi = {};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = i;
        lvi.pszText  = (LPWSTR)display.c_str();
        SendMessageW(s_popup.hwndList, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
    }

    SendMessage(s_popup.hwndList, WM_SETREDRAW, TRUE, 0);

    // Select first item
    if (!s_popup.filteredItems.empty()) {
        ListView_SetItemState(s_popup.hwndList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        s_popup.selectedIndex = 0;
    }
}

// ============================================================================
// Create the popup window (once, hidden until needed)
// ============================================================================
void Win32IDE::createCompletionPopup()
{
    if (s_popup.hwndPopup)
        return;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DROPSHADOW;
    wc.lpfnWndProc   = CompletionPopupProc;
    wc.hInstance     = m_hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(37, 37, 38));
    wc.lpszClassName = L"RawrXD_CompletionPopup";
    RegisterClassExW(&wc);

    s_popup.hwndPopup = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"RawrXD_CompletionPopup",
        L"",
        WS_POPUP | WS_BORDER,
        0, 0, 400, 250,
        m_hwndMain,
        nullptr,
        m_hInstance,
        nullptr
    );

    if (!s_popup.hwndPopup)
        return;

    SetPropW(s_popup.hwndPopup, L"IDE_PTR", (HANDLE)this);

    // Create ListView inside popup
    s_popup.hwndList = CreateWindowExW(
        0,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER |
            LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        0, 0, 400, 200,
        s_popup.hwndPopup,
        (HMENU)(INT_PTR)IDC_COMPLETION_LIST,
        m_hInstance,
        nullptr
    );

    // Set ListView colors (VS Code Dark+ theme)
    ListView_SetBkColor(s_popup.hwndList, RGB(37, 37, 38));
    ListView_SetTextBkColor(s_popup.hwndList, RGB(37, 37, 38));
    ListView_SetTextColor(s_popup.hwndList, RGB(204, 204, 204));
    ListView_SetExtendedListViewStyle(s_popup.hwndList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    // Add single column
    LVCOLUMNW col = {};
    col.mask  = LVCF_WIDTH | LVCF_FMT;
    col.cx    = 396;
    col.fmt   = LVCFMT_LEFT;
    SendMessageW(s_popup.hwndList, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    // Detail label at bottom
    s_popup.hwndDetail = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        4, 202, 392, 44,
        s_popup.hwndPopup,
        (HMENU)(INT_PTR)IDC_COMPLETION_DETAIL,
        m_hInstance,
        nullptr
    );

    // Set fonts
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    SendMessage(s_popup.hwndList, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_popup.hwndDetail, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// ============================================================================
// Show the completion popup at the editor caret position
// ============================================================================
void Win32IDE::showCompletionPopup()
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return;

    createCompletionPopup();
    if (!s_popup.hwndPopup)
        return;

    // Get caret position in screen coords
    DWORD selStart = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, 0);

    POINTL pt = {};
    SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, selStart);

    POINT screenPt = {pt.x, pt.y};
    ClientToScreen(m_hwndEditor, &screenPt);

    // Get line height for offset
    HDC hdc = GetDC(m_hwndEditor);
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(hdc, hOld);
    ReleaseDC(m_hwndEditor, hdc);

    // Position popup below caret
    int popupX = screenPt.x;
    int popupY = screenPt.y + lineHeight + 2;

    // Clamp to screen bounds
    HMONITOR hMon = MonitorFromPoint(screenPt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hMon, &mi)) {
        if (popupX + 400 > mi.rcWork.right)
            popupX = mi.rcWork.right - 400;
        if (popupY + 250 > mi.rcWork.bottom)
            popupY = screenPt.y - 250 - 2;  // Show above caret if no room below
    }

    SetWindowPos(s_popup.hwndPopup, HWND_TOPMOST, popupX, popupY, 400, 250,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    s_popup.visible = true;

    populateListView();
}

// ============================================================================
// Hide the completion popup
// ============================================================================
void Win32IDE::hideCompletionPopup()
{
    if (s_popup.hwndPopup && s_popup.visible) {
        ShowWindow(s_popup.hwndPopup, SW_HIDE);
        s_popup.visible = false;
        s_popup.items.clear();
        s_popup.filteredItems.clear();
        s_popup.prefix.clear();
        s_popup.selectedIndex = 0;
    }
}

// ============================================================================
// Extract the partial word before the cursor (the "prefix" being typed)
// ============================================================================
static std::string extractPrefix(HWND hwndEditor, int& outPrefixStartChar) {
    DWORD selStart = 0;
    SendMessage(hwndEditor, EM_GETSEL, (WPARAM)&selStart, 0);
    if (selStart == 0) {
        outPrefixStartChar = 0;
        return {};
    }

    // Get the current line text
    int lineIndex = (int)SendMessage(hwndEditor, EM_LINEFROMCHAR, selStart, 0);
    int lineStart = (int)SendMessage(hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen   = (int)SendMessage(hwndEditor, EM_LINELENGTH, lineStart, 0);

    if (lineLen <= 0 || lineLen > 4096) {
        outPrefixStartChar = (int)selStart;
        return {};
    }

    std::vector<wchar_t> buf(lineLen + 2, 0);
    *(WORD*)buf.data() = (WORD)(lineLen + 1);
    SendMessageW(hwndEditor, EM_GETLINE, lineIndex, (LPARAM)buf.data());
    buf[lineLen] = 0;

    int colInLine = (int)selStart - lineStart;
    if (colInLine < 0 || colInLine > lineLen)
        colInLine = lineLen;

    // Walk backwards from cursor to find word start
    int wordStart = colInLine;
    while (wordStart > 0) {
        wchar_t ch = buf[wordStart - 1];
        if (std::iswalnum(ch) || ch == L'_')
            --wordStart;
        else
            break;
    }

    outPrefixStartChar = lineStart + wordStart;

    // Convert prefix to UTF-8
    std::wstring wprefix(&buf[wordStart], &buf[colInLine]);
    int needed = WideCharToMultiByte(CP_UTF8, 0, wprefix.c_str(), (int)wprefix.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return {};
    std::string prefix(needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wprefix.c_str(), (int)wprefix.size(), &prefix[0], needed, nullptr, nullptr);
    return prefix;
}

// ============================================================================
// Trigger completion request (Ctrl+Space or auto-trigger on . -> ::)
// ============================================================================
void Win32IDE::triggerCodeCompletion()
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return;

    auto [line, col] = getCursorPosition();
    int prefixStart = 0;
    std::string prefix = extractPrefix(m_hwndEditor, prefixStart);

    s_popup.prefix = prefix;
    s_popup.prefixStartChar = prefixStart;

    // Request completions on background thread to avoid blocking UI
    std::string filePath = m_currentFile;
    HWND hwndMain = m_hwndMain;

    std::thread([this, filePath, line, col, hwndMain]() {
        auto items = requestHybridCompletion(filePath, line - 1, col);

        // Post results to UI thread
        auto* resultItems = new std::vector<HybridCompletionItem>(std::move(items));
        PostMessage(hwndMain, WM_COMPLETION_READY, 0, (LPARAM)resultItems);
    }).detach();
}

// ============================================================================
// Handle completion results arriving on UI thread
// ============================================================================
void Win32IDE::onCompletionReady(LPARAM lParam)
{
    auto* resultItems = reinterpret_cast<std::vector<HybridCompletionItem>*>(lParam);
    if (!resultItems)
        return;

    s_popup.items = std::move(*resultItems);
    delete resultItems;

    if (s_popup.items.empty()) {
        hideCompletionPopup();
        return;
    }

    filterItems(s_popup.prefix);

    if (s_popup.filteredItems.empty()) {
        hideCompletionPopup();
        return;
    }

    showCompletionPopup();
}

// ============================================================================
// Accept the currently selected completion
// ============================================================================
void Win32IDE::acceptCompletion()
{
    if (!s_popup.visible || s_popup.filteredItems.empty())
        return;

    int idx = s_popup.selectedIndex;
    if (idx < 0 || idx >= (int)s_popup.filteredItems.size())
        idx = 0;

    const auto& item = s_popup.filteredItems[idx];

    // Determine the insert text
    const std::string& text = item.insertText.empty() ? item.label : item.insertText;

    // Select the prefix range in the editor and replace it
    DWORD selEnd = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, 0, (LPARAM)&selEnd);

    SendMessage(m_hwndEditor, EM_SETSEL, s_popup.prefixStartChar, selEnd);

    // Convert to wide and insert
    int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    if (needed > 0) {
        std::wstring wtext(needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &wtext[0], needed);

        SETTEXTEX st = {};
        st.flags    = ST_SELECTION;
        st.codepage = 1200;
        SendMessageW(m_hwndEditor, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)wtext.c_str());
    }

    hideCompletionPopup();
}

// ============================================================================
// Navigate the completion list (called from editor keydown handler)
// Returns true if the key was consumed.
// ============================================================================
bool Win32IDE::handleCompletionKeyDown(WPARAM vk)
{
    if (!s_popup.visible)
        return false;

    switch (vk) {
    case VK_UP:
        if (s_popup.selectedIndex > 0) {
            --s_popup.selectedIndex;
            ListView_SetItemState(s_popup.hwndList, s_popup.selectedIndex,
                LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(s_popup.hwndList, s_popup.selectedIndex, FALSE);
            updateCompletionDetail();
        }
        return true;

    case VK_DOWN:
        if (s_popup.selectedIndex + 1 < (int)s_popup.filteredItems.size()) {
            ++s_popup.selectedIndex;
            ListView_SetItemState(s_popup.hwndList, s_popup.selectedIndex,
                LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(s_popup.hwndList, s_popup.selectedIndex, FALSE);
            updateCompletionDetail();
        }
        return true;

    case VK_TAB:
    case VK_RETURN:
        acceptCompletion();
        return true;

    case VK_ESCAPE:
        hideCompletionPopup();
        return true;

    default:
        break;
    }
    return false;
}

// ============================================================================
// Update prefix and re-filter as user types (called on EN_CHANGE)
// ============================================================================
void Win32IDE::updateCompletionFilter()
{
    if (!s_popup.visible)
        return;

    int prefixStart = 0;
    std::string newPrefix = extractPrefix(m_hwndEditor, prefixStart);

    if (newPrefix.empty()) {
        hideCompletionPopup();
        return;
    }

    s_popup.prefix = newPrefix;
    s_popup.prefixStartChar = prefixStart;

    filterItems(newPrefix);
    if (s_popup.filteredItems.empty()) {
        hideCompletionPopup();
        return;
    }

    populateListView();
    updateCompletionDetail();
}

// ============================================================================
// Update detail label with the selected item's documentation
// ============================================================================
void Win32IDE::updateCompletionDetail()
{
    if (!s_popup.hwndDetail || s_popup.filteredItems.empty())
        return;

    int idx = s_popup.selectedIndex;
    if (idx < 0 || idx >= (int)s_popup.filteredItems.size())
        return;

    const auto& item = s_popup.filteredItems[idx];
    std::string detailText = item.detail;
    if (detailText.empty())
        detailText = item.source + " (" + item.label + ")";

    int needed = MultiByteToWideChar(CP_UTF8, 0, detailText.c_str(), -1, nullptr, 0);
    if (needed > 0) {
        std::wstring wtext(needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, detailText.c_str(), -1, &wtext[0], needed);
        SetWindowTextW(s_popup.hwndDetail, wtext.c_str());
    }
}

// ============================================================================
// Check if a typed character should auto-trigger completion
// ============================================================================
bool Win32IDE::shouldAutoTriggerCompletion(wchar_t ch)
{
    // Trigger on . (member access), : (after :: scope), > (after ->)
    if (ch == L'.')
        return true;

    if (ch == L':' || ch == L'>') {
        // Check previous character for :: or ->
        DWORD selStart = 0;
        SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, 0);
        if (selStart < 2)
            return false;

        int lineIdx = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selStart - 1, 0);
        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lineStart, 0);
        if (lineLen <= 0 || lineLen > 4096)
            return false;

        std::vector<wchar_t> buf(lineLen + 2, 0);
        *(WORD*)buf.data() = (WORD)(lineLen + 1);
        SendMessageW(m_hwndEditor, EM_GETLINE, lineIdx, (LPARAM)buf.data());

        int col = (int)(selStart - 1) - lineStart;
        if (col < 1)
            return false;

        if (ch == L':' && buf[col - 1] == L':')
            return true;   // ::
        if (ch == L'>' && buf[col - 1] == L'-')
            return true;   // ->
    }

    return false;
}

// ============================================================================
// Completion Popup WndProc
// ============================================================================
LRESULT CALLBACK Win32IDE::CompletionPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = (Win32IDE*)GetPropW(hwnd, L"IDE_PTR");

    switch (uMsg) {
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm->idFrom == IDC_COMPLETION_LIST) {
            if (nm->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* nlv = (NMLISTVIEW*)lParam;
                if ((nlv->uNewState & LVIS_SELECTED) && ide) {
                    s_popup.selectedIndex = nlv->iItem;
                    ide->updateCompletionDetail();
                }
            }
            if (nm->code == NM_DBLCLK && ide) {
                ide->acceptCompletion();
            }
            if (nm->code == LVN_GETDISPINFOW) {
                // Virtual list: provide item text on demand
                NMLVDISPINFOW* di = (NMLVDISPINFOW*)lParam;
                int idx = di->item.iItem;
                if (idx >= 0 && idx < (int)s_popup.filteredItems.size()) {
                    static thread_local std::wstring s_displayBuf;
                    const auto& item = s_popup.filteredItems[idx];

                    s_displayBuf.clear();
                    s_displayBuf += kindIcon(item.source, item.confidence);
                    s_displayBuf += L" ";

                    int n = MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, nullptr, 0);
                    if (n > 0) {
                        std::wstring wl(n, 0);
                        MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, &wl[0], n);
                        wl.resize(n - 1);
                        s_displayBuf += wl;
                    }

                    if (!item.detail.empty()) {
                        s_displayBuf += L"  \x2014 ";
                        int nd = MultiByteToWideChar(CP_UTF8, 0, item.detail.c_str(), -1, nullptr, 0);
                        if (nd > 0) {
                            std::wstring wd(nd, 0);
                            MultiByteToWideChar(CP_UTF8, 0, item.detail.c_str(), -1, &wd[0], nd);
                            wd.resize(nd - 1);
                            s_displayBuf += wd;
                        }
                    }

                    di->item.pszText = (LPWSTR)s_displayBuf.c_str();
                }
            }
        }
        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(37, 37, 38));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        return 1;
    }

    case WM_MOUSEACTIVATE:
        // Don't steal focus from editor
        return MA_NOACTIVATE;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Completion visibility query
// ============================================================================
bool Win32IDE::isCompletionPopupVisible() const
{
    return s_popup.visible;
}
