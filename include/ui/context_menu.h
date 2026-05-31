#pragma once
/**
 * @file context_menu.h
 * @brief Right-click context menus with dynamic items
 * Batch 4 - Item 49: Context menu
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class MenuItemType {
    Normal,
    Separator,
    Submenu,
    Checkbox,
    Radio
};

struct ContextMenuItem {
    std::string id;
    std::string label;
    std::string shortcut;
    std::string icon;
    MenuItemType type;
    bool enabled;
    bool visible;
    bool checked;
    int group;
    std::function<void()> action;
    std::vector<ContextMenuItem> submenu;
};

struct ContextMenuSection {
    std::string name;
    std::vector<ContextMenuItem> items;
};

enum class ContextType {
    Editor,
    Explorer,
    Tab,
    Terminal,
    StatusBar,
    Sidebar,
    Panel,
    Custom
};

class ContextMenu {
public:
    ContextMenu();
    ~ContextMenu();

    // Initialization
    void initialize();
    void shutdown();

    // Registration
    void registerItem(ContextType context, const ContextMenuItem& item);
    void registerSection(ContextType context, const ContextMenuSection& section);
    void unregisterItem(ContextType context, const std::string& itemId);
    void clearContext(ContextType context);

    // Default menus
    void registerEditorContextMenu();
    void registerExplorerContextMenu();
    void registerTabContextMenu();
    void registerTerminalContextMenu();

    // Display
    void show(ContextType context, int x, int y, HWND parent);
    void showAtCursor(ContextType context, HWND parent);
    void showAtPosition(ContextType context, POINT pt, HWND parent);
    void hide();

    // Dynamic items
    void addDynamicItem(ContextType context,
                        std::function<bool()> condition,
                        const ContextMenuItem& item);
    void clearDynamicItems(ContextType context);

    // Selection
    void setSelectedItem(const std::string& itemId);
    std::string getSelectedItem() const;

    // State
    void setItemEnabled(ContextType context, const std::string& itemId, bool enabled);
    void setItemChecked(ContextType context, const std::string& itemId, bool checked);
    void setItemVisible(ContextType context, const std::string& itemId, bool visible);

    // Customization
    void setMenuFont(HFONT font);
    void setMenuColors(uint32_t bgColor, uint32_t textColor, uint32_t highlightColor);

    // Events
    using MenuShowCallback = std::function<void(ContextType)>;
    using MenuHideCallback = std::function<void()>;
    using ItemClickCallback = std::function<void(const std::string&)>;
    void onMenuShow(MenuShowCallback callback);
    void onMenuHide(MenuHideCallback callback);
    void onItemClick(ItemClickCallback callback);

private:
    std::map<ContextType, std::vector<ContextMenuSection>> m_menus;
    std::map<ContextType, std::vector<std::pair<std::function<bool()>, ContextMenuItem>>> m_dynamicItems;
    std::string m_selectedItem;
    HMENU m_currentMenu{nullptr};
    HWND m_parent{nullptr};
    ContextType m_currentContext;

    HFONT m_font{nullptr};
    uint32_t m_bgColor{0xFFFFFF};
    uint32_t m_textColor{0x000000};
    uint32_t m_highlightColor{0x3399FF};

    MenuShowCallback m_showCallback;
    MenuHideCallback m_hideCallback;
    ItemClickCallback m_clickCallback;

    HMENU buildMenu(ContextType context);
    void appendMenuItem(HMENU menu, const ContextMenuItem& item);
    void handleCommand(uint32_t commandId);
    uint32_t getNextCommandId();

    static uint32_t s_nextCommandId;
    static std::map<uint32_t, std::function<void()>> s_commandHandlers;
};

// Global instance
ContextMenu& getContextMenu();

// Utility functions
HMENU createPopupMenu();
void destroyMenu(HMENU menu);

} // namespace RawrXD::UI
