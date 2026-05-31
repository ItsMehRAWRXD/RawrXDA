#include "Win32IDE.h"
#include "Win32IDE_Commands.h"

#include <commctrl.h>

namespace
{
constexpr int kActivityBarWidth = 48;
constexpr int kSidebarDefaultWidth = 250;
constexpr int kSidebarHeightHint = 600;

constexpr int IDC_ACTIVITY_EXPLORER = 6001;
constexpr int IDC_ACTIVITY_SEARCH = 6002;
constexpr int IDC_ACTIVITY_SCM = 6003;
constexpr int IDC_ACTIVITY_DEBUG = 6004;
constexpr int IDC_ACTIVITY_EXTENSIONS = 6005;
constexpr int IDC_ACTIVITY_GITHUB = 6006;
constexpr int IDC_ACTIVITY_GITHUB_PULL_RELEASE = 6007;
constexpr int IDC_ACTIVITY_ACCOUNTS = 6008;
constexpr int IDC_ACTIVITY_MANAGE = 6009;
constexpr int IDC_ACTIVITY_RECOVERY = 6010;
constexpr int IDC_ACTIVITY_CHAT = 6011;

constexpr int IDC_GITHUB_OPEN_REPO = 6060;
constexpr int IDC_GITHUB_OPEN_ISSUES = 6061;
constexpr int IDC_GITHUB_OPEN_PULLS = 6062;
constexpr int IDC_GITHUB_OPEN_RELEASES = 6063;
constexpr int IDC_ACCOUNTS_OPEN_SETTINGS = 6064;
constexpr int IDC_ACCOUNTS_OPEN_GITHUB_LOGIN = 6065;
constexpr int IDC_MANAGE_OPEN_EXTENSIONS = 6066;
constexpr int IDC_MANAGE_OPEN_SETTINGS = 6067;
constexpr int IDC_MANAGE_OPEN_LAYOUTS = 6068;

constexpr int IDC_DEBUG_CONFIGS = 6040;
constexpr int IDC_DEBUG_START = 6041;
constexpr int IDC_DEBUG_STOP = 6042;
constexpr int IDC_DEBUG_VARIABLES = 6043;
constexpr int IDC_DEBUG_VARIABLES_LABEL = 6046;

void destroySidebarChildren(HWND sidebar)
{
    if (!sidebar)
        return;

    HWND child = GetWindow(sidebar, GW_CHILD);
    while (child)
    {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
}

void createSidebarHeaderText(HWND parent, const char* title, const char* subtitle)
{
    if (!parent)
        return;

    CreateWindowExA(0, "STATIC", title, WS_CHILD | WS_VISIBLE | SS_LEFT,
                    8, 8, 220, 20, parent, nullptr, nullptr, nullptr);

    CreateWindowExA(0, "STATIC", subtitle, WS_CHILD | WS_VISIBLE | SS_LEFT,
                    8, 30, 230, 32, parent, nullptr, nullptr, nullptr);
}

void createSidebarActionButton(HWND parent, int id, const char* label, int y)
{
    if (!parent)
        return;

    CreateWindowExA(0, "BUTTON", label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    8, y, 220, 28,
                    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}
}

void Win32IDE::createActivityBar(HWND hwndParent)
{
    if (!hwndParent)
        return;

    if (m_hwndActivityBar && IsWindow(m_hwndActivityBar))
        return;

    m_hwndActivityBar = CreateWindowExA(0, "STATIC", "",
                                        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                        0, 0, kActivityBarWidth, kSidebarHeightHint,
                                        hwndParent, nullptr, m_hInstance, nullptr);
    if (!m_hwndActivityBar)
        return;

    SetWindowLongPtrA(m_hwndActivityBar, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrA(m_hwndActivityBar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ActivityBarProc));

    struct ButtonSpec
    {
        int id;
        const char* text;
    };

    const ButtonSpec buttons[] = {
        {IDC_ACTIVITY_EXPLORER, "Files"},
        {IDC_ACTIVITY_SEARCH, "Search"},
        {IDC_ACTIVITY_SCM, "Source"},
        {IDC_ACTIVITY_DEBUG, "Debug"},
        {IDC_ACTIVITY_EXTENSIONS, "Exts"},
        {IDC_ACTIVITY_GITHUB, "GitHub"},
        {IDC_ACTIVITY_GITHUB_PULL_RELEASE, "Pulls"},
        {IDC_ACTIVITY_ACCOUNTS, "Acct"},
        {IDC_ACTIVITY_MANAGE, "Mng"},
        {IDC_ACTIVITY_RECOVERY, "Recov"},
        {IDC_ACTIVITY_CHAT, "Chat"},
    };

    int y = 8;
    for (const auto& button : buttons)
    {
        CreateWindowExA(0, "BUTTON", button.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        4, y, 40, 40,
                        m_hwndActivityBar, reinterpret_cast<HMENU>(static_cast<INT_PTR>(button.id)),
                        m_hInstance, nullptr);
        y += 48;
    }
}

void Win32IDE::createPrimarySidebar(HWND hwndParent)
{
    if (!hwndParent)
        return;

    if (m_hwndSidebar && IsWindow(m_hwndSidebar))
        return;

    m_hwndSidebar = CreateWindowExA(0, "STATIC", "",
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                    kActivityBarWidth, 0, kSidebarDefaultWidth, kSidebarHeightHint,
                                    hwndParent, nullptr, m_hInstance, nullptr);
    if (!m_hwndSidebar)
        return;

    SetWindowLongPtrA(m_hwndSidebar, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrA(m_hwndSidebar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SidebarProc));

    m_sidebarVisible = true;
    m_sidebarWidth = (m_sidebarWidth > 0) ? m_sidebarWidth : kSidebarDefaultWidth;
    m_currentSidebarView = SidebarView::None;
    m_hwndSidebarContent = m_hwndSidebar;
    m_hwndSidebarTitle = nullptr;

    setSidebarView(SidebarView::Explorer);
}

void Win32IDE::toggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    if (m_hwndSidebar && IsWindow(m_hwndSidebar))
        ShowWindow(m_hwndSidebar, m_sidebarVisible ? SW_SHOW : SW_HIDE);

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        RECT rc = {};
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right - rc.left, rc.bottom - rc.top);
    }
}

