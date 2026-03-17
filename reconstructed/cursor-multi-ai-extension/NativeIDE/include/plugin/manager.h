#pragma once

#include "native_ide.h"

class IDEApplication;

struct PluginInfo {
    std::wstring name;
    std::wstring version;
    std::wstring fileName;
    std::wstring description;
    HMODULE moduleHandle = nullptr;
    std::unique_ptr<IPlugin> instance;
    bool isLoaded = false;
    bool isEnabled = true;
};

class PluginManager {
private:
    IDEApplication* m_app;
    std::vector<std::unique_ptr<PluginInfo>> m_plugins;
    std::wstring m_pluginDirectory;
    
public:
    explicit PluginManager(IDEApplication* app);
    ~PluginManager();
    
    // Plugin discovery and loading
    bool LoadAllPlugins(const std::wstring& pluginDirectory);
    bool LoadPlugin(const std::wstring& pluginPath);
    bool UnloadPlugin(const std::wstring& pluginName);
    void UnloadAllPlugins();
    
    // Plugin management
    std::vector<PluginInfo*> GetLoadedPlugins() const;
    PluginInfo* GetPlugin(const std::wstring& name) const;
    bool IsPluginLoaded(const std::wstring& name) const;
    bool EnablePlugin(const std::wstring& name, bool enabled);
    
    // Event broadcasting to plugins
    void NotifyFileOpened(const std::wstring& filename);
    void NotifyFileSaved(const std::wstring& filename);
    void NotifyMenuCommand(int command_id);
    void NotifyKeyPressed(int key_code, int modifiers);
    void NotifyRender(ID2D1RenderTarget* target, const RECT& rect);
    void NotifyProjectOpened(const std::wstring& project_path);
    
    // Plugin discovery
    std::vector<std::wstring> DiscoverPlugins(const std::wstring& directory) const;
    
private:
    bool LoadPluginFromFile(const std::wstring& pluginPath, std::unique_ptr<PluginInfo>& pluginInfo);
    void ScanForPlugins(const std::wstring& directory);
    
    // Plugin DLL management
    typedef IPlugin* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(IPlugin*);
};