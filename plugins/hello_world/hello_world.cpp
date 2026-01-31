#include "../include/plugin_system/plugin_api.h"
#include <stdio.h>

static PluginContext* g_context = nullptr;

PluginInfo* plugin_init(PluginContext* context) {
    g_context = context;
    static PluginInfo info = {
        "HelloWorld",
        "1.0.0",
        "A simple hello world plugin"
    };
    printf("HelloWorld plugin initialized\n");
    return &info;
}

void plugin_onFileSave(const char* filePath) {
    printf("HelloWorld plugin: File saved %s\n", filePath);
}

void plugin_onChatMessage(const char* message) {
    printf("HelloWorld plugin: Chat message %s\n", message);
}

void plugin_onCommand(const char* command) {
    printf("HelloWorld plugin: Command %s\n", command);
}

void plugin_onModelLoad(const char* modelPath) {
    printf("HelloWorld plugin: Model loaded %s\n", modelPath);
}

void plugin_cleanup() {
    printf("HelloWorld plugin cleaned up\n");
}