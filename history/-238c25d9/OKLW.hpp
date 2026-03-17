/**
 * @file Win32ContextMenu.hpp
 * @brief Header for Win32 context menu system
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

namespace RawrXD::Win32 {

struct ContextMenuItem {
    std::string text;
    std::function<void()> callback;
    bool enabled = true;
    bool checked = false;
    std::vector<ContextMenuItem> children; // For submenus
};

class Win32ContextMenu {
public:
    Win32ContextMenu();
    ~Win32ContextMenu();

    // Add items
    void addItem(const std::string& text, std::function<void()> callback = nullptr);
    void addSeparator();
    void addSubMenu(const std::string& text, Win32ContextMenu* subMenu);
    void addCheckableItem(const std::string& text, bool checked, std::function<void(bool)> callback);

    // File operations context menu
    void buildFileContextMenu();
    void buildFolderContextMenu();
    void buildEditorContextMenu();
    void buildTerminalContextMenu();

    // Show menu
    void show(HWND hwnd, int x, int y);
    void showAtCursor(HWND hwnd);

    // Menu handle access
    HMENU getMenuHandle() const { return m_hMenu; }

    // Command handling
    bool handleCommand(UINT id);

private:
    HMENU m_hMenu;
    std::vector<std::function<void()>> m_callbacks;
    std::vector<std::function<void(bool)>> m_checkCallbacks;
    UINT m_nextId = 2000; // Start IDs at 2000 to avoid conflicts

    void addMenuItemToHMENU(HMENU hMenu, const ContextMenuItem& item);
};

} // namespace RawrXD::Win32