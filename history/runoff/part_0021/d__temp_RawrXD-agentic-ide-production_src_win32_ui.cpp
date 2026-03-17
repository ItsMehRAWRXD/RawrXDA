#include "win32_ui.h"
#include <commctrl.h>
#include <richedit.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

static LRESULT CALLBACK CommandPaletteSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    auto* palette = reinterpret_cast<Win32CommandPalette*>(dwRefData);
    if (palette && palette->handleMessage(hwnd, msg, wParam, lParam)) {
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, CommandPaletteSubclassProc, 1);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Win32MenuBar Implementation
// ============================================================================

Win32MenuBar::Win32MenuBar() : m_menuBar(nullptr), m_nextId(1000) {
}

Win32MenuBar::~Win32MenuBar() {
    destroy();
}

bool Win32MenuBar::create(HWND parent) {
    m_parent = parent;
    m_menuBar = CreateMenu();
    return m_menuBar != nullptr;
}

void Win32MenuBar::destroy() {
    if (m_menuBar) {
        DestroyMenu(m_menuBar);
        m_menuBar = nullptr;
    }
    m_commands.clear();
}

HMENU Win32MenuBar::addMenu(const std::string& name) {
    if (!m_menuBar) return nullptr;
    
    HMENU menu = CreatePopupMenu();
    if (menu) {
        AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)menu, name.c_str());
    }
    return menu;
}

void Win32MenuBar::addAction(HMENU menu, const std::string& text, const std::string& shortcut, std::function<void()> callback, int id) {
    if (!menu) return;
    
    if (id == -1) {
        id = m_nextId++;
    }
    
    std::string menuText = text;
    if (!shortcut.empty()) {
        menuText += "\t" + shortcut;
    }
    
    AppendMenuA(menu, MF_STRING, id, menuText.c_str());
    m_commands[id] = callback;
}

void Win32MenuBar::addSeparator(HMENU menu) {
    if (!menu) return;
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
}

void Win32MenuBar::setMenu(HWND parent) {
    if (m_menuBar && parent) {
        SetMenu(parent, m_menuBar);
    }
}

bool Win32MenuBar::handleCommand(int commandId) {
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        it->second();
        return true;
    }
    return false;
}

// ============================================================================
// Win32ToolBar Implementation
// ============================================================================

Win32ToolBar::Win32ToolBar() : m_toolBar(nullptr), m_nextId(2000), m_buttonCount(0) {
}

Win32ToolBar::~Win32ToolBar() {
    destroy();
}

bool Win32ToolBar::create(HWND parent) {
    m_parent = parent;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    m_toolBar = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
                              WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
                              0, 0, 0, 0,
                              parent, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (!m_toolBar) return false;
    
    // Send the TB_BUTTONSTRUCTSIZE message, required for toolbar
    SendMessage(m_toolBar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    
    return true;
}

void Win32ToolBar::destroy() {
    if (m_toolBar) {
        DestroyWindow(m_toolBar);
        m_toolBar = nullptr;
    }
    m_commands.clear();
}

void Win32ToolBar::addAction(const std::string& text, std::function<void()> callback) {
    if (!m_toolBar) return;
    
    int id = m_nextId++;
    m_commands[id] = callback;
    
    TBBUTTON tbButton;
    ZeroMemory(&tbButton, sizeof(tbButton));
    tbButton.iBitmap = m_buttonCount;
    tbButton.idCommand = id;
    tbButton.fsState = TBSTATE_ENABLED;
    tbButton.fsStyle = TBSTYLE_BUTTON;
    tbButton.dwData = 0;
    tbButton.iString = 0;
    
    SendMessage(m_toolBar, TB_ADDBUTTONS, 1, (LPARAM)&tbButton);
    
    // Set button text
    TBBUTTONINFO tbi;
    tbi.cbSize = sizeof(TBBUTTONINFO);
    tbi.dwMask = TBIF_TEXT;
    tbi.pszText = (LPSTR)text.c_str();
    SendMessage(m_toolBar, TB_SETBUTTONINFO, id, (LPARAM)&tbi);
    
    m_buttonCount++;
}

void Win32ToolBar::addSeparator() {
    if (!m_toolBar) return;
    
    TBBUTTON tbButton;
    ZeroMemory(&tbButton, sizeof(tbButton));
    tbButton.fsStyle = TBSTYLE_SEP;
    
    SendMessage(m_toolBar, TB_ADDBUTTONS, 1, (LPARAM)&tbButton);
}

void Win32ToolBar::setPosition(int x, int y, int width, int height) {
    if (m_toolBar) {
        MoveWindow(m_toolBar, x, y, width, height, TRUE);
    }
}

bool Win32ToolBar::handleCommand(int commandId) {
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        it->second();
        return true;
    }
    return false;
}

// ============================================================================
// Win32CommandPalette Implementation
// ============================================================================

