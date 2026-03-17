#pragma once
#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD {

// Color representation
struct Color {
    uint8_t r, g, b, a;

    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    static Color fromHex(const std::string& hex);
    std::string toHex() const;
    COLORREF toCOLORREF() const;
    
    static Color fromCOLORREF(COLORREF color);
};

// Theme color palette
struct ThemePalette {
    // Background colors
    Color background;
    Color backgroundAlt;
    Color backgroundHover;
    Color backgroundActive;
    
    // Foreground colors
    Color foreground;
    Color foregroundAlt;
    Color foregroundDisabled;
    
    // UI element colors
    Color border;
    Color borderFocus;
    Color selection;
    Color selectionInactive;
    
    // Editor colors
    Color lineNumber;
    Color lineNumberActive;
    Color cursor;
    Color currentLine;
    Color whitespace;
    
    // Syntax highlighting
    Color syntaxKeyword;
    Color syntaxString;
    Color syntaxComment;
    Color syntaxFunction;
    Color syntaxVariable;
    Color syntaxType;
    Color syntaxNumber;
    Color syntaxOperator;
    
    // Status colors
    Color success;
    Color warning;
    Color error;
    Color info;
    
    // Git colors
    Color gitAdded;
    Color gitModified;
    Color gitDeleted;
    Color gitUntracked;
    Color gitIgnored;
    
    // Terminal colors
    Color terminalBlack;
    Color terminalRed;
    Color terminalGreen;
    Color terminalYellow;
    Color terminalBlue;
    Color terminalMagenta;
    Color terminalCyan;
    Color terminalWhite;
};

// Theme definition
struct Theme {
    std::string name;
    std::string author;
    std::string version;
    bool isDark;
    ThemePalette palette;
    
    // Serialization
    std::string toJSON() const;
    static std::optional<Theme> fromJSON(const std::string& json);
};

// Theme manager
class ThemeManager {
public:
    static ThemeManager& getInstance();
    
    // Theme loading
    bool loadTheme(const std::string& name);
    bool loadThemeFromFile(const std::string& path);
    bool loadThemeFromJSON(const std::string& json);
    
    // Theme saving
    bool saveTheme(const Theme& theme, const std::string& path);
    bool saveCurrentTheme();
    
    // Built-in themes
    static Theme getDarkTheme();
    static Theme getLightTheme();
    static Theme getHighContrastTheme();
    
    // Theme management
    std::vector<std::string> getAvailableThemes() const;
    std::optional<Theme> getTheme(const std::string& name) const;
    const Theme& getCurrentTheme() const;
    bool setCurrentTheme(const std::string& name);
    
    // Theme modification
    bool setColor(const std::string& colorName, const Color& color);
    std::optional<Color> getColor(const std::string& colorName) const;
    
    // Theme application
    void applyTheme(HWND hwnd);
    void applyThemeToAllWindows();
    
    // Theme change notifications
    using ThemeChangeCallback = std::function<void(const Theme&)>;
    void registerThemeChangeCallback(ThemeChangeCallback callback);
    
    // Color utilities
    static Color interpolate(const Color& a, const Color& b, float t);
    static Color lighten(const Color& color, float amount);
    static Color darken(const Color& color, float amount);
    static Color withAlpha(const Color& color, uint8_t alpha);

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    Theme currentTheme_;
    std::unordered_map<std::string, Theme> themes_;
    std::vector<ThemeChangeCallback> callbacks_;
    
    void notifyThemeChanged();
    void loadBuiltInThemes();
    std::string getThemesDirectory() const;
};

// Theme-aware control base
class ThemedControl {
public:
    virtual ~ThemedControl() = default;
    virtual void onThemeChanged(const Theme& theme) = 0;
    
protected:
    void subscribeToThemeChanges();
};

} // namespace RawrXD

#endif // THEME_MANAGER_H
