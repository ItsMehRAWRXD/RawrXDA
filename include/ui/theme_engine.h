#pragma once
/**
 * @file theme_engine.h
 * @brief Theme management and dynamic switching
 * Batch 4 - Item 46: Theme engine
 */

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

struct ColorScheme {
    // Editor colors
    uint32_t background;
    uint32_t foreground;
    uint32_t selectionBg;
    uint32_t selectionFg;
    uint32_t cursor;
    uint32_t lineHighlight;
    uint32_t lineNumber;
    uint32_t lineNumberActive;

    // Syntax colors
    uint32_t keyword;
    uint32_t string;
    uint32_t comment;
    uint32_t number;
    uint32_t function;
    uint32_t type;
    uint32_t variable;
    uint32_t operator_;
    uint32_t preprocessor;

    // UI colors
    uint32_t sidebarBg;
    uint32_t sidebarFg;
    uint32_t activityBarBg;
    uint32_t activityBarFg;
    uint32_t statusBarBg;
    uint32_t statusBarFg;
    uint32_t panelBg;
    uint32_t panelFg;
    uint32_t tabBg;
    uint32_t tabFg;
    uint32_t tabActiveBg;
    uint32_t tabActiveFg;
    uint32_t border;
    uint32_t accent;

    // Semantic colors
    uint32_t error;
    uint32_t warning;
    uint32_t info;
    uint32_t hint;
    uint32_t success;
};

struct Theme {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    bool isDark;
    ColorScheme colors;
    std::map<std::string, std::string> tokenColors;
    std::map<std::string, std::string> uiColors;
};

struct TokenStyle {
    std::string scope;
    uint32_t foreground;
    uint32_t background;
    bool bold;
    bool italic;
    bool underline;
};

class ThemeEngine {
public:
    ThemeEngine();
    ~ThemeEngine();

    // Initialization
    bool initialize();
    void shutdown();

    // Theme loading
    bool loadTheme(const std::string& themePath);
    bool loadThemeFromJson(const std::string& json);
    bool loadBuiltInThemes();

    // Theme management
    bool setTheme(const std::string& themeId);
    std::string getCurrentTheme() const;
    std::vector<std::string> getAvailableThemes() const;
    std::optional<Theme> getTheme(const std::string& themeId) const;

    // Color access
    uint32_t getColor(const std::string& colorId) const;
    uint32_t getSyntaxColor(const std::string& tokenType) const;
    COLORREF getColorRef(const std::string& colorId) const;

    // Token styles
    TokenStyle getTokenStyle(const std::string& scope) const;
    void registerTokenStyle(const std::string& scope, const TokenStyle& style);

    // Customization
    void overrideColor(const std::string& colorId, uint32_t color);
    void resetColor(const std::string& colorId);
    void saveCustomizations();
    void loadCustomizations();

    // Events
    using ThemeChangeCallback = std::function<void(const std::string& themeId)>;
    void onThemeChanged(ThemeChangeCallback callback);

    // Built-in themes
    void registerBuiltInDarkTheme();
    void registerBuiltInLightTheme();
    void registerBuiltInHighContrastTheme();

    // Import/Export
    bool exportTheme(const std::string& themeId, const std::string& outputPath);
    std::string themeToJson(const Theme& theme) const;

    // System integration
    void followSystemTheme();
    bool isSystemDarkMode() const;

private:
    std::map<std::string, Theme> m_themes;
    std::string m_currentTheme;
    std::map<std::string, uint32_t> m_colorOverrides;
    std::vector<ThemeChangeCallback> m_callbacks;
    mutable std::mutex m_mutex;

    uint32_t parseColor(const std::string& colorStr) const;
    std::string colorToString(uint32_t color) const;
    void notifyThemeChanged(const std::string& themeId);
    Theme createDefaultDarkTheme();
    Theme createDefaultLightTheme();
    Theme createHighContrastTheme();
};

// Global instance
ThemeEngine& getThemeEngine();

// Utility functions
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
COLORREF toColorRef(uint32_t color);
uint32_t fromColorRef(COLORREF color);

} // namespace RawrXD::UI
