/**
 * @file Win32ContextMenu.cpp
 * @brief Implementation of Win32 context menu system
 */

#include "Win32ContextMenu.hpp"
#include <algorithm>

namespace RawrXD::Win32 {

Win32ContextMenu::Win32ContextMenu()
{
    m_hMenu = CreatePopupMenu();
    if (!m_hMenu) {
        throw std::runtime_error("Failed to create context menu");
    }
}

Win32ContextMenu::~Win32ContextMenu()
{
    if (m_hMenu) {
        DestroyMenu(m_hMenu);
    }
}

void Win32ContextMenu::addItem(const std::string& text, std::function<void()> callback)
{
    UINT id = m_nextId++;
    AppendMenuA(m_hMenu, MF_STRING, id, text.c_str());
    m_callbacks.push_back(callback);
}

void Win32ContextMenu::addSeparator()
{
    AppendMenuA(m_hMenu, MF_SEPARATOR, 0, nullptr);
}

void Win32ContextMenu::addSubMenu(const std::string& text, Win32ContextMenu* subMenu)
{
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)subMenu->getMenuHandle(), text.c_str());
}

void Win32ContextMenu::addCheckableItem(const std::string& text, bool checked, std::function<void(bool)> callback)
{
    UINT id = m_nextId++;
    UINT flags = MF_STRING;
    if (checked) flags |= MF_CHECKED;

    AppendMenuA(m_hMenu, flags, id, text.c_str());
    m_callbacks.push_back([callback, checked]() { callback(!checked); });
    m_checkCallbacks.push_back(callback);
}

void Win32ContextMenu::show(HWND hwnd, int x, int y)
{
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);

    UINT id = TrackPopupMenu(m_hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                           pt.x, pt.y, 0, hwnd, nullptr);

    if (id >= 2000 && id < 2000 + m_callbacks.size()) {
        size_t index = id - 2000;
        if (index < m_callbacks.size() && m_callbacks[index]) {
            m_callbacks[index]();
        }
    }
}

void Win32ContextMenu::showAtCursor(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    show(hwnd, pt.x, pt.y);
}

bool Win32ContextMenu::handleCommand(UINT id)
{
    if (id >= 2000 && id < 2000 + m_callbacks.size()) {
        size_t index = id - 2000;
        if (index < m_callbacks.size() && m_callbacks[index]) {
            m_callbacks[index]();
            return true;
        }
    }
    return false;
}

void Win32ContextMenu::buildFileContextMenu()
{
    // Clear existing items
    while (GetMenuItemCount(m_hMenu) > 0) {
        DeleteMenu(m_hMenu, 0, MF_BYPOSITION);
    }

    addItem("Open", []() {
        // TODO: Open file
    });

    addItem("Open With...", []() {
        // TODO: Open with dialog
    });

    addSeparator();

    addItem("Cut", []() {
        // TODO: Cut file
    });

    addItem("Copy", []() {
        // TODO: Copy file
    });

    addItem("Copy Path", []() {
        // TODO: Copy file path to clipboard
    });

    addItem("Copy Relative Path", []() {
        // TODO: Copy relative path
    });

    addSeparator();

    addItem("Rename", []() {
        // TODO: Rename file
    });

    addItem("Delete", []() {
        // TODO: Delete file
    });

    addSeparator();

    addItem("Reveal in File Explorer", []() {
        // TODO: Open containing folder
    });

    addItem("Open in Terminal", []() {
        // TODO: Open terminal at file location
    });

    addSeparator();

    addItem("Properties", []() {
        // TODO: Show file properties
    });
}

void Win32ContextMenu::buildFolderContextMenu()
{
    // Clear existing items
    while (GetMenuItemCount(m_hMenu) > 0) {
        DeleteMenu(m_hMenu, 0, MF_BYPOSITION);
    }

    addItem("Open", []() {
        // TODO: Open folder
    });

    addItem("Open in New Window", []() {
        // TODO: Open folder in new window
    });

    addSeparator();

    addItem("Cut", []() {
        // TODO: Cut folder
    });

    addItem("Copy", []() {
        // TODO: Copy folder
    });

    addItem("Copy Path", []() {
        // TODO: Copy folder path
    });

    addSeparator();

    addItem("Rename", []() {
        // TODO: Rename folder
    });

    addItem("Delete", []() {
        // TODO: Delete folder
    });

    addSeparator();

    addItem("New File", []() {
        // TODO: Create new file in folder
    });

    addItem("New Folder", []() {
        // TODO: Create new folder
    });

    addSeparator();

    addItem("Reveal in File Explorer", []() {
        // TODO: Open containing folder
    });

    addItem("Open in Terminal", []() {
        // TODO: Open terminal at folder location
    });

    addSeparator();

    addItem("Find in Folder...", []() {
        // TODO: Search in folder
    });

    addSeparator();

    addItem("Properties", []() {
        // TODO: Show folder properties
    });
}

void Win32ContextMenu::buildEditorContextMenu()
{
    // Clear existing items
    while (GetMenuItemCount(m_hMenu) > 0) {
        DeleteMenu(m_hMenu, 0, MF_BYPOSITION);
    }

    addItem("Cut", []() {
        // TODO: Cut selection
    });

    addItem("Copy", []() {
        // TODO: Copy selection
    });

    addItem("Paste", []() {
        // TODO: Paste
    });

    addSeparator();

    addItem("Select All", []() {
        // TODO: Select all
    });

    addItem("Select Line", []() {
        // TODO: Select current line
    });

    addSeparator();

    addItem("Find", []() {
        // TODO: Open find
    });

    addItem("Replace", []() {
        // TODO: Open replace
    });

    addSeparator();

    addItem("Command Palette...", []() {
        // TODO: Open command palette
    });

    addSeparator();

    addItem("Format Document", []() {
        // TODO: Format document
    });

    addItem("Format Selection", []() {
        // TODO: Format selection
    });

    addSeparator();

    addItem("Go to Definition", []() {
        // TODO: Go to definition
    });

    addItem("Go to References", []() {
        // TODO: Find references
    });

    addItem("Peek Definition", []() {
        // TODO: Peek definition
    });

    addSeparator();

    addItem("Toggle Line Comment", []() {
        // TODO: Toggle comment
    });

    addItem("Toggle Block Comment", []() {
        // TODO: Toggle block comment
    });
}

void Win32ContextMenu::buildTerminalContextMenu()
{
    // Clear existing items
    while (GetMenuItemCount(m_hMenu) > 0) {
        DeleteMenu(m_hMenu, 0, MF_BYPOSITION);
    }

    addItem("Copy", []() {
        // TODO: Copy terminal selection
    });

    addItem("Paste", []() {
        // TODO: Paste into terminal
    });

    addSeparator();

    addItem("Select All", []() {
        // TODO: Select all in terminal
    });

    addItem("Clear", []() {
        // TODO: Clear terminal
    });

    addSeparator();

    addItem("Split Terminal", []() {
        // TODO: Split terminal
    });

    addItem("Kill Terminal", []() {
        // TODO: Kill current terminal
    });

    addSeparator();

    addItem("Run Task...", []() {
        // TODO: Run task
    });

    addItem("Configure Tasks...", []() {
        // TODO: Configure tasks
    });

    addSeparator();

    addItem("Change Color...", []() {
        // TODO: Change terminal color
    });

    addItem("Change Font...", []() {
        // TODO: Change terminal font
    });
}

} // namespace RawrXD::Win32