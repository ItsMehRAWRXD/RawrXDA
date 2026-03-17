// digestion_gui_widget.cpp — Pure C++20/Win32 (zero Qt)
#include "digestion_gui_widget.h"
#include <shlobj.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

// ============================================================================
// Construction / Destruction
// ============================================================================

DigestionGuiWidget::DigestionGuiWidget(HWND hwndParent)
    : m_hwndParent(hwndParent)
{
    m_digester = new DigestionReverseEngineeringSystem(nullptr);
}

DigestionGuiWidget::~DigestionGuiWidget()
{
    if (m_hDlg && IsWindow(m_hDlg)) DestroyWindow(m_hDlg);
    delete m_digester;
}

// ============================================================================
// Public API
// ============================================================================

void DigestionGuiWidget::show()
{
    if (m_hDlg && IsWindow(m_hDlg)) {
        SetForegroundWindow(m_hDlg);
        return;
    }

    // Build the dialog as a popup window (no .rc needed)
    const int W = 700, H = 520;
    RECT rc;
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 200; rc.top = 150; }
    int x = rc.left + 80, y = rc.top + 60;

    m_hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
        L"STATIC", L"Digestion Pipeline",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, W, H, m_hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!m_hDlg) return;
    SetWindowLongPtrW(m_hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    onInitDialog(m_hDlg);
    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
}

void DigestionGuiWidget::setRootDirectory(const std::string& path)
{
    m_rootDir = path;
    if (m_hwndPathEdit) SetWindowTextA(m_hwndPathEdit, path.c_str());
}

// ============================================================================
// Dialog Init — create child controls
// ============================================================================

void DigestionGuiWidget::onInitDialog(HWND hDlg)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int y = 12;

    // Row 1: path + browse
    CreateWindowExW(0, L"STATIC", L"Root Directory:",
        WS_CHILD | WS_VISIBLE, 12, y + 2, 100, 20, hDlg, nullptr, hInst, nullptr);

    m_hwndPathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        m_rootDir.empty() ? L"" : std::wstring(m_rootDir.begin(), m_rootDir.end()).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        116, y, 440, 22, hDlg, reinterpret_cast<HMENU>(IDC_DIG_PATH), hInst, nullptr);

    m_hwndBrowseBtn = CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        564, y, 110, 24, hDlg, reinterpret_cast<HMENU>(IDC_DIG_BROWSE), hInst, nullptr);
    y += 34;

    // Row 2: options checkboxes
    m_hwndApplyFixes = CreateWindowExW(0, L"BUTTON", L"Apply Fixes",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        12, y, 120, 20, hDlg, reinterpret_cast<HMENU>(IDC_DIG_APPLY), hInst, nullptr);

    m_hwndGitMode = CreateWindowExW(0, L"BUTTON", L"Git Mode Only",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        140, y, 120, 20, hDlg, reinterpret_cast<HMENU>(IDC_DIG_GIT), hInst, nullptr);

    m_hwndIncremental = CreateWindowExW(0, L"BUTTON", L"Incremental (Cached)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        268, y, 160, 20, hDlg, reinterpret_cast<HMENU>(IDC_DIG_INCR), hInst, nullptr);
    SendMessage(m_hwndIncremental, BM_SETCHECK, BST_CHECKED, 0);
    y += 30;

    // Row 3: progress bar
    m_hwndProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        12, y, 660, 18, hDlg, reinterpret_cast<HMENU>(IDC_DIG_PROG), hInst, nullptr);
    y += 26;

    // Row 4: results list-view
    m_hwndResultsLV = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        12, y, 660, 320, hDlg, reinterpret_cast<HMENU>(IDC_DIG_LIST), hInst, nullptr);
    ListView_SetExtendedListViewStyle(m_hwndResultsLV,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Columns
    auto addCol = [&](int idx, const wchar_t* title, int cx) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.iSubItem = idx;
        col.pszText = const_cast<LPWSTR>(title);
        col.cx = cx;
        ListView_InsertColumn(m_hwndResultsLV, idx, &col);
    };
    addCol(0, L"File",     300);
    addCol(1, L"Language", 100);
    addCol(2, L"Stubs",     80);
    addCol(3, L"Status",   160);
    y += 330;

    // Row 5: start / stop buttons
    m_hwndStartBtn = CreateWindowExW(0, L"BUTTON", L"Start Digestion",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        12, y, 140, 28, hDlg, reinterpret_cast<HMENU>(IDC_DIG_START), hInst, nullptr);

    m_hwndStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        160, y, 90, 28, hDlg, reinterpret_cast<HMENU>(IDC_DIG_STOP), hInst, nullptr);

    // Subclass to handle button clicks via our own WndProc
    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DialogProc));
}