void Win32IDE::toggleSecondarySidebar()
{
    m_secondarySidebarVisible = !m_secondarySidebarVisible;
    if (m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar))
        ShowWindow(m_hwndSecondarySidebar, m_secondarySidebarVisible ? SW_SHOW : SW_HIDE);

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        RECT rc = {};
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right - rc.left, rc.bottom - rc.top);
    }
}

void Win32IDE::setSidebarView(SidebarView view)
{
    if (!m_hwndSidebar || !IsWindow(m_hwndSidebar))
        return;

    m_currentSidebarView = view;

    destroySidebarChildren(m_hwndSidebar);
    m_hwndSidebarContent = m_hwndSidebar;

    m_hwndFileExplorer = nullptr;
    m_hwndSearchInput = nullptr;
    m_hwndSearchResults = nullptr;
    m_hwndSearchOptions = nullptr;
    m_hwndSearchReplace = nullptr;
    m_hwndSearchInclude = nullptr;
    m_hwndSearchExclude = nullptr;
    m_hwndSearchStatus = nullptr;
    m_hwndSCMFileList = nullptr;
    m_hwndSCMToolbar = nullptr;
    m_hwndSCMMessageBox = nullptr;
    m_hwndDebugConfigs = nullptr;
    m_hwndDebugToolbar = nullptr;
    m_hwndDebugVariables = nullptr;
    m_hwndExtensionsList = nullptr;
    m_hwndExtensionSearch = nullptr;
    m_hwndRecoveryTitle = nullptr;
    m_hwndRecoveryDriveList = nullptr;
    m_hwndRecoveryOutPath = nullptr;
    m_hwndRecoveryStatus = nullptr;
    m_hwndRecoveryProgress = nullptr;
    m_hwndRecoveryLog = nullptr;

    switch (view)
    {
        case SidebarView::Explorer:
            createExplorerView(m_hwndSidebar);
            break;
        case SidebarView::Search:
            createSearchView(m_hwndSidebar);
            break;
        case SidebarView::SourceControl:
            createSourceControlView(m_hwndSidebar);
            break;
        case SidebarView::RunDebug:
            createRunDebugView(m_hwndSidebar);
            break;
        case SidebarView::Extensions:
            createExtensionsView(m_hwndSidebar);
            break;
        case SidebarView::GitHub:
            createSidebarHeaderText(m_hwndSidebar, "GitHub", "Open repository links and collaboration surfaces.");
            createSidebarActionButton(m_hwndSidebar, IDC_GITHUB_OPEN_REPO, "Open Repository", 76);
            createSidebarActionButton(m_hwndSidebar, IDC_GITHUB_OPEN_ISSUES, "Open Issues", 110);
            break;
        case SidebarView::GitHubPullRelease:
            createSidebarHeaderText(m_hwndSidebar, "GitHub Pull / Release", "Review pull requests and release streams.");
            createSidebarActionButton(m_hwndSidebar, IDC_GITHUB_OPEN_PULLS, "Open Pull Requests", 76);
            createSidebarActionButton(m_hwndSidebar, IDC_GITHUB_OPEN_RELEASES, "Open Releases", 110);
            break;
        case SidebarView::Accounts:
            createSidebarHeaderText(m_hwndSidebar, "Accounts", "Manage identity and sign-in state.");
            createSidebarActionButton(m_hwndSidebar, IDC_ACCOUNTS_OPEN_SETTINGS, "Open Account Settings", 76);
            createSidebarActionButton(m_hwndSidebar, IDC_ACCOUNTS_OPEN_GITHUB_LOGIN, "Sign In: GitHub", 110);
            break;
        case SidebarView::Manage:
            createSidebarHeaderText(m_hwndSidebar, "Manage", "Open settings, layouts, and extension controls.");
            createSidebarActionButton(m_hwndSidebar, IDC_MANAGE_OPEN_EXTENSIONS, "Open Extensions", 76);
            createSidebarActionButton(m_hwndSidebar, IDC_MANAGE_OPEN_SETTINGS, "Open Settings", 110);
            createSidebarActionButton(m_hwndSidebar, IDC_MANAGE_OPEN_LAYOUTS, "Open Layout Profiles", 144);
            break;
        case SidebarView::DiskRecovery:
            createDiskRecoveryView(m_hwndSidebar);
            break;
        case SidebarView::None:
        default:
            break;
    }

    updateSidebarContent();
    InvalidateRect(m_hwndSidebar, nullptr, TRUE);
    UpdateWindow(m_hwndSidebar);
}

