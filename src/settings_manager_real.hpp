#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @class SettingsManager
 * @brief Persistent configuration system with JSON backend
 * 
 * Features:
 * - JSON-based settings storage
 * - Theme management
 * - Keybinding registry
 * - Model configuration
 * - Auto-save on changes
 * - Settings validation
 */
class SettingsManager {
public:
    // Setting value variant
    using SettingValue = json;

    struct Theme {
        std::string name;
        std::map<std::string, std::string> colors;
        std::string fontName;
        int fontSize;
        bool darkMode;
    };

    struct Keybinding {
        std::string command;
        std::string keys;  // e.g., "Ctrl+S"
        std::string description;
        bool enabled;
    };

    struct ModelConfig {
        std::string name;
        std::string path;
        std::string type;  // "GGUF", "ONNX", etc.
        int contextWindow;
        int gpuLayers;
        bool loaded;
    };

    explicit SettingsManager(const std::string& configPath = "settings.json");
    ~SettingsManager();

    // Initialization
    bool load();
    bool save();
    bool reset();

    // Generic settings
    bool set(const std::string& key, const SettingValue& value);
    SettingValue get(const std::string& key) const;
    SettingValue get(const std::string& key, const SettingValue& defaultValue) const;
    bool has(const std::string& key) const;
    bool remove(const std::string& key);
    void clear();

    // Theme management
    bool setTheme(const Theme& theme);
    Theme getTheme(const std::string& themeName) const;
    std::vector<std::string> listThemes() const;
    std::string getCurrentTheme() const;
    bool switchTheme(const std::string& themeName);
    void createTheme(const std::string& baseName, const Theme& theme);

    // Keybinding management
    bool setKeybinding(const Keybinding& binding);
    Keybinding getKeybinding(const std::string& command) const;
    std::vector<Keybinding> listKeybindings() const;
    std::vector<Keybinding> getKeybindingsByKeys(const std::string& keys) const;
    bool removeKeybinding(const std::string& command);
    void resetKeybindingsToDefaults();

    // Model configuration
    bool addModel(const ModelConfig& config);
    ModelConfig getModel(const std::string& modelName) const;
    std::vector<ModelConfig> listModels() const;
    bool removeModel(const std::string& modelName);
    std::string getDefaultModel() const;
    bool setDefaultModel(const std::string& modelName);

    // Editor settings
    struct EditorSettings {
        int tabSize;
        bool useSpaces;
        bool autoFormat;
        bool wordWrap;
        bool minimap;
        bool lineNumbers;
        std::string fontFamily;
        int fontSize;
        bool autoSave;
        int autoSaveDelay;
    };
    EditorSettings getEditorSettings() const;
    void setEditorSettings(const EditorSettings& settings);

    // Terminal settings
    struct TerminalSettings {
        std::string defaultShell;
        bool inheritEnv;
        int bufferSize;
        bool enableLogging;
        std::string logPath;
    };
    TerminalSettings getTerminalSettings() const;
    void setTerminalSettings(const TerminalSettings& settings);

    // AI settings
    struct AISettings {
        std::string defaultModel;
        float temperature;
        float topP;
        int maxTokens;
        int contextWindow;
        bool enableCompletion;
        int completionDelay;
        float confidenceThreshold;
        bool enableDeepThinking;
        bool enableAutoCorrection;
    };
    AISettings getAISettings() const;
    void setAISettings(const AISettings& settings);

    // Cloud settings
    struct CloudSettings {
        bool enableCloudModels;
        std::string openaiApiKey;
        std::string azureApiKey;
        std::string azureEndpoint;
        std::string anthropicApiKey;
        bool preferLocal;  // Use local models by default
    };
    CloudSettings getCloudSettings() const;
    void setCloudSettings(const CloudSettings& settings);

    // File operations
    std::string getWorkspaceDirectory() const;
    void setWorkspaceDirectory(const std::string& path);
    std::vector<std::string> getRecentProjects(int maxCount = 10) const;
    void addRecentProject(const std::string& path);

    // Callbacks
    using ChangeCallback = std::function<void(const std::string&, const SettingValue&)>;
    void onSettingChanged(ChangeCallback callback) { m_changeCallback = callback; }

    // Validation
    bool validateSetting(const std::string& key, const SettingValue& value);
    std::vector<std::string> getValidationErrors() const;

    // Export/Import
    bool exportToFile(const std::string& filePath) const;
    bool importFromFile(const std::string& filePath);

    // Debugging
    void printAllSettings() const;

private:
    // Internal helpers
    void loadDefaults();
    void mergeSettings(const json& defaults, json& current);
    json serializeTheme(const Theme& theme) const;
    Theme deserializeTheme(const json& data) const;
    json serializeKeybinding(const Keybinding& kb) const;
    Keybinding deserializeKeybinding(const json& data) const;
    json serializeModel(const ModelConfig& model) const;
    ModelConfig deserializeModel(const json& data) const;

    // File I/O
    bool readFromDisk();
    bool writeToDisk();

    // State
    std::string m_configPath;
    json m_settings;
    json m_defaults;
    bool m_dirty = false;
    bool m_autoSave = true;

    // Callbacks
    ChangeCallback m_changeCallback;

    // Validation errors
    mutable std::vector<std::string> m_validationErrors;

    // Mutex for thread-safety
    mutable std::mutex m_mutex;
};
