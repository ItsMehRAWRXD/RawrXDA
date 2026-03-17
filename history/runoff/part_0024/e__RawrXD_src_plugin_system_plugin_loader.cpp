#include "plugin_loader.h"
#include <QLibrary>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

PluginLoader::PluginLoader(QObject *parent)
    : QObject(parent)
{
    // Initialize plugin context
    m_context.requestFileOperation = [](const char* operation, const char* filePath, const char* content) {
        // This would call into your AgenticFileOperations system
        // For now, we'll just print a message
        qDebug() << "Plugin requested file operation:" << operation << filePath;
    };
}

PluginLoader::~PluginLoader()
{
    unloadAllPlugins();
}

bool PluginLoader::loadPlugin(const QString &pluginPath)
{
    QLibrary *library = new QLibrary(pluginPath, this);
    if (!library->load()) {
        qWarning() << "Failed to load plugin:" << pluginPath << library->errorString();
        delete library;
        return false;
    }

    // Resolve symbols
    PluginInfo* (*plugin_init)(PluginContext*) = (PluginInfo* (*)(PluginContext*))library->resolve("plugin_init");
    if (!plugin_init) {
        qWarning() << "Failed to resolve plugin_init in:" << pluginPath;
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

    QString pluginName = QFileInfo(pluginPath).baseName();
    m_plugins[pluginName] = instance;

    qDebug() << "Loaded plugin:" << pluginName;
    return true;
}

void PluginLoader::unloadPlugin(const QString &pluginName)
{
    if (m_plugins.contains(pluginName)) {
        PluginInstance &instance = m_plugins[pluginName];
        if (instance.plugin_cleanup) {
            instance.plugin_cleanup();
        }
        instance.library->unload();
        delete instance.library;
        m_plugins.remove(pluginName);
        qDebug() << "Unloaded plugin:" << pluginName;
    }
}

void PluginLoader::unloadAllPlugins()
{
    QVector<QString> pluginNames = m_plugins.keys().toVector();
    for (const QString &pluginName : pluginNames) {
        unloadPlugin(pluginName);
    }
}

void PluginLoader::onFileSave(const QString &filePath)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onFileSave) {
            it.value().plugin_onFileSave(filePath.toUtf8().constData());
        }
    }
}

void PluginLoader::onChatMessage(const QString &message)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onChatMessage) {
            it.value().plugin_onChatMessage(message.toUtf8().constData());
        }
    }
}

void PluginLoader::onCommand(const QString &command)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onCommand) {
            it.value().plugin_onCommand(command.toUtf8().constData());
        }
    }
}

void PluginLoader::onModelLoad(const QString &modelPath)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().plugin_onModelLoad) {
            it.value().plugin_onModelLoad(modelPath.toUtf8().constData());
        }
    }
}