void Win32IDE::updateSidebarContent()
{
    switch (m_currentSidebarView)
    {
        case SidebarView::Explorer:
            refreshFileTree();
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
        case SidebarView::GitHub:
        case SidebarView::GitHubPullRelease:
        case SidebarView::Accounts:
        case SidebarView::Manage:
        case SidebarView::Search:
        case SidebarView::DiskRecovery:
        case SidebarView::None:
        default:
            break;
    }
}

void Win32IDE::resizeSidebar(int width, int height)
{
    m_sidebarWidth = width;
    if (m_hwndFileExplorer && IsWindow(m_hwndFileExplorer))
        MoveWindow(m_hwndFileExplorer, 0, 0, width, height, TRUE);
}

LRESULT CALLBACK Win32IDE::ActivityBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (uMsg)
    {
        case WM_COMMAND:
            if (!ide)
                return 0;
            switch (LOWORD(wParam))
            {
                case IDC_ACTIVITY_EXPLORER:
                    ide->setSidebarView(SidebarView::Explorer);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_SEARCH:
                    ide->setSidebarView(SidebarView::Search);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_SCM:
                    ide->setSidebarView(SidebarView::SourceControl);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_DEBUG:
                    ide->setSidebarView(SidebarView::RunDebug);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_EXTENSIONS:
                    ide->setSidebarView(SidebarView::Extensions);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_GITHUB:
                    ide->setSidebarView(SidebarView::GitHub);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_GITHUB_PULL_RELEASE:
                    ide->setSidebarView(SidebarView::GitHubPullRelease);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_ACCOUNTS:
                    ide->setSidebarView(SidebarView::Accounts);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_MANAGE:
                    ide->setSidebarView(SidebarView::Manage);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_RECOVERY:
                    ide->setSidebarView(SidebarView::DiskRecovery);
                    if (!ide->m_sidebarVisible)
                        ide->toggleSidebar();
                    return 0;
                case IDC_ACTIVITY_CHAT:
                    ide->toggleSecondarySidebar();
                    return 0;
            }
            break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc = {};
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(RGB(51, 51, 51));
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::SidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (uMsg)
    {
        case WM_COMMAND:
            if (!ide)
                return 0;

            switch (LOWORD(wParam))
            {
                case IDC_GITHUB_OPEN_REPO:
                    ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD", nullptr, nullptr, SW_SHOW);
                    return 0;
                case IDC_GITHUB_OPEN_ISSUES:
                    ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD/issues", nullptr, nullptr,
                                 SW_SHOW);
                    return 0;
                case IDC_GITHUB_OPEN_PULLS:
                    ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD/pulls", nullptr, nullptr,
                                 SW_SHOW);
                    return 0;
                case IDC_GITHUB_OPEN_RELEASES:
                    ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD/releases", nullptr, nullptr,
                                 SW_SHOW);
                    return 0;
                case IDC_ACCOUNTS_OPEN_SETTINGS:
                    PostMessageA(ide->m_hwndMain, WM_COMMAND, IDM_T1_SETTINGS_GUI, 0);
                    return 0;
                case IDC_ACCOUNTS_OPEN_GITHUB_LOGIN:
                    ShellExecuteA(nullptr, "open", "https://github.com/login", nullptr, nullptr, SW_SHOW);
                    return 0;
                case IDC_MANAGE_OPEN_EXTENSIONS:
                    ide->setSidebarView(SidebarView::Extensions);
                    return 0;
                case IDC_MANAGE_OPEN_SETTINGS:
                    PostMessageA(ide->m_hwndMain, WM_COMMAND, IDM_T1_SETTINGS_GUI, 0);
                    return 0;
                case IDC_MANAGE_OPEN_LAYOUTS:
                    PostMessageA(ide->m_hwndMain, WM_COMMAND, IDM_VIEW_LAYOUT_PROFILE_APPLY, 0);
                    return 0;
            }
            break;
        case WM_SIZE:
            if (ide)
                ide->resizeSidebar(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc = {};
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::SidebarContentProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::createExplorerView(HWND hwndParent)
{
    createFileExplorer(hwndParent ? hwndParent : m_hwndSidebar);
}

void Win32IDE::refreshFileTree()
{
    populateFileTree();
}

void Win32IDE::createSearchView(HWND hwndParent)
{
    (void)hwndParent;
    createSearchPanel();
}

void Win32IDE::createSourceControlView(HWND hwndParent)
{
    (void)hwndParent;
    createGitPanel();
}

void Win32IDE::refreshSourceControlView()
{
    refreshGitStatus();
}

void Win32IDE::createRunDebugView(HWND hwndParent)
{
    HWND parent = hwndParent ? hwndParent : m_hwndSidebar;
    if (!parent)
        return;

    m_hwndDebugToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
                                         0, 0, m_sidebarWidth, 36,
                                         parent, nullptr, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    5, 5, 54, 24,
                    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEBUG_START)), m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    64, 5, 54, 24,
                    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEBUG_STOP)), m_hInstance, nullptr);

    m_hwndDebugConfigs = CreateWindowExA(0, "COMBOBOX", "",
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         5, 42, m_sidebarWidth - 10, 120,
                                         parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEBUG_CONFIGS)),
                                         m_hInstance, nullptr);
    SendMessageA(m_hwndDebugConfigs, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Local Native Debug"));
    SendMessageA(m_hwndDebugConfigs, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Attach to Process"));
    SendMessageA(m_hwndDebugConfigs, CB_SETCURSEL, 0, 0);

    CreateWindowExA(0, "STATIC", "Variables", WS_CHILD | WS_VISIBLE | SS_LEFT,
                    5, 74, 100, 16,
                    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEBUG_VARIABLES_LABEL)),
                    m_hInstance, nullptr);

    m_hwndDebugVariables = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                           WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL,
                                           5, 94, m_sidebarWidth - 10, 260,
                                           parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEBUG_VARIABLES)),
                                           m_hInstance, nullptr);
    ListView_SetExtendedListViewStyle(m_hwndDebugVariables, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 100;
    col.pszText = const_cast<char*>("Name");
    ListView_InsertColumn(m_hwndDebugVariables, 0, &col);
    col.cx = m_sidebarWidth - 130;
    col.pszText = const_cast<char*>("Value");
    ListView_InsertColumn(m_hwndDebugVariables, 1, &col);

    updateDebugVariables();
}

void Win32IDE::updateDebugVariables()
{
    if (!m_hwndDebugVariables || !IsWindow(m_hwndDebugVariables))
        return;

    ListView_DeleteAllItems(m_hwndDebugVariables);
    for (size_t i = 0; i < m_localVariables.size(); ++i)
    {
        const auto& variable = m_localVariables[i];

        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<char*>(variable.name.c_str());
        ListView_InsertItem(m_hwndDebugVariables, &item);
        ListView_SetItemText(m_hwndDebugVariables, static_cast<int>(i), 1, const_cast<char*>(variable.value.c_str()));
    }
}
