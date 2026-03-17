/**
 * SettingsManager - Cross-platform preferences and configuration management
 * 
 * Handles persistent storage of user preferences, layout settings, and configuration
 * using platform-native methods:
 * - Windows: Registry or INI files in AppData
 * - Linux: ~/.config/rawrxd/settings.json
 * - macOS: ~/Library/Preferences/com.rawrxd.plist
 * 
 * Thread-safe with automatic serialization/deserialization
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <variant>
#include <functional>

class SettingsManager
{
public:
    using SettingValue = std::variant<int, double, bool, std::string>;

    /**
     * Get singleton instance
     */
    static SettingsManager& getInstance();

    /**
     * Initialize settings manager
     * Creates necessary directories and loads existing settings
     */
    void initialize();

    // ========== Basic Get/Set Operations ==========

    /**
     * Get integer setting with default value
     */
    int getInt(const std::string& key, int defaultValue = 0);

    /**
     * Get double setting with default value
     */
    double getDouble(const std::string& key, double defaultValue = 0.0);

    /**
     * Get boolean setting with default value
     */
    bool getBool(const std::string& key, bool defaultValue = false);

    /**
     * Get string setting with default value
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "");

    /**
     * Set integer setting
     */
    void setInt(const std::string& key, int value);

    /**
     * Set double setting
     */
    void setDouble(const std::string& key, double value);

    /**
     * Set boolean setting
     */
    void setBool(const std::string& key, bool value);

    /**
     * Set string setting
     */
    void setString(const std::string& key, const std::string& value);

    /**
     * Set any variant value
     */
    void setValue(const std::string& key, const SettingValue& value);

    /**
     * Get any variant value
     */
    SettingValue getValue(const std::string& key, const SettingValue& defaultValue = "");

    /**
     * Check if setting exists
     */
    bool contains(const std::string& key) const;

    /**
     * Remove setting
     */
    void remove(const std::string& key);

    /**
     * Clear all settings
     */
    void clear();

    /**
     * Get all settings as a map
     */
    const std::map<std::string, SettingValue>& getAll() const;

    // ========== Batch Operations ==========

    /**
     * Save all settings to disk
     * Called automatically on destruction, but can be called manually for checkpoint
     */
    void save();

    /**
     * Load settings from disk, overwriting current settings
     */
    void load();

    /**
     * Export settings to JSON file
     */
    void exportToJSON(const std::string& filePath);

    /**
     * Import settings from JSON file
     */
    void importFromJSON(const std::string& filePath);

    // ========== IDE-Specific Convenience Methods ==========

    /**
     * Save UI layout preferences
     */
    void saveUILayout(int outputHeight, int selectedTab, int terminalHeight, bool outputVisible);

    /**
     * Load UI layout preferences
     */
    void loadUILayout(int& outputHeight, int& selectedTab, int& terminalHeight, bool& outputVisible);

    /**
     * Save inference settings
     */
    void saveInferenceSettings(const std::string& ollamaUrl, const std::string& modelTag, 
                              bool useStreaming, const std::string& tokenLimit);

    /**
     * Load inference settings
     */
    void loadInferenceSettings(std::string& ollamaUrl, std::string& modelTag, 
                              bool& useStreaming, std::string& tokenLimit);

    /**
     * Save editor preferences
     */
    void saveEditorPreferences(const std::string& fontName, int fontSize, 
                              int tabWidth, bool autoFormat, bool wordWrap);

    /**
     * Load editor preferences
     */
    void loadEditorPreferences(std::string& fontName, int& fontSize, 
                              int& tabWidth, bool& autoFormat, bool& wordWrap);

    /**
     * Save filter preferences
     */
    void saveSeverityFilter(int filterLevel);

    /**
     * Load filter preferences
     */
    int loadSeverityFilter();

    /**
     * Save last workspace path
     */
    void saveLastWorkspace(const std::string& workspacePath);

    /**
     * Load last workspace path
     */
    std::string loadLastWorkspace();

    /**
     * Save renderer preference
     */
    void saveRendererPreference(bool useVulkan);

    /**
     * Load renderer preference
     */
    bool loadRendererPreference();

    // ========== Lifecycle ==========

    /**
     * Destructor - saves all settings
     */
    ~SettingsManager();

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

private:
    SettingsManager();

    /**
     * Get settings file path (platform-specific)
     */
    std::string getSettingsFilePath();

    /**
     * Load from INI file
     */
    void loadFromINI(const std::string& filePath);

    /**
     * Save to INI file
     */
    void saveToINI(const std::string& filePath);

    /**
     * Convert variant to string
     */
    static std::string variantToString(const SettingValue& value);

    /**
     * Parse string to variant based on type hints
     */
    static SettingValue stringToVariant(const std::string& value);

    std::map<std::string, SettingValue> m_settings;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_dirty = false;  // Mark if settings have been modified since last save
};
