#pragma once
// =============================================================================
// Win32PluginLoader — Native Win32 Plugin System (No Qt Dependency)
//
// Phase 43: Port of Qt PluginLoader to pure Win32 using LoadLibrary/GetProcAddress.
// Uses the stable C ABI from plugin_api.h.
// Supports hot-load, hot-unload, hook dispatch, and config-gated loading.
//
// Thread safety: All public methods are mutex-protected.
// Error model: No exceptions — returns bool/PatchResult-style results with logging.
// =============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include "plugin_api.h"

namespace RawrXD {

// Forward declare for logging callback
using PluginLogCallback = std::function<void(const std::string& message, int severity)>;

// Plugin state
enum class PluginState {
    Loaded,
    Active,
    Error,
    Unloading
};

// Plugin instance — one per loaded DLL
struct PluginInstance {
    HMODULE                         hModule = nullptr;
    std::string                     name;
    std::string                     path;
    PluginState                     state = PluginState::Loaded;

    // Resolved C ABI function pointers (nullable)
    PluginInfo* (*fn_init)(PluginContext*)       = nullptr;
    void        (*fn_onFileSave)(const char*)    = nullptr;
    void        (*fn_onChatMessage)(const char*) = nullptr;
    void        (*fn_onCommand)(const char*)     = nullptr;
    void        (*fn_onModelLoad)(const char*)   = nullptr;
    void        (*fn_cleanup)()                  = nullptr;

    // Plugin metadata (returned by plugin_init)
    PluginInfo*  info = nullptr;

    // Stats
    uint64_t loadTimeMs  = 0;
    uint64_t hookCalls   = 0;
};

// Win32PluginLoader — main class
class Win32PluginLoader {
public:
    Win32PluginLoader();
    ~Win32PluginLoader();

    // Lifecycle
    bool loadPlugin(const std::string& pluginPath);
    bool unloadPlugin(const std::string& pluginName);
    void unloadAll();

    // Hook dispatch — calls all loaded plugins
    void dispatchFileSave(const std::string& filePath);
    void dispatchChatMessage(const std::string& message);
    void dispatchCommand(const std::string& command);
    void dispatchModelLoad(const std::string& modelPath);

    // Query
    size_t pluginCount() const;
    std::vector<std::string> pluginNames() const;
    const PluginInstance* getPlugin(const std::string& name) const;
    bool isLoaded(const std::string& name) const;

    // Scan a directory for .dll plugins
    std::vector<std::string> scanDirectory(const std::string& dirPath) const;

    // Logging callback
    void setLogCallback(PluginLogCallback cb);

    // Config: enable/disable hot-loading
    void setHotLoadEnabled(bool enabled) { m_hotLoadEnabled = enabled; }
    bool isHotLoadEnabled() const { return m_hotLoadEnabled; }

private:
    void log(const std::string& msg, int severity = 0);
    std::string extractPluginName(const std::string& path) const;

    // File-operation callback passed into plugin context
    static void pluginFileOperationCallback(const char* operation,
                                            const char* filePath,
                                            const char* content);

    mutable std::mutex                      m_mutex;
    std::map<std::string, PluginInstance>    m_plugins;
    PluginContext                            m_context;
    PluginLogCallback                        m_logCallback;
    bool                                    m_hotLoadEnabled = false;

    // Static instance pointer for C callback routing
    static Win32PluginLoader*               s_instance;
};

} // namespace RawrXD
