#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

// C++20, no Qt. Hot-load .dll/.so via platform API (no QLibrary).

#include <string>
#include <map>
#include "plugin_api.h"

struct PluginInstance {
    void* library = nullptr;  // HMODULE or dlopen handle
    PluginInfo* (*plugin_init)(PluginContext*) = nullptr;
    void (*plugin_onFileSave)(const char*) = nullptr;
    void (*plugin_onChatMessage)(const char*) = nullptr;
    void (*plugin_onCommand)(const char*) = nullptr;
    void (*plugin_onModelLoad)(const char*) = nullptr;
    void (*plugin_cleanup)() = nullptr;
    PluginInfo* info = nullptr;
    int refCount = 0;
};

class PluginLoader
{
public:
    PluginLoader() = default;
    ~PluginLoader();

    bool loadPlugin(const std::string& pluginPath);
    void unloadPlugin(const std::string& pluginName);
    void unloadAllPlugins();

    void onFileSave(const std::string& filePath);
    void onChatMessage(const std::string& message);
    void onCommand(const std::string& command);
    void onModelLoad(const std::string& modelPath);

private:
    std::map<std::string, PluginInstance> m_plugins;
    PluginContext m_context;
};

#endif // PLUGIN_LOADER_H
