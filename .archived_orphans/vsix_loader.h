#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unordered_map>
#include <functional>

struct VSIXPlugin {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::filesystem::path install_path;
    bool enabled;
    std::vector<std::string> commands;
    std::vector<std::string> dependencies;
    nlohmann::json manifest;
    std::function<void()> onLoad;
    std::function<void()> onUnload;
    std::function<void(const std::string&)> onCommand;
    std::function<void(const nlohmann::json&)> onConfigure;
};

class VSIXLoader {
private:
    std::filesystem::path plugins_dir_;
    std::unordered_map<std::string, std::unique_ptr<VSIXPlugin>> plugins_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::function<void()>> command_handlers_;
    
public:
    VSIXLoader();
    ~VSIXLoader();
    
    // Core operations
    bool Initialize(const std::string& plugins_dir);
    bool LoadPlugin(const std::string& vsix_path);
    bool UnloadPlugin(const std::string& plugin_id);
    bool EnablePlugin(const std::string& plugin_id);
    bool DisablePlugin(const std::string& plugin_id);
    bool ReloadPlugin(const std::string& plugin_id);
    bool ConfigurePlugin(const std::string& plugin_id, const nlohmann::json& config);
    
    // Query operations
    std::vector<VSIXPlugin*> GetLoadedPlugins();
    VSIXPlugin* GetPlugin(const std::string& plugin_id);
    bool IsPluginLoaded(const std::string& plugin_id);
    bool IsPluginEnabled(const std::string& plugin_id);
    nlohmann::json GetPluginConfig(const std::string& plugin_id);
    std::string GetPluginHelp(const std::string& plugin_id);
    std::string GetAllPluginsHelp();
    std::string GetCLIHelp();
    std::string GetGUIHelp();
    
    // Execution
    bool ExecutePluginCommand(const std::string& plugin_id, const std::string& command, const std::vector<std::string>& args);
    std::string GetPluginUsage(const std::string& plugin_id);
    
    // Memory modules (special plugins)
    bool LoadMemoryModule(const std::string& module_path, size_t context_size);
    bool UnloadMemoryModule(size_t context_size);
    std::vector<size_t> GetAvailableMemoryModules();
    
    // Command registration
    void RegisterCommand(const std::string& command, std::function<void()> handler);
    bool ExecuteCommand(const std::string& command);
    
    // Help system
    std::string GetCommandHelp(const std::string& command);
    
    // Singleton access
    static VSIXLoader& GetInstance();
    
    // Engine management
    bool LoadEngine(const std::string& engine_path, const std::string& engine_id);
    bool UnloadEngine(const std::string& engine_id);
    bool SwitchEngine(const std::string& engine_id);
    std::vector<std::string> GetAvailableEngines();
    std::string GetCurrentEngine();
    
private:
    bool ExtractVSIX(const std::string& vsix_path, const std::filesystem::path& extract_dir);
    bool ValidateManifest(const nlohmann::json& manifest);
    bool LoadPluginFromDirectory(const std::filesystem::path& plugin_dir);
    void SavePluginState();
    void LoadPluginState();
    std::string GetSizeName(size_t size);
    
    // Engine management
    std::unordered_map<std::string, std::string> engines_; // id -> path
    std::string current_engine_id_;
};
