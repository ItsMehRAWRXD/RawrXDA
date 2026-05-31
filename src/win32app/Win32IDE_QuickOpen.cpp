// ============================================================================
// Win32IDE_QuickOpen.cpp — Quick Open File Picker (Ctrl+P)
// ============================================================================
// VS Code-style "Quick Open" file picker. Presents a centered overlay with
// a text EDIT control that filters files from the workspace.  Dynamically
// searches the current directory and subdirectories, updating a ListBox as
// the user types. Enter→open selected file, Esc→dismiss.
//
// Architecture:
//   - WS_POPUP overlay with EDIT filter + ListBox file list
//   - Enumerates files from m_currentDirectory recursively
//   - Fuzzy substring matching on relative path
//   - Dark theme: matches IDE palette
//   - Up/Down/Enter/Esc keyboard handling
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── File-static state ───────────────────────────────────────────────────────
static struct QuickOpenState {
    HWND hwndOverlay  = nullptr;
    HWND hwndEdit     = nullptr;
    HWND hwndList     = nullptr;
    HWND hwndLabel    = nullptr;
    WNDPROC oldEditProc = nullptr;
    WNDPROC oldListProc = nullptr;
    Win32IDE* pIDE    = nullptr;
    bool active       = false;
    std::vector<std::wstring> allFiles;          // All files in workspace
    std::vector<std::wstring> filteredFiles;     // Filtered by current search
    std::string workspaceRoot;                   // Cached workspace root
} s_qo;

#define IDC_QO_EDIT  9903
#define IDC_QO_LIST  9904
#define IDC_QO_LABEL 9905

// ── Forward declarations ────────────────────────────────────────────────────
static LRESULT CALLBACK QuickOpenOverlayProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK QuickOpenEditProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK QuickOpenListProc(HWND, UINT, WPARAM, LPARAM);

// ============================================================================
// Helper: Recursive file enumeration from workspace root
// ============================================================================
static void enumerateWorkspaceFiles()
{
    s_qo.allFiles.clear();
    if (s_qo.workspaceRoot.empty()) return;

    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(s_qo.workspaceRoot))
        {
            if (entry.is_regular_file())
            {
                // Skip common large/binary directories
                std::string path = entry.path().string();
                std::string lower = path;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower.find("\\node_modules\\") != std::string::npos ||
                    lower.find("\\build\\") != std::string::npos ||
                    lower.find("\\.git\\") != std::string::npos ||
                    lower.find("\\obj\\") != std::string::npos ||
                    lower.find("\\bin\\") != std::string::npos)
                    continue;

                // Convert to wide and store relative path
                size_t prefixLen = s_qo.workspaceRoot.length();
                if (path.length() > prefixLen && path[prefixLen] == '\\')
                    prefixLen++;
                std::string relPath = path.substr(prefixLen);

                // Convert to wchar_t
                int wlen = MultiByteToWideChar(CP_UTF8, 0, relPath.c_str(), -1, nullptr, 0);
                if (wlen > 0)
                {
                    std::wstring wrelPath(wlen - 1, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, relPath.c_str(), -1, &wrelPath[0], wlen);
                    s_qo.allFiles.push_back(wrelPath);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Silently ignore enumeration errors
    }

    // Sort files for consistent ordering
    std::sort(s_qo.allFiles.begin(), s_qo.allFiles.end());
}

// ============================================================================
// Helper: Fuzzy substring match
// ============================================================================
static bool fuzzyMatch(const std::wstring& filename, const std::wstring& query)
{
    if (query.empty()) return true;

    // Simple substring match (case-insensitive)
    std::wstring lower_filename = filename;
    std::wstring lower_query = query;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::towlower);
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::towlower);

    return lower_filename.find(lower_query) != std::wstring::npos;
}

