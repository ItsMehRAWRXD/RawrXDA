// ============================================================================
// extension_configuration_ui.h — Extension Configuration UI Settings Surface
// ============================================================================
// PURPOSE:
//   Provides UI surface for extension settings and configuration. Extensions
//   declare configuration schema in package.json contributions.configuration,
//   and this system renders a settings UI panel for users to customize.
//   Features:
//   - Auto-generate UI from JSON schema
//   - Multi-type support (string, number, boolean, enum, array)
//   - Real-time configuration changes
//   - Settings persistence
//   - Per-scope settings (user/workspace/folder)\
//   - Change notifications to extensions
//
// Architecture: C++20 | Win32 | No exceptions | Qt-free | Minimal UI stub
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED\n// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Configuration Schema Types
// ============================================================================

enum class ConfigValueType {
    String,
    Number,
    Integer,
    Boolean,
    Array,
    Object,
};

enum class ConfigScope {
    User,                   // Global user settings
    Workspace,              // Workspace-level settings
    WorkspaceFolder,        // Specific workspace folder
};

// ============================================================================
// Configuration Schema Entry
// ============================================================================

struct ConfigurationSchemaEntry {
    std::string key;                    // e.g., "myExtension.setting1"
    ConfigValueType type;
    json defaultValue;
    std::string title;
    std::string description;
    std::string category;
    
    // Type-specific constraints
    json minimum;                       // For numbers
    json maximum;                       // For numbers
    std::vector<std::string> enum_values; // For enums
    std::string pattern;                // Regex pattern for strings
    bool required = false;
    bool readOnly = false;
};

// ============================================================================
// Configuration Value
// ============================================================================

struct ConfigurationValue {
    std::string key;
    json value;
    ConfigScope scope = ConfigScope::User;
    std::string resourceUri;            // For workspace folder scope
};

// ============================================================================
// Configuration Provider
// ============================================================================

class ExtensionConfigurationUI {
public:
    explicit ExtensionConfigurationUI();
    ~ExtensionConfigurationUI();

    // ── Schema Registration ────────────────────────────────────────

    // Register configuration schema for extension
    bool RegisterConfigurationSchema(
        const std::string& extensionId,
        const std::vector<ConfigurationSchemaEntry>& schema
    );

    // ── Configuration CRUD ─────────────────────────────────────────

    // Get configuration value
    bool GetConfiguration(const std::string& key,
                          ConfigScope scope,
                          json& outValue);

    // Set configuration value
    bool SetConfiguration(const std::string& key,
                          ConfigScope scope,
                          const json& value);

    // Get all configuration for extension
    std::vector<ConfigurationValue> GetExtensionConfiguration(
        const std::string& extensionId,
        ConfigScope scope = ConfigScope::User
    );

    // Reset configuration to defaults
    bool ResetConfiguration(const std::string& key);

    // ── UI Queries ─────────────────────────────────────────────────

    // Get schema for UI rendering
    std::vector<ConfigurationSchemaEntry> GetConfigurationSchema(
        const std::string& extensionId
    ) const;

    // Get grouped schema (by category)
    std::unordered_map<std::string, std::vector<ConfigurationSchemaEntry>>
        GetGroupedConfigurationSchema(const std::string& extensionId) const;

    // ── Validation ─────────────────────────────────────────────────

    // Validate value against schema
    bool ValidateConfigurationValue(const std::string& key,
                                     const json& value,
                                     std::string& outErrorMessage);

    // ── Callbacks ──────────────────────────────────────────────────

    // Register callback for configuration changes
    void OnConfigurationChanged(
        const std::string& extensionId,
        std::function<void(const std::string& key, const json& newValue)> callback
    );

    // ── Persistence ────────────────────────────────────────────────

    // Load configuration from storage
    bool LoadConfiguration(const std::string& storagePath);

    // Save configuration to storage
    bool SaveConfiguration(const std::string& storagePath);

private:
    mutable std::mutex m_lock;

    // Schema storage
    std::unordered_map<std::string, std::vector<ConfigurationSchemaEntry>> m_schemas;

    // Configuration values
    std::unordered_map<std::string, ConfigurationValue> m_values;  // key -> value

    // Change callbacks
    std::unordered_map<std::string, 
        std::function<void(const std::string&, const json&)>> m_changeCallbacks;

    // Internal helpers
    ConfigurationSchemaEntry* FindSchemaEntry(const std::string& key);
    bool ValidateValue(const ConfigurationSchemaEntry& schema, const json& value);
    json ApplyDefaults(const std::string& extensionId);
    void NotifyConfigurationChanged(const std::string& key, const json& newValue);
};

// ============================================================================
// Configuration Settings Panel (Minimal UI Stub)
// ============================================================================

class ExtensionSettingsPanel {
public:
    explicit ExtensionSettingsPanel(ExtensionConfigurationUI* configUI);
    ~ExtensionSettingsPanel();

    // Render settings UI for extension
    bool RenderSettingsPanel(const std::string& extensionId);

    // Get panel title
    std::string GetPanelTitle(const std::string& extensionId) const;

    // Apply changes from UI
    bool ApplyChanges(const std::string& extensionId,
                      const std::vector<ConfigurationValue>& changes);

private:
    ExtensionConfigurationUI* m_configUI;

    // UI rendering helpers (implemented in extension_configuration_ui.cpp)
    std::string RenderStringControl(const ConfigurationSchemaEntry& schema);
    std::string RenderNumberControl(const ConfigurationSchemaEntry& schema);
    std::string RenderBooleanControl(const ConfigurationSchemaEntry& schema);
    std::string RenderEnumControl(const ConfigurationSchemaEntry& schema);
    std::string RenderArrayControl(const ConfigurationSchemaEntry& schema);
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton configuration UI instance
ExtensionConfigurationUI& GetConfigurationUI();

// Convenience functions
bool GetExtensionSetting(const std::string& key, json& outValue);
bool SetExtensionSetting(const std::string& key, const json& value);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_CONFIGURATION_UI_H
