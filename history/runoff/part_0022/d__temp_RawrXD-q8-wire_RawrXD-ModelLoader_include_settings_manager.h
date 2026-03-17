/**
 * @file settings_manager.h
 * @brief Production-ready settings system with JSON persistence
 * 
 * Features:
 * - Hierarchical settings with categories
 * - JSON file persistence
 * - Win32 Settings dialog
 * - Change notifications
 * - Default value management
 * - Type-safe value access
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

namespace RawrXD::Settings {

// ============================================================================
// SETTING TYPES
// ============================================================================

enum class SettingType {
    Boolean,
    Integer,
    Float,
    String,
    StringList,
    Enum,       // String from predefined list
    Color,      // COLORREF as hex string
    FilePath,
    FolderPath,
    Font        // Font name + size
};

using SettingValue = std::variant<
    bool,
    int,
    double,
    std::string,
    std::vector<std::string>,
    COLORREF
>;

// ============================================================================
// SETTING DEFINITION
// ============================================================================

struct SettingDef {
    std::string key;           // e.g., "editor.fontSize"
    std::string label;         // "Font Size"
    std::string description;   // Tooltip/help text
    std::string category;      // "Editor", "Terminal", "AI", etc.
    
    SettingType type = SettingType::String;
    SettingValue defaultValue;
    SettingValue currentValue;
    
    // For enum types
    std::vector<std::string> enumOptions;
    
    // Validation
    std::optional<int> minInt;
    std::optional<int> maxInt;
    std::optional<double> minFloat;
    std::optional<double> maxFloat;
    
    // Change callback
    std::function<void(const SettingValue&)> onChange;
    
    // UI hints
    bool requiresRestart = false;
    bool hidden = false;
};

// ============================================================================
// SETTINGS CATEGORIES
// ============================================================================

namespace Categories {
    constexpr const char* Editor = "Editor";
    constexpr const char* Workbench = "Workbench";
    constexpr const char* Terminal = "Terminal";
    constexpr const char* AI = "AI & Copilot";
    constexpr const char* Git = "Git";
    constexpr const char* Keyboard = "Keyboard Shortcuts";
    constexpr const char* Theme = "Appearance";
    constexpr const char* Files = "Files";
    constexpr const char* Debug = "Debug";
    constexpr const char* Extensions = "Extensions";
}

// ============================================================================
// COMMON SETTING KEYS
// ============================================================================

namespace Keys {
    // Editor settings
    constexpr const char* EditorFontSize = "editor.fontSize";
    constexpr const char* EditorFontFamily = "editor.fontFamily";
    constexpr const char* EditorTabSize = "editor.tabSize";
    constexpr const char* EditorInsertSpaces = "editor.insertSpaces";
    constexpr const char* EditorWordWrap = "editor.wordWrap";
    constexpr const char* EditorLineNumbers = "editor.lineNumbers";
    constexpr const char* EditorMinimap = "editor.minimap.enabled";
    constexpr const char* EditorBreadcrumbs = "editor.breadcrumbs.enabled";
    constexpr const char* EditorAutoSave = "editor.autoSave";
    constexpr const char* EditorAutoSaveDelay = "editor.autoSaveDelay";
    constexpr const char* EditorFormatOnSave = "editor.formatOnSave";
    constexpr const char* EditorFormatOnPaste = "editor.formatOnPaste";
    constexpr const char* EditorStickyScroll = "editor.stickyScroll.enabled";
    constexpr const char* EditorCursorStyle = "editor.cursorStyle";
    constexpr const char* EditorCursorBlinking = "editor.cursorBlinking";
    constexpr const char* EditorSmoothScrolling = "editor.smoothScrolling";
    constexpr const char* EditorMouseWheelZoom = "editor.mouseWheelZoom";
    
    // Terminal settings
    constexpr const char* TerminalFontSize = "terminal.fontSize";
    constexpr const char* TerminalFontFamily = "terminal.fontFamily";
    constexpr const char* TerminalShell = "terminal.shell";
    constexpr const char* TerminalCursorStyle = "terminal.cursorStyle";
    constexpr const char* TerminalScrollback = "terminal.scrollback";
    
    // AI/Copilot settings
    constexpr const char* AiEnabled = "ai.enabled";
    constexpr const char* AiModel = "ai.model";
    constexpr const char* AiModelPath = "ai.modelPath";
    constexpr const char* AiMaxTokens = "ai.maxTokens";
    constexpr const char* AiTemperature = "ai.temperature";
    constexpr const char* AiTopP = "ai.topP";
    constexpr const char* AiInlineEnabled = "ai.inline.enabled";
    constexpr const char* AiInlineDelay = "ai.inline.delay";
    constexpr const char* AiAgentAutoApprove = "ai.agent.autoApprove";
    
    // Workbench settings
    constexpr const char* ThemeColorTheme = "workbench.colorTheme";
    constexpr const char* ThemeIconTheme = "workbench.iconTheme";
    constexpr const char* WorkbenchStartup = "workbench.startupEditor";
    constexpr const char* WorkbenchSidebarPosition = "workbench.sideBar.position";
    constexpr const char* WorkbenchActivityBarVisible = "workbench.activityBar.visible";
    constexpr const char* WorkbenchStatusBarVisible = "workbench.statusBar.visible";
    
    // Files settings
    constexpr const char* FilesAutoGuessEncoding = "files.autoGuessEncoding";
    constexpr const char* FilesEncoding = "files.encoding";
    constexpr const char* FilesEol = "files.eol";
    constexpr const char* FilesTrimTrailingWhitespace = "files.trimTrailingWhitespace";
    constexpr const char* FilesInsertFinalNewline = "files.insertFinalNewline";
    constexpr const char* FilesExclude = "files.exclude";
    
    // Git settings
    constexpr const char* GitEnabled = "git.enabled";
    constexpr const char* GitPath = "git.path";
    constexpr const char* GitAutoFetch = "git.autoFetch";
    constexpr const char* GitConfirmSync = "git.confirmSync";
}

// ============================================================================
// SETTINGS MANAGER - Singleton
// ============================================================================

class SettingsManager {
public:
    static SettingsManager& instance();
    
    // Initialization
    void initialize();
    void shutdown();
    
    // Setting definition
    void defineSetting(const SettingDef& def);
    SettingDef* getSettingDef(const std::string& key);
    std::vector<SettingDef*> getSettingsInCategory(const std::string& category);
    std::vector<std::string> getCategories() const;
    
    // Value access - type-safe
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const;
    
    bool getBool(const std::string& key, bool defaultValue = false) const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    double getFloat(const std::string& key, double defaultValue = 0.0) const;
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    std::vector<std::string> getStringList(const std::string& key) const;
    COLORREF getColor(const std::string& key, COLORREF defaultValue = RGB(0, 0, 0)) const;
    
    // Value modification
    void setValue(const std::string& key, const SettingValue& value);
    void setBool(const std::string& key, bool value);
    void setInt(const std::string& key, int value);
    void setFloat(const std::string& key, double value);
    void setString(const std::string& key, const std::string& value);
    void setStringList(const std::string& key, const std::vector<std::string>& value);
    void setColor(const std::string& key, COLORREF value);
    
    // Reset
    void resetToDefault(const std::string& key);
    void resetAllToDefaults();
    
    // Persistence
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path);
    std::string getDefaultSettingsPath() const;
    void autoSave();
    
    // JSON access
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);
    
    // Change notifications
    using ChangeCallback = std::function<void(const std::string& key, const SettingValue& newValue)>;
    void addChangeListener(const std::string& key, ChangeCallback callback);
    void addGlobalChangeListener(ChangeCallback callback);
    
    // UI
    void showSettingsDialog(HWND parent);
    void showSettingsSearch(HWND parent, const std::string& query = "");
    
private:
    SettingsManager() = default;
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    void registerDefaultSettings();
    void notifyChange(const std::string& key, const SettingValue& value);
    
    std::unordered_map<std::string, SettingDef> m_settings;
    std::unordered_map<std::string, std::vector<ChangeCallback>> m_listeners;
    std::vector<ChangeCallback> m_globalListeners;
    std::string m_settingsPath;
    mutable CRITICAL_SECTION m_cs;
    bool m_initialized = false;
    bool m_dirty = false;
};

// ============================================================================
// SETTINGS DIALOG
// ============================================================================

class SettingsDialog {
public:
    static INT_PTR show(HWND parent);
    
private:
    static INT_PTR CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    static void onCreate(HWND hwnd);
    static void onDestroy(HWND hwnd);
    static void onCategorySelect(HWND hwnd, int index);
    static void onSettingChange(HWND hwnd, const std::string& key);
    static void onSearch(HWND hwnd, const std::string& query);
    static void onApply(HWND hwnd);
    static void onReset(HWND hwnd);
    
    static void populateCategories(HWND listBox);
    static void populateSettings(HWND parent, const std::string& category);
    static HWND createSettingControl(HWND parent, const SettingDef& def, int x, int y, int width);
    static void updateControlValue(HWND control, const SettingDef& def);
    
    static std::string s_currentCategory;
    static std::unordered_map<HWND, std::string> s_controlToKey;
};

// ============================================================================
// TEMPLATE IMPLEMENTATIONS
// ============================================================================

template<typename T>
T SettingsManager::getValue(const std::string& key, const T& defaultValue) const {
    auto it = m_settings.find(key);
    if (it == m_settings.end()) {
        return defaultValue;
    }
    
    try {
        return std::get<T>(it->second.currentValue);
    } catch (const std::bad_variant_access&) {
        return defaultValue;
    }
}

} // namespace RawrXD::Settings
