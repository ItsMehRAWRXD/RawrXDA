// Win32IDE_GoToSymbol.cpp — @ / workspace symbol picker (filter + ListView, LSP-backed when available).

#include "Win32IDE.h"
#include "RawrXD_SymbolEngine.h"
#include <algorithm>
#include <cctype>
#include <commctrl.h>
#include <cwctype>
#include <filesystem>
#include <richedit.h>
#include <string>
#include <unordered_set>


// ── File-static state ───────────────────────────────────────────────────────
struct FlatSymbol
{
    std::wstring name;
    std::wstring detail;
    std::wstring kindLabel;
    std::wstring pathOrContainer;
    std::string filePath;
    int line = 0;
    int kindValue = 0;
    bool workspaceResult = false;
};

static struct GoToSymbolState
{
    HWND hwndOverlay = nullptr;
    HWND hwndEdit = nullptr;
    HWND hwndList = nullptr;
    WNDPROC oldEditProc = nullptr;
    Win32IDE* pIDE = nullptr;
    bool active = false;
    bool workspaceMode = false;
    std::vector<FlatSymbol> allSymbols;
    std::vector<FlatSymbol> filtered;
} s_gts;

#define IDC_GTS_EDIT 9911
#define IDC_GTS_LIST 9912

// ── Forward declarations ────────────────────────────────────────────────────
static LRESULT CALLBACK GoToSymbolOverlayProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK GoToSymbolEditProc(HWND, UINT, WPARAM, LPARAM);
static void populateList();
void filterSymbols();
void commitSymbolSelection();
static void showGoToSymbolPickerImpl(Win32IDE* ide, bool workspaceMode);

static const wchar_t* symbolKindLabel(int kind)
{
    switch (kind)
    {
        case 1:
            return L"File";
        case 2:
            return L"Module";
        case 3:
            return L"Namespace";
        case 4:
            return L"Package";
        case 5:
            return L"Class";
        case 6:
            return L"Method";
        case 7:
            return L"Property";
        case 8:
            return L"Field";
        case 9:
            return L"Constructor";
        case 10:
            return L"Enum";
        case 11:
            return L"Interface";
        case 12:
            return L"Function";
        case 13:
            return L"Variable";
        case 14:
            return L"Constant";
        case 15:
            return L"String";
        case 16:
            return L"Number";
        case 17:
            return L"Boolean";
        case 18:
            return L"Array";
        case 19:
            return L"Object";
        case 20:
            return L"Key";
        case 21:
            return L"Null";
        case 22:
            return L"EnumMember";
        case 23:
            return L"Struct";
        case 24:
            return L"Event";
        case 25:
            return L"Operator";
        case 26:
            return L"TypeParameter";
        default:
            return L"Symbol";
    }
}

static const wchar_t* symbolKindIcon(int kind)
{
    switch (kind)
    {
        case 5:
            return L"\x25A0 ";  // ■ Class
        case 6:
            return L"\x25B6 ";  // ▶ Method
        case 9:
            return L"\x25B6 ";  // ▶ Constructor
        case 12:
            return L"\x0192 ";  // ƒ Function
        case 13:
            return L"\x03B1 ";  // α Variable
        case 14:
            return L"\x03C0 ";  // π Constant
        case 10:
            return L"\x2261 ";  // ≡ Enum
        case 22:
            return L"\x2022 ";  // • EnumMember
        case 23:
            return L"\x25A1 ";  // □ Struct
        case 11:
            return L"\x25CB ";  // ○ Interface
        case 7:
            return L"\x25C6 ";  // ◆ Property
        case 8:
            return L"\x25C6 ";  // ◆ Field
        case 3:
            return L"\x2302 ";  // ⌂ Namespace
        default:
            return L"\x00B7 ";  // · default
    }
}

