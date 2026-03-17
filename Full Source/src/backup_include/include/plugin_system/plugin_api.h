#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdint.h>

// Stable C interface for plugins
#ifdef __cplusplus
extern "C" {
#endif
    typedef struct PluginContext {
        void (*requestFileOperation)(const char* operation, const char* filePath, const char* content);
    } PluginContext;

    typedef struct PluginInfo {
        const char* name;
        const char* version;
        const char* description;
    } PluginInfo;

    // Plugin entry point
    PluginInfo* plugin_init(PluginContext* context);
    void plugin_onFileSave(const char* filePath);
    void plugin_onChatMessage(const char* message);
    void plugin_onCommand(const char* command);
    void plugin_onModelLoad(const char* modelPath);
    void plugin_cleanup();
#ifdef __cplusplus
}
#endif

#endif // PLUGIN_API_H