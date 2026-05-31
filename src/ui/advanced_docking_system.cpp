// ============================================================================
// advanced_docking_system.cpp - Docking Layout Engine Implementation
// ============================================================================

#include "advanced_docking_system.h"
#include <windowsx.h>
#include <commctrl.h>
#include <fstream>
#include <iostream>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
namespace UI {

// ============================================================================
// DockingConfig Serialization
// ============================================================================
JsonValue DockingConfig::toJson() const {
    JsonValue root;
    root["leftSidebarWidth"] = leftSidebarWidth;
    root["rightSidebarWidth"] = rightSidebarWidth;
    root["leftSidebarState"] = panelStateToString(leftSidebarState);
    root["rightSidebarState"] = panelStateToString(rightSidebarState);
    root["bottomPanelHeight"] = bottomPanelHeight;
    root["bottomPanelState"] = panelStateToString(bottomPanelState);
    root["showTabCloseButtons"] = showTabCloseButtons;
    root["enableTabDragDrop"] = enableTabDragDrop;
    root["showTabIcons"] = showTabIcons;
    root["tabOrientation"] = (tabOrientation == TabGroupOrientation::Horizontal) ? "horizontal" : "vertical";
    root["autoHideSidebars"] = autoHideSidebars;
    root["restoreLayoutOnStartup"] = restoreLayoutOnStartup;
    return root;
}

DockingConfig DockingConfig::fromJson(const JsonValue& json) {
    DockingConfig config;
    if (json.contains("leftSidebarWidth")) config.leftSidebarWidth = json["leftSidebarWidth"].get<int>();
    if (json.contains("rightSidebarWidth")) config.rightSidebarWidth = json["rightSidebarWidth"].get<int>();
    if (json.contains("leftSidebarState")) config.leftSidebarState = stringToPanelState(json["leftSidebarState"].get<std::string>());
    if (json.contains("rightSidebarState")) config.rightSidebarState = stringToPanelState(json["rightSidebarState"].get<std::string>());
    if (json.contains("bottomPanelHeight")) config.bottomPanelHeight = json["bottomPanelHeight"].get<int>();
    if (json.contains("bottomPanelState")) config.bottomPanelState = stringToPanelState(json["bottomPanelState"].get<std::string>());
    if (json.contains("showTabCloseButtons")) config.showTabCloseButtons = json["showTabCloseButtons"].get<bool>();
    if (json.contains("enableTabDragDrop")) config.enableTabDragDrop = json["enableTabDragDrop"].get<bool>();
    if (json.contains("showTabIcons")) config.showTabIcons = json["showTabIcons"].get<bool>();
    if (json.contains("tabOrientation")) {
        config.tabOrientation = (json["tabOrientation"].get<std::string>() == "horizontal")
            ? TabGroupOrientation::Horizontal : TabGroupOrientation::Vertical;
    }
    if (json.contains("autoHideSidebars")) config.autoHideSidebars = json["autoHideSidebars"].get<bool>();
    if (json.contains("restoreLayoutOnStartup")) config.restoreLayoutOnStartup = json["restoreLayoutOnStartup"].get<bool>();
    return config;
}

// ============================================================================
// TabGroup Implementation
// ============================================================================
TabGroup::TabGroup(DockingManager* manager, const std::string& id)
    : manager_(manager), id_(id) {}

TabGroup::~TabGroup() {
    destroy();
}

size_t TabGroup::addTab(std::shared_ptr<TabItem> tab) {
    tabs_.push_back(tab);
    if (tabs_.size() == 1) {
        activeTabIndex_ = 0;
        if (tab->onActivateCallback) tab->onActivateCallback();
    }
    updateTabBar();
    return tabs_.size() - 1;
}

void TabGroup::removeTab(const std::string& tabId) {
    for (auto it = tabs_.begin(); it != tabs_.end(); ++it) {
        if ((*it)->id == tabId) {
            if ((*it)->onCloseCallback) (*it)->onCloseCallback();
            tabs_.erase(it);
            if (activeTabIndex_ >= tabs_.size()) {
                activeTabIndex_ = tabs_.empty() ? 0 : tabs_.size() - 1;
            }
            updateTabBar();
            return;
        }
    }
}

void TabGroup::activateTab(const std::string& tabId) {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i]->id == tabId) {
            activeTabIndex_ = i;
            updateTabBar();
            if (tabs_[i]->onActivateCallback) tabs_[i]->onActivateCallback();
            return;
        }
    }
}