static std::string wideToUtf8Local(std::wstring_view w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

// Flatten outline into the symbol picker list (calls refreshOutlineFromLSP).
void Win32IDE::populateSymbolPickerData()
{
    s_gts.allSymbols.clear();

    // Refresh from LSP (accesses private members)
    refreshOutlineFromLSP();

    // Recursively flatten OutlineSymbol tree → FlatSymbol list
    auto addSymbol = [](const OutlineSymbol& sym, auto& self) -> void
    {
        FlatSymbol fs;
        fs.name = std::wstring(sym.name.begin(), sym.name.end());
        fs.detail = std::wstring(sym.detail.begin(), sym.detail.end());
        fs.kindLabel = symbolKindLabel(sym.kind);
        fs.kindValue = sym.kind;
        fs.line = sym.line;
        s_gts.allSymbols.push_back(fs);

        for (const auto& child : sym.children)
        {
            self(child, self);
        }
    };

    for (const auto& sym : m_outlineSymbols)
    {
        addSymbol(sym, addSymbol);
    }

    // Sort by line number
    std::sort(s_gts.allSymbols.begin(), s_gts.allSymbols.end(),
              [](const FlatSymbol& a, const FlatSymbol& b) { return a.line < b.line; });
}

void Win32IDE::refreshWorkspaceSymbolPickerResults(const std::string& queryUtf8)
{
    s_gts.filtered.clear();
    std::unordered_set<std::wstring> seen;

    auto workspaceSymbols = lspWorkspaceSymbols(queryUtf8);
    s_gts.filtered.reserve(workspaceSymbols.size() + 100);

    for (const auto& sym : workspaceSymbols)
    {
        FlatSymbol fs;
        fs.name = std::wstring(sym.name.begin(), sym.name.end());
        if (!sym.detail.empty())
        {
            fs.detail = std::wstring(sym.detail.begin(), sym.detail.end());
        }
        else
        {
            fs.detail = std::wstring(sym.containerName.begin(), sym.containerName.end());
        }
        fs.kindLabel = symbolKindLabel(sym.kind);
        fs.kindValue = sym.kind;
        fs.line = sym.location.range.start.line + 1;
        fs.filePath = uriToFilePath(sym.location.uri);
        fs.workspaceResult = true;

        std::filesystem::path targetPath(fs.filePath);
        const std::wstring fileTitle(targetPath.filename().wstring());
        if (!sym.detail.empty())
        {
            fs.pathOrContainer = fileTitle + L" — " + std::wstring(sym.detail.begin(), sym.detail.end());
        }
        else
        {
            std::string pathLabel = sym.containerName.empty() ? targetPath.filename().string() : sym.containerName;
            fs.pathOrContainer = std::wstring(pathLabel.begin(), pathLabel.end());
        }
        s_gts.filtered.push_back(std::move(fs));

        std::wstring dedupeKey = std::wstring(sym.name.begin(), sym.name.end());
        dedupeKey += L"|";
        dedupeKey += std::wstring(sym.location.uri.begin(), sym.location.uri.end());
        dedupeKey += L"|" + std::to_wstring(sym.location.range.start.line);
        seen.insert(std::move(dedupeKey));
    }

    std::wstring queryWide(queryUtf8.begin(), queryUtf8.end());
    auto localSymbols = RawrXD::SymbolEngine::GlobalSymbolDatabase().SearchWorkspace(queryWide, 100);
    for (const auto& sym : localSymbols)
    {
        if (!sym)
            continue;

        std::wstring dedupeKey = sym->name + L"|" + sym->sourceUri + L"|" + std::to_wstring(sym->range.lineStart);
        if (seen.find(dedupeKey) != seen.end())
            continue;
        seen.insert(dedupeKey);

        FlatSymbol fs;
        fs.name = sym->name;
        fs.detail = sym->detail;
        fs.kindValue = static_cast<int>(sym->kind);
        fs.kindLabel = symbolKindLabel(fs.kindValue);
        fs.line = static_cast<int>(sym->range.lineStart) + 1;
        fs.filePath = wideToUtf8Local(sym->sourceUri);
        fs.workspaceResult = true;

        std::filesystem::path targetPath(sym->sourceUri);
        const std::wstring fileTitle = targetPath.filename().wstring();
        fs.pathOrContainer = fileTitle.empty() ? sym->sourceUri : fileTitle;
        if (!sym->detail.empty())
        {
            fs.pathOrContainer += L" — " + sym->detail;
        }

        s_gts.filtered.push_back(std::move(fs));
    }
}

void Win32IDE::jumpToWorkspaceSymbolPickerResult(const std::string& filePath, uint32_t line1Based)
{
    navigateToFileLine(filePath, line1Based);
}

static bool wideSubstringMatch(const std::wstring& haystack, const std::wstring& needle)
{
    if (needle.empty())
        return true;
    size_t hLen = haystack.size();
    size_t nLen = needle.size();
    if (nLen > hLen)
        return false;
    for (size_t i = 0; i <= hLen - nLen; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < nLen; ++j)
        {
            if (towlower(haystack[i + j]) != towlower(needle[j]))
            {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

void filterSymbols()
{
    if (!s_gts.hwndEdit || !s_gts.hwndList)
        return;

    wchar_t buf[256] = {};
    GetWindowTextW(s_gts.hwndEdit, buf, _countof(buf));
    std::wstring query(buf);

    // Strip leading @ (VS Code convention)
    if (!query.empty() && query[0] == L'@')
        query = query.substr(1);

    if (!query.empty() && query[0] == L'#')
        query = query.substr(1);

    if (s_gts.workspaceMode)
    {
        if (!s_gts.pIDE)
        {
            populateList();
            return;
        }

        if (query.empty())
        {
            populateList();
            return;
        }

        std::string queryUtf8(query.begin(), query.end());
        s_gts.pIDE->refreshWorkspaceSymbolPickerResults(queryUtf8);

        populateList();
        return;
    }

    s_gts.filtered.clear();
    for (const auto& sym : s_gts.allSymbols)
    {
        if (wideSubstringMatch(sym.name, query))
        {
            s_gts.filtered.push_back(sym);
        }
    }

    populateList();
}

static void populateList()
{
    if (!s_gts.hwndList)
        return;

    SendMessage(s_gts.hwndList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(s_gts.hwndList);

    LVITEMW lvi = {};
    lvi.mask = LVIF_TEXT;

    for (int i = 0; i < (int)s_gts.filtered.size(); ++i)
    {
        const auto& sym = s_gts.filtered[i];

        // Column 0: icon + name
        std::wstring nameLabel = symbolKindIcon(sym.kindValue) + sym.name;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(nameLabel.c_str());
        ListView_InsertItem(s_gts.hwndList, &lvi);

        // Column 1: kind
        {
            std::wstring secondary = s_gts.workspaceMode ? sym.pathOrContainer : sym.kindLabel;
            LVITEMW lvi2 = {};
            lvi2.iSubItem = 1;
            lvi2.pszText = const_cast<LPWSTR>(secondary.c_str());
            SendMessageW(s_gts.hwndList, LVM_SETITEMTEXTW, i, (LPARAM)&lvi2);
        }

        // Column 2: line number
        {
            wchar_t lineBuf[16];
            _snwprintf_s(lineBuf, _countof(lineBuf), _TRUNCATE, L"%d", sym.line);
            LVITEMW lvi3 = {};
            lvi3.iSubItem = 2;
            lvi3.pszText = lineBuf;
            SendMessageW(s_gts.hwndList, LVM_SETITEMTEXTW, i, (LPARAM)&lvi3);
        }
    }

    SendMessage(s_gts.hwndList, WM_SETREDRAW, TRUE, 0);

    // Auto-select first item
    if (!s_gts.filtered.empty())
    {
        ListView_SetItemState(s_gts.hwndList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void commitSymbolSelection()
{
    if (!s_gts.hwndList || !s_gts.pIDE)
        return;

    int sel = ListView_GetNextItem(s_gts.hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)s_gts.filtered.size())
        return;

    int line = s_gts.filtered[sel].line;
    if (s_gts.workspaceMode && !s_gts.filtered[sel].filePath.empty())
        s_gts.pIDE->jumpToWorkspaceSymbolPickerResult(s_gts.filtered[sel].filePath, (uint32_t)line);
    else
        s_gts.pIDE->navigateToLine(line);
    s_gts.pIDE->hideGoToSymbolPicker();
}

void Win32IDE::showGoToSymbolPicker()
{
    showGoToSymbolPickerImpl(this, false);
}

void Win32IDE::showGoToWorkspaceSymbolPicker()
{
    showGoToSymbolPickerImpl(this, true);
}

static void showGoToSymbolPickerImpl(Win32IDE* ide, bool workspaceMode)
{
    if (s_gts.active)
    {
        if (s_gts.workspaceMode != workspaceMode && s_gts.pIDE)
        {
            s_gts.pIDE->hideGoToSymbolPicker();
        }
        else
        {
            SetFocus(s_gts.hwndEdit);
            SendMessage(s_gts.hwndEdit, EM_SETSEL, 0, -1);
            return;
        }
    }

    s_gts.pIDE = ide;
    s_gts.workspaceMode = workspaceMode;

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = GoToSymbolOverlayProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(37, 37, 38));
        wc.lpszClassName = L"RawrXD_GoToSymbol";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    if (!workspaceMode)
        ide->populateSymbolPickerData();
    else
        s_gts.allSymbols.clear();

    RECT rcMain = {};
    GetWindowRect(ide->getMainWindow(), &rcMain);
    const int popW = 560;
    const int popH = 340;
    const int popX = rcMain.left + (rcMain.right - rcMain.left - popW) / 2;
    const int popY = rcMain.top + 50;

    s_gts.hwndOverlay =
        CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"RawrXD_GoToSymbol", L"", WS_POPUP | WS_BORDER, popX, popY,
                        popW, popH, ide->getMainWindow(), nullptr, GetModuleHandleW(nullptr), nullptr);

    const wchar_t* seedText = workspaceMode ? L"" : L"@";
    s_gts.hwndEdit =
        CreateWindowExW(0, L"EDIT", seedText, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER, 8, 8, popW - 16, 24,
                        s_gts.hwndOverlay, (HMENU)(UINT_PTR)IDC_GTS_EDIT, GetModuleHandleW(nullptr), nullptr);

    s_gts.oldEditProc = (WNDPROC)SetWindowLongPtrW(s_gts.hwndEdit, GWLP_WNDPROC, (LONG_PTR)GoToSymbolEditProc);

    s_gts.hwndList = CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER, 8, 38, popW - 16,
        popH - 46, s_gts.hwndOverlay, (HMENU)(UINT_PTR)IDC_GTS_LIST, GetModuleHandleW(nullptr), nullptr);

    ListView_SetBkColor(s_gts.hwndList, RGB(37, 37, 38));
    ListView_SetTextBkColor(s_gts.hwndList, RGB(37, 37, 38));
    ListView_SetTextColor(s_gts.hwndList, RGB(204, 204, 204));
    ListView_SetExtendedListViewStyle(s_gts.hwndList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_WIDTH | LVCF_TEXT;
    lvc.cx = popW - 16 - 180 - 60;
    lvc.pszText = (LPWSTR)L"Symbol";
    ListView_InsertColumn(s_gts.hwndList, 0, &lvc);
    lvc.cx = 180;
    lvc.pszText = (LPWSTR)(workspaceMode ? L"Container / File" : L"Kind");
    ListView_InsertColumn(s_gts.hwndList, 1, &lvc);
    lvc.cx = 60;
    lvc.pszText = (LPWSTR)L"Line";
    ListView_InsertColumn(s_gts.hwndList, 2, &lvc);

    HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(s_gts.hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_gts.hwndList, WM_SETFONT, (WPARAM)hFont, TRUE);

    if (workspaceMode)
    {
        s_gts.filtered.clear();
        populateList();
    }
    else
    {
        s_gts.filtered = s_gts.allSymbols;
        populateList();
    }

    ShowWindow(s_gts.hwndOverlay, SW_SHOWNA);
    SetForegroundWindow(s_gts.hwndOverlay);
    SetFocus(s_gts.hwndEdit);
    SendMessage(s_gts.hwndEdit, EM_SETSEL, workspaceMode ? 0 : 1, -1);

    s_gts.active = true;
}

void Win32IDE::hideGoToSymbolPicker()
{
    if (!s_gts.active)
        return;

    if (s_gts.hwndOverlay)
    {
        DestroyWindow(s_gts.hwndOverlay);
        s_gts.hwndOverlay = nullptr;
        s_gts.hwndEdit = nullptr;
        s_gts.hwndList = nullptr;
    }
    s_gts.allSymbols.clear();
    s_gts.filtered.clear();
    s_gts.active = false;

    if (m_hwndEditor && IsWindow(m_hwndEditor))
        SetFocus(m_hwndEditor);
}

bool Win32IDE::isGoToSymbolPickerVisible() const
{
    return s_gts.active;
}

static LRESULT CALLBACK GoToSymbolOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(204, 204, 204));
            SetBkColor(hdc, RGB(37, 37, 38));
            static HBRUSH s_brush = CreateSolidBrush(RGB(37, 37, 38));
            return (LRESULT)s_brush;
        }

        case WM_NOTIFY:
        {
            NMHDR* pNM = reinterpret_cast<NMHDR*>(lParam);
            if (pNM && pNM->hwndFrom == s_gts.hwndList)
            {
                if (pNM->code == NM_DBLCLK || pNM->code == NM_RETURN)
                {
                    commitSymbolSelection();
                    return 0;
                }
            }
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                if (s_gts.pIDE)
                    s_gts.pIDE->hideGoToSymbolPicker();
            }
            return 0;

        case WM_CLOSE:
            if (s_gts.pIDE)
                s_gts.pIDE->hideGoToSymbolPicker();
            return 0;

        case WM_DESTROY:
            s_gts.hwndOverlay = nullptr;
            s_gts.active = false;
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK GoToSymbolEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                commitSymbolSelection();
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                if (s_gts.pIDE)
                    s_gts.pIDE->hideGoToSymbolPicker();
                return 0;
            }
            if (wParam == VK_DOWN)
            {
                int sel = ListView_GetNextItem(s_gts.hwndList, -1, LVNI_SELECTED);
                int count = ListView_GetItemCount(s_gts.hwndList);
                if (sel + 1 < count)
                {
                    ListView_SetItemState(s_gts.hwndList, sel, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_SetItemState(s_gts.hwndList, sel + 1, LVIS_SELECTED | LVIS_FOCUSED,
                                          LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(s_gts.hwndList, sel + 1, FALSE);
                }
                return 0;
            }
            if (wParam == VK_UP)
            {
                int sel = ListView_GetNextItem(s_gts.hwndList, -1, LVNI_SELECTED);
                if (sel > 0)
                {
                    ListView_SetItemState(s_gts.hwndList, sel, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_SetItemState(s_gts.hwndList, sel - 1, LVIS_SELECTED | LVIS_FOCUSED,
                                          LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(s_gts.hwndList, sel - 1, FALSE);
                }
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == '\r' || wParam == '\n')
                return 0;
            // Fall through — let the edit handle the character, then filter
            {
                LRESULT result = CallWindowProcW(s_gts.oldEditProc, hwnd, msg, wParam, lParam);
                filterSymbols();
                return result;
            }
    }

    return CallWindowProcW(s_gts.oldEditProc, hwnd, msg, wParam, lParam);
}
