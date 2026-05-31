#include "plugin_loader.h"
PluginLoader::PluginLoader()
    
{
    // Initialize plugin context
    m_context.requestFileOperation = [](const char* operation, const char* filePath, const char* content) {
        // This would call into your AgenticFileOperations system
        // For now, we'll just print a message
    };
}

PluginLoader::~PluginLoader()
{
    unloadAllPlugins();
}

bool PluginLoader::loadPlugin(const std::string &pluginPath)
{
    QLibrary *library = nullptr;
    if (!library->load()) {
        delete library;
        return false;
    }

    // Resolve symbols
    PluginInfo* (*plugin_init)(PluginContext*) = (PluginInfo* (*)(PluginContext*))library->resolve("plugin_init");
    if (!plugin_init) {
        library->unload();
        delete library;
        return false;
    }

    PluginInstance instance;
    instance.library = library;
    instance.plugin_init = plugin_init;
    instance.plugin_onFileSave = (void (*)(const char*))library->resolve("plugin_onFileSave");
    instance.plugin_onChatMessage = (void (*)(const char*))library->resolve("plugin_onChatMessage");
    instance.plugin_onCommand = (void (*)(const char*))library->resolve("plugin_onCommand");
    instance.plugin_onModelLoad = (void (*)(const char*))library->resolve("plugin_onModelLoad");
    instance.plugin_cleanup = (void (*)())library->resolve("plugin_cleanup");
    instance.info = plugin_init(&m_context);
    instance.refCount = 1;

    std::string pluginName = // FileInfo: pluginPath).baseName();
    m_plugins[pluginName] = instance;

    return true;
}

void PluginLoader::unloadPlugin(const std::string &pluginName)
{
    if (m_plugins.contains(pluginName)) {
        PluginInstance &instance = m_plugins[pluginName];
        if (instance.plugin_cleanup) {
            instance.plugin_cleanup();
        }
        instance.library->unload();
        delete instance.library;
        m_plugins.remove(pluginName);
    }
}

void PluginLoader::unloadAllPlugins()
{
    std::vector<std::string> pluginNames = m_plugins.keys().toVector();
    for (const std::string &pluginName : pluginNames) {
        unloadPlugin(pluginName);
    }
}

void PluginLoader::onFileSave(const std::string &filePath)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onFileSave) {
            it.value().plugin_onFileSave(filePath.toUtf8().constData());
        }
    }
}

void PluginLoader::onChatMessage(const std::string &message)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onChatMessage) {
            it.value().plugin_onChatMessage(message.toUtf8().constData());
        }
    }
}

void PluginLoader::onCommand(const std::string &command)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onCommand) {
            it.value().plugin_onCommand(command.toUtf8().constData());
        }
    }
}

void PluginLoader::onModelLoad(const std::string &modelPath)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onModelLoad) {
            it.value().plugin_onModelLoad(modelPath.toUtf8().constData());
        }
    }
}

