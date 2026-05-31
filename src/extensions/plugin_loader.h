/**
 * @file plugin_loader.h
 * @brief Dynamic plugin loading system
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <chrono>

namespace RawrXD::Extensions {

// ============================================================================
// Plugin State
// ============================================================================

enum class PluginState {
    Unknown,
    Discovered,
    Loading,
    Loaded,
    Active,
    Error,
    Unloading
};

// ============================================================================
// Plugin Info
// ============================================================================

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string path;
    int apiVersion = 1;
    bool valid = false;
    std::vector<std::string> dependencies;
    std::vector<std::string> permissions;
};

// ============================================================================
// Plugin Context
// ============================================================================

struct PluginContext {
    int apiVersion;
    const char* hostVersion;
};

// ============================================================================
// Plugin Function Types
// ============================================================================

using PluginInitFunc = bool (*)(PluginContext* context);
using PluginShutdownFunc = void (*)();
using PluginGetInfoFunc = PluginInfo* (*)();

// ============================================================================
// Loaded Plugin
// ============================================================================

struct LoadedPlugin {
    int64_t id = -1;
    PluginInfo info;
    void* hModule = nullptr;
    PluginInitFunc initFunc = nullptr;
    PluginShutdownFunc shutdownFunc = nullptr;
    PluginGetInfoFunc getInfoFunc = nullptr;
    PluginState state = PluginState::Unknown;
    std::filesystem::file_time_type loadTime;
};

// ============================================================================
// Loader Configuration
// ============================================================================

struct LoaderConfig {
    std::filesystem::path pluginDir;
    bool enableHotReload = false;
    int hotReloadIntervalMs = 5000;
    size_t maxPlugins = 64;
};

// ============================================================================
// Plugin Loader
// ============================================================================

class PluginLoader {
public:
    explicit PluginLoader(const LoaderConfig& config);
    ~PluginLoader();
    
    // Discovery
    std::vector<PluginInfo> discoverPlugins();
    PluginInfo inspectPlugin(const std::string& path);
    PluginInfo loadManifest(const std::string& manifestPath);
    
    // Loading
    int64_t loadPlugin(const std::string& path);
    bool unloadPlugin(int64_t pluginId);
    void unloadAllPlugins();
    
    // Hot reloading
    bool reloadPlugin(int64_t pluginId);
    void checkForUpdates();
    
    // Queries
    std::vector<PluginInfo> getLoadedPlugins() const;
    PluginState getPluginState(int64_t pluginId) const;
    
private:
    bool resolveDependencies(const PluginInfo& info);
    
    LoaderConfig m_config;
    mutable std::mutex m_mutex;
    int64_t m_nextPluginId;
    std::map<int64_t, LoadedPlugin> m_plugins;
};

} // namespace RawrXD::Extensions
