#pragma once
/**
 * @file breadcrumb_navigator.h
 * @brief File path breadcrumbs and symbol navigation
 * Batch 4 - Item 57: Breadcrumb navigator
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

struct BreadcrumbItem {
    std::string name;
    std::string path;
    std::string type;
    int line;
    int column;
    HICON icon;
    std::vector<BreadcrumbItem> children;
};

enum class BreadcrumbMode {
    FilePath,
    SymbolPath,
    Mixed
};

class BreadcrumbNavigator {
public:
    BreadcrumbNavigator();
    ~BreadcrumbNavigator();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);

    // Path breadcrumbs
    void setFilePath(const std::string& path);
    void clearFilePath();
    std::string getFilePath() const;

    // Symbol breadcrumbs
    void setSymbolPath(const std::vector<BreadcrumbItem>& symbols);
    void addSymbol(const BreadcrumbItem& symbol);
    void clearSymbols();

    // Mixed mode
    void setMode(BreadcrumbMode mode);
    BreadcrumbMode getMode() const;

    // Navigation
    void navigateTo(const std::string& path);
    void navigateUp();
    void navigateBack();
    void navigateForward();
    bool canGoBack() const;
    bool canGoForward() const;

    // Dropdown
    void showDropdown(const BreadcrumbItem& item, int x, int y);
    void hideDropdown();
    bool isDropdownVisible() const;

    // Symbol providers
    void registerSymbolProvider(std::function<std::vector<BreadcrumbItem>(
        const std::string& path)> provider);
    void updateSymbols();

    // Events
    using NavigationCallback = std::function<void(const std::string& path)>;
    using SymbolSelectCallback = std::function<void(const BreadcrumbItem& symbol)>;
    void onPathNavigate(NavigationCallback callback);
    void onSymbolSelect(SymbolSelectCallback callback);

    // Configuration
    void setShowIcons(bool show);
    void setShowRoot(bool show);
    void setSeparator(const std::string& separator);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    HWND m_dropdown{nullptr};
    std::vector<BreadcrumbItem> m_pathItems;
    std::vector<BreadcrumbItem> m_symbolItems;
    std::vector<std::string> m_history;
    size_t m_historyIndex{0};
    BreadcrumbMode m_mode{BreadcrumbMode::Mixed};
    bool m_showIcons{true};
    bool m_showRoot{true};
    std::string m_separator{" > "};

    NavigationCallback m_navigateCallback;
    SymbolSelectCallback m_symbolCallback;
    std::vector<std::function<std::vector<BreadcrumbItem>(const std::string&)>> m_symbolProviders;

    void layout();
    void draw(HDC hdc);
    void drawItem(HDC hdc, const BreadcrumbItem& item, RECT& rect);
    void drawSeparator(HDC hdc, RECT& rect);
    std::vector<BreadcrumbItem> parsePath(const std::string& path);
    BreadcrumbItem hitTest(int x, int y);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
BreadcrumbNavigator& getBreadcrumbNavigator();

} // namespace RawrXD::UI
