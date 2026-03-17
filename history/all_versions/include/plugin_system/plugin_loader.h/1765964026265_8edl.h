#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include <QObject>
#include <QString>
#include <QLibrary>
#include <QMap>
#include <QVector>
#include "plugin_api.h"

class PluginLoader : public QObject
{
    Q_OBJECT

public:
    explicit PluginLoader(QObject *parent = nullptr);
    ~PluginLoader();

    // Hot-load .dll / .so via QLibrary
    // Manifest-driven: name, version, permissions (["file_ops", "terminal"])
    // Sand-boxed: plugins must call rawrxd::requestFileOperation() → goes through your AgenticFileOperations → Keep/Undo dialog.
    // Hooks: onFileSave, onChatMessage, onCommand, onModelLoad
    // Unload-safe (RAII shared-lib handle, refcount).
    bool loadPlugin(const QString &pluginPath);
    void unloadPlugin(const QString &pluginName);
    void unloadAllPlugins();

    // Call plugin hooks
    void onFileSave(const QString &filePath);
    void onChatMessage(const QString &message);
    void onCommand(const QString &command);
    void onModelLoad(const QString &modelPath);

private:
    struct PluginInstance {
        QLibrary *library;
        PluginInfo* (*plugin_init)(PluginContext*);
        void (*plugin_onFileSave)(const char*);
        void (*plugin_onChatMessage)(const char*);
        void (*plugin_onCommand)(const char*);
        void (*plugin_onModelLoad)(const char*);
        void (*plugin_cleanup)();
        PluginInfo *info;
        int refCount;
    };

    QMap<QString, PluginInstance> m_plugins;
    PluginContext m_context;
};

#endif // PLUGIN_LOADER_H