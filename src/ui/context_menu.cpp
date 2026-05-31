#include "ui/context_menu.h"
#include <algorithm>

namespace RawrXD::UI {

uint32_t ContextMenu::s_nextCommandId = 1000;
std::map<uint32_t, std::function<void()>> ContextMenu::s_commandHandlers;

ContextMenu::ContextMenu() = default;
ContextMenu::~ContextMenu() {
    if (m_currentMenu) {
        DestroyMenu(m_currentMenu);
    }
}

void ContextMenu::initialize() {
    // Register default context menus
    registerEditorContextMenu();
    registerExplorerContextMenu();
    registerTabContextMenu();
    registerTerminalContextMenu();
}

void ContextMenu::shutdown() {
    clearContext(ContextType::Editor);
    clearContext(ContextType::Explorer);
    clearContext(ContextType::Tab);
    clearContext(ContextType::Terminal);
}

void ContextMenu::registerItem(ContextType context, const ContextMenuItem& item) {
    if (m_menus[context].empty()) {
        m_menus[context].push_back({"default", {}});
    }
    m_menus[context][0].items.push_back(item);
}

void ContextMenu::registerSection(ContextType context, const ContextMenuSection& section) {
    m_menus[context].push_back(section);
}

void ContextMenu::unregisterItem(ContextType context, const std::string& itemId) {
    auto& sections = m_menus[context];
    for (auto& section : sections) {
        auto& items = section.items;
        items.erase(std::remove_if(items.begin(), items.end(),
            [&itemId](const ContextMenuItem& item) { return item.id == itemId; }), items.end());
    }
}

void ContextMenu::clearContext(ContextType context) {
    m_menus[context].clear();
    m_dynamicItems[context].clear();
}

void ContextMenu::registerEditorContextMenu() {
    ContextMenuSection section;
    section.name = "editor";

    section.items.push_back({"cut", "Cut", "Ctrl+X", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"copy", "Copy", "Ctrl+C", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"paste", "Paste", "Ctrl+V", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"", "", "", "", MenuItemType::Separator, true, true, false, 0, nullptr});
    section.items.push_back({"find", "Find", "Ctrl+F", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"replace", "Replace", "Ctrl+H", "", MenuItemType::Normal, true, true, false, 0, []() {}});

    registerSection(ContextType::Editor, section);
}

void ContextMenu::registerExplorerContextMenu() {
    ContextMenuSection section;
    section.name = "explorer";

    section.items.push_back({"newFile", "New File", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"newFolder", "New Folder", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"", "", "", "", MenuItemType::Separator, true, true, false, 0, nullptr});
    section.items.push_back({"refresh", "Refresh", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});

    registerSection(ContextType::Explorer, section);
}

void ContextMenu::registerTabContextMenu() {
    ContextMenuSection section;
    section.name = "tab";

    section.items.push_back({"close", "Close", "Ctrl+W", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"closeOthers", "Close Others", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"closeAll", "Close All", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"", "", "", "", MenuItemType::Separator, true, true, false, 0, nullptr});
    section.items.push_back({"pin", "Pin", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});

    registerSection(ContextType::Tab, section);
}

void ContextMenu::registerTerminalContextMenu() {
    ContextMenuSection section;
    section.name = "terminal";

    section.items.push_back({"copy", "Copy", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"paste", "Paste", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});
    section.items.push_back({"", "", "", "", MenuItemType::Separator, true, true, false, 0, nullptr});
    section.items.push_back({"clear", "Clear", "", "", MenuItemType::Normal, true, true, false, 0, []() {}});

    registerSection(ContextType::Terminal, section);
}

void ContextMenu::show(ContextType context, int x, int y, HWND parent) {
    m_currentContext = context;
    m_parent = parent;

    if (m_currentMenu) {
        DestroyMenu(m_currentMenu);
    }

    m_currentMenu = buildMenu(context);
    if (!m_currentMenu) return;

    if (m_showCallback) {
        m_showCallback(context);
    }

    TrackPopupMenu(m_currentMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                   x, y, 0, parent, nullptr);

    if (m_hideCallback) {
        m_hideCallback();
    }
}

void ContextMenu::showAtCursor(ContextType context, HWND parent) {
    POINT pt;
    GetCursorPos(&pt);
    show(context, pt.x, pt.y, parent);
}

void ContextMenu::showAtPosition(ContextType context, POINT pt, HWND parent) {
    show(context, pt.x, pt.y, parent);
}

void ContextMenu::hide() {
    // Menu is automatically hidden after selection
}

void ContextMenu::addDynamicItem(ContextType context,
                                 std::function<bool()> condition,
                                 const ContextMenuItem& item) {
    m_dynamicItems[context].push_back({condition, item});
}

void ContextMenu::clearDynamicItems(ContextType context) {
    m_dynamicItems[context].clear();
}

void ContextMenu::setSelectedItem(const std::string& itemId) {
    m_selectedItem = itemId;
}

std::string ContextMenu::getSelectedItem() const {
    return m_selectedItem;
}

void ContextMenu::setItemEnabled(ContextType context, const std::string& itemId, bool enabled) {
    auto& sections = m_menus[context];
    for (auto& section : sections) {
        for (auto& item : section.items) {
            if (item.id == itemId) {
                item.enabled = enabled;
                return;
            }
        }
    }
}

void ContextMenu::setItemChecked(ContextType context, const std::string& itemId, bool checked) {
    auto& sections = m_menus[context];
    for (auto& section : sections) {
        for (auto& item : section.items) {
            if (item.id == itemId) {
                item.checked = checked;
                return;
            }
        }
    }
}

void ContextMenu::setItemVisible(ContextType context, const std::string& itemId, bool visible) {
    auto& sections = m_menus[context];
    for (auto& section : sections) {
        for (auto& item : section.items) {
            if (item.id == itemId) {
                item.visible = visible;
                return;
            }
        }
    }
}

void ContextMenu::setMenuFont(HFONT font) {
    m_font = font;
}

void ContextMenu::setMenuColors(uint32_t bgColor, uint32_t textColor, uint32_t highlightColor) {
    m_bgColor = bgColor;
    m_textColor = textColor;
    m_highlightColor = highlightColor;
}

void ContextMenu::onMenuShow(MenuShowCallback callback) {
    m_showCallback = callback;
}

void ContextMenu::onMenuHide(MenuHideCallback callback) {
    m_hideCallback = callback;
}

void ContextMenu::onItemClick(ItemClickCallback callback) {
    m_clickCallback = callback;
}

HMENU ContextMenu::buildMenu(ContextType context) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;