void TabGroup::moveTab(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= tabs_.size() || toIndex >= tabs_.size()) return;
    auto tab = tabs_[fromIndex];
    tabs_.erase(tabs_.begin() + fromIndex);
    tabs_.insert(tabs_.begin() + toIndex, tab);
    activeTabIndex_ = toIndex;
    updateTabBar();
}

void TabGroup::mergeWith(TabGroup* other) {
    if (!other) return;
    for (auto& tab : other->tabs_) {
        tabs_.push_back(tab);
    }
    other->tabs_.clear();
    updateTabBar();
}

std::shared_ptr<TabItem> TabGroup::getActiveTab() const {
    if (activeTabIndex_ < tabs_.size()) {
        return tabs_[activeTabIndex_];
    }
    return nullptr;
}

std::shared_ptr<TabItem> TabGroup::getTab(const std::string& tabId) const {
    for (auto& tab : tabs_) {
        if (tab->id == tabId) return tab;
    }
    return nullptr;
}

void TabGroup::create(HWND parentHwnd, const RECT& rect) {
    // Create tab group window
    hwnd_ = CreateWindowExW(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (!hwnd_) {
        std::cerr << "[Docking] Failed to create tab group window\n";
        return;
    }
    
    createTabBar();
    SetWindowPos(contentHwnd_, nullptr, rect.left, rect.top + getTabBarHeight(),
                 rect.right - rect.left, rect.bottom - rect.top - getTabBarHeight(),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void TabGroup::destroy() {
    if (contentHwnd_) DestroyWindow(contentHwnd_);
    if (tabBarHwnd_) DestroyWindow(tabBarHwnd_);
    if (hwnd_) DestroyWindow(hwnd_);
    contentHwnd_ = nullptr;
    tabBarHwnd_ = nullptr;
    hwnd_ = nullptr;
}

void TabGroup::layout(const RECT& rect) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (contentHwnd_) {
        RECT contentRect = getContentRect();
        SetWindowPos(contentHwnd_, nullptr, contentRect.left, contentRect.top,
                     contentRect.right - contentRect.left, contentRect.bottom - contentRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void TabGroup::paint(HDC hdc) {
    // Paint tab bar background
    RECT tabBarRect = { 0, 0, 0, getTabBarHeight() };
    if (hwnd_) GetClientRect(hwnd_, &tabBarRect);
    tabBarRect.bottom = getTabBarHeight();
    
    FillRect(hdc, &tabBarRect, manager_->getPanelBrush());
    
    // Paint tabs
    int x = 5;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        int tabWidth = 120;
        RECT tabRect = { x, 2, x + tabWidth, getTabBarHeight() - 2 };
        drawTab(hdc, i, tabRect, i == activeTabIndex_);
        x += tabWidth + 2;
    }
}

LRESULT TabGroup::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            size_t tabIndex = hitTestTab(x, y);
            if (tabIndex < tabs_.size()) {
                activateTab(tabs_[tabIndex]->id);
                if (manager_->getConfig().enableTabDragDrop) {
                    beginDrag(tabIndex);
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (isDragging_) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                // Update drag visual
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (isDragging_) {
                endDrag(false);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void TabGroup::createTabBar() {
    // Tab bar is drawn manually for custom styling
    tabBarHwnd_ = hwnd_;  // Use main window for tab bar
}

void TabGroup::updateTabBar() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

RECT TabGroup::getContentRect() const {
    RECT rect = { 0, 0, 0, 0 };
    if (hwnd_) {
        GetClientRect(hwnd_, &rect);
        rect.top += getTabBarHeight();
    }
    return rect;
}

int TabGroup::getTabBarHeight() const {
    return TAB_HEIGHT;
}

size_t TabGroup::hitTestTab(int x, int y) const {
    if (y < 2 || y > getTabBarHeight() - 2) return tabs_.size();
    int tabX = 5;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (x >= tabX && x < tabX + 120) return i;
        tabX += 122;
    }
    return tabs_.size();
}

void TabGroup::drawTab(HDC hdc, size_t index, const RECT& rect, bool active) {
    // Draw tab background
    COLORREF bgColor = active ? manager_->getTabActiveColor() : manager_->getTabInactiveColor();
    HBRUSH brush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    // Draw tab text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    HFONT oldFont = (HFONT)SelectObject(hdc, manager_->getTabFont());
    
    RECT textRect = rect;
    textRect.left += 8;
    textRect.right -= 8;
    DrawTextA(hdc, tabs_[index]->title.c_str(), -1, &textRect, 
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    SelectObject(hdc, oldFont);
    
    // Draw close button if enabled
    if (manager_->getConfig().showTabCloseButtons) {
        RECT closeRect = { rect.right - 18, rect.top + 6, rect.right - 6, rect.bottom - 6 };
        // Draw X
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, closeRect.left + 3, closeRect.top + 3, nullptr);
        LineTo(hdc, closeRect.right - 3, closeRect.bottom - 3);
        MoveToEx(hdc, closeRect.right - 3, closeRect.top + 3, nullptr);
        LineTo(hdc, closeRect.left + 3, closeRect.bottom - 3);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

bool TabGroup::beginDrag(size_t tabIndex) {
    isDragging_ = true;
    dragTabIndex_ = tabIndex;
    SetCapture(hwnd_);
    return true;
}

void TabGroup::endDrag(bool cancelled) {
    isDragging_ = false;
    ReleaseCapture();
    if (!cancelled && dragTabIndex_ < tabs_.size()) {
        // Handle drop logic - could move to another tab group
    }
}

JsonValue TabGroup::serialize() const {
    JsonValue json;
    json["id"] = id_;
    json["activeTabIndex"] = static_cast<int>(activeTabIndex_);
    JsonValue tabsJson = JsonValue::array();
    for (const auto& tab : tabs_) {
        JsonValue tabJson;
        tabJson["id"] = tab->id;
        tabJson["title"] = tab->title;
        tabJson["isModified"] = tab->isModified;
        tabJson["isPinned"] = tab->isPinned;
        tabsJson.push_back(tabJson);
    }
    json["tabs"] = tabsJson;
    return json;
}

void TabGroup::deserialize(const JsonValue& json) {
    // Restore tabs from serialized state
    activeTabIndex_ = json.value("activeTabIndex", 0);
}

// ============================================================================
// DockingPanel Implementation
// ============================================================================
DockingPanel::DockingPanel(DockingManager* manager, DockZone zone, const std::string& id)
    : manager_(manager), zone_(zone), id_(id) {
    switch (zone) {
        case DockZone::Left:
        case DockZone::Right:
            size_ = DEFAULT_SIDEBAR_WIDTH;
            break;
        case DockZone::Bottom:
            size_ = DEFAULT_BOTTOM_PANEL_HEIGHT;
            break;
        default:
            size_ = DEFAULT_SIDEBAR_WIDTH;
    }
}

DockingPanel::~DockingPanel() {
    destroy();
}

void DockingPanel::setState(PanelState state) {
    state_ = state;
    manager_->onLayoutChanged([]() {});
}

void DockingPanel::toggle() {
    if (state_ == PanelState::Expanded) {
        collapse();
    } else {
        expand();
    }
}

void DockingPanel::expand() {
    setState(PanelState::Expanded);
}

void DockingPanel::collapse() {
    setState(PanelState::Collapsed);
}

void DockingPanel::hide() {
    setState(PanelState::Hidden);
}

void DockingPanel::setSize(int size) {
    size_ = std::max(MIN_PANEL_SIZE, size);
    manager_->onLayoutChanged([]() {});
}

int DockingPanel::getActualSize() const {
    switch (state_) {
        case PanelState::Hidden:
            return 0;
        case PanelState::Collapsed:
            return 28;  // Just the title bar
        case PanelState::Expanded:
        case PanelState::AutoHide:
            return size_;
    }
    return size_;
}

void DockingPanel::setContent(HWND hwnd) {
    contentHwnd_ = hwnd;
    if (hwnd_ && contentHwnd_) {
        SetParent(contentHwnd_, hwnd_);
        RECT contentRect = getContentRect();
        SetWindowPos(contentHwnd_, nullptr, contentRect.left, contentRect.top,
                     contentRect.right - contentRect.left, contentRect.bottom - contentRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

TabGroup* DockingPanel::createTabGroup(const std::string& id) {
    auto tabGroup = std::make_unique<TabGroup>(manager_, id);
    TabGroup* ptr = tabGroup.get();
    tabGroups_[id] = std::move(tabGroup);
    return ptr;
}

void DockingPanel::removeTabGroup(const std::string& id) {
    tabGroups_.erase(id);
}

TabGroup* DockingPanel::getTabGroup(const std::string& id) const {
    auto it = tabGroups_.find(id);
    if (it != tabGroups_.end()) return it->second.get();
    return nullptr;
}

void DockingPanel::create(HWND parentHwnd) {
    hwnd_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        0, 0, 0, 0,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (!hwnd_) {
        std::cerr << "[Docking] Failed to create panel window\n";
        return;
    }

    createTitleBar();
}

void DockingPanel::destroy() {
    if (titleBarHwnd_) DestroyWindow(titleBarHwnd_);
    if (hwnd_) DestroyWindow(hwnd_);
    titleBarHwnd_ = nullptr;
    hwnd_ = nullptr;
}

void DockingPanel::layout(const RECT& rect) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (contentHwnd_) {
        RECT contentRect = getContentRect();
        SetWindowPos(contentHwnd_, nullptr, contentRect.left, contentRect.top,
                     contentRect.right - contentRect.left, contentRect.bottom - contentRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void DockingPanel::paint(HDC hdc) {
    // Paint title bar
    RECT titleRect = { 0, 0, 0, 28 };
    if (hwnd_) GetClientRect(hwnd_, &titleRect);
    titleRect.bottom = 28;
    
    FillRect(hdc, &titleRect, manager_->getPanelBrush());
    
    // Draw title text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    HFONT oldFont = (HFONT)SelectObject(hdc, manager_->getTabFont());
    
    RECT textRect = titleRect;
    textRect.left += 10;
    DrawTextA(hdc, title_.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
    
    // Draw gripper
    drawGripper(hdc);
    
    // Draw collapse button
    drawCollapseButton(hdc);
}

LRESULT DockingPanel::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (hitTestCollapseButton(x, y)) {
                toggle();
            }
            return 0;
        }
        case WM_SETCURSOR: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (hitTestGripper(x, y)) {
                SetCursor(manager_->getResizeCursor(zone_));
                return TRUE;
            }
            break;
        }
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

bool DockingPanel::hitTestGripper(int x, int y) const {
    RECT rect;
    if (!hwnd_) return false;
    GetClientRect(hwnd_, &rect);
    
    // Gripper is on the edge facing the center
    switch (zone_) {
        case DockZone::Left:
            return x > rect.right - GRIPPER_WIDTH - 5 && x < rect.right - 5;
        case DockZone::Right:
            return x > 5 && x < 5 + GRIPPER_WIDTH;
        case DockZone::Bottom:
            return y > 5 && y < 5 + GRIPPER_WIDTH;
        default:
            return false;
    }
}

bool DockingPanel::hitTestCollapseButton(int x, int y) const {
    // Collapse button is in the title bar
    return y > 2 && y < 26 && x > 5 && x < 25;
}

void DockingPanel::createTitleBar() {
    // Title bar is drawn on the main panel window
    titleBarHwnd_ = hwnd_;
}

void DockingPanel::updateTitleBar() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

RECT DockingPanel::getContentRect() const {
    RECT rect = { 0, 0, 0, 0 };
    if (hwnd_) {
        GetClientRect(hwnd_, &rect);
        if (state_ != PanelState::Collapsed) {
            rect.top += 28;  // Title bar height
        }
    }
    return rect;
}

void DockingPanel::drawGripper(HDC hdc) {
    RECT rect;
    if (!hwnd_) return;
    GetClientRect(hwnd_, &rect);
    
    // Draw gripper dots
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    int centerX, centerY;
    switch (zone_) {
        case DockZone::Left:
            centerX = rect.right - 7;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 8; ++j) {
                    SetPixel(hdc, centerX + i * 2, rect.top + 40 + j * 4, RGB(100, 100, 100));
                }
            }
            break;
        case DockZone::Right:
            centerX = 7;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 8; ++j) {
                    SetPixel(hdc, centerX + i * 2, rect.top + 40 + j * 4, RGB(100, 100, 100));
                }
            }
            break;
        case DockZone::Bottom:
            centerY = 7;
            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 3; ++j) {
                    SetPixel(hdc, rect.left + 40 + i * 4, centerY + j * 2, RGB(100, 100, 100));
                }
            }
            break;
        default:
            break;
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DockingPanel::drawCollapseButton(HDC hdc) {
    // Draw collapse/expand arrow
    RECT rect = { 5, 2, 25, 26 };
    
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Draw arrow pointing left/right/up based on state
    if (state_ == PanelState::Expanded) {
        // Pointing toward edge (collapse)
        switch (zone_) {
            case DockZone::Left:
                // Arrow pointing left
                MoveToEx(hdc, 15, 8, nullptr);
                LineTo(hdc, 8, 14);
                LineTo(hdc, 15, 20);
                break;
            case DockZone::Right:
                // Arrow pointing right
                MoveToEx(hdc, 10, 8, nullptr);
                LineTo(hdc, 17, 14);
                LineTo(hdc, 10, 20);
                break;
            case DockZone::Bottom:
                // Arrow pointing down
                MoveToEx(hdc, 8, 10, nullptr);
                LineTo(hdc, 14, 17);
                LineTo(hdc, 20, 10);
                break;
            default:
                break;
        }
    } else {
        // Arrow pointing away from edge (expand)
        switch (zone_) {
            case DockZone::Left:
                MoveToEx(hdc, 8, 8, nullptr);
                LineTo(hdc, 15, 14);
                LineTo(hdc, 8, 20);
                break;
            case DockZone::Right:
                MoveToEx(hdc, 17, 8, nullptr);
                LineTo(hdc, 10, 14);
                LineTo(hdc, 17, 20);
                break;
            case DockZone::Bottom:
                MoveToEx(hdc, 8, 17, nullptr);
                LineTo(hdc, 14, 10);
                LineTo(hdc, 20, 17);
                break;
            default:
                break;
        }
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

JsonValue DockingPanel::serialize() const {
    JsonValue json;
    json["id"] = id_;
    json["zone"] = dockZoneToString(zone_);
    json["title"] = title_;
    json["state"] = panelStateToString(state_);
    json["size"] = size_;
    return json;
}

void DockingPanel::deserialize(const JsonValue& json) {
    title_ = json.value("title", "");
    state_ = stringToPanelState(json.value("state", "expanded"));
    size_ = json.value("size", DEFAULT_SIDEBAR_WIDTH);
}

// ============================================================================
// DockingManager Implementation
// ============================================================================
DockingManager& DockingManager::instance() {
    static DockingManager inst;
    return inst;
}

bool DockingManager::initialize(HWND mainHwnd) {
    if (initialized_) return true;
    
    mainHwnd_ = mainHwnd;
    
    // Create default panels
    createPanel(DockZone::Left, "explorer");
    createPanel(DockZone::Right, "properties");
    createPanel(DockZone::Bottom, "terminal");
    
    // Create main tab group
    mainTabGroup_ = std::make_unique<TabGroup>(this, "main");
    
    // Create GDI resources
    createGdiResources();
    
    // Restore layout if configured
    if (config_.restoreLayoutOnStartup) {
        restoreLayout();
    }
    
    initialized_ = true;
    std::cout << "[Docking] Advanced docking system initialized\n";
    return true;
}

void DockingManager::shutdown() {
    if (!initialized_) return;
    
    // Save layout
    saveLayout();
    
    // Clean up
    destroyGdiResources();
    panels_.clear();
    tabGroups_.clear();
    splitContainers_.clear();
    mainTabGroup_.reset();
    
    initialized_ = false;
    std::cout << "[Docking] Advanced docking system shutdown\n";
}

void DockingManager::updateLayout() {
    if (!mainHwnd_) return;
    
    RECT clientRect;
    GetClientRect(mainHwnd_, &clientRect);
    
    // Layout panels
    for (auto& [zone, panel] : panels_) {
        RECT panelRect;
        switch (zone) {
            case DockZone::Left:
                panelRect = getLeftPanelRect(clientRect);
                break;
            case DockZone::Right:
                panelRect = getRightPanelRect(clientRect);
                break;
            case DockZone::Bottom:
                panelRect = getBottomPanelRect(clientRect);
                break;
            default:
                continue;
        }
        panel->layout(panelRect);
    }
    
    // Layout main tab group
    if (mainTabGroup_) {
        RECT centerRect = getCenterRect(clientRect);
        mainTabGroup_->layout(centerRect);
    }
    
    notifyLayoutChanged();
}

void DockingManager::saveLayout() {
    JsonValue root;
    root["config"] = config_.toJson();

    JsonValue panelsJson = JsonValue::array();
    for (const auto& [zone, panel] : panels_) {
        panelsJson.push_back(panel->serialize());
    }
    root["panels"] = panelsJson;

    if (mainTabGroup_) {
        root["mainTabGroup"] = mainTabGroup_->serialize();
    }

    std::ofstream file(config_.layoutFilePath);
    if (file.is_open()) {
        file << root.dump(4);
        file.close();
        std::cout << "[Docking] Layout saved to " << config_.layoutFilePath << "\n";
    }
}

void DockingManager::restoreLayout() {
    std::ifstream file(config_.layoutFilePath);
    if (!file.is_open()) return;

    try {
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        JsonValue root = JsonValue::parse(content);

        if (root.contains("config")) {
            config_ = DockingConfig::fromJson(root["config"]);
        }

        // Restore panels
        if (root.contains("panels")) {
            for (const auto& panelJson : root["panels"]) {
                DockZone zone = stringToDockZone(panelJson.value("zone", ""));
                std::string id = panelJson.value("id", "");
                if (auto* panel = getPanel(zone)) {
                    panel->deserialize(panelJson);
                }
            }
        }

        if (root.contains("mainTabGroup") && mainTabGroup_) {
            mainTabGroup_->deserialize(root["mainTabGroup"]);
        }

        updateLayout();
        std::cout << "[Docking] Layout restored from " << config_.layoutFilePath << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Docking] Failed to parse layout file: " << e.what() << "\n";
    }
}

void DockingManager::resetLayout() {
    config_ = DockingConfig();
    for (auto& [zone, panel] : panels_) {
        panel->setState(PanelState::Expanded);
    }
    updateLayout();
    saveLayout();
}

DockingPanel* DockingManager::getPanel(DockZone zone) const {
    auto it = panels_.find(zone);
    if (it != panels_.end()) return it->second.get();
    return nullptr;
}

DockingPanel* DockingManager::createPanel(DockZone zone, const std::string& id) {
    auto panel = std::make_unique<DockingPanel>(this, zone, id);
    DockingPanel* ptr = panel.get();
    panels_[zone] = std::move(panel);
    
    if (mainHwnd_) {
        ptr->create(mainHwnd_);
    }
    
    return ptr;
}

void DockingManager::destroyPanel(const std::string& id) {
    for (auto it = panels_.begin(); it != panels_.end(); ++it) {
        if (it->second->getTitle() == id) {
            panels_.erase(it);
            return;
        }
    }
}

void DockingManager::togglePanel(DockZone zone) {
    if (auto* panel = getPanel(zone)) {
        panel->toggle();
        updateLayout();
    }
}

void DockingManager::showPanel(DockZone zone) {
    if (auto* panel = getPanel(zone)) {
        panel->expand();
        updateLayout();
    }
}

void DockingManager::hidePanel(DockZone zone) {
    if (auto* panel = getPanel(zone)) {
        panel->hide();
        updateLayout();
    }
}

TabGroup* DockingManager::createTabGroup(const std::string& id) {
    auto tabGroup = std::make_unique<TabGroup>(this, id);
    TabGroup* ptr = tabGroup.get();
    tabGroups_[id] = std::move(tabGroup);
    return ptr;
}

void DockingManager::destroyTabGroup(const std::string& id) {
    tabGroups_.erase(id);
}

TabGroup* DockingManager::findTabGroup(const std::string& id) const {
    auto it = tabGroups_.find(id);
    if (it != tabGroups_.end()) return it->second.get();
    return nullptr;
}

SplitContainer* DockingManager::createSplitContainer(SplitContainer::Orientation orient) {
    auto container = std::make_unique<SplitContainer>(this, orient);
    SplitContainer* ptr = container.get();
    splitContainers_.push_back(std::move(container));
    return ptr;
}

void DockingManager::destroySplitContainer(SplitContainer* container) {
    splitContainers_.erase(
        std::remove_if(splitContainers_.begin(), splitContainers_.end(),
            [container](const std::unique_ptr<SplitContainer>& ptr) {
                return ptr.get() == container;
            }),
        splitContainers_.end()
    );
}

LRESULT DockingManager::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            updateLayout();
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HCURSOR DockingManager::getResizeCursor(DockZone zone) const {
    switch (zone) {
        case DockZone::Left:
        case DockZone::Right:
            return hCursorResizeWE_;
        case DockZone::Bottom:
            return hCursorResizeNS_;
        default:
            return LoadCursor(nullptr, IDC_ARROW);
    }
}

void DockingManager::createGdiResources() {
    // Create fonts
    tabFont_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    
    // Create brushes
    panelBrush_ = CreateSolidBrush(RGB(37, 37, 38));
    
    // Load cursors
    hCursorResizeWE_ = LoadCursor(nullptr, IDC_SIZEWE);
    hCursorResizeNS_ = LoadCursor(nullptr, IDC_SIZENS);
}

void DockingManager::destroyGdiResources() {
    if (tabFont_) DeleteObject(tabFont_);
    if (panelBrush_) DeleteObject(panelBrush_);
    tabFont_ = nullptr;
    panelBrush_ = nullptr;
}

void DockingManager::notifyLayoutChanged() {
    for (auto& cb : layoutChangedCallbacks_) {
        cb();
    }
}

RECT DockingManager::getMainClientRect() const {
    RECT rect = { 0, 0, 0, 0 };
    if (mainHwnd_) GetClientRect(mainHwnd_, &rect);
    return rect;
}

RECT DockingManager::getLeftPanelRect(const RECT& total) const {
    RECT rect = total;
    if (auto* panel = getPanel(DockZone::Left)) {
        int size = panel->getActualSize();
        rect.right = rect.left + size;
    }
    return rect;
}

RECT DockingManager::getRightPanelRect(const RECT& total) const {
    RECT rect = total;
    if (auto* panel = getPanel(DockZone::Right)) {
        int size = panel->getActualSize();
        rect.left = rect.right - size;
    }
    return rect;
}

RECT DockingManager::getBottomPanelRect(const RECT& total) const {
    RECT rect = total;
    if (auto* panel = getPanel(DockZone::Bottom)) {
        int size = panel->getActualSize();
        rect.top = rect.bottom - size;
    }
    return rect;
}

RECT DockingManager::getCenterRect(const RECT& total) const {
    RECT rect = total;
    
    // Subtract left panel
    if (auto* left = getPanel(DockZone::Left)) {
        rect.left += left->getActualSize();
    }
    
    // Subtract right panel
    if (auto* right = getPanel(DockZone::Right)) {
        rect.right -= right->getActualSize();
    }
    
    // Subtract bottom panel
    if (auto* bottom = getPanel(DockZone::Bottom)) {
        rect.bottom -= bottom->getActualSize();
    }
    
    return rect;
}

// ============================================================================
// SplitContainer Implementation
// ============================================================================
SplitContainer::SplitContainer(DockingManager* manager, Orientation orient)
    : manager_(manager), orientation_(orient), splitRatio_(0.5f), hwnd_(nullptr),
      firstType_(PaneType::None), secondType_(PaneType::None), isDragging_(false) {
    firstPane_.tabGroup_ = nullptr;
    secondPane_.tabGroup_ = nullptr;
}

SplitContainer::~SplitContainer() {
    destroy();
}

void SplitContainer::setFirstPane(TabGroup* pane) {
    firstType_ = PaneType::TabGroup;
    firstPane_.tabGroup_ = pane;
}

void SplitContainer::setSecondPane(TabGroup* pane) {
    secondType_ = PaneType::TabGroup;
    secondPane_.tabGroup_ = pane;
}

void SplitContainer::setFirstPane(SplitContainer* pane) {
    firstType_ = PaneType::SplitContainer;
    firstPane_.splitContainer_ = pane;
}

void SplitContainer::setSecondPane(SplitContainer* pane) {
    secondType_ = PaneType::SplitContainer;
    secondPane_.splitContainer_ = pane;
}

void SplitContainer::setSplitRatio(float ratio) {
    splitRatio_ = std::max(0.1f, std::min(0.9f, ratio));
}

void SplitContainer::create(HWND parentHwnd, const RECT& rect) {
    (void)parentHwnd;
    (void)rect;
    // TODO: Implement window creation
}

void SplitContainer::destroy() {
    // TODO: Implement cleanup
    hwnd_ = nullptr;
}

void SplitContainer::layout(const RECT& rect) {
    (void)rect;
    // TODO: Implement layout
}

void SplitContainer::paint(HDC hdc) {
    (void)hdc;
    // TODO: Implement painting
}

LRESULT SplitContainer::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    (void)lParam;
    switch (msg) {
        case WM_PAINT:
        case WM_SIZE:
            return 0;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

bool SplitContainer::hitTestSplitter(int x, int y) const {
    (void)x;
    (void)y;
    return false;
}

void SplitContainer::beginSplitterDrag(int x, int y) {
    isDragging_ = true;
    dragStartPos_ = {x, y};
    dragStartRatio_ = splitRatio_;
}

void SplitContainer::updateSplitterDrag(int x, int y) {
    if (!isDragging_) return;
    (void)x;
    (void)y;
    // TODO: Calculate new split ratio based on drag position
}

void SplitContainer::endSplitterDrag() {
    isDragging_ = false;
}

// ============================================================================
// Utility Functions
// ============================================================================
std::string dockZoneToString(DockZone zone) {
    switch (zone) {
        case DockZone::Left: return "left";
        case DockZone::Right: return "right";
        case DockZone::Bottom: return "bottom";
        case DockZone::Center: return "center";
        case DockZone::Floating: return "floating";
        default: return "none";
    }
}

DockZone stringToDockZone(const std::string& str) {
    if (str == "left") return DockZone::Left;
    if (str == "right") return DockZone::Right;
    if (str == "bottom") return DockZone::Bottom;
    if (str == "center") return DockZone::Center;
    if (str == "floating") return DockZone::Floating;
    return DockZone::None;
}

std::string panelStateToString(PanelState state) {
    switch (state) {
        case PanelState::Hidden: return "hidden";
        case PanelState::Collapsed: return "collapsed";
        case PanelState::Expanded: return "expanded";
        case PanelState::AutoHide: return "autohide";
        default: return "expanded";
    }
}

PanelState stringToPanelState(const std::string& str) {
    if (str == "hidden") return PanelState::Hidden;
    if (str == "collapsed") return PanelState::Collapsed;
    if (str == "expanded") return PanelState::Expanded;
    if (str == "autohide") return PanelState::AutoHide;
    return PanelState::Expanded;
}

} // namespace UI
} // namespace RawrXD