Win32CommandPalette::Win32CommandPalette() : m_dialog(nullptr), m_visible(false) {
}

Win32CommandPalette::~Win32CommandPalette() {
    destroy();
}

bool Win32CommandPalette::create(HWND parent) {
    m_parent = parent;
    
    // Create a simple dialog for the command palette
    m_dialog = CreateWindowEx(WS_EX_TOPMOST, "STATIC", "Command Palette",
                             WS_POPUP | WS_BORDER | WS_CAPTION,
                             100, 100, 600, 400,
                             parent, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (!m_dialog) return false;
    
    // Create input field
    m_input = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            10, 10, 580, 30,
                            m_dialog, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create list box
    m_list = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
                           WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_HASSTRINGS,
                           10, 50, 580, 340,
                           m_dialog, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Set font
    HFONT font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, "Segoe UI");
    SendMessage(m_input, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(m_list, WM_SETFONT, (WPARAM)font, TRUE);

    // Subclass palette windows so we can intercept keyboard/input events.
    // This keeps everything dependency-free and avoids a custom window class.
    SetWindowSubclass(m_dialog, CommandPaletteSubclassProc, 1, (DWORD_PTR)this);
    SetWindowSubclass(m_input, CommandPaletteSubclassProc, 1, (DWORD_PTR)this);
    SetWindowSubclass(m_list, CommandPaletteSubclassProc, 1, (DWORD_PTR)this);
    
    return true;
}

void Win32CommandPalette::destroy() {
    if (m_dialog) {
        DestroyWindow(m_dialog);
        m_dialog = nullptr;
    }
    m_commands.clear();
    m_filteredIndices.clear();
}

void Win32CommandPalette::addCommand(const std::string& name, const std::string& description, std::function<void()> callback) {
    Command cmd;
    cmd.name = name;
    cmd.description = description;
    cmd.callback = callback;
    m_commands.push_back(cmd);
}

void Win32CommandPalette::show() {
    if (m_dialog) {
        ShowWindow(m_dialog, SW_SHOW);
        SetFocus(m_input);
        m_visible = true;
        updateList();
    }
}

void Win32CommandPalette::hide() {
    if (m_dialog) {
        ShowWindow(m_dialog, SW_HIDE);
        m_visible = false;
    }
}

void Win32CommandPalette::executeCommand(int index) {
    if (index >= 0 && index < (int)m_filteredIndices.size()) {
        int cmdIndex = m_filteredIndices[index];
        if (cmdIndex >= 0 && cmdIndex < (int)m_commands.size()) {
            m_commands[cmdIndex].callback();
        }
    }
}

void Win32CommandPalette::updateList() {
    if (!m_list) return;
    
    SendMessage(m_list, LB_RESETCONTENT, 0, 0);
    m_filteredIndices.clear();
    
    for (size_t i = 0; i < m_commands.size(); ++i) {
        std::string item = m_commands[i].name + " - " + m_commands[i].description;
        SendMessageA(m_list, LB_ADDSTRING, 0, (LPARAM)item.c_str());
        m_filteredIndices.push_back((int)i);
    }

    if (!m_filteredIndices.empty()) {
        SendMessage(m_list, LB_SETCURSEL, 0, 0);
    }
}

void Win32CommandPalette::filterCommands(const std::string& filter) {
    if (!m_list) return;
    
    SendMessage(m_list, LB_RESETCONTENT, 0, 0);
    m_filteredIndices.clear();
    
    for (size_t i = 0; i < m_commands.size(); ++i) {
        const auto& cmd = m_commands[i];
        if (filter.empty() || 
            cmd.name.find(filter) != std::string::npos ||
            cmd.description.find(filter) != std::string::npos) {
            std::string item = cmd.name + " - " + cmd.description;
            SendMessageA(m_list, LB_ADDSTRING, 0, (LPARAM)item.c_str());
            m_filteredIndices.push_back((int)i);
        }
    }

    if (!m_filteredIndices.empty()) {
        SendMessage(m_list, LB_SETCURSEL, 0, 0);
    }
}

bool Win32CommandPalette::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (hwnd != m_dialog && hwnd != m_input && hwnd != m_list) return false;
    
    switch (msg) {
        case WM_CLOSE: {
            hide();
            return true;
        }
        case WM_COMMAND: {
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == m_input) {
                // Filter commands based on input
                char buffer[256];
                GetWindowTextA(m_input, buffer, sizeof(buffer));
                filterCommands(buffer);
                return true;
            }
            else if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == m_list) {
                // Execute selected command
                int index = (int)SendMessage(m_list, LB_GETCURSEL, 0, 0);
                executeCommand(index);
                hide();
                return true;
            }
            break;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                hide();
                return true;
            }
            else if (wParam == VK_RETURN) {
                int index = (int)SendMessage(m_list, LB_GETCURSEL, 0, 0);
                executeCommand(index);
                hide();
                return true;
            }
            break;
        }
    }
    
    return false;
}