// ============================================================================
// Helper: Update filtered file list based on EDIT content
// ============================================================================
static void updateQuickOpenFilter()
{
    if (!s_qo.hwndEdit || !s_qo.hwndList) return;

    wchar_t buf[256] = {};
    GetWindowTextW(s_qo.hwndEdit, buf, _countof(buf));
    std::wstring query(buf);

    s_qo.filteredFiles.clear();
    for (const auto& file : s_qo.allFiles)
    {
        if (fuzzyMatch(file, query))
            s_qo.filteredFiles.push_back(file);
    }

    // Update ListBox
    SendMessageW(s_qo.hwndList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < s_qo.filteredFiles.size() && i < 100; ++i)
    {
        SendMessageW(s_qo.hwndList, LB_ADDSTRING, 0, (LPARAM)s_qo.filteredFiles[i].c_str());
    }

    if (!s_qo.filteredFiles.empty())
        SendMessageW(s_qo.hwndList, LB_SETCURSEL, 0, 0);
}

// ============================================================================
// Helper: Open selected file
// ============================================================================
static void commitQuickOpenSelection()
{
    if (!s_qo.hwndList || !s_qo.pIDE) return;

    INT idx = (INT)SendMessageW(s_qo.hwndList, LB_GETCURSEL, 0, 0);
    if (idx >= 0 && idx < (INT)s_qo.filteredFiles.size())
    {
        const std::wstring& relPath = s_qo.filteredFiles[idx];

        // Convert to UTF-8 for file path
        int len = WideCharToMultiByte(CP_UTF8, 0, relPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0)
        {
            std::string path(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, relPath.c_str(), -1, &path[0], len, nullptr, nullptr);

            // Construct full path
            std::string fullPath = s_qo.workspaceRoot + "\\" + path;

            // Open file using the public IDE API
            s_qo.pIDE->openFile(fullPath);
        }
    }

    // Dismiss
    s_qo.pIDE->hideQuickOpenPicker();
}

// ============================================================================
// Public: Show Quick Open Picker
// ============================================================================
void Win32IDE::showQuickOpenPicker()
{
    if (s_qo.active) return;

    s_qo.pIDE = this;
    s_qo.workspaceRoot = m_currentDirectory;

    // Enumerate files once
    enumerateWorkspaceFiles();

    RECT rcMain;
    GetWindowRect(m_hwndMain, &rcMain);

    int overlayW = 600;
    int overlayH = 400;
    int x = rcMain.left + (rcMain.right - rcMain.left - overlayW) / 2;
    int y = rcMain.top + 60;

    // Create overlay window
    s_qo.hwndOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"STATIC",
        L"Quick Open",
        WS_POPUP | WS_BORDER,
        x, y, overlayW, overlayH,
        m_hwndMain,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!s_qo.hwndOverlay) return;

    SetWindowLongPtrW(s_qo.hwndOverlay, GWLP_WNDPROC, (LONG_PTR)QuickOpenOverlayProc);

    // Create label
    s_qo.hwndLabel = CreateWindowExW(
        0, L"STATIC", L"Quick Open (type to filter, Esc to cancel)",
        WS_CHILD | WS_VISIBLE,
        10, 10, overlayW - 20, 20,
        s_qo.hwndOverlay, (HMENU)IDC_QO_LABEL, GetModuleHandleW(nullptr), nullptr
    );

    // Create EDIT
    s_qo.hwndEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 35, overlayW - 20, 25,
        s_qo.hwndOverlay, (HMENU)IDC_QO_EDIT, GetModuleHandleW(nullptr), nullptr
    );

    if (s_qo.hwndEdit)
    {
        s_qo.oldEditProc = (WNDPROC)GetWindowLongPtrW(s_qo.hwndEdit, GWLP_WNDPROC);
        SetWindowLongPtrW(s_qo.hwndEdit, GWLP_WNDPROC, (LONG_PTR)QuickOpenEditProc);
        SetFocus(s_qo.hwndEdit);
    }

    // Create ListBox
    s_qo.hwndList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        10, 65, overlayW - 20, overlayH - 85,
        s_qo.hwndOverlay, (HMENU)IDC_QO_LIST, GetModuleHandleW(nullptr), nullptr
    );

    if (s_qo.hwndList)
    {
        // Dark theme
        SendMessageW(s_qo.hwndList, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);

        s_qo.oldListProc = (WNDPROC)GetWindowLongPtrW(s_qo.hwndList, GWLP_WNDPROC);
        SetWindowLongPtrW(s_qo.hwndList, GWLP_WNDPROC, (LONG_PTR)QuickOpenListProc);

        // Set colors
        HDC hdc = GetDC(s_qo.hwndList);
        SetTextColor(hdc, RGB(204, 204, 204));
        SetBkColor(hdc, RGB(37, 37, 38));
        ReleaseDC(s_qo.hwndList, hdc);
    }

    // Populate initial list
    updateQuickOpenFilter();

    ShowWindow(s_qo.hwndOverlay, SW_SHOW);
    s_qo.active = true;
}

