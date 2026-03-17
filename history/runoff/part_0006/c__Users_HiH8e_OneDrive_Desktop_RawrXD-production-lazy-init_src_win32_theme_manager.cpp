#include "theme_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

using json = nlohmann::json;

namespace RawrXD {

// Color implementation
Color Color::fromHex(const std::string& hex) {
    std::string clean = hex;
    if (clean[0] == '#') clean = clean.substr(1);
    
    unsigned int value;
    std::istringstream(clean) >> std::hex >> value;
    
    if (clean.length() == 6) {
        return Color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
    } else if (clean.length() == 8) {
        return Color((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }
    
    return Color();
}

std::string Color::toHex() const {
    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << static_cast<int>(r)
       << std::setw(2) << static_cast<int>(g)
       << std::setw(2) << static_cast<int>(b);
    if (a != 255) {
        ss << std::setw(2) << static_cast<int>(a);
    }
    return ss.str();
}

COLORREF Color::toCOLORREF() const {
    return RGB(r, g, b);
}

Color Color::fromCOLORREF(COLORREF color) {
    return Color(GetRValue(color), GetGValue(color), GetBValue(color));
}

// Theme implementation
std::string Theme::toJSON() const {
    json j;
    j["name"] = name;
    j["author"] = author;
    j["version"] = version;
    j["isDark"] = isDark;
    
    auto colorToJson = [](const Color& c) {
        return c.toHex();
    };
    
    j["palette"]["background"] = colorToJson(palette.background);
    j["palette"]["foreground"] = colorToJson(palette.foreground);
    j["palette"]["selection"] = colorToJson(palette.selection);
    j["palette"]["cursor"] = colorToJson(palette.cursor);
    
    j["palette"]["syntaxKeyword"] = colorToJson(palette.syntaxKeyword);
    j["palette"]["syntaxString"] = colorToJson(palette.syntaxString);
    j["palette"]["syntaxComment"] = colorToJson(palette.syntaxComment);
    j["palette"]["syntaxFunction"] = colorToJson(palette.syntaxFunction);
    
    j["palette"]["gitAdded"] = colorToJson(palette.gitAdded);
    j["palette"]["gitModified"] = colorToJson(palette.gitModified);
    j["palette"]["gitDeleted"] = colorToJson(palette.gitDeleted);
    
    return j.dump(2);
}

std::optional<Theme> Theme::fromJSON(const std::string& jsonStr) {
    try {
        auto j = json::parse(jsonStr);
        Theme theme;
        
        theme.name = j.contains("name") ? j["name"].get<std::string>() : std::string("Untitled");
        theme.author = j.contains("author") ? j["author"].get<std::string>() : std::string("Unknown");
        theme.version = j.contains("version") ? j["version"].get<std::string>() : std::string("1.0.0");
        theme.isDark = j.contains("isDark") ? j["isDark"].get<bool>() : true;
        
        auto loadColor = [&](const std::string& path, const Color& defaultColor) -> Color {
            if (j.contains("palette") && j["palette"].contains(path)) {
                return Color::fromHex(j["palette"][path].get<std::string>());
            }
            return defaultColor;
        };
        
        theme.palette.background = loadColor("background", Color(30, 30, 30));
        theme.palette.foreground = loadColor("foreground", Color(220, 220, 220));
        theme.palette.selection = loadColor("selection", Color(51, 153, 255));
        theme.palette.cursor = loadColor("cursor", Color(255, 255, 255));
        
        theme.palette.syntaxKeyword = loadColor("syntaxKeyword", Color(86, 156, 214));
        theme.palette.syntaxString = loadColor("syntaxString", Color(206, 145, 120));
        theme.palette.syntaxComment = loadColor("syntaxComment", Color(106, 153, 85));
        theme.palette.syntaxFunction = loadColor("syntaxFunction", Color(220, 220, 170));
        
        theme.palette.gitAdded = loadColor("gitAdded", Color(80, 250, 123));
        theme.palette.gitModified = loadColor("gitModified", Color(255, 184, 108));
        theme.palette.gitDeleted = loadColor("gitDeleted", Color(255, 85, 85));
        
        return theme;
    } catch (...) {
        return std::nullopt;
    }
}

// ThemeManager implementation
ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    loadBuiltInThemes();
    currentTheme_ = getDarkTheme();
}

void ThemeManager::loadBuiltInThemes() {
    themes_["dark"] = getDarkTheme();
    themes_["light"] = getLightTheme();
    themes_["high-contrast"] = getHighContrastTheme();
}

Theme ThemeManager::getDarkTheme() {
    Theme theme;
    theme.name = "Dark";
    theme.author = "RawrXD";
    theme.version = "1.0.0";
    theme.isDark = true;
    
    theme.palette.background = Color(30, 30, 30);
    theme.palette.backgroundAlt = Color(37, 37, 38);
    theme.palette.backgroundHover = Color(45, 45, 48);
    theme.palette.backgroundActive = Color(51, 51, 55);
    
    theme.palette.foreground = Color(220, 220, 220);
    theme.palette.foregroundAlt = Color(180, 180, 180);
    theme.palette.foregroundDisabled = Color(100, 100, 100);
    
    theme.palette.border = Color(60, 60, 60);
    theme.palette.borderFocus = Color(0, 122, 204);
    theme.palette.selection = Color(51, 153, 255);
    theme.palette.selectionInactive = Color(80, 80, 80);
    
    theme.palette.lineNumber = Color(133, 133, 133);
    theme.palette.lineNumberActive = Color(200, 200, 200);
    theme.palette.cursor = Color(255, 255, 255);
    theme.palette.currentLine = Color(40, 40, 40);
    theme.palette.whitespace = Color(80, 80, 80);
    
    theme.palette.syntaxKeyword = Color(86, 156, 214);
    theme.palette.syntaxString = Color(206, 145, 120);
    theme.palette.syntaxComment = Color(106, 153, 85);
    theme.palette.syntaxFunction = Color(220, 220, 170);
    theme.palette.syntaxVariable = Color(156, 220, 254);
    theme.palette.syntaxType = Color(78, 201, 176);
    theme.palette.syntaxNumber = Color(181, 206, 168);
    theme.palette.syntaxOperator = Color(212, 212, 212);
    
    theme.palette.success = Color(80, 250, 123);
    theme.palette.warning = Color(255, 184, 108);
    theme.palette.error = Color(255, 85, 85);
    theme.palette.info = Color(139, 233, 253);
    
    theme.palette.gitAdded = Color(80, 250, 123);
    theme.palette.gitModified = Color(255, 184, 108);
    theme.palette.gitDeleted = Color(255, 85, 85);
    theme.palette.gitUntracked = Color(200, 200, 200);
    theme.palette.gitIgnored = Color(100, 100, 100);
    
    theme.palette.terminalBlack = Color(0, 0, 0);
    theme.palette.terminalRed = Color(205, 49, 49);
    theme.palette.terminalGreen = Color(13, 188, 121);
    theme.palette.terminalYellow = Color(229, 229, 16);
    theme.palette.terminalBlue = Color(36, 114, 200);
    theme.palette.terminalMagenta = Color(188, 63, 188);
    theme.palette.terminalCyan = Color(17, 168, 205);
    theme.palette.terminalWhite = Color(229, 229, 229);
    
    return theme;
}

Theme ThemeManager::getLightTheme() {
    Theme theme;
    theme.name = "Light";
    theme.author = "RawrXD";
    theme.version = "1.0.0";
    theme.isDark = false;
    
    theme.palette.background = Color(255, 255, 255);
    theme.palette.backgroundAlt = Color(245, 245, 245);
    theme.palette.backgroundHover = Color(235, 235, 235);
    theme.palette.backgroundActive = Color(225, 225, 225);
    
    theme.palette.foreground = Color(0, 0, 0);
    theme.palette.foregroundAlt = Color(60, 60, 60);
    theme.palette.foregroundDisabled = Color(160, 160, 160);
    
    theme.palette.border = Color(200, 200, 200);
    theme.palette.borderFocus = Color(0, 122, 204);
    theme.palette.selection = Color(173, 214, 255);
    theme.palette.selectionInactive = Color(220, 220, 220);
    
    theme.palette.syntaxKeyword = Color(0, 0, 255);
    theme.palette.syntaxString = Color(163, 21, 21);
    theme.palette.syntaxComment = Color(0, 128, 0);
    theme.palette.syntaxFunction = Color(121, 94, 38);
    
    theme.palette.gitAdded = Color(0, 200, 0);
    theme.palette.gitModified = Color(255, 140, 0);
    theme.palette.gitDeleted = Color(200, 0, 0);
    
    return theme;
}

Theme ThemeManager::getHighContrastTheme() {
    Theme theme = getDarkTheme();
    theme.name = "High Contrast";
    theme.isDark = true;
    
    theme.palette.background = Color(0, 0, 0);
    theme.palette.foreground = Color(255, 255, 255);
    theme.palette.selection = Color(0, 255, 255);
    
    return theme;
}

bool ThemeManager::loadTheme(const std::string& name) {
    auto it = themes_.find(name);
    if (it != themes_.end()) {
        currentTheme_ = it->second;
        notifyThemeChanged();
        return true;
    }
    return false;
}

bool ThemeManager::loadThemeFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadThemeFromJSON(buffer.str());
}

bool ThemeManager::loadThemeFromJSON(const std::string& jsonStr) {
    auto theme = Theme::fromJSON(jsonStr);
    if (!theme) return false;
    
    themes_[theme->name] = *theme;
    currentTheme_ = *theme;
    notifyThemeChanged();
    return true;
}

bool ThemeManager::saveTheme(const Theme& theme, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << theme.toJSON();
    return true;
}

const Theme& ThemeManager::getCurrentTheme() const {
    return currentTheme_;
}

bool ThemeManager::setCurrentTheme(const std::string& name) {
    return loadTheme(name);
}

void ThemeManager::registerThemeChangeCallback(ThemeChangeCallback callback) {
    callbacks_.push_back(callback);
}

void ThemeManager::notifyThemeChanged() {
    for (auto& callback : callbacks_) {
        callback(currentTheme_);
    }
}

void ThemeManager::applyTheme(HWND hwnd) {
    // Apply colors to window
    InvalidateRect(hwnd, nullptr, TRUE);
}

void ThemeManager::applyThemeToAllWindows() {
    notifyThemeChanged();
}

Color ThemeManager::interpolate(const Color& a, const Color& b, float t) {
    return Color(
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t),
        static_cast<uint8_t>(a.a + (b.a - a.a) * t)
    );
}

Color ThemeManager::lighten(const Color& color, float amount) {
    return interpolate(color, Color(255, 255, 255), amount);
}

Color ThemeManager::darken(const Color& color, float amount) {
    return interpolate(color, Color(0, 0, 0), amount);
}

Color ThemeManager::withAlpha(const Color& color, uint8_t alpha) {
    return Color(color.r, color.g, color.b, alpha);
}

} // namespace RawrXD
