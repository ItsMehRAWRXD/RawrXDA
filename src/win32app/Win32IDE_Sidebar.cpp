// Win32IDE_Sidebar.cpp - Primary Sidebar Implementation
// Implements VS Code-style Activity Bar and Sidebar with 5 views:
// Explorer, Search, Source Control, Run & Debug, Extensions

#include "../../include/quickjs_extension_host.h"
#include "../core/enterprise_license.h"
#include "VSIXInstaller.hpp"
#include "Win32IDE.h"
#include "Win32IDE_IELabels.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <shellapi.h>
#include <shlwapi.h>
#include <sstream>

#include "IDELogger.h"
#include "vsix_loader.h"
#include <nlohmann/json.hpp>

// Define GET_X_LPARAM and GET_Y_LPARAM if not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

namespace
{
// UTF-8 to UTF-16 for Unicode Win32 APIs (C++20, no Qt)
static std::wstring utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) == 0)
        return {};
    return out;
}
}  // namespace

// Activity Bar constants
constexpr int ACTIVITY_BAR_WIDTH = 48;
constexpr int SIDEBAR_DEFAULT_WIDTH = 250;
constexpr int ACTIVITY_ICON_SIZE = 32;
constexpr int ACTIVITY_BUTTON_HEIGHT = 48;

// Control IDs
constexpr int IDC_ACTIVITY_EXPLORER = 6001;
constexpr int IDC_ACTIVITY_SEARCH = 6002;
constexpr int IDC_ACTIVITY_SCM = 6003;
constexpr int IDC_ACTIVITY_DEBUG = 6004;
constexpr int IDC_ACTIVITY_EXTENSIONS = 6005;
constexpr int IDC_ACTIVITY_RECOVERY = 6006;
constexpr int IDC_ACTIVITY_CHAT = 6007;  // AI Chat / Agent panel (secondary sidebar)

constexpr int IDC_EXPLORER_TREE = 6010;
constexpr int IDC_EXPLORER_NEW_FILE = 6011;
constexpr int IDC_EXPLORER_NEW_FOLDER = 6012;
constexpr int IDC_EXPLORER_REFRESH = 6013;
constexpr int IDC_EXPLORER_COLLAPSE = 6014;

constexpr int IDC_SEARCH_INPUT = 6020;
constexpr int IDC_SEARCH_BUTTON = 6021;
constexpr int IDC_SEARCH_RESULTS = 6022;
constexpr int IDC_SEARCH_REGEX = 6023;
constexpr int IDC_SEARCH_CASE = 6024;
constexpr int IDC_SEARCH_WHOLE_WORD = 6025;
constexpr int IDC_SEARCH_INCLUDE = 6026;
constexpr int IDC_SEARCH_EXCLUDE = 6027;

constexpr int IDC_SCM_FILE_LIST = 6030;
constexpr int IDC_SCM_STAGE = 6031;
constexpr int IDC_SCM_UNSTAGE = 6032;
constexpr int IDC_SCM_COMMIT = 6033;
constexpr int IDC_SCM_SYNC = 6034;
constexpr int IDC_SCM_MESSAGE = 6035;
constexpr int IDC_SCM_BRANCH = 6036;

constexpr int IDC_DEBUG_CONFIGS = 6040;
constexpr int IDC_DEBUG_START = 6041;
constexpr int IDC_DEBUG_STOP = 6042;
constexpr int IDC_DEBUG_VARIABLES = 6043;
constexpr int IDC_DEBUG_CALLSTACK = 6044;
constexpr int IDC_DEBUG_CONSOLE = 6045;

constexpr int IDC_EXT_SEARCH = 6050;
constexpr int IDC_EXT_LIST = 6051;
constexpr int IDC_EXT_DETAILS = 6052;
constexpr int IDC_EXT_INSTALL = 6053;
constexpr int IDC_EXT_UNINSTALL = 6054;
constexpr int IDC_EXT_INSTALL_VSIX = 6055;

// File Explorer IDs are defined centrally in Win32IDE_Commands.h

WNDPROC Win32IDE::s_sidebarContentOldProc = nullptr;

// Maps ListView index to extension ID (order may differ from m_extensions during search)
static std::vector<std::string> s_extensionDisplayIds;

// ============================================================================
// Activity Bar Implementation
// ============================================================================

// ESP:m_hwndActivityBar — Activity Bar (Files, Search, SCM, Debug, Extensions, Recovery)
void Win32IDE::createActivityBar(HWND hwndParent)
{
    m_hwndActivityBar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, ACTIVITY_BAR_WIDTH,
                                        600, hwndParent, nullptr, m_hInstance, nullptr);

    SetWindowLongPtrA(m_hwndActivityBar, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtrA(m_hwndActivityBar, GWLP_WNDPROC, (LONG_PTR)ActivityBarProc);

    // Create activity buttons (icon buttons for each view)
    int y = 10;
    const struct
    {
        int id;
        const char* text;
    } buttons[] = {{IDC_ACTIVITY_EXPLORER, "Files"},  {IDC_ACTIVITY_SEARCH, "Search"},
                   {IDC_ACTIVITY_SCM, "Source"},      {IDC_ACTIVITY_DEBUG, "Debug"},
                   {IDC_ACTIVITY_EXTENSIONS, "Exts"}, {IDC_ACTIVITY_RECOVERY, "Recov"},
                   {IDC_ACTIVITY_CHAT, "Chat"}};

    for (const auto& btn : buttons)
    {
        HWND hwndBtn = CreateWindowExA(0, "BUTTON", btn.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 4,
                                       y, 40, 40, m_hwndActivityBar, (HMENU)(INT_PTR)btn.id, m_hInstance, nullptr);
        y += 48;
    }

    appendToOutput("Activity Bar created with 7 views (Files, Search, Source, Debug, Exts, Recov, Chat)\n", "Output",
                   OutputSeverity::Info);
}