// ============================================================================
// Public: Hide Quick Open Picker
// ============================================================================
void Win32IDE::hideQuickOpenPicker()
{
    if (s_qo.hwndOverlay)
    {
        DestroyWindow(s_qo.hwndOverlay);
        s_qo.hwndOverlay = nullptr;
    }

    s_qo.hwndEdit = nullptr;
    s_qo.hwndList = nullptr;
    s_qo.hwndLabel = nullptr;
    s_qo.active = false;

    SetFocus(m_hwndEditor);
}

// ============================================================================
// Public: Check if Quick Open is visible
// ============================================================================
bool Win32IDE::isQuickOpenPickerVisible() const
{
    return s_qo.active;
}

// ============================================================================
// Window Procedures
// ============================================================================

static LRESULT CALLBACK QuickOpenOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                if (s_qo.pIDE) s_qo.pIDE->hideQuickOpenPicker();
                return 0;
            }
            break;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(204, 204, 204));
            SetBkColor(hdc, RGB(37, 37, 38));
            return (LRESULT)CreateSolidBrush(RGB(37, 37, 38));
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)CreateSolidBrush(RGB(37, 37, 38)));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK QuickOpenEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                if (s_qo.pIDE) s_qo.pIDE->hideQuickOpenPicker();
                return 0;
            }
            if (wParam == VK_RETURN)
            {
                commitQuickOpenSelection();
                return 0;
            }
            if (wParam == VK_DOWN)
            {
                INT cur = (INT)SendMessageW(s_qo.hwndList, LB_GETCURSEL, 0, 0);
                if (cur < (INT)s_qo.filteredFiles.size() - 1)
                    SendMessageW(s_qo.hwndList, LB_SETCURSEL, cur + 1, 0);
                return 0;
            }
            if (wParam == VK_UP)
            {
                INT cur = (INT)SendMessageW(s_qo.hwndList, LB_GETCURSEL, 0, 0);
                if (cur > 0)
                    SendMessageW(s_qo.hwndList, LB_SETCURSEL, cur - 1, 0);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == 27) return 0;  // Suppress Esc bell
            break;
    }

    // Call original EDIT proc
    if (s_qo.oldEditProc)
    {
        LRESULT res = CallWindowProcW(s_qo.oldEditProc, hwnd, msg, wParam, lParam);

        // Update filter after every keystroke
        if (msg == WM_CHAR || msg == WM_KEYDOWN)
            updateQuickOpenFilter();

        return res;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK QuickOpenListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                commitQuickOpenSelection();
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                if (s_qo.pIDE) s_qo.pIDE->hideQuickOpenPicker();
                return 0;
            }
            break;

        case WM_LBUTTONDBLCLK:
            commitQuickOpenSelection();
            return 0;
    }

    return CallWindowProcW(s_qo.oldListProc, hwnd, msg, wParam, lParam);
}
