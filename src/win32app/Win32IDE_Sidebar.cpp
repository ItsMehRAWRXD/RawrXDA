// Win32IDE_Sidebar.cpp - Primary Sidebar Implementation
// Implements VS Code-style Activity Bar and Sidebar with 5 views:
// Explorer, Search, Source Control, Run & Debug, Extensions

#include "../../include/quickjs_extension_host.h"
#include "../core/enterprise_license.h"
#include "VSIXInstaller.hpp"
#include "Win32IDE.h"
#include "RawrXD_Layout.hpp"
#include "Win32IDE_IELabels.h"
#include "Win32IDE_Git.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
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

static std::string wideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr,
                                        nullptr);
    if (len <= 0)
        return {};
    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), len, nullptr,
                            nullptr) == 0)
        return {};
    return out;
}

static bool launchExplorerSelectPath(const std::string& utf8Path)
{
    const std::wstring widePath = utf8ToWide(utf8Path);
    if (widePath.empty())
        return false;

    std::wstring commandLine = L"explorer.exe /select,\"";
    commandLine += widePath;
    commandLine += L"\"";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back(L'\0');

    const BOOL launched = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!launched)
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static bool shouldSkipFindData(const WIN32_FIND_DATAW& findData)
{
    if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
        return true;
    if ((findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
        return true;
    if ((findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
        return true;
    return false;
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

constexpr int IDC_DEBUG_CONFIGS = 6040;
constexpr int IDC_DEBUG_START = 6041;
constexpr int IDC_DEBUG_STOP = 6042;
constexpr int IDC_DEBUG_VARIABLES = 6043;
constexpr int IDC_DEBUG_CALLSTACK = 6044;
constexpr int IDC_DEBUG_CONSOLE = 6045;
constexpr int IDC_DEBUG_VARIABLES_LABEL = 6046;

constexpr int IDC_EXT_SEARCH = 6050;
constexpr int IDC_EXT_LIST = 6051;
constexpr int IDC_EXT_DETAILS = 6052;
constexpr int IDC_EXT_INSTALL = 6053;
constexpr int IDC_EXT_UNINSTALL = 6054;
constexpr int IDC_EXT_INSTALL_VSIX = 6055;

// File Explorer IDs are defined centrally in Win32IDE_Commands.h

namespace
{
HFONT g_sidebarUiFont = nullptr;
HFONT g_sidebarTitleFont = nullptr;
UINT g_sidebarFontDpi = 0;

UINT getWindowDpiSafe(HWND hwnd)
{
    if (!hwnd)
        return 96;
    UINT dpi = GetDpiForWindow(hwnd);
    return dpi > 0 ? dpi : 96;
}

HFONT createUiFontForDpi(UINT dpi, int pointSize, int weight)
{
    return CreateFontA(-MulDiv(pointSize, static_cast<int>(dpi), 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

void ensureSidebarFonts(UINT dpi)
{
    if (dpi == 0)
        dpi = 96;
    if (g_sidebarUiFont && g_sidebarTitleFont && g_sidebarFontDpi == dpi)
        return;

    if (g_sidebarUiFont)
    {
        DeleteObject(g_sidebarUiFont);
        g_sidebarUiFont = nullptr;
    }
    if (g_sidebarTitleFont)
    {
        DeleteObject(g_sidebarTitleFont);
        g_sidebarTitleFont = nullptr;
    }

    g_sidebarUiFont = createUiFontForDpi(dpi, 9, FW_NORMAL);
    g_sidebarTitleFont = createUiFontForDpi(dpi, 10, FW_SEMIBOLD);
    g_sidebarFontDpi = dpi;
}

void applySidebarFonts(HWND hwndSidebarTitle, HWND hwndExplorerToolbar, HWND hwndExplorerTree)
{
    const UINT dpi = getWindowDpiSafe(hwndSidebarTitle ? hwndSidebarTitle : hwndExplorerTree);
    ensureSidebarFonts(dpi);

    if (hwndSidebarTitle && g_sidebarTitleFont)
        SendMessageA(hwndSidebarTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_sidebarTitleFont), TRUE);
    if (hwndExplorerTree && g_sidebarUiFont)
        SendMessageA(hwndExplorerTree, WM_SETFONT, reinterpret_cast<WPARAM>(g_sidebarUiFont), TRUE);

    if (hwndExplorerToolbar && g_sidebarUiFont)
    {
        const int ids[] = {IDC_EXPLORER_NEW_FILE, IDC_EXPLORER_NEW_FOLDER, IDC_EXPLORER_REFRESH, IDC_EXPLORER_COLLAPSE};
        for (int id : ids)
        {
            if (HWND hBtn = GetDlgItem(hwndExplorerToolbar, id))
                SendMessageA(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_sidebarUiFont), TRUE);
        }
    }
}

void layoutExplorerToolbarButtons(HWND hwndToolbar, int width, int toolbarHeight, UINT dpi)
{
    if (!hwndToolbar)
        return;

    const int kButtonCount = 4;
    const int buttonIds[kButtonCount] = {IDC_EXPLORER_NEW_FILE, IDC_EXPLORER_NEW_FOLDER, IDC_EXPLORER_REFRESH,
                                          IDC_EXPLORER_COLLAPSE};

    HWND buttons[kButtonCount] = {};
    int visibleCount = 0;
    for (int i = 0; i < kButtonCount; ++i)
    {
        buttons[i] = GetDlgItem(hwndToolbar, buttonIds[i]);
        if (buttons[i])
            ++visibleCount;
    }
    if (visibleCount == 0)
        return;

    const int pad = MulDiv(5, static_cast<int>(dpi), 96);
    const int gap = MulDiv(4, static_cast<int>(dpi), 96);
    const int safeWidth = (width > 0) ? width : 0;
    const int avail = std::max(0, safeWidth - (2 * pad));
    const int minSingleWidths[kButtonCount] = {52, 66, 72, 76};

    int requiredSingle = 0;
    for (int w : minSingleWidths)
        requiredSingle += MulDiv(w, static_cast<int>(dpi), 96);
    requiredSingle += gap * (kButtonCount - 1);

    const bool useWrappedLayout = avail < requiredSingle;

    HDWP hdwp = BeginDeferWindowPos(kButtonCount);
    if (!hdwp)
        return;

    if (useWrappedLayout)
    {
        const int colW = std::max(44, (avail - gap) / 2);
        const int rowGap = gap;
        const int rowH = std::max(18, (toolbarHeight - (2 * pad) - rowGap) / 2);
        const int x0 = pad;
        const int x1 = pad + colW + gap;
        const int y0 = pad;
        const int y1 = pad + rowH + rowGap;

        if (buttons[0])
            hdwp = DeferWindowPos(hdwp, buttons[0], nullptr, x0, y0, colW, rowH, SWP_NOZORDER | SWP_NOACTIVATE);
        if (buttons[1])
            hdwp = DeferWindowPos(hdwp, buttons[1], nullptr, x1, y0, colW, rowH, SWP_NOZORDER | SWP_NOACTIVATE);
        if (buttons[2])
            hdwp = DeferWindowPos(hdwp, buttons[2], nullptr, x0, y1, colW, rowH, SWP_NOZORDER | SWP_NOACTIVATE);
        if (buttons[3])
            hdwp = DeferWindowPos(hdwp, buttons[3], nullptr, x1, y1, colW, rowH, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        int x = pad;
        const int rowH = std::max(18, toolbarHeight - (2 * pad));
        for (int i = 0; i < kButtonCount; ++i)
        {
            if (!buttons[i])
                continue;
            const int btnW = MulDiv(minSingleWidths[i], static_cast<int>(dpi), 96);
            hdwp = DeferWindowPos(hdwp, buttons[i], nullptr, x, pad, btnW, rowH, SWP_NOZORDER | SWP_NOACTIVATE);
            x += btnW + gap;
        }
    }

    EndDeferWindowPos(hdwp);
}

void applySidebarPaneLayout(HWND parent, std::initializer_list<RXDControl> controls)
{
    if (!parent)
        return;

    RXDLayoutEngine layout;
    layout.Reserve(controls.size());
    for (const auto& c : controls)
    {
        if (!c.hwnd)
            continue;
        layout.AddControl(c.hwnd, c.x_pct, c.y_pct, c.w_pct, c.h_pct, c.off_x, c.off_y, c.off_w, c.off_h);
    }
    layout.Update(parent);
}

void layoutActivityBarButtons(HWND hwndActivityBar)
{
    if (!hwndActivityBar)
        return;

    RECT rc = {};
    if (!GetClientRect(hwndActivityBar, &rc))
        return;

    const int iconSize = 40;
    const int slotHeight = 48;
    const int margin = 4;
    const int centeredLeft = (rc.right - iconSize) / 2;
    const int left = centeredLeft > 0 ? centeredLeft : 0;

    // Top group (primary workspace navigators).
    const int topIds[] = {IDC_ACTIVITY_EXPLORER, IDC_ACTIVITY_SEARCH, IDC_ACTIVITY_SCM,
                          IDC_ACTIVITY_DEBUG,    IDC_ACTIVITY_EXTENSIONS, IDC_ACTIVITY_RECOVERY};
    // Bottom group (global tools).
    const int bottomIds[] = {IDC_ACTIVITY_CHAT};

    int y = margin;
    for (int id : topIds)
    {
        if (HWND h = GetDlgItem(hwndActivityBar, id))
        {
            MoveWindow(h, left, y, iconSize, iconSize, FALSE);
        }
        y += slotHeight;
    }

    const int bottomGroupHeight = static_cast<int>(std::size(bottomIds)) * slotHeight;
    int yBottom = rc.bottom - margin - bottomGroupHeight;
    if (yBottom < y)
        yBottom = y;

    for (int id : bottomIds)
    {
        if (HWND h = GetDlgItem(hwndActivityBar, id))
        {
            MoveWindow(h, left, yBottom, iconSize, iconSize, FALSE);
        }
        yBottom += slotHeight;
    }
}
}  // namespace

WNDPROC Win32IDE::s_sidebarContentOldProc = nullptr;

// Maps ListView index to extension ID (order may differ from m_extensions during search)
static std::vector<std::string> s_extensionDisplayIds;

// ============================================================================
// Activity Bar Implementation
// ============================================================================
