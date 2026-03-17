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
    std::string current_engine_;
    std::unordered_map<std::string, std::string> engines_;

public:
    VSIXLoader();
    ~VSIXLoader();

    bool Initialize(const std::string& plugins_dir);
    bool LoadPlugin(const std::string& vsix_path);
    bool UnloadPlugin(const std::string& plugin_id);
    bool EnablePlugin(const std::string& plugin_id);
    bool DisablePlugin(const std::string& plugin_id);
    bool ReloadPlugin(const std::string& plugin_id);
    bool ConfigurePlugin(const std::string& plugin_id, const nlohmann::json& config);

    std::vector<VSIXPlugin*> GetLoadedPlugins();
    VSIXPlugin* GetPlugin(const std::string& plugin_id);
    bool IsPluginLoaded(const std::string& plugin_id);
    bool IsPluginEnabled(const std::string& plugin_id);
    nlohmann::json GetPluginConfig(const std::string& plugin_id);
    std::string GetPluginHelp(const std::string& plugin_id);
    std::string GetAllPluginsHelp();
    std::string GetCLIHelp();
    std::string GetGUIHelp();

    bool ExecutePluginCommand(const std::string& plugin_id, const std::string& command, const std::vector<std::string>& args);
    std::string GetPluginUsage(const std::string& plugin_id);

    bool LoadMemoryModule(const std::string& module_path, size_t context_size);
    bool UnloadMemoryModule(size_t context_size);
    std::vector<size_t> GetAvailableMemoryModules();

    void RegisterCommand(const std::string& command, std::function<void()> handler);
    bool ExecuteCommand(const std::string& command);

    std::string GetCommandHelp(const std::string& command);

    static VSIXLoader& GetInstance();
    
    // Engine management methods
    std::string GetCurrentEngine();
    void SwitchEngine(const std::string& engine_name);
    bool LoadEngine(const std::string& name, const std::string& path);
    bool UnloadEngine(const std::string& name);
    std::vector<std::string> GetAvailableEngines();

private:
    bool ExtractVSIX(const std::string& vsix_path, const std::filesystem::path& extract_dir);
    bool ValidateManifest(const nlohmann::json& manifest);
    bool LoadPluginFromDirectory(const std::filesystem::path& plugin_dir);
    void SavePluginState();
    void LoadPluginState();
    std::string GetSizeName(size_t size);
};