    const auto& sections = m_menus[context];
    bool firstSection = true;

    for (const auto& section : sections) {
        if (!firstSection) {
            AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        }
        firstSection = false;

        for (const auto& item : section.items) {
            if (!item.visible) continue;
            appendMenuItem(menu, item);
        }
    }

    // Add dynamic items
    const auto& dynamic = m_dynamicItems[context];
    for (const auto& [condition, item] : dynamic) {
        if (condition() && item.visible) {
            appendMenuItem(menu, item);
        }
    }

    return menu;
}

void ContextMenu::appendMenuItem(HMENU menu, const ContextMenuItem& item) {
    UINT flags = MF_STRING;

    if (item.type == MenuItemType::Separator) {
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        return;
    }

    if (!item.enabled) flags |= MF_GRAYED;
    if (item.checked) flags |= MF_CHECKED;

    std::string label = item.label;
    if (!item.shortcut.empty()) {
        label += "\t" + item.shortcut;
    }

    uint32_t commandId = getNextCommandId();
    AppendMenu(menu, flags, commandId, label.c_str());

    if (item.action) {
        s_commandHandlers[commandId] = item.action;
    }
}

void ContextMenu::handleCommand(uint32_t commandId) {
    auto it = s_commandHandlers.find(commandId);
    if (it != s_commandHandlers.end() && it->second) {
        it->second();
    }

    if (m_clickCallback) {
        m_clickCallback(m_selectedItem);
    }
}

uint32_t ContextMenu::getNextCommandId() {
    return s_nextCommandId++;
}

// Global instance
ContextMenu& getContextMenu() {
    static ContextMenu menu;
    return menu;
}

HMENU createPopupMenu() {
    return CreatePopupMenu();
}

void destroyMenu(HMENU menu) {
    if (menu) {
        DestroyMenu(menu);
    }
}

} // namespace RawrXD::UI
