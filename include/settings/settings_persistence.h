#pragma once
/**
 * @file settings_persistence.h
 * @brief JSON-based settings persistence with schema validation
 * Batch 3 - Item 32: Settings persistence
 */

#include <string>
#include <map>
#include <vector>
#include <any>
#include <optional>
#include <functional>

namespace RawrXD::Settings {

enum class SettingType {
    Boolean,
    Integer,
    Float,
    String,
    Array,
    Object,
    Color,
    Font,
    Keybinding
};

struct SettingSchema {
    std::string key;
    SettingType type;
    std::any defaultValue;
    std::optional<std::any> minValue;
    std::optional<std::any> maxValue;
    std::vector<std::string> enumValues;
    std::string description;
    std::string category;
    bool requiresRestart;
};

struct SettingChange {
    std::string key;
    std::any oldValue;
    std::any newValue;
    std::string source;
    uint64_t timestamp;
};

class SettingsPersistence {
public:
    SettingsPersistence();
    ~SettingsPersistence();

    // Initialization
    bool initialize(const std::string& configPath);
    void registerSchema(const SettingSchema& schema);
    void registerSchemas(const std::vector<SettingSchema>& schemas);

    // Value access
    template<typename T>
    T get(const std::string& key) const;
    template<typename T>
    void set(const std::string& key, const T& value);
    bool has(const std::string& key) const;
    void reset(const std::string& key);
    void resetAll();

    // Type-safe getters
    bool getBool(const std::string& key) const;
    int64_t getInt(const std::string& key) const;
    double getFloat(const std::string& key) const;
    std::string getString(const std::string& key) const;
    std::vector<std::any> getArray(const std::string& key) const;
    std::map<std::string, std::any> getObject(const std::string& key) const;

    // Type-safe setters
    void setBool(const std::string& key, bool value);
    void setInt(const std::string& key, int64_t value);
    void setFloat(const std::string& key, double value);
    void setString(const std::string& key, const std::string& value);
    void setArray(const std::string& key, const std::vector<std::any>& value);
    void setObject(const std::string& key, const std::map<std::string, std::any>& value);

    // Persistence
    bool load();
    bool save();
    bool loadFromString(const std::string& json);
    std::string saveToString() const;
    bool importFromFile(const std::string& path);
    bool exportToFile(const std::string& path) const;

    // Validation
    bool validate(const std::string& key, const std::any& value) const;
    std::vector<std::string> validateAll() const;
    std::optional<std::string> getValidationError(const std::string& key, const std::any& value) const;

    // Change tracking
    using ChangeCallback = std::function<void(const SettingChange&)>;
    void onChanged(const std::string& key, ChangeCallback callback);
    void onAnyChanged(ChangeCallback callback);
    std::vector<SettingChange> getChangeHistory(size_t limit = 100) const;
    void clearHistory();

    // Categories
    std::vector<std::string> getCategories() const;
    std::vector<SettingSchema> getSettingsInCategory(const std::string& category) const;

    // Search
    std::vector<SettingSchema> searchSettings(const std::string& query) const;

    // Sync
    void setSyncEnabled(bool enabled);
    bool isSyncEnabled() const;
    void syncNow();

    // Migration
    void migrateFromVersion(int oldVersion, int newVersion);
    int getCurrentVersion() const;

private:
    std::string m_configPath;
    std::map<std::string, SettingSchema> m_schemas;
    std::map<std::string, std::any> m_values;
    std::map<std::string, std::vector<ChangeCallback>> m_callbacks;
    std::vector<ChangeCallback> m_globalCallbacks;
    std::vector<SettingChange> m_history;
    mutable std::mutex m_mutex;
    bool m_syncEnabled{true};
    int m_version{1};

    void notifyChange(const SettingChange& change);
    std::any convertToType(const std::any& value, SettingType targetType) const;
};

// Global instance
SettingsPersistence& getSettingsPersistence();

// Utility functions
std::string settingTypeToString(SettingType type);
SettingType stringToSettingType(const std::string& str);

} // namespace RawrXD::Settings