LRESULT CALLBACK Win32IDE::ActivityBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_COMMAND:
            if (pThis)
            {
                int id = LOWORD(wParam);
                switch (id)
                {
                    case IDC_ACTIVITY_EXPLORER:
                        pThis->setSidebarView(SidebarView::Explorer);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_SEARCH:
                        pThis->setSidebarView(SidebarView::Search);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_SCM:
                        pThis->setSidebarView(SidebarView::SourceControl);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_DEBUG:
                        pThis->setSidebarView(SidebarView::RunDebug);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_EXTENSIONS:
                        pThis->setSidebarView(SidebarView::Extensions);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_RECOVERY:
                        pThis->setSidebarView(SidebarView::DiskRecovery);
                        if (!pThis->m_sidebarVisible)
                            pThis->toggleSidebar();
                        break;
                    case IDC_ACTIVITY_CHAT:
                        pThis->toggleSecondarySidebar();
                        break;
                }
            }
            return 0;

        case WM_DRAWITEM:
        {
            // Draw activity bar buttons with visible labels (fixes empty square boxes)
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (!dis || dis->CtlType != ODT_BUTTON)
                break;
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            UINT state = dis->itemState;
            int id = dis->CtlID;
            HWND hBtn = dis->hwndItem;
            COLORREF bgColor = RGB(51, 51, 51);
            if (state & ODS_SELECTED)
                bgColor = RGB(70, 70, 70);
            else if (state & ODS_HOTLIGHT)
                bgColor = RGB(62, 62, 62);
            if (pThis && pThis->m_currentSidebarView != SidebarView::None)
            {
                if ((id == IDC_ACTIVITY_EXPLORER && pThis->m_currentSidebarView == SidebarView::Explorer) ||
                    (id == IDC_ACTIVITY_SEARCH && pThis->m_currentSidebarView == SidebarView::Search) ||
                    (id == IDC_ACTIVITY_SCM && pThis->m_currentSidebarView == SidebarView::SourceControl) ||
                    (id == IDC_ACTIVITY_DEBUG && pThis->m_currentSidebarView == SidebarView::RunDebug) ||
                    (id == IDC_ACTIVITY_EXTENSIONS && pThis->m_currentSidebarView == SidebarView::Extensions) ||
                    (id == IDC_ACTIVITY_RECOVERY && pThis->m_currentSidebarView == SidebarView::DiskRecovery))
                    bgColor = RGB(0, 122, 204);
            }
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            if (pThis && (id == IDC_ACTIVITY_EXPLORER || id == IDC_ACTIVITY_SEARCH || id == IDC_ACTIVITY_SCM ||
                          id == IDC_ACTIVITY_DEBUG || id == IDC_ACTIVITY_EXTENSIONS || id == IDC_ACTIVITY_RECOVERY ||
                          id == IDC_ACTIVITY_CHAT))
            {
                RECT indR = {rc.left, rc.top, rc.left + 3, rc.bottom};
                if (bgColor == RGB(0, 122, 204))
                {
                    HBRUSH hInd = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(hdc, &indR, hInd);
                    DeleteObject(hInd);
                }
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(220, 220, 220));
            char label[32] = {};
            if (hBtn && GetWindowTextA(hBtn, label, sizeof(label)) > 0)
            {
                DrawTextA(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            return TRUE;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Dark background for activity bar
            HBRUSH hBrush = CreateSolidBrush(RGB(51, 51, 51));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Primary Sidebar Container — ESP:m_hwndSidebar, m_hwndSidebarContent
// ============================================================================

constexpr int SIDEBAR_TITLE_HEIGHT = 28;

void Win32IDE::createPrimarySidebar(HWND hwndParent)
{
    m_hwndSidebar =
        CreateWindowExA(0, "STATIC", RAWRXD_IDE_LABEL_SIDEBAR, WS_CHILD | WS_VISIBLE | WS_BORDER, ACTIVITY_BAR_WIDTH, 0,
                        SIDEBAR_DEFAULT_WIDTH, 600, hwndParent, nullptr, m_hInstance, nullptr);

    // Visible title bar so the pane is clearly named (e.g. "File Explorer", "Search")
    m_hwndSidebarTitle =
        CreateWindowExA(0, "STATIC", "File Explorer", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, SIDEBAR_DEFAULT_WIDTH,
                        SIDEBAR_TITLE_HEIGHT, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    if (m_hwndSidebarTitle)
    {
        SetWindowLongPtrA(m_hwndSidebarTitle, GWLP_USERDATA, (LONG_PTR)this);
        HFONT hFont = CreateFontA(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        if (hFont)
            SendMessage(m_hwndSidebarTitle, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndSidebarContent =
        CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, SIDEBAR_TITLE_HEIGHT, SIDEBAR_DEFAULT_WIDTH,
                        600 - SIDEBAR_TITLE_HEIGHT, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    SetWindowLongPtrA(m_hwndSidebarContent, GWLP_USERDATA, (LONG_PTR)this);
    s_sidebarContentOldProc =
        (WNDPROC)SetWindowLongPtrA(m_hwndSidebarContent, GWLP_WNDPROC, (LONG_PTR)Win32IDE::SidebarContentProc);

    SetWindowLongPtrA(m_hwndSidebar, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtrA(m_hwndSidebar, GWLP_WNDPROC, (LONG_PTR)SidebarProc);

    m_sidebarVisible = true;
    m_sidebarWidth = SIDEBAR_DEFAULT_WIDTH;
    m_currentSidebarView = SidebarView::None;

    // Create all views (hidden initially)
    createExplorerView(m_hwndSidebarContent);
    createSearchView(m_hwndSidebarContent);
    createSourceControlView(m_hwndSidebarContent);
    createRunDebugView(m_hwndSidebarContent);
    createExtensionsView(m_hwndSidebarContent);
    createDiskRecoveryView(m_hwndSidebarContent);

    // Default to Explorer view, but keep startup responsive by deferring the
    // initial directory population until after the window is visible.
    m_currentSidebarView = SidebarView::Explorer;
    if (m_hwndSidebarTitle)
        SetWindowTextA(m_hwndSidebarTitle, "File Explorer");
    if (m_hwndExplorerTree)
        ShowWindow(m_hwndExplorerTree, SW_SHOW);
    if (m_hwndExplorerToolbar)
        ShowWindow(m_hwndExplorerToolbar, SW_SHOW);

    appendToOutput("Primary Sidebar initialized\n", "Output", OutputSeverity::Info);
    appendToOutput("[System] File Explorer: View > File Explorer (Ctrl+Shift+E) or Activity Bar > Files\n", "Output",
                   OutputSeverity::Info);
    appendToOutput("[System] Agent Chat: View > AI Chat (Ctrl+Alt+B) or View > Agent Chat or Activity Bar > Chat\n",
                   "Output", OutputSeverity::Info);
    {
        bool unlocked = RawrXD::EnterpriseLicense::is800BUnlocked();
        const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
        std::string lic = std::string("[License] Edition: ") + (ed ? ed : "Unknown") +
                          " | 800B: " + (unlocked ? "UNLOCKED (Enterprise)" : "locked (requires Enterprise license)") +
                          " | Tools > License Creator | Tools > Feature Registry for V2 manifest\n";
        appendToOutput(lic, "Output", OutputSeverity::Info);
    }
    appendToOutput("[UX] View > File Explorer (Ctrl+Shift+E) | View > AI Chat (Ctrl+Shift+C) | Tools > License Creator "
                   "| Tools > Feature Registry\n",
                   "Output", OutputSeverity::Info);
}

LRESULT CALLBACK Win32IDE::SidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_CTLCOLORSTATIC:
        {
            // Make sidebar title bar readable: dark background, light text
            if (pThis && (HWND)lParam == pThis->m_hwndSidebarTitle)
            {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, RGB(230, 230, 230));
                SetBkColor(hdc, RGB(45, 45, 48));
                static HBRUSH s_titleBrush = nullptr;
                if (!s_titleBrush)
                    s_titleBrush = CreateSolidBrush(RGB(45, 45, 48));
                return (LRESULT)s_titleBrush;
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Light gray background
            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis)
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                pThis->resizeSidebar(width, height);
            }
            return 0;
        }

        case WM_NOTIFY:
        {
            if (pThis)
            {
                NMHDR* pnmh = (NMHDR*)lParam;
                if (pnmh->code == TVN_DELETEITEM && pnmh->idFrom == IDC_FILE_EXPLORER)
                {
                    NMTREEVIEWA* pnmtv = (NMTREEVIEWA*)lParam;
                    if (pnmtv->itemOld.lParam)
                        delete[] reinterpret_cast<char*>(pnmtv->itemOld.lParam);
                    return 0;
                }
            }
            break;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::SidebarContentProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!pThis)
        return CallWindowProcA(Win32IDE::s_sidebarContentOldProc, hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            // Explorer toolbar
            if (id == IDC_EXPLORER_NEW_FILE)
            {
                pThis->newFileInExplorer();
                return 0;
            }
            if (id == IDC_EXPLORER_NEW_FOLDER)
            {
                pThis->newFolderInExplorer();
                return 0;
            }
            if (id == IDC_EXPLORER_REFRESH)
            {
                pThis->refreshFileTree();
                return 0;
            }
            if (id == IDC_EXPLORER_COLLAPSE)
            {
                pThis->refreshFileTree();
                return 0;
            }
            // Extensions toolbar
            if (id == IDC_EXT_INSTALL_VSIX)
            {
                pThis->installFromVSIXFile();
                return 0;
            }
            if (id == IDC_EXT_INSTALL)
            {
                int sel = ListView_GetNextItem(pThis->m_hwndExtensionsList, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < (int)s_extensionDisplayIds.size())
                {
                    pThis->installExtension(s_extensionDisplayIds[sel]);
                }
                else
                {
                    pThis->appendToOutput("Select an extension to install\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_EXT_UNINSTALL)
            {
                int sel = ListView_GetNextItem(pThis->m_hwndExtensionsList, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < (int)s_extensionDisplayIds.size())
                {
                    pThis->uninstallExtension(s_extensionDisplayIds[sel]);
                }
                else
                {
                    pThis->appendToOutput("Select an extension to uninstall\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_EXT_DETAILS)
            {
                int sel = ListView_GetNextItem(pThis->m_hwndExtensionsList, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < (int)s_extensionDisplayIds.size())
                {
                    pThis->showExtensionDetails(s_extensionDisplayIds[sel]);
                }
                else
                {
                    pThis->appendToOutput("Select an extension for details\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            // Extension search EN_CHANGE
            if (id == IDC_EXT_SEARCH && code == EN_CHANGE)
            {
                char buf[256] = {};
                GetWindowTextA((HWND)lParam, buf, sizeof(buf));
                pThis->searchExtensions(buf);
                return 0;
            }
            break;
        }
        case WM_NOTIFY:
        {
            if (pThis && pThis->m_hwndExtensionsList)
            {
                NMHDR* pnmh = (NMHDR*)lParam;
                if (pnmh->hwndFrom == pThis->m_hwndExtensionsList && pnmh->code == NM_DBLCLK)
                {
                    int sel = ListView_GetNextItem(pThis->m_hwndExtensionsList, -1, LVNI_SELECTED);
                    if (sel >= 0 && sel < (int)s_extensionDisplayIds.size())
                    {
                        pThis->installExtension(s_extensionDisplayIds[sel]);
                    }
                    return 0;
                }
            }
            break;
        }
    }
    return CallWindowProcA(Win32IDE::s_sidebarContentOldProc, hwnd, uMsg, wParam, lParam);
}

void Win32IDE::toggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    ShowWindow(m_hwndSidebar, m_sidebarVisible ? SW_SHOW : SW_HIDE);

    // Trigger layout recalculation
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);

    appendToOutput(m_sidebarVisible ? "Sidebar shown (Ctrl+B)\n" : "Sidebar hidden (Ctrl+B)\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::toggleSecondarySidebar()
{
    m_secondarySidebarVisible = !m_secondarySidebarVisible;

    if (m_hwndSecondarySidebar)
    {
        ShowWindow(m_hwndSecondarySidebar, m_secondarySidebarVisible ? SW_SHOW : SW_HIDE);
    }

    // Trigger layout recalculation
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);

    appendToOutput(m_secondarySidebarVisible ? "AI Chat Panel shown\n" : "AI Chat Panel hidden\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::setSidebarView(SidebarView view)
{
    if (m_currentSidebarView == view)
        return;

    // Hide all views
    ShowWindow(m_hwndExplorerTree, SW_HIDE);
    ShowWindow(m_hwndExplorerToolbar, SW_HIDE);
    ShowWindow(m_hwndSearchInput, SW_HIDE);
    ShowWindow(m_hwndSearchResults, SW_HIDE);
    ShowWindow(m_hwndSearchOptions, SW_HIDE);
    ShowWindow(m_hwndSCMFileList, SW_HIDE);
    ShowWindow(m_hwndSCMToolbar, SW_HIDE);
    ShowWindow(m_hwndSCMMessageBox, SW_HIDE);
    ShowWindow(m_hwndDebugConfigs, SW_HIDE);
    ShowWindow(m_hwndDebugToolbar, SW_HIDE);
    ShowWindow(m_hwndExtensionsList, SW_HIDE);
    ShowWindow(m_hwndExtensionSearch, SW_HIDE);
    // Recovery view controls
    if (m_hwndRecoveryTitle)
        ShowWindow(m_hwndRecoveryTitle, SW_HIDE);
    if (m_hwndRecoveryDriveList)
        ShowWindow(m_hwndRecoveryDriveList, SW_HIDE);
    if (m_hwndRecoveryOutPath)
        ShowWindow(m_hwndRecoveryOutPath, SW_HIDE);
    if (m_hwndRecoveryStatus)
        ShowWindow(m_hwndRecoveryStatus, SW_HIDE);
    if (m_hwndRecoveryProgress)
        ShowWindow(m_hwndRecoveryProgress, SW_HIDE);
    if (m_hwndRecoveryLog)
        ShowWindow(m_hwndRecoveryLog, SW_HIDE);
    // Also hide Extensions view Install button (IDC_EXT_INSTALL_VSIX)
    if (HWND hInstall = GetDlgItem(m_hwndSidebarContent, IDC_EXT_INSTALL_VSIX))
        ShowWindow(hInstall, SW_HIDE);
    // Hide Disk Recovery buttons
    EnumChildWindows(
        m_hwndSidebarContent,
        [](HWND hwnd, LPARAM) -> BOOL
        {
            int id = GetDlgCtrlID(hwnd);
            if (id >= 10301 && id <= 10312)
                ShowWindow(hwnd, SW_HIDE);
            return TRUE;
        },
        0);

    m_currentSidebarView = view;

    // Update visible sidebar title so the pane is clearly named
    const char* titleText = "Explorer";
    const char* viewName = "explorer";
    switch (view)
    {
        case SidebarView::Explorer:
            titleText = "File Explorer";
            viewName = "explorer";
            break;
        case SidebarView::Search:
            titleText = "Search";
            viewName = "search";
            break;
        case SidebarView::SourceControl:
            titleText = "Source Control";
            viewName = "source";
            break;
        case SidebarView::RunDebug:
            titleText = "Run and Debug";
            viewName = "debug";
            break;
        case SidebarView::Extensions:
            titleText = "Extensions";
            viewName = "extensions";
            break;
        case SidebarView::DiskRecovery:
            titleText = "Disk Recovery";
            viewName = "recovery";
            break;
        default:
            break;
    }
    if (m_hwndSidebarTitle)
        SetWindowTextA(m_hwndSidebarTitle, titleText);
    if (m_hwndActivityBar)
        InvalidateRect(m_hwndActivityBar, nullptr, TRUE);

    // Status + telemetry wiring (dashboard + backend)
    std::string featureName = std::string("sidebar.view.") + viewName;
    telemetryTrack(featureName.c_str(), 1.0);
    std::string payload = std::string("{\"view\":\"") + viewName + "\"}";
    telemetryDashboardTrack("sidebar.view", "ui", payload.c_str(), 0.0);
    if (m_hwndStatusBar)
    {
        std::wstring status = utf8ToWide(std::string("Sidebar: ") + viewName);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 1, (LPARAM)status.c_str());
    }

    // Show selected view
    switch (view)
    {
        case SidebarView::Explorer:
            ShowWindow(m_hwndExplorerTree, SW_SHOW);
            ShowWindow(m_hwndExplorerToolbar, SW_SHOW);
            refreshFileTree();
            appendToOutput("Explorer view activated\n", "Output", OutputSeverity::Info);
            break;

        case SidebarView::Search:
            ShowWindow(m_hwndSearchInput, SW_SHOW);
            ShowWindow(m_hwndSearchResults, SW_SHOW);
            ShowWindow(m_hwndSearchOptions, SW_SHOW);
            SetFocus(m_hwndSearchInput);
            appendToOutput("Search view activated\n", "Output", OutputSeverity::Info);
            break;

        case SidebarView::SourceControl:
            ShowWindow(m_hwndSCMFileList, SW_SHOW);
            ShowWindow(m_hwndSCMToolbar, SW_SHOW);
            ShowWindow(m_hwndSCMMessageBox, SW_SHOW);
            refreshSourceControlView();
            appendToOutput("Source Control view activated\n", "Output", OutputSeverity::Info);
            break;

        case SidebarView::RunDebug:
            ShowWindow(m_hwndDebugConfigs, SW_SHOW);
            ShowWindow(m_hwndDebugToolbar, SW_SHOW);
            appendToOutput("Run and Debug view activated\n", "Output", OutputSeverity::Info);
            break;

        case SidebarView::Extensions:
            ShowWindow(m_hwndExtensionsList, SW_SHOW);
            ShowWindow(m_hwndExtensionSearch, SW_SHOW);
            if (HWND hInstall = GetDlgItem(m_hwndSidebarContent, IDC_EXT_INSTALL_VSIX))
                ShowWindow(hInstall, SW_SHOW);
            loadInstalledExtensions();
            appendToOutput("Extensions view activated\n", "Output", OutputSeverity::Info);
            break;

        case SidebarView::DiskRecovery:
            if (m_hwndRecoveryTitle)
                ShowWindow(m_hwndRecoveryTitle, SW_SHOW);
            if (m_hwndRecoveryDriveList)
                ShowWindow(m_hwndRecoveryDriveList, SW_SHOW);
            if (m_hwndRecoveryOutPath)
                ShowWindow(m_hwndRecoveryOutPath, SW_SHOW);
            if (m_hwndRecoveryStatus)
                ShowWindow(m_hwndRecoveryStatus, SW_SHOW);
            if (m_hwndRecoveryProgress)
                ShowWindow(m_hwndRecoveryProgress, SW_SHOW);
            if (m_hwndRecoveryLog)
                ShowWindow(m_hwndRecoveryLog, SW_SHOW);
            // Show all recovery buttons/controls
            EnumChildWindows(
                m_hwndSidebarContent,
                [](HWND hwnd, LPARAM) -> BOOL
                {
                    int id = GetDlgCtrlID(hwnd);
                    if (id >= 10301 && id <= 10312)
                        ShowWindow(hwnd, SW_SHOW);
                    return TRUE;
                },
                0);
            appendToOutput("Disk Recovery view activated\n", "Output", OutputSeverity::Info);
            break;

        default:
            break;
    }

    updateSidebarContent();
}

void Win32IDE::updateSidebarContent()
{
    // Refresh current view's content
    switch (m_currentSidebarView)
    {
        case SidebarView::Explorer:
            refreshFileTree();
            break;
        case SidebarView::Search:
            // Search results are updated on demand
            break;
        case SidebarView::SourceControl:
            refreshSourceControlView();
            break;
        case SidebarView::RunDebug:
            updateDebugVariables();
            break;
        case SidebarView::Extensions:
            loadInstalledExtensions();
            break;
        case SidebarView::DiskRecovery:
            // Recovery panel is self-updating via timer
            break;
        default:
            break;
    }
}

void Win32IDE::resizeSidebar(int width, int height)
{
    if (!m_hwndSidebarContent)
        return;

    // Title bar at top; content pane below
    const int titleH = SIDEBAR_TITLE_HEIGHT;
    const int contentH = (height > titleH) ? (height - titleH) : 0;

    if (m_hwndSidebarTitle)
        MoveWindow(m_hwndSidebarTitle, 0, 0, width, titleH, TRUE);
    MoveWindow(m_hwndSidebarContent, 0, titleH, width, contentH, TRUE);

    // Resize active view controls within content area
    if (m_hwndExplorerTree && m_currentSidebarView == SidebarView::Explorer)
    {
        MoveWindow(m_hwndExplorerToolbar, 0, 0, width, 30, TRUE);
        MoveWindow(m_hwndExplorerTree, 0, 30, width, contentH - 30, TRUE);
    }
    else if (m_hwndSearchInput && m_currentSidebarView == SidebarView::Search)
    {
        MoveWindow(m_hwndSearchInput, 5, 10, width - 10, 25, TRUE);
        MoveWindow(m_hwndSearchOptions, 5, 40, width - 10, 80, TRUE);
        MoveWindow(m_hwndSearchResults, 5, 125, width - 10, contentH - 130, TRUE);
    }
    else if (m_hwndSCMFileList && m_currentSidebarView == SidebarView::SourceControl)
    {
        MoveWindow(m_hwndSCMToolbar, 0, 0, width, 35, TRUE);
        MoveWindow(m_hwndSCMMessageBox, 5, 40, width - 10, 60, TRUE);
        MoveWindow(m_hwndSCMFileList, 5, 105, width - 10, contentH - 110, TRUE);
    }
    else if (m_hwndDebugConfigs && m_currentSidebarView == SidebarView::RunDebug)
    {
        MoveWindow(m_hwndDebugToolbar, 0, 0, width, 35, TRUE);
        MoveWindow(m_hwndDebugConfigs, 5, 40, width - 10, 100, TRUE);
        MoveWindow(m_hwndDebugVariables, 5, 145, width - 10, contentH - 150, TRUE);
    }
    else if (m_hwndExtensionsList && m_currentSidebarView == SidebarView::Extensions)
    {
        MoveWindow(m_hwndExtensionSearch, 5, 10, width - 10, 25, TRUE);
        MoveWindow(m_hwndExtensionsList, 5, 68, width - 10, contentH - 73, TRUE);
    }
}

// ============================================================================
// Explorer View Implementation
// ============================================================================

void Win32IDE::createExplorerView(HWND hwndParent)
{
    appendToOutput("createExplorerView() called\n", "Output", OutputSeverity::Info);

    // Toolbar with actions
    m_hwndExplorerToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | SS_OWNERDRAW, 0, 0, SIDEBAR_DEFAULT_WIDTH, 30,
                                            hwndParent, nullptr, m_hInstance, nullptr);
    if (!m_hwndExplorerToolbar)
    {
        appendToOutput("Failed to create explorer toolbar\n", "Output", OutputSeverity::Error);
        return;
    }

    // Toolbar buttons
    const struct
    {
        int id;
        const char* text;
        int x;
    } buttons[] = {{IDC_EXPLORER_NEW_FILE, "New", 5},
                   {IDC_EXPLORER_NEW_FOLDER, "Folder", 50},
                   {IDC_EXPLORER_REFRESH, "Refresh", 105},
                   {IDC_EXPLORER_COLLAPSE, "Collapse", 165}};

    for (const auto& btn : buttons)
    {
        CreateWindowExA(0, "BUTTON", btn.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, btn.x, 3, 45, 24,
                        m_hwndExplorerToolbar, (HMENU)(INT_PTR)btn.id, m_hInstance, nullptr);
    }

    // ESP:m_hwndExplorerTree — File Explorer TreeView (IDC_EXPLORER_TREE 6010)
    appendToOutput("Creating Explorer TreeView control\n", "Output", OutputSeverity::Debug);
    m_hwndExplorerTree =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, RAWRXD_IDE_LABEL_FILE_EXPLORER,
                        WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, 0, 30,
                        SIDEBAR_DEFAULT_WIDTH, 570, hwndParent, (HMENU)IDC_EXPLORER_TREE, m_hInstance, nullptr);
    if (!m_hwndExplorerTree)
    {
        appendToOutput("Failed to create Explorer TreeView\n", "Output", OutputSeverity::Error);
        return;
    }

    SetWindowLongPtrA(m_hwndExplorerTree, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtrA(m_hwndExplorerTree, GWLP_WNDPROC, (LONG_PTR)ExplorerTreeProc);

    // LOGGING AS REQUESTED
    char buf[256];
    sprintf_s(buf, "ExplorerTree HWND created: %p (SidebarContent: %p)", m_hwndExplorerTree, m_hwndSidebarContent);
    LOG_INFO(std::string(buf));
    appendToOutput(std::string(buf) + "\n", "Output", OutputSeverity::Debug);

    // Set current workspace as root (portable — relative to exe directory)
    {
        char exeDir[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        char* lastSlash = strrchr(exeDir, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';
        m_explorerRootPath = std::string(exeDir);
        // Trim trailing backslash
        if (!m_explorerRootPath.empty() && m_explorerRootPath.back() == '\\')
            m_explorerRootPath.pop_back();
    }

    appendToOutput("Explorer view created with file tree at: " + m_explorerRootPath + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::refreshFileTree()
{
    appendToOutput("refreshFileTree() called\n", "Output", OutputSeverity::Debug);
    if (!m_hwndExplorerTree)
    {
        appendToOutput("Cannot refresh file tree - m_hwndExplorerTree is null\n", "Output", OutputSeverity::Warning);
        return;
    }

    TreeView_DeleteAllItems(m_hwndExplorerTree);

    // Add root folder
    TVINSERTSTRUCTA tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

    // Use static buffer for root text
    static char rootText[] = "Workspace";
    tvis.item.pszText = rootText;
    tvis.item.lParam = 0;

    HTREEITEM hRoot = TreeView_InsertItem(m_hwndExplorerTree, &tvis);
    if (!hRoot)
    {
        appendToOutput("Failed to create tree root\n", "Output", OutputSeverity::Error);
        return;
    }

    // Enumerate files and folders with error handling
    try
    {
        if (!fs::exists(m_explorerRootPath))
        {
            appendToOutput("Explorer root path does not exist: " + m_explorerRootPath + "\n", "Output",
                           OutputSeverity::Warning);
            return;
        }

        appendToOutput("Enumerating directory: " + m_explorerRootPath + "\n", "Output", OutputSeverity::Debug);

        for (const auto& entry : fs::directory_iterator(m_explorerRootPath))
        {
            try
            {
                std::string name = entry.path().filename().string();

                // Allocate on heap to avoid scope issues
                char* nameBuffer = new char[name.size() + 1];
                strcpy_s(nameBuffer, name.size() + 1, name.c_str());

                tvis.hParent = hRoot;
                tvis.item.pszText = nameBuffer;
                tvis.item.lParam = entry.is_directory() ? 1 : 0;

                HTREEITEM hItem = TreeView_InsertItem(m_hwndExplorerTree, &tvis);

                // Store path mapping
                if (hItem)
                {
                    m_treeItemPaths[hItem] = entry.path().string();
                }

                // Clean up buffer after insertion
                delete[] nameBuffer;
            }
            catch (const std::exception& e)
            {
                // Skip problematic entries
                continue;
            }
        }

        appendToOutput("File tree refreshed successfully\n", "Output", OutputSeverity::Info);
    }
    catch (const std::exception& e)
    {
        appendToOutput("Error refreshing file tree: " + std::string(e.what()) + "\n", "Output", OutputSeverity::Error);
    }

    TreeView_Expand(m_hwndExplorerTree, hRoot, TVE_EXPAND);
}

void Win32IDE::expandFolder(const std::string& path)
{
    LOG_FUNCTION();
    if (!m_hwndExplorerTree || path.empty())
        return;

    // Find the tree item that corresponds to this path
    HTREEITEM hTarget = nullptr;
    for (const auto& [item, itemPath] : m_treeItemPaths)
    {
        if (itemPath == path)
        {
            hTarget = item;
            break;
        }
    }
    if (!hTarget)
    {
        LOG_WARNING("expandFolder: no tree item found for path: " + path);
        return;
    }

    // Check if children are already loaded (first child is not the "Loading..." dummy)
    HTREEITEM hChild = TreeView_GetChild(m_hwndExplorerTree, hTarget);
    bool alreadyLoaded = false;
    if (hChild)
    {
        char buf[MAX_PATH] = {};
        TVITEMA tv = {};
        tv.hItem = hChild;
        tv.mask = TVIF_TEXT;
        tv.pszText = buf;
        tv.cchTextMax = MAX_PATH;
        if (TreeView_GetItem(m_hwndExplorerTree, &tv) && strcmp(buf, "Loading...") != 0)
        {
            alreadyLoaded = true;
        }
    }

    if (!alreadyLoaded)
    {
        // Remove any "Loading..." dummy children before populating
        while (hChild)
        {
            HTREEITEM hNext = TreeView_GetNextSibling(m_hwndExplorerTree, hChild);
            TreeView_DeleteItem(m_hwndExplorerTree, hChild);
            hChild = hNext;
        }

        // Populate with real directory contents
        try
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                try
                {
                    std::string name = entry.path().filename().string();
                    // Skip hidden/system entries starting with '.'
                    if (!name.empty() && name[0] == '.')
                        continue;

                    TVINSERTSTRUCTA tvis = {};
                    tvis.hParent = hTarget;
                    tvis.hInsertAfter = TVI_LAST;
                    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

                    char* nameBuffer = new char[name.size() + 1];
                    strcpy_s(nameBuffer, name.size() + 1, name.c_str());
                    tvis.item.pszText = nameBuffer;
                    tvis.item.lParam = entry.is_directory() ? 1 : 0;

                    HTREEITEM hNew = TreeView_InsertItem(m_hwndExplorerTree, &tvis);
                    if (hNew)
                    {
                        m_treeItemPaths[hNew] = entry.path().string();
                        // Add dummy child for subdirectories so expand arrow shows (intentional UX: "Loading..." until
                        // expanded)
                        if (entry.is_directory())
                        {
                            TVINSERTSTRUCTA dummy = {};
                            dummy.hParent = hNew;
                            dummy.hInsertAfter = TVI_FIRST;
                            dummy.item.mask = TVIF_TEXT;
                            static char loadingText[] = "Loading...";
                            dummy.item.pszText = loadingText;
                            TreeView_InsertItem(m_hwndExplorerTree, &dummy);
                        }
                    }
                    delete[] nameBuffer;
                }
                catch (...)
                {
                    continue;
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("expandFolder failed: " + std::string(e.what()));
        }
    }

    TreeView_Expand(m_hwndExplorerTree, hTarget, TVE_EXPAND);
    LOG_INFO("Expanded folder: " + path);
}

void Win32IDE::collapseAllFolders()
{
    if (!m_hwndExplorerTree)
        return;

    HTREEITEM hRoot = TreeView_GetRoot(m_hwndExplorerTree);
    TreeView_Expand(m_hwndExplorerTree, hRoot, TVE_COLLAPSE | TVE_COLLAPSERESET);

    appendToOutput("All folders collapsed\n", "Output", OutputSeverity::Info);
}

void Win32IDE::newFileInExplorer()
{
    LOG_FUNCTION();
    try
    {
        newFile();
        LOG_INFO("New file created from Explorer");
        appendToOutput("New file created from Explorer\n", "Explorer", OutputSeverity::Info);
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL(std::string("Exception during new file creation: ") + e.what());
        MessageBoxA(m_hwndMain, "An unexpected error occurred while creating the file.", "System Error",
                    MB_ICONERROR | MB_OK);
    }
}

void Win32IDE::newFolderInExplorer()
{
    LOG_FUNCTION();
    try
    {
        std::string baseName = "NewFolder";
        std::string fullPath = m_explorerRootPath + "\\" + baseName;
        int counter = 1;
        while (fs::exists(fullPath))
        {
            fullPath = m_explorerRootPath + "\\" + baseName + std::to_string(counter++);
        }
        if (fs::create_directory(fullPath))
        {
            LOG_INFO("New folder created: " + fullPath);
            refreshFileTree();
            appendToOutput("New folder created: " + fullPath + "\n", "Explorer", OutputSeverity::Info);
        }
        else
        {
            LOG_ERROR("Failed to create folder: " + fullPath);
            MessageBoxA(m_hwndMain, "Failed to create folder. Check permissions.", "Creation Failed", MB_ICONERROR);
        }
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL(std::string("Exception during folder creation: ") + e.what());
        MessageBoxA(m_hwndMain, "An error occurred while creating the folder.", "System Error", MB_ICONERROR);
    }
}

void Win32IDE::deleteItemInExplorer(const std::string& path)
{
    if (path.empty())
        return;
    std::string prompt = "Delete '" + path + "'? This action cannot be undone.";
    if (MessageBoxA(m_hwndMain, prompt.c_str(), "Confirm Delete", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    try
    {
        if (std::filesystem::is_directory(path))
        {
            std::filesystem::remove_all(path);
        }
        else
        {
            std::filesystem::remove(path);
        }
        if (m_hwndExplorerTree)
            refreshFileTree();
        if (m_hwndFileExplorer)
            refreshFileExplorer();
        appendToOutput("Deleted: " + path + "\n", "Output", OutputSeverity::Info);
    }
    catch (const std::exception& e)
    {
        appendToOutput(std::string("Error deleting: ") + e.what() + "\n", "Output", OutputSeverity::Error);
    }
}

void Win32IDE::deleteItemInExplorer()
{
    HTREEITEM hSelected = TreeView_GetSelection(m_hwndExplorerTree);
    if (!hSelected)
        return;
    std::string fullPath = getTreeItemPath(hSelected);
    if (!fullPath.empty())
        deleteItemInExplorer(fullPath);
}

void Win32IDE::renameItemInExplorer(const std::string& oldPath)
{
    if (oldPath.empty())
        return;
    char buffer[MAX_PATH] = {};
    strcpy_s(buffer, oldPath.c_str());
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn))
    {
        std::string newPath = buffer;
        try
        {
            std::filesystem::rename(oldPath, newPath);
            if (m_hwndExplorerTree)
                refreshFileTree();
            if (m_hwndFileExplorer)
                refreshFileExplorer();
            appendToOutput("Renamed: " + oldPath + " -> " + newPath + "\n", "Output", OutputSeverity::Info);
        }
        catch (const std::exception& e)
        {
            appendToOutput(std::string("Error renaming: ") + e.what() + "\n", "Output", OutputSeverity::Error);
        }
    }
}

void Win32IDE::renameItemInExplorer()
{
    HTREEITEM hSelected = TreeView_GetSelection(m_hwndExplorerTree);
    if (!hSelected)
        return;
    std::string oldPath = getTreeItemPath(hSelected);
    if (!oldPath.empty())
        renameItemInExplorer(oldPath);
}

void Win32IDE::revealInExplorer(const std::string& filePath)
{
    if (filePath.empty())
        return;

    // Try to find matching tree item
    for (const auto& kv : m_treeItemPaths)
    {
        if (_stricmp(kv.second.c_str(), filePath.c_str()) == 0)
        {
            // Select and expand parents
            HTREEITEM item = kv.first;
            HTREEITEM parent = TreeView_GetParent(m_hwndExplorerTree, item);
            while (parent)
            {
                TreeView_Expand(m_hwndExplorerTree, parent, TVE_EXPAND);
                parent = TreeView_GetParent(m_hwndExplorerTree, parent);
            }
            TreeView_SelectItem(m_hwndExplorerTree, item);
            SetFocus(m_hwndExplorerTree);
            appendToOutput("Revealed in Explorer: " + filePath + "\n", "Output", OutputSeverity::Info);
            return;
        }
    }

    // Fallback: open system Explorer and select the file
    ShellExecuteA(nullptr, "open", "explorer.exe", ("/select,\"" + filePath + "\"").c_str(), nullptr, SW_SHOWNORMAL);
}

void Win32IDE::handleExplorerContextMenu(POINT pt)
{
    static constexpr int IDC_CTX_DELETE = 50020, IDC_CTX_RENAME = 50021;
    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, IDC_EXPLORER_NEW_FILE, "New File");
    AppendMenuA(hMenu, MF_STRING, IDC_EXPLORER_NEW_FOLDER, "New Folder");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, IDC_CTX_DELETE, "Delete");
    AppendMenuA(hMenu, MF_STRING, IDC_CTX_RENAME, "Rename");
    int cmd = (int)TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, m_hwndMain, nullptr);
    DestroyMenu(hMenu);
    if (cmd == IDC_CTX_DELETE)
        deleteItemInExplorer();
    else if (cmd == IDC_CTX_RENAME)
        renameItemInExplorer();
    else if (cmd == IDC_EXPLORER_NEW_FILE)
        newFileInExplorer();
    else if (cmd == IDC_EXPLORER_NEW_FOLDER)
        newFolderInExplorer();
}

LRESULT CALLBACK Win32IDE::ExplorerTreeProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_RBUTTONDOWN:
        {
            if (pThis)
            {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hwnd, &pt);
                pThis->handleExplorerContextMenu(pt);
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK:
        {
            if (pThis)
            {
                HTREEITEM hItem = TreeView_GetSelection(hwnd);
                if (hItem)
                {
                    TVITEMA item = {};
                    char text[260];
                    item.mask = TVIF_TEXT | TVIF_PARAM;
                    item.pszText = text;
                    item.cchTextMax = 260;
                    item.hItem = hItem;

                    if (TreeView_GetItem(hwnd, &item))
                    {
                        if (item.lParam == 0)
                        {  // File, not folder
                            // Phase 37: Use m_treeItemPaths for correct full path
                            // (handles nested directories properly)
                            std::string filePath;
                            auto pathIt = pThis->m_treeItemPaths.find(hItem);
                            if (pathIt != pThis->m_treeItemPaths.end())
                            {
                                filePath = pathIt->second;
                            }
                            else
                            {
                                // Fallback: build path by walking tree parents
                                std::string fullPath;
                                HTREEITEM hCurrent = hItem;
                                while (hCurrent)
                                {
                                    TVITEMA parentItem = {};
                                    char parentText[260] = {};
                                    parentItem.mask = TVIF_TEXT;
                                    parentItem.pszText = parentText;
                                    parentItem.cchTextMax = 260;
                                    parentItem.hItem = hCurrent;
                                    if (TreeView_GetItem(hwnd, &parentItem))
                                    {
                                        if (fullPath.empty())
                                        {
                                            fullPath = parentText;
                                        }
                                        else
                                        {
                                            fullPath = std::string(parentText) + "\\" + fullPath;
                                        }
                                    }
                                    hCurrent = TreeView_GetParent(hwnd, hCurrent);
                                }
                                filePath = pThis->m_explorerRootPath + "\\" + fullPath;
                            }

                            // Actually open the file in the editor
                            if (!filePath.empty() && fs::exists(filePath) && fs::is_regular_file(filePath))
                            {
                                pThis->openFile(filePath);
                            }
                            else
                            {
                                pThis->appendToOutput("File not found: " + filePath + "\n", "Output",
                                                      OutputSeverity::Warning);
                            }
                        }
                        else
                        {
                            // Folder: toggle expand/collapse
                            TreeView_Expand(hwnd, hItem, TVE_TOGGLE);
                        }
                    }
                }
            }
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Search View Implementation
// ============================================================================

void Win32IDE::createSearchView(HWND hwndParent)
{
    // Search input
    m_hwndSearchInput =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 5, 10, SIDEBAR_DEFAULT_WIDTH - 10, 25,
                        hwndParent, (HMENU)IDC_SEARCH_INPUT, m_hInstance, nullptr);

    // Options panel
    m_hwndSearchOptions = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_BORDER, 5, 40, SIDEBAR_DEFAULT_WIDTH - 10, 80,
                                          hwndParent, nullptr, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 5, 5, 70, 20, m_hwndSearchOptions,
                    (HMENU)IDC_SEARCH_REGEX, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Case", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 80, 5, 70, 20, m_hwndSearchOptions,
                    (HMENU)IDC_SEARCH_CASE, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Whole", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 155, 5, 70, 20, m_hwndSearchOptions,
                    (HMENU)IDC_SEARCH_WHOLE_WORD, m_hInstance, nullptr);

    CreateWindowExA(0, "STATIC", "Include:", WS_CHILD | WS_VISIBLE, 5, 30, 50, 20, m_hwndSearchOptions, nullptr,
                    m_hInstance, nullptr);
    m_hwndIncludePattern =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "*.ps1,*.cpp", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 60, 28, 160,
                        20, m_hwndSearchOptions, (HMENU)IDC_SEARCH_INCLUDE, m_hInstance, nullptr);

    CreateWindowExA(0, "STATIC", "Exclude:", WS_CHILD | WS_VISIBLE, 5, 55, 50, 20, m_hwndSearchOptions, nullptr,
                    m_hInstance, nullptr);
    m_hwndExcludePattern =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "node_modules,bin", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 60, 53,
                        160, 20, m_hwndSearchOptions, (HMENU)IDC_SEARCH_EXCLUDE, m_hInstance, nullptr);

    // Results listbox
    m_hwndSearchResults =
        CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 5, 125,
                        SIDEBAR_DEFAULT_WIDTH - 10, 470, hwndParent, (HMENU)IDC_SEARCH_RESULTS, m_hInstance, nullptr);

    m_searchInProgress = false;

    appendToOutput("Search view created with regex/case options\n", "Output", OutputSeverity::Info);
}

void Win32IDE::performWorkspaceSearch(const std::string& query, bool useRegex, bool caseSensitive, bool wholeWord)
{
    if (query.empty())
        return;

    m_searchInProgress = true;
    m_searchResults.clear();
    if (m_hwndSearchResults)
        SendMessageA(m_hwndSearchResults, LB_RESETCONTENT, 0, 0);

    appendToOutput("Searching for: '" + query + "'\n", "Output", OutputSeverity::Info);

    try
    {
        std::regex pattern(query, caseSensitive ? std::regex::ECMAScript : std::regex::icase);

        for (const auto& entry : fs::recursive_directory_iterator(m_explorerRootPath))
        {
            if (!entry.is_regular_file())
                continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".ps1" && ext != ".cpp" && ext != ".h" && ext != ".txt")
                continue;

            std::ifstream file(entry.path());
            std::string line;
            int lineNum = 0;

            while (std::getline(file, line))
            {
                lineNum++;
                if (useRegex)
                {
                    if (std::regex_search(line, pattern))
                    {
                        std::string result =
                            entry.path().filename().string() + " (" + std::to_string(lineNum) + "): " + line;
                        m_searchResults.push_back(result);
                        if (m_hwndSearchResults)
                            SendMessageA(m_hwndSearchResults, LB_ADDSTRING, 0, (LPARAM)result.c_str());
                    }
                }
                else
                {
                    std::string searchLine = caseSensitive ? line : line;
                    std::string searchQuery = caseSensitive ? query : query;

                    if (searchLine.find(searchQuery) != std::string::npos)
                    {
                        std::string result =
                            entry.path().filename().string() + " (" + std::to_string(lineNum) + "): " + line;
                        m_searchResults.push_back(result);
                        if (m_hwndSearchResults)
                            SendMessageA(m_hwndSearchResults, LB_ADDSTRING, 0, (LPARAM)result.c_str());
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        appendToOutput("Search error: " + std::string(e.what()) + "\n", "Output", OutputSeverity::Error);
    }

    m_searchInProgress = false;
    appendToOutput("Search complete: " + std::to_string(m_searchResults.size()) + " results\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::updateSearchResults(const std::vector<std::string>& results)
{
    SendMessageA(m_hwndSearchResults, LB_RESETCONTENT, 0, 0);
    for (const auto& result : results)
    {
        SendMessageA(m_hwndSearchResults, LB_ADDSTRING, 0, (LPARAM)result.c_str());
    }
}

void Win32IDE::applySearchFilters(const std::string& includePattern, const std::string& excludePattern)
{
    LOG_FUNCTION();
    if (m_searchResults.empty())
        return;

    // Convert simple glob patterns (*.cpp, *.h) to check against results
    auto matchesGlob = [](const std::string& filename, const std::string& pattern) -> bool
    {
        if (pattern.empty() || pattern == "*")
            return true;
        // Support simple *.ext patterns
        if (pattern.size() >= 2 && pattern[0] == '*' && pattern[1] == '.')
        {
            std::string ext = pattern.substr(1);  // ".cpp"
            if (filename.size() >= ext.size())
            {
                std::string fileExt = filename.substr(filename.size() - ext.size());
                // Case-insensitive compare
                std::string lowerFileExt = fileExt, lowerExt = ext;
                std::transform(lowerFileExt.begin(), lowerFileExt.end(), lowerFileExt.begin(), ::tolower);
                std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
                return lowerFileExt == lowerExt;
            }
            return false;
        }
        // Substring match fallback
        return filename.find(pattern) != std::string::npos;
    };

    // Parse comma-separated patterns
    auto parsePatterns = [](const std::string& input) -> std::vector<std::string>
    {
        std::vector<std::string> patterns;
        std::istringstream ss(input);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos)
                patterns.push_back(token.substr(start, end - start + 1));
        }
        return patterns;
    };

    auto includes = parsePatterns(includePattern);
    auto excludes = parsePatterns(excludePattern);

    std::vector<std::string> filtered;
    for (const auto& result : m_searchResults)
    {
        // Extract filename from result format "filename (line): content"
        std::string filename = result.substr(0, result.find(' '));

        // Check include (if include patterns specified, file must match at least one)
        bool includeMatch = includes.empty();
        for (const auto& pat : includes)
        {
            if (matchesGlob(filename, pat))
            {
                includeMatch = true;
                break;
            }
        }
        if (!includeMatch)
            continue;

        // Check exclude (if any exclude pattern matches, skip)
        bool excludeMatch = false;
        for (const auto& pat : excludes)
        {
            if (matchesGlob(filename, pat))
            {
                excludeMatch = true;
                break;
            }
        }
        if (excludeMatch)
            continue;

        filtered.push_back(result);
    }

    // Update UI with filtered results
    m_searchResults = filtered;
    updateSearchResults(filtered);

    appendToOutput("Search filtered: " + std::to_string(filtered.size()) + " results (include: " + includePattern +
                       ", exclude: " + excludePattern + ")\n",
                   "Output", OutputSeverity::Info);
}

void Win32IDE::searchInFiles(const std::string& query)
{
    bool useRegex = (SendMessageA(GetDlgItem(m_hwndSearchOptions, IDC_SEARCH_REGEX), BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool caseSensitive =
        (SendMessageA(GetDlgItem(m_hwndSearchOptions, IDC_SEARCH_CASE), BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool wholeWord =
        (SendMessageA(GetDlgItem(m_hwndSearchOptions, IDC_SEARCH_WHOLE_WORD), BM_GETCHECK, 0, 0) == BST_CHECKED);

    performWorkspaceSearch(query, useRegex, caseSensitive, wholeWord);
}

void Win32IDE::runWorkspaceSearchFromDialog(const std::string& query)
{
    performWorkspaceSearch(query, false, false, false);
}

void Win32IDE::replaceInFiles(const std::string& searchText, const std::string& replaceText)
{
    if (searchText.empty())
        return;

    if (MessageBoxW(m_hwndMain, L"Replace occurrences across workspace?", L"Confirm Replace",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    size_t totalReplacements = 0;
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(m_explorerRootPath))
        {
            if (!entry.is_regular_file())
                continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".ps1" && ext != ".cpp" && ext != ".h" && ext != ".txt" && ext != ".md")
                continue;

            std::ifstream file(entry.path());
            if (!file.is_open())
                continue;
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            size_t count = 0;
            size_t pos = 0;
            while ((pos = content.find(searchText, pos)) != std::string::npos)
            {
                content.replace(pos, searchText.length(), replaceText);
                pos += replaceText.length();
                count++;
            }

            if (count > 0)
            {
                // Backup original
                std::string backup = entry.path().string() + ".bak";
                std::filesystem::copy_file(entry.path(), backup, std::filesystem::copy_options::overwrite_existing);

                // Write new content
                std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
                out << content;
                out.close();

                totalReplacements += count;
                appendToOutput("Replaced " + std::to_string(count) + " occurrences in " + entry.path().string() + "\n",
                               "Output", OutputSeverity::Info);
            }
        }
    }
    catch (const std::exception& e)
    {
        appendToOutput(std::string("Replace in files error: ") + e.what() + "\n", "Output", OutputSeverity::Error);
        return;
    }

    appendToOutput("Replace complete: " + std::to_string(totalReplacements) + " total replacements\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::clearSearchResults()
{
    m_searchResults.clear();
    SendMessageA(m_hwndSearchResults, LB_RESETCONTENT, 0, 0);
}

// ============================================================================
// Source Control View Implementation
// ============================================================================

void Win32IDE::createSourceControlView(HWND hwndParent)
{
    // Toolbar
    m_hwndSCMToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | SS_OWNERDRAW, 0, 0, SIDEBAR_DEFAULT_WIDTH, 35,
                                       hwndParent, nullptr, m_hInstance, nullptr);

    const struct
    {
        int id;
        const char* text;
        int x;
    } buttons[] = {{IDC_SCM_STAGE, "Stage", 5},
                   {IDC_SCM_UNSTAGE, "Unstage", 55},
                   {IDC_SCM_COMMIT, "Commit", 115},
                   {IDC_SCM_SYNC, "Sync", 175}};

    for (const auto& btn : buttons)
    {
        CreateWindowExA(0, "BUTTON", btn.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, btn.x, 5, 50, 25,
                        m_hwndSCMToolbar, (HMENU)(INT_PTR)btn.id, m_hInstance, nullptr);
    }

    // Branch indicator at top-right of SCM toolbar
    CreateWindowExA(0, "STATIC", "Branch: -", WS_CHILD | WS_VISIBLE | SS_LEFT,
                    230, 9, SIDEBAR_DEFAULT_WIDTH - 235, 18,
                    m_hwndSCMToolbar, (HMENU)(INT_PTR)IDC_SCM_BRANCH, m_hInstance, nullptr);

    // Commit message box
    CreateWindowExA(0, "STATIC", "Message:", WS_CHILD | WS_VISIBLE, 5, 40, 60, 20, hwndParent, nullptr, m_hInstance,
                    nullptr);
    m_hwndSCMMessageBox =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 5, 40,
                        SIDEBAR_DEFAULT_WIDTH - 10, 60, hwndParent, (HMENU)IDC_SCM_MESSAGE, m_hInstance, nullptr);

    // File list (ListView for changed files)
    m_hwndSCMFileList =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL, 5, 105,
                        SIDEBAR_DEFAULT_WIDTH - 10, 490, hwndParent, (HMENU)IDC_SCM_FILE_LIST, m_hInstance, nullptr);

    // Setup columns
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 40;
    col.pszText = (LPSTR) "Stat";
    ListView_InsertColumn(m_hwndSCMFileList, 0, &col);

    col.cx = 180;
    col.pszText = (LPSTR) "File";
    ListView_InsertColumn(m_hwndSCMFileList, 1, &col);

    appendToOutput("Source Control view created with Git integration\n", "Output", OutputSeverity::Info);
}

void Win32IDE::refreshSourceControlView()
{
    if (!m_hwndSCMFileList)
        return;

    ListView_DeleteAllItems(m_hwndSCMFileList);

    // Update current branch indicator if present
    if (m_hwndSCMToolbar)
    {
        std::string branchOutput;
        if (isGitRepository() && executeGitCommand("git rev-parse --abbrev-ref HEAD", branchOutput))
        {
            while (!branchOutput.empty() &&
                   (branchOutput.back() == '\r' || branchOutput.back() == '\n' || branchOutput.back() == ' '))
            {
                branchOutput.pop_back();
            }
            if (branchOutput.empty())
                branchOutput = "(detached)";
            std::string label = "Branch: " + branchOutput;
            SetWindowTextA(GetDlgItem(m_hwndSCMToolbar, IDC_SCM_BRANCH), label.c_str());
        }
        else
        {
            SetWindowTextA(GetDlgItem(m_hwndSCMToolbar, IDC_SCM_BRANCH), "Branch: (not a git repo)");
        }
    }

    if (!isGitRepository())
    {
        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = 0;
        item.iSubItem = 0;
        item.pszText = (LPSTR)"-";
        ListView_InsertItem(m_hwndSCMFileList, &item);
        item.iSubItem = 1;
        item.pszText = (LPSTR)"No Git repository detected in workspace/current directory";
        ListView_SetItem(m_hwndSCMFileList, &item);
        appendToOutput("Source Control: no Git repository detected\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Get changed files from Git
    std::vector<GitFile> files = getGitChangedFiles();

    LVITEMA item = {};
    item.mask = LVIF_TEXT;

    for (size_t i = 0; i < files.size(); i++)
    {
        item.iItem = (int)i;
        item.iSubItem = 0;

        char status[3] = {files[i].status, 0, 0};
        item.pszText = status;
        ListView_InsertItem(m_hwndSCMFileList, &item);

        item.iSubItem = 1;
        item.pszText = (LPSTR)files[i].path.c_str();
        ListView_SetItem(m_hwndSCMFileList, &item);
    }

    appendToOutput("Source Control refreshed: " + std::to_string(files.size()) + " changes\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::stageSelectedFiles()
{
    int idx = ListView_GetNextItem(m_hwndSCMFileList, -1, LVNI_SELECTED);
    if (idx >= 0)
    {
        char file[260];
        LVITEMA lvi = {0};
        lvi.iSubItem = 1;
        lvi.pszText = file;
        lvi.cchTextMax = 260;
        SendMessage(m_hwndSCMFileList, LVM_GETITEMTEXTA, idx, (LPARAM)&lvi);
        gitStageFile(file);
        refreshSourceControlView();
    }
}

void Win32IDE::unstageSelectedFiles()
{
    int idx = ListView_GetNextItem(m_hwndSCMFileList, -1, LVNI_SELECTED);
    if (idx >= 0)
    {
        char file[260];
        LVITEMA lvi = {0};
        lvi.iSubItem = 1;
        lvi.pszText = file;
        lvi.cchTextMax = 260;
        SendMessage(m_hwndSCMFileList, LVM_GETITEMTEXTA, idx, (LPARAM)&lvi);
        gitUnstageFile(file);
        refreshSourceControlView();
    }
}

void Win32IDE::discardChanges()
{
    if (MessageBoxA(m_hwndMain, "Discard all changes? This cannot be undone.", "Confirm", MB_YESNO | MB_ICONWARNING) ==
        IDYES)
    {
        std::string output;
        executeGitCommand("git reset --hard HEAD", output);
        refreshSourceControlView();
        appendToOutput("Changes discarded\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::commitChangesFromSidebar()
{
    char message[512];
    GetWindowTextA(m_hwndSCMMessageBox, message, 512);

    if (strlen(message) > 0)
    {
        gitCommit(message);
        SetWindowTextA(m_hwndSCMMessageBox, "");
        refreshSourceControlView();
    }
    else
    {
        MessageBoxA(m_hwndMain, "Please enter a commit message", "Commit", MB_OK | MB_ICONWARNING);
    }
}

void Win32IDE::syncRepository()
{
    gitPull();
    gitPush();
    refreshSourceControlView();
}

void Win32IDE::showSCMContextMenu(POINT pt)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, IDC_SCM_STAGE, "Stage");
    AppendMenuA(hMenu, MF_STRING, IDC_SCM_UNSTAGE, "Unstage");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, 998, "Discard Changes");

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwndMain, nullptr);
    DestroyMenu(hMenu);
}

// ============================================================================
// Run and Debug View Implementation
// ============================================================================

void Win32IDE::createRunDebugView(HWND hwndParent)
{
    // Toolbar
    m_hwndDebugToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | SS_OWNERDRAW, 0, 0, SIDEBAR_DEFAULT_WIDTH, 35,
                                         hwndParent, nullptr, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 5, 50, 25, m_hwndDebugToolbar,
                    (HMENU)IDC_DEBUG_START, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 60, 5, 50, 25, m_hwndDebugToolbar,
                    (HMENU)IDC_DEBUG_STOP, m_hInstance, nullptr);

    // Configuration dropdown
    m_hwndDebugConfigs =
        CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 5, 40, SIDEBAR_DEFAULT_WIDTH - 10,
                        100, hwndParent, (HMENU)IDC_DEBUG_CONFIGS, m_hInstance, nullptr);

    SendMessageA(m_hwndDebugConfigs, CB_ADDSTRING, 0, (LPARAM) "PowerShell Script");
    SendMessageA(m_hwndDebugConfigs, CB_ADDSTRING, 0, (LPARAM) "C++ Debug");
    SendMessageA(m_hwndDebugConfigs, CB_ADDSTRING, 0, (LPARAM) "Python Script");
    SendMessageA(m_hwndDebugConfigs, CB_SETCURSEL, 0, 0);

    // Variables view
    CreateWindowExA(0, "STATIC", "Variables:", WS_CHILD | WS_VISIBLE, 5, 145, 100, 20, hwndParent, nullptr, m_hInstance,
                    nullptr);
    m_hwndDebugVariables =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | WS_VSCROLL, 5, 145,
                        SIDEBAR_DEFAULT_WIDTH - 10, 450, hwndParent, (HMENU)IDC_DEBUG_VARIABLES, m_hInstance, nullptr);

    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 80;
    col.pszText = (LPSTR) "Name";
    ListView_InsertColumn(m_hwndDebugVariables, 0, &col);

    col.cx = 140;
    col.pszText = (LPSTR) "Value";
    ListView_InsertColumn(m_hwndDebugVariables, 1, &col);

    m_debuggingActive = false;

    appendToOutput("Run and Debug view created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::createLaunchConfiguration()
{
    LOG_FUNCTION();

    // Create .rawrxd directory if it doesn't exist
    std::string configDir = m_explorerRootPath + "\\.rawrxd";
    try
    {
        fs::create_directories(configDir);
    }
    catch (...)
    {
    }

    std::string launchPath = configDir + "\\launch.json";

    // Detect project type from current file or workspace
    std::string progName = "${workspaceFolder}\\build\\main.exe";
    std::string debuggerType = "gdb";
    std::string stopAtEntry = "true";

    if (!m_currentFile.empty())
    {
        std::string ext = fs::path(m_currentFile).extension().string();
        if (ext == ".ps1")
        {
            debuggerType = "powershell";
            progName = m_currentFile;
        }
        else if (ext == ".py")
        {
            debuggerType = "python";
            progName = m_currentFile;
        }
    }

    // Write launch.json
    std::ofstream out(launchPath, std::ios::trunc);
    if (!out.is_open())
    {
        LOG_ERROR("Failed to create launch configuration at: " + launchPath);
        MessageBoxA(m_hwndMain, "Failed to create launch.json", "Error", MB_ICONERROR);
        return;
    }

    out << "{\n";
    out << "  \"version\": \"0.2.0\",\n";
    out << "  \"configurations\": [\n";
    out << "    {\n";
    out << "      \"name\": \"RawrXD Debug (GDB)\",\n";
    out << "      \"type\": \"" << debuggerType << "\",\n";
    out << "      \"request\": \"launch\",\n";
    out << "      \"program\": \"" << progName << "\",\n";
    out << "      \"args\": [],\n";
    out << "      \"stopAtEntry\": " << stopAtEntry << ",\n";
    out << "      \"cwd\": \"" << m_explorerRootPath << "\",\n";
    out << "      \"environment\": [],\n";
    out << "      \"externalConsole\": false,\n";
    out << "      \"preLaunchTask\": \"build\"\n";
    out << "    },\n";
    out << "    {\n";
    out << "      \"name\": \"RawrXD Attach\",\n";
    out << "      \"type\": \"gdb\",\n";
    out << "      \"request\": \"attach\",\n";
    out << "      \"processId\": \"${command:pickProcess}\"\n";
    out << "    }\n";
    out << "  ]\n";
    out << "}\n";
    out.close();

    appendToOutput("Launch configuration created: " + launchPath + "\n", "Output", OutputSeverity::Info);
    LOG_INFO("Created launch.json at: " + launchPath);

    // Open the file in the editor so user can modify
    openFile(launchPath);
}

void Win32IDE::onDebugConfigurationChanged()
{
    if (!m_hwndDebugConfigs)
    {
        return;
    }

    int sel = (int)SendMessageA(m_hwndDebugConfigs, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR)
    {
        return;
    }

    char selectedText[128] = {0};
    SendMessageA(m_hwndDebugConfigs, CB_GETLBTEXT, sel, (LPARAM)selectedText);
    const std::string debugProfile = selectedText;

    // Keep selection behavior deterministic and user-visible.
    if (debugProfile == "PowerShell Script" || debugProfile == "Python Script")
    {
        createLaunchConfiguration();
    }

    appendToOutput("Debug configuration selected: " + debugProfile + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::startDebugging()
{
    attachDebugger();
    m_debuggingActive = m_debuggerAttached;
    appendToOutput(m_debuggerAttached ? "Debugging started\n" : "Debugging failed to start\n",
                   "Output", m_debuggerAttached ? OutputSeverity::Info : OutputSeverity::Error);
    updateDebugVariables();
}

void Win32IDE::stopDebugging()
{
    stopDebugger();
    m_debuggingActive = false;
    ListView_DeleteAllItems(m_hwndDebugVariables);
    appendToOutput("Debugging stopped\n", "Output", OutputSeverity::Info);
}

// setBreakpoint and removeBreakpoint are implemented in Win32IDE_Debugger.cpp

void Win32IDE::stepOver()
{
    stepOverExecution();
}

void Win32IDE::stepInto()
{
    stepIntoExecution();
}

void Win32IDE::stepOut()
{
    stepOutExecution();
}

void Win32IDE::continueExecution()
{
    resumeExecution();
}

void Win32IDE::showDebugConsole()
{
    // ─── VS Code Debug Console: REPL with expression evaluation ──────────
    // Creates a panel with output display + input field for evaluating
    // expressions against the debugger's current execution context.

    // If debug console panel already exists, just show it
    if (m_hwndDebugConsole && IsWindow(m_hwndDebugConsole)) {
        ShowWindow(m_hwndDebugConsole, SW_SHOW);
        SetFocus(m_hwndDebugConsoleInput);
        return;
    }

    // Get panel area (reuse output panel area or create new)
    RECT panelRect;
    if (m_hwndOutputPanel && IsWindow(m_hwndOutputPanel)) {
        GetClientRect(m_hwndOutputPanel, &panelRect);
        MapWindowPoints(m_hwndOutputPanel, m_hwndMain, (POINT*)&panelRect, 2);
    } else {
        // Fallback: bottom 200px of main window
        RECT mainRect;
        GetClientRect(m_hwndMain, &mainRect);
        panelRect = { 0, mainRect.bottom - 200, mainRect.right, mainRect.bottom };
    }

    // Create debug console container
    m_hwndDebugConsole = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        panelRect.left, panelRect.top,
        panelRect.right - panelRect.left, panelRect.bottom - panelRect.top,
        m_hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!m_hwndDebugConsole) {
        appendToOutput("Failed to create debug console panel\n", "Output", OutputSeverity::Error);
        return;
    }

    int panelW = panelRect.right - panelRect.left;
    int panelH = panelRect.bottom - panelRect.top;
    int inputH = 24;

    // Output area (RichEdit for colored output)
    m_hwndDebugConsoleOutput = CreateWindowExA(
        0, RICHEDIT_CLASSA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, panelW, panelH - inputH - 2,
        m_hwndDebugConsole, (HMENU)201, GetModuleHandle(nullptr), nullptr);

    // Dark theme for output
    SendMessage(m_hwndDebugConsoleOutput, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    CHARFORMAT2A cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = RGB(204, 204, 204);
    cf.yHeight = 180;
    strcpy_s(cf.szFaceName, "Consolas");
    SendMessageA(m_hwndDebugConsoleOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Input field with ">" prompt feel
    m_hwndDebugConsoleInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, panelH - inputH, panelW, inputH,
        m_hwndDebugConsole, (HMENU)202, GetModuleHandle(nullptr), nullptr);

    // Set monospace font on input
    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessage(m_hwndDebugConsoleInput, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Subclass input for Enter key handling
    m_debugConsoleOrigInputProc = (WNDPROC)SetWindowLongPtrA(
        m_hwndDebugConsoleInput, GWLP_WNDPROC,
        (LONG_PTR)+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            if (msg == WM_KEYDOWN && wp == VK_RETURN) {
                // Get text from input
                char buf[1024] = {};
                GetWindowTextA(hwnd, buf, sizeof(buf));
                std::string expr(buf);

                if (!expr.empty()) {
                    // Get IDE pointer from parent chain
                    HWND hParent = GetParent(hwnd);
                    HWND hMain = GetParent(hParent);
                    auto* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hMain, GWLP_USERDATA));
                    if (ide) {
                        ide->debugConsoleEvaluate(expr);
                    }
                    SetWindowTextA(hwnd, "");
                }
                return 0;
            }
            if (msg == WM_KEYDOWN && wp == VK_UP) {
                // History navigation (up)
                HWND hParent = GetParent(hwnd);
                HWND hMain = GetParent(hParent);
                auto* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hMain, GWLP_USERDATA));
                if (ide) {
                    std::string prev = ide->debugConsoleHistoryPrev();
                    if (!prev.empty()) SetWindowTextA(hwnd, prev.c_str());
                }
                return 0;
            }
            if (msg == WM_KEYDOWN && wp == VK_DOWN) {
                HWND hParent = GetParent(hwnd);
                HWND hMain = GetParent(hParent);
                auto* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hMain, GWLP_USERDATA));
                if (ide) {
                    std::string next = ide->debugConsoleHistoryNext();
                    SetWindowTextA(hwnd, next.c_str());
                }
                return 0;
            }
            // Call original proc
            WNDPROC orig = (WNDPROC)GetPropA(hwnd, "OrigProc");
            if (orig) return CallWindowProcA(orig, hwnd, msg, wp, lp);
            return DefWindowProcA(hwnd, msg, wp, lp);
        });

    // Store original proc for callback
    SetPropA(m_hwndDebugConsoleInput, "OrigProc", (HANDLE)m_debugConsoleOrigInputProc);

    // Welcome message
    debugConsoleAppend("[Debug Console] Type expressions to evaluate.\n", RGB(86, 156, 214));
    debugConsoleAppend("  Use Up/Down arrows for history.\n", RGB(128, 128, 128));

    SetFocus(m_hwndDebugConsoleInput);
    appendToOutput("Debug Console opened\n", "Output", OutputSeverity::Info);
}

// =============================================================================
// Debug Console — Expression Evaluation
// =============================================================================

void Win32IDE::debugConsoleEvaluate(const std::string& expression)
{
    if (expression.empty()) return;

    // Add to history
    m_debugConsoleHistory.push_back(expression);
    m_debugConsoleHistoryIndex = (int)m_debugConsoleHistory.size();

    // Echo the expression
    debugConsoleAppend("> " + expression + "\n", RGB(78, 201, 176)); // teal for input

    // Route to debugger engine for evaluation
    if (m_debuggingActive && m_debugEngine) {
        // Use the native debugger's evaluate functionality
        try {
            debuggerEvaluateExpression(expression);
            debugConsoleAppend("  (evaluated)\n", RGB(204, 204, 204));
        } catch (...) {
            debugConsoleAppend("  Error evaluating expression\n", RGB(244, 71, 71));
        }
    } else {
        // No active debug session — provide helpful eval for simple cases
        // Support basic expressions like variable inspection
        debugConsoleAppend("  [No active debug session]\n", RGB(204, 120, 50));
        debugConsoleAppend("  Start debugging (F5) to evaluate expressions.\n", RGB(128, 128, 128));
    }
}

// =============================================================================
// Debug Console — Append colored text to output
// =============================================================================

void Win32IDE::debugConsoleAppend(const std::string& text, COLORREF color)
{
    if (!m_hwndDebugConsoleOutput || !IsWindow(m_hwndDebugConsoleOutput)) return;

    // Move caret to end
    int len = GetWindowTextLengthA(m_hwndDebugConsoleOutput);
    SendMessage(m_hwndDebugConsoleOutput, EM_SETSEL, len, len);

    // Set color for new text
    CHARFORMAT2A cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageA(m_hwndDebugConsoleOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Insert text
    SendMessageA(m_hwndDebugConsoleOutput, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

    // Scroll to bottom
    SendMessage(m_hwndDebugConsoleOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

// =============================================================================
// Debug Console — History Navigation
// =============================================================================

std::string Win32IDE::debugConsoleHistoryPrev()
{
    if (m_debugConsoleHistory.empty()) return "";
    if (m_debugConsoleHistoryIndex > 0) m_debugConsoleHistoryIndex--;
    return m_debugConsoleHistory[m_debugConsoleHistoryIndex];
}

std::string Win32IDE::debugConsoleHistoryNext()
{
    if (m_debugConsoleHistory.empty()) return "";
    if (m_debugConsoleHistoryIndex < (int)m_debugConsoleHistory.size() - 1) {
        m_debugConsoleHistoryIndex++;
        return m_debugConsoleHistory[m_debugConsoleHistoryIndex];
    }
    m_debugConsoleHistoryIndex = (int)m_debugConsoleHistory.size();
    return ""; // cleared input
}

void Win32IDE::updateDebugVariables()
{
    if (!m_debuggingActive || !m_hwndDebugVariables)
        return;
    LOG_FUNCTION();

    ListView_DeleteAllItems(m_hwndDebugVariables);

    // Collect real environment and debugger state
    struct VarEntry
    {
        std::string name;
        std::string value;
    };
    std::vector<VarEntry> vars;

    // System variables
    vars.push_back(VarEntry{"$DebuggerActive", m_debuggerAttached ? "true" : "false"});
    vars.push_back(VarEntry{"$DebuggerPaused", m_debuggerPaused ? "true" : "false"});
    vars.push_back(VarEntry{"$CurrentFile", m_currentFile.empty() ? "(none)" : m_currentFile});
    vars.push_back(VarEntry{"$CurrentLine", std::to_string(m_debuggerCurrentLine)});
    vars.push_back(VarEntry{"$BreakpointCount", std::to_string(m_breakpoints.size())});

    // Watch expressions
    for (const auto& w : m_watchList)
    {
        if (w.enabled)
        {
            vars.push_back(VarEntry{"[watch] " + w.expression, w.value.empty() ? "<not evaluated>" : w.value});
        }
    }

    // Call stack info
    if (!m_callStack.empty())
    {
        vars.push_back(VarEntry{"$CallStackDepth", std::to_string(m_callStack.size())});
        vars.push_back(VarEntry{"$TopFrame", m_callStack[0].function + " @ " + m_callStack[0].file + ":" +
                                                 std::to_string(m_callStack[0].line)});
    }

    // Model/inference state
    vars.push_back(VarEntry{"$ModelLoaded", m_loadedModelPath.empty() ? "false" : "true"});
    vars.push_back(VarEntry{"$ModelPath", m_loadedModelPath.empty() ? "(none)" : m_loadedModelPath});
    vars.push_back(VarEntry{"$InferenceRunning", m_inferenceRunning ? "true" : "false"});
    vars.push_back(VarEntry{"$WorkingDirectory", m_explorerRootPath});

    // Process environment
    char envBuf[256] = {};
    if (GetEnvironmentVariableA("USERNAME", envBuf, sizeof(envBuf)))
    {
        vars.push_back(VarEntry{"$USERNAME", envBuf});
    }
    if (GetEnvironmentVariableA("COMPUTERNAME", envBuf, sizeof(envBuf)))
    {
        vars.push_back(VarEntry{"$COMPUTERNAME", envBuf});
    }

    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    for (int i = 0; i < (int)vars.size(); i++)
    {
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = (LPSTR)vars[i].name.c_str();
        ListView_InsertItem(m_hwndDebugVariables, &item);

        item.iSubItem = 1;
        item.pszText = (LPSTR)vars[i].value.c_str();
        ListView_SetItem(m_hwndDebugVariables, &item);
    }

    LOG_DEBUG("Updated " + std::to_string(vars.size()) + " debug variables");
}

// ============================================================================
// Extensions View Implementation
// ============================================================================

void Win32IDE::createExtensionsView(HWND hwndParent)
{
    // Search box
    m_hwndExtensionSearch =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 5, 10, SIDEBAR_DEFAULT_WIDTH - 10, 25,
                        hwndParent, (HMENU)IDC_EXT_SEARCH, m_hInstance, nullptr);

    // Extension action buttons
    CreateWindowExA(0, "BUTTON", "Install VSIX...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 38, 75, 24, hwndParent,
                    (HMENU)IDC_EXT_INSTALL_VSIX, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Install", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 85, 38, 50, 24, hwndParent,
                    (HMENU)IDC_EXT_INSTALL, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Uninstall", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 140, 38, 55, 24, hwndParent,
                    (HMENU)IDC_EXT_UNINSTALL, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Details", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 200, 38, 50, 24, hwndParent,
                    (HMENU)IDC_EXT_DETAILS, m_hInstance, nullptr);

    // Extensions list (shifted down to make room for buttons)
    m_hwndExtensionsList =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL, 5, 68,
                        SIDEBAR_DEFAULT_WIDTH - 10, 527, hwndParent, (HMENU)IDC_EXT_LIST, m_hInstance, nullptr);

    // Setup columns
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 150;
    col.pszText = (LPSTR) "Name";
    ListView_InsertColumn(m_hwndExtensionsList, 0, &col);

    col.cx = 60;
    col.pszText = (LPSTR) "Version";
    ListView_InsertColumn(m_hwndExtensionsList, 1, &col);

    appendToOutput("Extensions view created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::searchExtensions(const std::string& query)
{
    LOG_FUNCTION();
    if (!m_hwndExtensionsList)
        return;

    // Filter loaded extensions by query
    ListView_DeleteAllItems(m_hwndExtensionsList);
    s_extensionDisplayIds.clear();

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    int idx = 0;

    for (const auto& ext : m_extensions)
    {
        std::string lowerName = ext.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string lowerId = ext.id;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        std::string lowerDesc = ext.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerQuery.empty() || lowerName.find(lowerQuery) != std::string::npos ||
            lowerId.find(lowerQuery) != std::string::npos || lowerDesc.find(lowerQuery) != std::string::npos)
        {

            s_extensionDisplayIds.push_back(ext.id);
            item.iItem = idx;
            item.iSubItem = 0;
            item.pszText = (LPSTR)ext.name.c_str();
            ListView_InsertItem(m_hwndExtensionsList, &item);

            item.iSubItem = 1;
            item.pszText = (LPSTR)ext.version.c_str();
            ListView_SetItem(m_hwndExtensionsList, &item);
            idx++;
        }
    }

    // Also scan plugins directory for unloaded extensions
    std::string pluginsDir = m_explorerRootPath + "\\plugins";
    if (fs::exists(pluginsDir) && fs::is_directory(pluginsDir))
    {
        for (const auto& entry : fs::directory_iterator(pluginsDir))
        {
            if (!entry.is_directory())
                continue;
            std::string dirName = entry.path().filename().string();
            std::string lowerDir = dirName;
            std::transform(lowerDir.begin(), lowerDir.end(), lowerDir.begin(), ::tolower);

            // Skip if already in loaded list
            bool alreadyLoaded = false;
            for (const auto& ext : m_extensions)
            {
                if (ext.id == dirName)
                {
                    alreadyLoaded = true;
                    break;
                }
            }
            if (alreadyLoaded)
                continue;

            if (lowerQuery.empty() || lowerDir.find(lowerQuery) != std::string::npos)
            {
                s_extensionDisplayIds.push_back(dirName);
                item.iItem = idx;
                item.iSubItem = 0;
                item.pszText = (LPSTR)dirName.c_str();
                ListView_InsertItem(m_hwndExtensionsList, &item);

                std::string notInstalled = "(available)";
                item.iSubItem = 1;
                item.pszText = (LPSTR)notInstalled.c_str();
                ListView_SetItem(m_hwndExtensionsList, &item);
                idx++;
            }
        }
    }

    appendToOutput("Extension search '" + query + "': " + std::to_string(idx) + " results\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::installExtension(const std::string& extensionId)
{
    LOG_FUNCTION();

    // Check if extension directory exists in plugins/
    std::string pluginsDir = m_explorerRootPath + "\\plugins";
    std::string extDir = pluginsDir + "\\" + extensionId;

    if (!fs::exists(extDir))
    {
        // Try loading as a .vsix file
        std::string vsixPath = pluginsDir + "\\" + extensionId + ".vsix";
        if (fs::exists(vsixPath))
        {
            extDir = vsixPath;
        }
        else
        {
            appendToOutput("Extension not found: " + extensionId + "\nPlace extension in: " + pluginsDir + "\n",
                           "Output", OutputSeverity::Warning);
            return;
        }
    }

    // Use VSIXLoader to load
    auto& loader = VSIXLoader::GetInstance();
    if (loader.LoadPlugin(extDir))
    {
        // Add to our internal list
        Extension info;
        info.id = extensionId;
        info.name = extensionId;
        info.version = "1.0.0";
        info.description = "Loaded from plugins directory";
        info.author = "Local";
        info.installed = true;
        info.enabled = true;

        // Try to read package.json for real metadata
        std::string manifestPath = extDir + "\\package.json";
        if (fs::exists(manifestPath))
        {
            try
            {
                std::ifstream mf(manifestPath);
                std::string content((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
                nlohmann::json manifest = nlohmann::json::parse(content);
                if (manifest.contains("name"))
                    info.name = manifest["name"].get<std::string>();
                if (manifest.contains("version"))
                    info.version = manifest["version"].get<std::string>();
                if (manifest.contains("description"))
                    info.description = manifest["description"].get<std::string>();
                if (manifest.contains("publisher"))
                    info.author = manifest["publisher"].get<std::string>();
            }
            catch (...)
            {
            }
        }

        m_extensions.push_back(info);
        loadInstalledExtensions();  // Refresh UI

        appendToOutput("Installed extension: " + info.name + " v" + info.version + "\n", "Output",
                       OutputSeverity::Info);
        LOG_INFO("Installed extension: " + extensionId);
    }
    else
    {
        appendToOutput("Failed to install extension: " + extensionId + "\n", "Output", OutputSeverity::Error);
        LOG_ERROR("Failed to install extension: " + extensionId);
    }
}

void Win32IDE::installFromVSIXFile()
{
    LOG_FUNCTION();
    wchar_t filePath[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"VSIX Packages (*.vsix)\0*.vsix\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Install VSIX Extension";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn))
        return;

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Path(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, utf8Path.data(), utf8Len, nullptr, nullptr);

    if (RawrXD::VSIXInstaller::Install(utf8Path))
    {
        std::string extStem = std::filesystem::path(utf8Path).stem().string();
        std::string installDir = RawrXD::GetExtensionsInstallRoot() + extStem;
        auto& loader = VSIXLoader::GetInstance();
        if (fs::exists(installDir) && fs::is_directory(installDir))
        {
            if (loader.LoadPlugin(installDir))
                appendToOutput("VSIXLoader: loaded " + installDir + "\n", "Output", OutputSeverity::Info);
        }
        loadInstalledExtensions();
        appendToOutput("Installed VSIX to %APPDATA%\\RawrXD\\extensions: " + utf8Path + "\n", "Output",
                       OutputSeverity::Info);
        // Extension host: load into QuickJS immediately (no stub)
        auto& jsHost = QuickJSExtensionHost::instance();
        auto result = jsHost.installVSIX(utf8Path.c_str());
        if (result.success)
        {
            appendToOutput(std::string("[Extension Host] Loaded: ") + (result.detail ? result.detail : "") + "\n",
                           "Output", OutputSeverity::Info);
        }
        else
        {
            appendToOutput(std::string("[Extension Host] ") + (result.detail ? result.detail : "") +
                               " (native-only or stub build)\n",
                           "Output", OutputSeverity::Info);
        }
        MessageBoxW(m_hwndMain, L"Extension installed successfully.", L"VSIX Install", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        appendToOutput("Failed to install VSIX: " + utf8Path + "\n", "Output", OutputSeverity::Error);
        MessageBoxW(
            m_hwndMain,
            L"Installation failed. Set RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1 for unsigned extensions. See Output panel.",
            L"VSIX Install", MB_OK | MB_ICONWARNING);
    }
}

void Win32IDE::uninstallExtension(const std::string& extensionId)
{
    LOG_FUNCTION();

    // Unload from VSIXLoader
    auto& loader = VSIXLoader::GetInstance();
    loader.UnloadPlugin(extensionId);

    // Remove from internal list
    m_extensions.erase(std::remove_if(m_extensions.begin(), m_extensions.end(),
                                      [&](const Extension& ext) { return ext.id == extensionId; }),
                       m_extensions.end());

    // Optionally remove from disk
    std::string extDir = m_explorerRootPath + "\\plugins\\" + extensionId;
    if (fs::exists(extDir))
    {
        const std::wstring msg = utf8ToWide("Delete extension files from disk?\n" + extDir);
        if (MessageBoxW(m_hwndMain, msg.c_str(), L"Uninstall Extension", MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            try
            {
                fs::remove_all(extDir);
            }
            catch (...)
            {
            }
        }
    }

    loadInstalledExtensions();  // Refresh UI
    appendToOutput("Uninstalled extension: " + extensionId + "\n", "Output", OutputSeverity::Info);
    LOG_INFO("Uninstalled extension: " + extensionId);
}

void Win32IDE::enableExtension(const std::string& extensionId)
{
    for (auto& ext : m_extensions)
    {
        if (ext.id == extensionId)
        {
            ext.enabled = true;
            appendToOutput("Extension enabled: " + extensionId + "\n", "Output", OutputSeverity::Info);
            break;
        }
    }
}

void Win32IDE::disableExtension(const std::string& extensionId)
{
    for (auto& ext : m_extensions)
    {
        if (ext.id == extensionId)
        {
            ext.enabled = false;
            appendToOutput("Extension disabled: " + extensionId + "\n", "Output", OutputSeverity::Info);
            break;
        }
    }
}

void Win32IDE::updateExtension(const std::string& extensionId)
{
    LOG_FUNCTION();

    auto& loader = VSIXLoader::GetInstance();
    if (loader.ReloadPlugin(extensionId))
    {
        appendToOutput("Extension updated: " + extensionId + "\n", "Output", OutputSeverity::Info);
        loadInstalledExtensions();
    }
    else
    {
        appendToOutput("Failed to update extension: " + extensionId + "\n", "Output", OutputSeverity::Error);
    }
}

void Win32IDE::showExtensionDetails(const std::string& extensionId)
{
    LOG_FUNCTION();

    std::string details;
    // Check internal list first
    for (const auto& ext : m_extensions)
    {
        if (ext.id == extensionId)
        {
            details += "Name: " + ext.name + "\n";
            details += "ID: " + ext.id + "\n";
            details += "Version: " + ext.version + "\n";
            details += "Author: " + ext.author + "\n";
            details += "Description: " + ext.description + "\n";
            details += "Status: " + std::string(ext.enabled ? "Enabled" : "Disabled") + "\n";
            details += "Installed: " + std::string(ext.installed ? "Yes" : "No") + "\n";
            break;
        }
    }

    // Check VSIXLoader for additional info (commands, dependencies)
    auto& loader = VSIXLoader::GetInstance();
    VSIXPlugin* plugin = loader.GetPlugin(extensionId);
    if (plugin)
    {
        details += "\n--- Plugin Details ---\n";
        details += "Install Path: " + plugin->install_path.string() + "\n";
        if (!plugin->commands.empty())
        {
            details += "Commands:\n";
            for (const auto& cmd : plugin->commands)
            {
                details += "  - " + cmd + "\n";
            }
        }
        if (!plugin->dependencies.empty())
        {
            details += "Dependencies:\n";
            for (const auto& dep : plugin->dependencies)
            {
                details += "  - " + dep + "\n";
            }
        }
    }

    if (details.empty())
    {
        details = "No details available for: " + extensionId;
    }

    MessageBoxA(m_hwndMain, details.c_str(), ("Extension: " + extensionId).c_str(), MB_OK | MB_ICONINFORMATION);
    appendToOutput("Extension details shown for: " + extensionId + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::loadInstalledExtensions()
{
    LOG_FUNCTION();
    if (!m_hwndExtensionsList)
        return;

    ListView_DeleteAllItems(m_hwndExtensionsList);
    m_extensions.clear();

    // Load from VSIXLoader (already-loaded plugins)
    auto& loader = VSIXLoader::GetInstance();
    auto loadedPlugins = loader.GetLoadedPlugins();
    for (auto* plugin : loadedPlugins)
    {
        Extension info;
        info.id = plugin->id;
        info.name = plugin->name;
        info.version = plugin->version;
        info.description = plugin->description;
        info.author = plugin->author;
        info.installed = true;
        info.enabled = plugin->enabled;
        m_extensions.push_back(info);
    }

    // Scan %APPDATA%\RawrXD\extensions (VSIXInstaller install location)
    std::string appDataExt = RawrXD::GetExtensionsInstallRoot();
    if (fs::exists(appDataExt) && fs::is_directory(appDataExt))
    {
        for (const auto& entry : fs::directory_iterator(appDataExt))
        {
            if (!entry.is_directory())
                continue;
            std::string dirName = entry.path().filename().string();
            bool found = false;
            for (const auto& ext : m_extensions)
            {
                if (ext.id == dirName)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;
            Extension info;
            info.id = dirName;
            info.name = dirName;
            info.version = "0.0.0";
            info.description = "";
            info.author = "";
            info.installed = true;
            info.enabled = true;
            // VS Code VSIX layout: extension/package.json (Amazon Q, Copilot, etc.)
            std::string basePath = entry.path().string();
            std::string manifestPath = basePath + "\\extension\\package.json";
            if (!fs::exists(manifestPath))
                manifestPath = basePath + "\\package.json";
            if (fs::exists(manifestPath))
            {
                try
                {
                    std::ifstream mf(manifestPath);
                    std::string manifestStr((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
                    nlohmann::json manifest = nlohmann::json::parse(manifestStr);
                    std::string pub = manifest.value("publisher", "");
                    std::string name = manifest.value("name", "");
                    if (!pub.empty() && !name.empty())
                        info.id = pub + "." + name;
                    if (manifest.contains("displayName"))
                        info.name = manifest["displayName"].get<std::string>();
                    else if (!name.empty())
                        info.name = name;
                    if (manifest.contains("version"))
                        info.version = manifest["version"].get<std::string>();
                    if (manifest.contains("description"))
                        info.description = manifest["description"].get<std::string>();
                    if (!pub.empty())
                        info.author = pub;
                }
                catch (...)
                {
                }
            }
            m_extensions.push_back(info);
        }
    }

    // Scan plugins directory for directory-based plugins and .vsix files not already in m_extensions
    std::string pluginsDir = m_explorerRootPath + "\\plugins";
    if (fs::exists(pluginsDir) && fs::is_directory(pluginsDir))
    {
        for (const auto& entry : fs::directory_iterator(pluginsDir))
        {
            std::string dirName = entry.path().filename().string();
            bool isVsix = false;
            if (entry.is_regular_file())
            {
                std::string ext = dirName.size() >= 5 ? dirName.substr(dirName.size() - 5) : "";
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".vsix")
                {
                    isVsix = true;
                    dirName = entry.path().stem().string();
                }
            }
            if (!entry.is_directory() && !isVsix)
                continue;

            // Skip if already in the list
            bool found = false;
            for (const auto& ext : m_extensions)
            {
                if (ext.id == dirName)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;

            Extension info;
            info.id = dirName;
            info.name = dirName;
            info.version = "0.0.0";
            info.description = isVsix ? "(Install .vsix)" : "";
            info.author = "";
            info.installed = !isVsix;
            info.enabled = false;

            if (isVsix)
            {
                m_extensions.push_back(info);
                continue;
            }

            // Try reading package.json
            std::string manifestPath = entry.path().string() + "\\package.json";
            if (fs::exists(manifestPath))
            {
                try
                {
                    std::ifstream mf(manifestPath);
                    std::string manifestStr((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
                    nlohmann::json manifest = nlohmann::json::parse(manifestStr);
                    if (manifest.contains("displayName"))
                        info.name = manifest["displayName"].get<std::string>();
                    else if (manifest.contains("name"))
                        info.name = manifest["name"].get<std::string>();
                    if (manifest.contains("version"))
                        info.version = manifest["version"].get<std::string>();
                    if (manifest.contains("description"))
                        info.description = manifest["description"].get<std::string>();
                    if (manifest.contains("publisher"))
                        info.author = manifest["publisher"].get<std::string>();
                }
                catch (...)
                {
                }
            }

            m_extensions.push_back(info);
        }
    }

    // Always include built-in extensions
    auto addBuiltIn = [&](const std::string& id, const std::string& name, const std::string& ver,
                          const std::string& desc, const std::string& author)
    {
        bool found = false;
        for (const auto& ext : m_extensions)
        {
            if (ext.id == id)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            m_extensions.push_back({id, name, ver, desc, author, true, true});
        }
    };
    addBuiltIn("rawrxd.inference", "RawrXD Inference", "7.1.0", "Native CPU inference engine", "RawrXD");
    addBuiltIn("rawrxd.agentic", "RawrXD Agentic", "7.1.0", "Agentic AI bridge", "RawrXD");
    addBuiltIn("rawrxd.reverse-eng", "RawrXD Reverse Engineering", "7.1.0", "Disassembler, DumpBin, Compiler",
               "RawrXD");

    // Populate ListView and display-id map
    s_extensionDisplayIds.clear();
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    for (size_t i = 0; i < m_extensions.size(); i++)
    {
        s_extensionDisplayIds.push_back(m_extensions[i].id);
        item.iItem = (int)i;
        item.iSubItem = 0;
        item.pszText = (LPSTR)m_extensions[i].name.c_str();
        ListView_InsertItem(m_hwndExtensionsList, &item);

        item.iSubItem = 1;
        item.pszText = (LPSTR)m_extensions[i].version.c_str();
        ListView_SetItem(m_hwndExtensionsList, &item);
    }

    appendToOutput("Loaded " + std::to_string(m_extensions.size()) + " extensions\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// Outline View Implementation - Shows code structure (functions, classes, etc.)
// ============================================================================

void Win32IDE::createOutlineView(HWND hwndParent)
{
    // Create TreeView for outline
    m_hwndOutlineTree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
                                        WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                                        0, 0, 280, 300, hwndParent, nullptr, m_hInstance, nullptr);

    SetWindowLongPtrA(m_hwndOutlineTree, GWLP_USERDATA, (LONG_PTR)this);

    appendToOutput("Outline view created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::updateOutlineView()
{
    if (!m_hwndOutlineTree)
        return;

    TreeView_DeleteAllItems(m_hwndOutlineTree);
    m_outlineItems.clear();

    // Parse current file for outline
    parseCodeForOutline();

    // Populate tree view
    TVINSERTSTRUCTA tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

    for (size_t i = 0; i < m_outlineItems.size(); ++i)
    {
        const auto& item = m_outlineItems[i];
        std::string text = item.type + " " + item.name + " (line " + std::to_string(item.line) + ")";
        tvis.item.pszText = (LPSTR)text.c_str();
        tvis.item.lParam = (LPARAM)i;
        TreeView_InsertItem(m_hwndOutlineTree, &tvis);
    }
}

void Win32IDE::parseCodeForOutline()
{
    if (!m_hwndEditor)
        return;

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen == 0)
        return;

    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Simple regex-based parsing for common patterns
    std::regex functionRegex(R"((function|def|void|int|string|bool|public|private)\s+(\w+)\s*\()");
    std::regex classRegex(R"((class|struct|interface)\s+(\w+))");
    std::regex variableRegex(R"(\$([\w_]+)\s*=)");

    // Split by lines for line number tracking
    std::istringstream stream(text);
    std::string line;
    int lineNum = 1;

    while (std::getline(stream, line))
    {
        std::smatch match;

        // Check for function definitions
        if (std::regex_search(line, match, functionRegex))
        {
            OutlineItem item;
            item.type = "function";
            item.name = match[2].str();
            item.line = lineNum;
            item.column = (int)match.position();
            m_outlineItems.push_back(item);
        }

        // Check for class definitions
        if (std::regex_search(line, match, classRegex))
        {
            OutlineItem item;
            item.type = match[1].str();
            item.name = match[2].str();
            item.line = lineNum;
            item.column = (int)match.position();
            m_outlineItems.push_back(item);
        }

        // Check for PowerShell variables (top-level)
        if (std::regex_search(line, match, variableRegex))
        {
            // Only add if it looks like a significant variable (not inside a function)
            if (line.find("function") == std::string::npos)
            {
                OutlineItem item;
                item.type = "variable";
                item.name = "$" + match[1].str();
                item.line = lineNum;
                item.column = (int)match.position();
                m_outlineItems.push_back(item);
            }
        }

        ++lineNum;
    }

    appendToOutput("Parsed " + std::to_string(m_outlineItems.size()) + " outline items\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::goToOutlineItem(int index)
{
    if (index < 0 || index >= (int)m_outlineItems.size())
        return;

    const auto& item = m_outlineItems[index];

    // Navigate to line in editor
    int charIndex = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, item.line - 1, 0);
    charIndex += item.column;

    SendMessage(m_hwndEditor, EM_SETSEL, charIndex, charIndex);
    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
    SetFocus(m_hwndEditor);

    appendToOutput("Navigated to: " + item.name + " at line " + std::to_string(item.line) + "\n", "Output",
                   OutputSeverity::Info);
}

// ============================================================================
// Timeline View Implementation - Shows file history (Git commits, local saves)
// ============================================================================

void Win32IDE::createTimelineView(HWND hwndParent)
{
    // Create ListView for timeline
    m_hwndTimelineList =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                        0, 0, 280, 200, hwndParent, nullptr, m_hInstance, nullptr);

    // Add columns
    LVCOLUMNA lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.pszText = (LPSTR) "Date";
    lvc.cx = 80;
    lvc.iSubItem = 0;
    ListView_InsertColumn(m_hwndTimelineList, 0, &lvc);

    lvc.pszText = (LPSTR) "Author";
    lvc.cx = 70;
    lvc.iSubItem = 1;
    ListView_InsertColumn(m_hwndTimelineList, 1, &lvc);

    lvc.pszText = (LPSTR) "Message";
    lvc.cx = 120;
    lvc.iSubItem = 2;
    ListView_InsertColumn(m_hwndTimelineList, 2, &lvc);

    SetWindowLongPtrA(m_hwndTimelineList, GWLP_USERDATA, (LONG_PTR)this);

    appendToOutput("Timeline view created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::updateTimelineView()
{
    if (!m_hwndTimelineList)
        return;

    ListView_DeleteAllItems(m_hwndTimelineList);
    m_timelineEntries.clear();

    // Load Git history for current file
    loadGitHistory();

    // Populate list view
    LVITEMA lvi = {0};
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < m_timelineEntries.size(); ++i)
    {
        const auto& entry = m_timelineEntries[i];

        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)entry.date.c_str();
        ListView_InsertItem(m_hwndTimelineList, &lvi);

        // Use SendMessage directly for setting item text
        LVITEMA lviSet = {0};
        lviSet.iSubItem = 1;
        lviSet.pszText = (LPSTR)entry.author.c_str();
        SendMessage(m_hwndTimelineList, LVM_SETITEMTEXTA, (int)i, (LPARAM)&lviSet);

        lviSet.iSubItem = 2;
        lviSet.pszText = (LPSTR)entry.message.c_str();
        SendMessage(m_hwndTimelineList, LVM_SETITEMTEXTA, (int)i, (LPARAM)&lviSet);
    }
}

void Win32IDE::loadGitHistory()
{
    if (m_currentFile.empty() || !isGitRepository())
    {
        // Fallback timeline entry when not in a Git repo or no file open
        TimelineEntry entry;
        entry.message = "File opened";
        entry.author = "Local";
        entry.date = "Today";
        entry.isGitCommit = false;
        m_timelineEntries.push_back(entry);
        return;
    }

    // Run git log for current file
    std::string command = "git log --oneline -10 --format=\"%h|%an|%ad|%s\" --date=short -- \"" + m_currentFile + "\"";
    std::string output;

    if (executeGitCommand(command, output))
    {
        std::istringstream stream(output);
        std::string line;

        while (std::getline(stream, line))
        {
            if (line.empty())
                continue;

            // Parse: hash|author|date|message
            std::vector<std::string> parts;
            size_t start = 0;
            size_t end = 0;
            while ((end = line.find('|', start)) != std::string::npos)
            {
                parts.push_back(line.substr(start, end - start));
                start = end + 1;
            }
            parts.push_back(line.substr(start));

            if (parts.size() >= 4)
            {
                TimelineEntry entry;
                entry.commitHash = parts[0];
                entry.author = parts[1];
                entry.date = parts[2];
                entry.message = parts[3];
                entry.isGitCommit = true;
                m_timelineEntries.push_back(entry);
            }
        }
    }

    appendToOutput("Loaded " + std::to_string(m_timelineEntries.size()) + " timeline entries\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::goToTimelineEntry(int index)
{
    if (index < 0 || index >= (int)m_timelineEntries.size())
        return;

    const auto& entry = m_timelineEntries[index];

    if (entry.isGitCommit && !entry.commitHash.empty())
    {
        // Show diff for this commit
        std::string command = "git show " + entry.commitHash + " -- \"" + m_currentFile + "\"";
        std::string output;

        if (executeGitCommand(command, output))
        {
            // Display in output panel
            appendToOutput("\n=== Commit: " + entry.commitHash + " ===\n", "Output", OutputSeverity::Info);
            appendToOutput(output + "\n", "Output", OutputSeverity::Info);
        }
    }
    else
    {
        appendToOutput("Selected local entry: " + entry.message + "\n", "Output", OutputSeverity::Info);
    }
}

// ============================================================================
// Git Panel Window Procedure
// ============================================================================

LRESULT CALLBACK Win32IDE::GitPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Dark background matching VS Code SCM panel
            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // Draw header text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(204, 204, 204));
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            RECT rcHeader = rc;
            rcHeader.left += 8;
            rcHeader.top += 4;
            rcHeader.bottom = rcHeader.top + 24;
            DrawTextA(hdc, "SOURCE CONTROL", -1, &rcHeader, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, hOldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);

                // Resize the git status text
                if (pThis->m_hwndGitStatusText)
                {
                    MoveWindow(pThis->m_hwndGitStatusText, 8, 30, rc.right - 16, 60, TRUE);
                }

                // Resize the git file list
                if (pThis->m_hwndGitFileList)
                {
                    MoveWindow(pThis->m_hwndGitFileList, 8, 96, rc.right - 16, rc.bottom - 104, TRUE);
                }
            }
            return 0;
        }

        case WM_COMMAND:
        {
            if (pThis)
            {
                int id = LOWORD(wParam);
                switch (id)
                {
                    case IDC_SCM_STAGE:
                        pThis->stageSelectedFiles();
                        return 0;
                    case IDC_SCM_UNSTAGE:
                        pThis->unstageSelectedFiles();
                        return 0;
                    case IDC_SCM_COMMIT:
                        pThis->commitChangesFromSidebar();
                        return 0;
                    case IDC_SCM_SYNC:
                        pThis->syncRepository();
                        return 0;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            // Dark theme colors for child controls
            HDC hdcChild = (HDC)wParam;
            SetTextColor(hdcChild, RGB(204, 204, 204));
            SetBkColor(hdcChild, RGB(30, 30, 30));
            static HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            return (LRESULT)hDarkBrush;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Module Panel Window Procedure
// ============================================================================

LRESULT CALLBACK Win32IDE::ModulePanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Dark background for module browser panel
            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // Draw header
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(204, 204, 204));
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            RECT rcHeader = rc;
            rcHeader.left += 8;
            rcHeader.top += 4;
            rcHeader.bottom = rcHeader.top + 24;
            DrawTextA(hdc, "MODULE BROWSER", -1, &rcHeader, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, hOldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);

                // Resize module list
                if (pThis->m_hwndModuleList)
                {
                    MoveWindow(pThis->m_hwndModuleList, 8, 62, rc.right - 16, rc.bottom - 70, TRUE);
                }

                // Reposition buttons horizontally in toolbar area
                int btnY = 30;
                int btnW = 65;
                int btnH = 25;
                int btnX = 8;
                if (pThis->m_hwndModuleLoadButton)
                {
                    MoveWindow(pThis->m_hwndModuleLoadButton, btnX, btnY, btnW, btnH, TRUE);
                    btnX += btnW + 5;
                }
                if (pThis->m_hwndModuleUnloadButton)
                {
                    MoveWindow(pThis->m_hwndModuleUnloadButton, btnX, btnY, btnW, btnH, TRUE);
                    btnX += btnW + 5;
                }
                if (pThis->m_hwndModuleRefreshButton)
                {
                    MoveWindow(pThis->m_hwndModuleRefreshButton, btnX, btnY, btnW, btnH, TRUE);
                }
            }
            return 0;
        }

        case WM_COMMAND:
        {
            if (pThis)
            {
                HWND hCtrl = (HWND)lParam;

                if (hCtrl == pThis->m_hwndModuleLoadButton)
                {
                    // Get selected module from the list
                    if (pThis->m_hwndModuleList)
                    {
                        int sel = (int)SendMessageA(pThis->m_hwndModuleList, LB_GETCURSEL, 0, 0);
                        if (sel != LB_ERR && sel < (int)pThis->m_modules.size())
                        {
                            pThis->loadModule(pThis->m_modules[sel].name);
                            pThis->m_moduleListDirty = true;
                        }
                        else
                        {
                            pThis->appendToOutput("No module selected for loading\n", "Output",
                                                  OutputSeverity::Warning);
                        }
                    }
                    return 0;
                }

                if (hCtrl == pThis->m_hwndModuleUnloadButton)
                {
                    if (pThis->m_hwndModuleList)
                    {
                        int sel = (int)SendMessageA(pThis->m_hwndModuleList, LB_GETCURSEL, 0, 0);
                        if (sel != LB_ERR && sel < (int)pThis->m_modules.size())
                        {
                            pThis->unloadModule(pThis->m_modules[sel].name);
                            pThis->m_moduleListDirty = true;
                        }
                        else
                        {
                            pThis->appendToOutput("No module selected for unloading\n", "Output",
                                                  OutputSeverity::Warning);
                        }
                    }
                    return 0;
                }

                if (hCtrl == pThis->m_hwndModuleRefreshButton)
                {
                    // Refresh the module list from PowerShell
                    pThis->m_moduleListDirty = true;
                    if (pThis->m_hwndModuleList)
                    {
                        SendMessageA(pThis->m_hwndModuleList, LB_RESETCONTENT, 0, 0);
                        for (const auto& mod : pThis->m_modules)
                        {
                            std::string display = mod.name;
                            if (mod.loaded)
                                display += " [loaded]";
                            if (!mod.version.empty())
                                display += " v" + mod.version;
                            SendMessageA(pThis->m_hwndModuleList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
                        }
                    }
                    pThis->appendToOutput("Module list refreshed\n", "Output", OutputSeverity::Info);
                    return 0;
                }
            }
            break;
        }

        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
        {
            HDC hdcChild = (HDC)wParam;
            SetTextColor(hdcChild, RGB(204, 204, 204));
            SetBkColor(hdcChild, RGB(30, 30, 30));
            static HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            return (LRESULT)hDarkBrush;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Commit Dialog Window Procedure
// ============================================================================

LRESULT CALLBACK Win32IDE::CommitDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_CREATE:
        {
            // Dark background
            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            SetClassLongPtrA(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);

            HINSTANCE hInst = ((CREATESTRUCTA*)lParam)->hInstance;

            // Commit message label
            CreateWindowExA(0, "STATIC", "Commit Message:", WS_CHILD | WS_VISIBLE, 10, 10, 280, 20, hwnd, nullptr,
                            hInst, nullptr);

            // Commit message edit (multi-line)
            CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 10, 35, 360, 120, hwnd,
                            (HMENU)8001, hInst, nullptr);

            // OK / Cancel buttons
            CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 200, 170, 80, 30, hwnd,
                            (HMENU)IDOK, hInst, nullptr);

            CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 290, 170, 80, 30, hwnd,
                            (HMENU)IDCANCEL, hInst, nullptr);

            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);

            if (id == IDOK && pThis)
            {
                // Retrieve commit message from the edit control
                HWND hEdit = GetDlgItem(hwnd, 8001);
                if (hEdit)
                {
                    int len = GetWindowTextLengthA(hEdit);
                    if (len > 0)
                    {
                        std::string message(len + 1, '\0');
                        GetWindowTextA(hEdit, &message[0], len + 1);
                        message.resize(len);

                        // Execute git commit
                        std::string cmd = "git commit -m \"" + message + "\"";
                        std::string output;
                        if (pThis->executeGitCommand(cmd, output))
                        {
                            pThis->appendToOutput("✅ Committed: " + message + "\n", "Output", OutputSeverity::Info);
                            pThis->refreshSourceControlView();
                        }
                        else
                        {
                            pThis->appendToOutput("❌ Commit failed: " + output + "\n", "Output",
                                                  OutputSeverity::Error);
                        }
                    }
                    else
                    {
                        pThis->appendToOutput("⚠️ Cannot commit with empty message\n", "Output",
                                              OutputSeverity::Warning);
                        return 0;  // Don't close dialog
                    }
                }
                DestroyWindow(hwnd);
                if (pThis)
                    pThis->m_hwndCommitDialog = nullptr;
                return 0;
            }

            if (id == IDCANCEL)
            {
                DestroyWindow(hwnd);
                if (pThis)
                    pThis->m_hwndCommitDialog = nullptr;
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            if (pThis)
                pThis->m_hwndCommitDialog = nullptr;
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            HDC hdcChild = (HDC)wParam;
            SetTextColor(hdcChild, RGB(204, 204, 204));
            SetBkColor(hdcChild, RGB(30, 30, 30));
            static HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            return (LRESULT)hDarkBrush;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// File Explorer Window Procedure
// ============================================================================

LRESULT CALLBACK Win32IDE::FileExplorerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Dark background for file explorer container
            HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);

                // Resize the file tree to fill the explorer container
                if (pThis->m_hwndFileTree)
                {
                    MoveWindow(pThis->m_hwndFileTree, 0, 0, rc.right, rc.bottom, TRUE);
                }
            }
            return 0;
        }

        case WM_NOTIFY:
        {
            if (pThis)
            {
                NMHDR* pnmh = (NMHDR*)lParam;
                if (pnmh->idFrom == IDC_FILE_TREE || pnmh->idFrom == IDC_EXPLORER_TREE ||
                    pnmh->idFrom == IDC_FILE_EXPLORER)
                {
                    switch (pnmh->code)
                    {
                        case TVN_DELETEITEM:
                        {
                            NMTREEVIEWA* pnmtv = (NMTREEVIEWA*)lParam;
                            if (pnmtv->itemOld.lParam)
                            {
                                if (pnmh->idFrom == IDC_FILE_EXPLORER)
                                {
                                    delete[] reinterpret_cast<char*>(pnmtv->itemOld.lParam);
                                }
                                /* IDC_FILE_TREE uses std::string* (freed in FileExplorerContainerProc);
                                 * IDC_EXPLORER_TREE uses lParam as flag/index, no heap */
                            }
                            return 0;
                        }

                        case TVN_SELCHANGEDA:
                        {
                            NMTREEVIEWA* pnmtv = (NMTREEVIEWA*)lParam;
                            if (pnmtv->itemNew.hItem)
                            {
                                pThis->onFileTreeSelect(pnmtv->itemNew.hItem);
                            }
                            return 0;
                        }

                        case NM_DBLCLK:
                        {
                            HTREEITEM hItem = TreeView_GetSelection(pnmh->hwndFrom);
                            if (hItem)
                            {
                                pThis->onFileTreeDoubleClick(hItem);
                            }
                            return 0;
                        }

                        case TVN_ITEMEXPANDINGA:
                        {
                            NMTREEVIEWA* pnmtv = (NMTREEVIEWA*)lParam;
                            if ((pnmtv->action & TVE_EXPAND) && pnmtv->itemNew.hItem)
                            {
                                std::string path = pThis->getTreeItemPath(pnmtv->itemNew.hItem);
                                if (!path.empty())
                                {
                                    pThis->onFileTreeExpand(pnmtv->itemNew.hItem, path);
                                }
                            }
                            return 0;
                        }
                    }
                }
            }
            break;
        }

        case WM_CONTEXTMENU:
        {
            if (pThis)
            {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                pThis->handleExplorerContextMenu(pt);
                return 0;
            }
            break;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// File Tree Selection and Navigation
// ============================================================================

void Win32IDE::onFileTreeSelect(HTREEITEM item)
{
    if (!item)
        return;

    std::string path = getTreeItemPath(item);
    if (path.empty())
    {
        // Try extracting from the tree item text directly
        if (m_hwndFileTree)
        {
            char buf[MAX_PATH] = {};
            TVITEMA tvi = {};
            tvi.mask = TVIF_TEXT;
            tvi.hItem = item;
            tvi.pszText = buf;
            tvi.cchTextMax = MAX_PATH;
            if (TreeView_GetItem(m_hwndFileTree, &tvi))
            {
                path = buf;
            }
        }
    }

    if (path.empty())
        return;

    // Update status bar with the selected file/folder path
    if (m_hwndStatusBar)
    {
        std::string statusText = "Selected: " + path;
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)statusText.c_str());
    }

    // Check if it's a file (has extension) or directory
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        // It's a file — show preview info in the output
        WIN32_FILE_ATTRIBUTE_DATA fileData = {};
        if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData))
        {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = fileData.nFileSizeLow;
            fileSize.HighPart = fileData.nFileSizeHigh;

            std::ostringstream oss;
            oss << "📄 " << path << " (" << fileSize.QuadPart << " bytes)";
            appendToOutput(oss.str() + "\n", "Output", OutputSeverity::Debug);
        }
    }
}

void Win32IDE::onFileTreeDoubleClick(HTREEITEM item)
{
    if (!item)
        return;

    std::string path = getTreeItemPath(item);
    if (path.empty())
    {
        // Fallback: extract from tree item text
        if (m_hwndFileTree)
        {
            char buf[MAX_PATH] = {};
            TVITEMA tvi = {};
            tvi.mask = TVIF_TEXT;
            tvi.hItem = item;
            tvi.pszText = buf;
            tvi.cchTextMax = MAX_PATH;
            if (TreeView_GetItem(m_hwndFileTree, &tvi))
            {
                path = buf;
            }
        }
    }

    if (path.empty())
        return;

    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        appendToOutput("⚠️ Cannot access: " + path + "\n", "Output", OutputSeverity::Warning);
        return;
    }

    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    {
        // It's a directory — expand the tree node
        if (m_hwndFileTree)
        {
            TreeView_Expand(m_hwndFileTree, item, TVE_TOGGLE);
        }
    }
    else
    {
        // It's a file — open it in the editor
        appendToOutput("Opening: " + path + "\n", "Output", OutputSeverity::Info);
        openFile(path);

        // Check if it's a GGUF model file and offer to load it
        std::string ext;
        size_t dotPos = path.rfind('.');
        if (dotPos != std::string::npos)
        {
            ext = path.substr(dotPos + 1);
            for (char& c : ext)
                c = (char)tolower((unsigned char)c);
        }

        if (ext == "gguf" || ext == "gguf2" || ext == "bin" || ext == "safetensors" || ext == "pt" || ext == "pth" || ext == "onnx")
        {
            const std::wstring msg = utf8ToWide("Load model file?\n\n" + path);
            int result = MessageBoxW(m_hwndMain, msg.c_str(), L"RawrXD - Load Model", MB_YESNO | MB_ICONQUESTION);
            if (result == IDYES)
            {
                loadModelFromPath(path);
            }
        }
    }
}
