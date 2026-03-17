/**
 * @file Win32SettingsRegistry.hpp
 * @brief Production-Ready Settings with Windows Registry Persistence
 * 
 * REAL implementations for:
 * - All IDE settings (editor, theme, terminal, etc.)
 * - Windows Registry persistence
 * - Real-time setting changes
 * - Settings UI with toggles/dropdowns
 */

#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <optional>
#include <nlohmann/json.hpp>

namespace RawrXD::Win32 {

// ============================================================================
// SETTING VALUE TYPES
// ============================================================================
using SettingVariant = std::variant<bool, int, double, std::string>;

enum class SettingType {
    Boolean,
    Integer,
    Double,
    String,
    Choice,
    Color,
    Path,
    KeyBinding
};

// ============================================================================
// SETTING CATEGORIES
// ============================================================================
namespace SettingCategory {
    constexpr const char* Editor = "Editor";
    constexpr const char* Terminal = "Terminal";
    constexpr const char* Appearance = "Appearance";
    constexpr const char* Workbench = "Workbench";
    constexpr const char* Files = "Files";
    constexpr const char* Extensions = "Extensions";
    constexpr const char* Agent = "Agent";
}

// ============================================================================
// SETTING DEFINITION
// ============================================================================
struct SettingDef {
    std::string key;                    // Unique key (e.g., "editor.fontSize")
    std::string displayName;            // Display name in UI
    std::string description;            // Description/help text
    std::string category;               // Category for grouping
    SettingType type;                   // Type of setting
    SettingVariant defaultValue;        // Default value
    SettingVariant currentValue;        // Current value
    std::vector<std::string> choices;   // For Choice type
    std::optional<int> minValue;        // For Integer/Double
    std::optional<int> maxValue;        // For Integer/Double
    std::function<void(const SettingVariant&)> onChange;
};

// ============================================================================
// SETTINGS REGISTRY - Windows Registry Persistence
// ============================================================================
class SettingsRegistry {
public:
    static SettingsRegistry& instance() {
        static SettingsRegistry registry;
        return registry;
    }

    // Initialize with default settings
    void initialize();

    // Setting operations
    void registerSetting(const SettingDef& setting);
    void setValue(const std::string& key, const SettingVariant& value);
    SettingVariant getValue(const std::string& key) const;
    SettingDef* getSetting(const std::string& key);
    const SettingDef* getSetting(const std::string& key) const;
    
    // Type-safe getters
    bool getBool(const std::string& key) const;
    int getInt(const std::string& key) const;
    double getDouble(const std::string& key) const;
    std::string getString(const std::string& key) const;

    // Registry persistence - REAL Windows Registry implementation
    bool loadFromRegistry();
    bool saveToRegistry();
    
    // JSON import/export (for settings.json compatibility)
    bool loadFromJson(const std::string& filepath);
    bool saveToJson(const std::string& filepath);
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);

    // Categories
    std::vector<std::string> getCategories() const;
    std::vector<SettingDef> getSettingsInCategory(const std::string& category) const;
    std::vector<SettingDef> getAllSettings() const;

    // Reset
    void resetToDefault(const std::string& key);
    void resetAllToDefaults();

private:
    SettingsRegistry() = default;
    
    std::unordered_map<std::string, SettingDef> m_settings;
    
    // Windows Registry helpers
    static constexpr const char* REGISTRY_ROOT = "SOFTWARE\\RawrXD\\IDE\\Settings";
    
    bool writeRegistryValue(const std::string& key, const SettingVariant& value);
    std::optional<SettingVariant> readRegistryValue(const std::string& key, SettingType type);
};

// ============================================================================
// SETTINGS PANEL UI
// ============================================================================
class Win32SettingsPanel {
public:
    Win32SettingsPanel();
    ~Win32SettingsPanel();

    // Create settings window
    HWND create(HWND parent, int x, int y, int width, int height);
    
    // Refresh UI after setting changes
    void refresh();
    
    // Category selection
    void selectCategory(const std::string& category);
    
    // Search/filter
    void setSearchFilter(const std::string& filter);

    HWND getHwnd() const { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
    HWND m_categoryList = nullptr;
    HWND m_settingsList = nullptr;
    HWND m_searchBox = nullptr;
    std::string m_currentCategory;
    std::string m_searchFilter;
    
    void createControls();
    void populateCategories();
    void populateSettings();
    void createSettingControl(const SettingDef& setting, int y);
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace RawrXD::Win32
