/**
 * @file Win32ContextMenu.hpp
 * @brief Production-Ready Win32 Context Menu System
 * 
 * All context menus use the CommandRegistry for real command execution.
 * Supports right-click on files, folders, breadcrumbs, tabs, and editor.
 */

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <functional>

namespace RawrXD::Win32 {

// Forward declaration
class CommandRegistry;

struct ContextMenuItem {
    std::string text;
    std::function<void()> callback;
    UINT commandId = 0;  // If non-zero, routes to CommandRegistry
    bool enabled = true;
    bool checked = false;
    std::vector<ContextMenuItem> children; // For submenus
};

class Win32ContextMenu {
public:
    Win32ContextMenu();
    ~Win32ContextMenu();

    // Add items - with callback or command ID
    void addItem(const std::string& text, std::function<void()> callback);
    void addItem(const std::string& text, UINT commandId);  // Routes to CommandRegistry
    void addSeparator();
    void addSubMenu(const std::string& text, Win32ContextMenu* subMenu);
    void addCheckableItem(const std::string& text, bool checked, std::function<void(bool)> callback);

    // Clear and rebuild
    void clear();

    // Context menu builders - ALL PRODUCTION READY (no TODOs)
    void buildFileContextMenu();       // For files in explorer
    void buildFolderContextMenu();     // For folders in explorer
    void buildEditorContextMenu();     // For text editor right-click
    void buildTerminalContextMenu();   // For terminal right-click
    void buildBreadcrumbContextMenu(); // For breadcrumb segments - supports Copy Path/Reveal
    void buildTabContextMenu();        // For editor tabs
    void buildEmptySpaceContextMenu(); // For empty space in explorer

    // Show menu
    void show(HWND hwnd, int x, int y);
    void showAtCursor(HWND hwnd);
    void showAtCursorWithPath(HWND hwnd, const std::string& path);

    // Context path - set before showing for path-based operations
    void setContextPath(const std::string& path);
    std::string getContextPath() const { return m_contextPath; }

    // Menu handle access
    HMENU getMenuHandle() const { return m_hMenu; }

    // Command handling - routes to CommandRegistry first
    bool handleCommand(UINT id);

private:
    HMENU m_hMenu;
    std::vector<std::function<void()>> m_callbacks;
    std::vector<std::function<void(bool)>> m_checkCallbacks;
    std::string m_contextPath;  // Path for context-sensitive operations
    UINT m_nextId = 2000; // Start IDs at 2000 to avoid conflicts

    void addMenuItemToHMENU(HMENU hMenu, const ContextMenuItem& item);
};

} // namespace RawrXD::Win32