// ============================================================================
// Window Procedure
// ============================================================================

INT_PTR CALLBACK DigestionGuiWidget::DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<DigestionGuiWidget*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    if (self) return self->handleMessage(hDlg, msg, wParam, lParam);
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

INT_PTR DigestionGuiWidget::handleMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_DIG_BROWSE: browseDirectory(); return TRUE;
        case IDC_DIG_START:  startDigestion();  return TRUE;
        case IDC_DIG_STOP:   stopDigestion();   return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hDlg);
        m_hDlg = nullptr;
        return TRUE;
    case WM_DESTROY:
        m_hDlg = nullptr;
        return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ============================================================================
// Actions
// ============================================================================

void DigestionGuiWidget::browseDirectory()
{
    wchar_t path[MAX_PATH] = {};
    BROWSEINFOW bi{};
    bi.hwndOwner = m_hDlg;
    bi.lpszTitle = L"Select Source Directory";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        SHGetPathFromIDListW(pidl, path);
        CoTaskMemFree(pidl);
        char utf8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
        m_rootDir = utf8;
        SetWindowTextA(m_hwndPathEdit, utf8);
    }
}

void DigestionGuiWidget::startDigestion()
{
    char buf[MAX_PATH * 3];
    GetWindowTextA(m_hwndPathEdit, buf, sizeof(buf));
    m_rootDir = buf;
    if (m_rootDir.empty()) return;

    ListView_DeleteAllItems(m_hwndResultsLV);
    EnableWindow(m_hwndStartBtn, FALSE);
    EnableWindow(m_hwndStopBtn,  TRUE);

    DigestionConfig config;
    config.applyExtensions = (SendMessage(m_hwndApplyFixes, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config.useGitMode      = (SendMessage(m_hwndGitMode,    BM_GETCHECK, 0, 0) == BST_CHECKED);
    config.incremental     = (SendMessage(m_hwndIncremental, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config.chunkSize       = 100;

    m_digester->runFullDigestionPipeline(m_rootDir, config);
}

void DigestionGuiWidget::stopDigestion()
{
    m_digester->stop();
    EnableWindow(m_hwndStopBtn,  FALSE);
    EnableWindow(m_hwndStartBtn, TRUE);
}

// ============================================================================
// Progress Callbacks
// ============================================================================

void DigestionGuiWidget::onProgress(int done, int total, int /*stubs*/, int /*percent*/)
{
    if (!m_hwndProgress) return;
    SendMessage(m_hwndProgress, PBM_SETRANGE32, 0, total);
    SendMessage(m_hwndProgress, PBM_SETPOS, done, 0);
}

void DigestionGuiWidget::onFileScanned(const std::string& path, const std::string& lang, int stubs)
{
    if (!m_hwndResultsLV) return;
    int row = ListView_GetItemCount(m_hwndResultsLV);

    std::wstring wPath(path.begin(), path.end());
    std::wstring wLang(lang.begin(), lang.end());
    std::wstring wStubs = std::to_wstring(stubs);

    LVITEMW item{};
    item.mask     = LVIF_TEXT;
    item.iItem    = row;
    item.iSubItem = 0;
    item.pszText  = const_cast<LPWSTR>(wPath.c_str());
    ListView_InsertItem(m_hwndResultsLV, &item);
    ListView_SetItemText(m_hwndResultsLV, row, 1, const_cast<LPWSTR>(wLang.c_str()));
    ListView_SetItemText(m_hwndResultsLV, row, 2, const_cast<LPWSTR>(wStubs.c_str()));
    ListView_SetItemText(m_hwndResultsLV, row, 3, stubs > 0 ? const_cast<LPWSTR>(L"Stubs Found") : const_cast<LPWSTR>(L"OK"));
}

void DigestionGuiWidget::onFinished(int totalFiles, int totalStubs, int64_t elapsedMs)
{
    EnableWindow(m_hwndStartBtn, TRUE);
    EnableWindow(m_hwndStopBtn,  FALSE);

    wchar_t msg[256];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"Digestion Complete\n\nScanned %d files in %.1f seconds\nFound %d stubs",
        totalFiles, elapsedMs / 1000.0, totalStubs);
    MessageBoxW(m_hDlg, msg, L"Digestion Pipeline", MB_OK | MB_ICONINFORMATION);
}

