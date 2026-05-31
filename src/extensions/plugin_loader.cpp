/**
 * @file plugin_loader.cpp
 * @brief Dynamic plugin loading system
 * 
 * Provides:
 * - Plugin discovery and enumeration
 * - Dynamic loading/unloading
 * - Dependency resolution
 * - Hot-reloading support
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#include "plugin_loader.h"
#include "../../include/rawrxd_version.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace RawrXD::Extensions {

// ============================================================================
// PluginLoader Implementation
// ============================================================================

PluginLoader::PluginLoader(const LoaderConfig& config)
    : m_config(config)
    , m_nextPluginId(1)
{
}

PluginLoader::~PluginLoader() {
    unloadAllPlugins();
}

// ============================================================================
// Plugin Discovery
// ============================================================================

std::vector<PluginInfo> PluginLoader::discoverPlugins() {
    std::vector<PluginInfo> plugins;
    
    if (!std::filesystem::exists(m_config.pluginDir)) {
        return plugins;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(m_config.pluginDir)) {
        if (!entry.is_regular_file()) continue;
        
        auto path = entry.path();
        auto ext = path.extension().string();
        
        // Check for plugin files (.dll, .so, .dylib)
        if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
            PluginInfo info = inspectPlugin(path.string());
            if (info.valid) {
                plugins.push_back(info);
            }
        }
        
        // Check for plugin directories with manifest
        if (entry.is_directory()) {
            auto manifestPath = path / "plugin.json";
            if (std::filesystem::exists(manifestPath)) {
                PluginInfo info = loadManifest(manifestPath.string());
                if (info.valid) {
                    info.path = path.string();
                    plugins.push_back(info);
                }
            }
        }
    }
    
    return plugins;
}

PluginInfo PluginLoader::inspectPlugin(const std::string& path) {
    PluginInfo info;
    info.path = path;
    info.valid = false;
    
    // Try to load and inspect the plugin
    HMODULE hModule = LoadLibraryExA(path.c_str(), nullptr, 
                                      LOAD_LIBRARY_AS_DATAFILE);
    if (!hModule) {
        return info;
    }
    
    // Try to read version info
    // In production, this would parse PE resources
    info.name = std::filesystem::path(path).stem().string();
    info.version = "1.0.0";
    info.valid = true;
    
    FreeLibrary(hModule);
    return info;
}

PluginInfo PluginLoader::loadManifest(const std::string& manifestPath) {
    PluginInfo info;
    info.valid = false;
    
    try {
        std::ifstream file(manifestPath);
        if (!file.is_open()) {
            return info;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        nlohmann::json manifest = nlohmann::json::parse(content);
        
        info.name = manifest.value("name", "");
        info.version = manifest.value("version", "1.0.0");
        info.description = manifest.value("description", "");
        info.author = manifest.value("author", "");
        info.apiVersion = manifest.value("apiVersion", 1);
        
        if (manifest.contains("dependencies")) {
            for (const auto& dep : manifest["dependencies"]) {
                info.dependencies.push_back(dep.get<std::string>());
            }
        }
        
        if (manifest.contains("permissions")) {
            for (const auto& perm : manifest["permissions"]) {
                info.permissions.push_back(perm.get<std::string>());
            }
        }
        
        info.valid = !info.name.empty();
    } catch (...) {
        info.valid = false;
    }
    
    return info;
}

// ============================================================================
// Plugin Loading
// ============================================================================

int64_t PluginLoader::loadPlugin(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already loaded
    for (const auto& [id, plugin] : m_plugins) {
        if (plugin.info.path == path) {
            return id; // Already loaded
        }
    }
    
    // Inspect plugin
    PluginInfo info = inspectPlugin(path);
    if (!info.valid) {
        return -1;
    }
    
    // Check dependencies
    if (!resolveDependencies(info)) {
        return -1;
    }
    
    // Load the plugin
    HMODULE hModule = LoadLibraryA(path.c_str());
    if (!hModule) {
        return -1;
    }
    
    // Resolve entry points
    auto initFunc = reinterpret_cast<PluginInitFunc>(
        GetProcAddress(hModule, "plugin_init"));
    auto shutdownFunc = reinterpret_cast<PluginShutdownFunc>(
        GetProcAddress(hModule, "plugin_shutdown"));
    auto getInfoFunc = reinterpret_cast<PluginGetInfoFunc>(
        GetProcAddress(hModule, "plugin_get_info"));
    
    if (!initFunc) {
        FreeLibrary(hModule);
        return -1;
    }
    
    // Initialize plugin
    PluginContext context;
    context.apiVersion = info.apiVersion;
    context.hostVersion = RAWRXD_VERSION_STR;
    
    if (!initFunc(&context)) {
        FreeLibrary(hModule);
        return -1;
    }
    
    // Get plugin info if available
    if (getInfoFunc) {
        PluginInfo* pluginInfo = getInfoFunc();
        if (pluginInfo) {
            info = *pluginInfo;
        }
    }
    
    int64_t pluginId = m_nextPluginId++;
    
    LoadedPlugin plugin;
    plugin.id = pluginId;
    plugin.info = info;
    plugin.hModule = hModule;
    plugin.initFunc = initFunc;
    plugin.shutdownFunc = shutdownFunc;
    plugin.getInfoFunc = getInfoFunc;
    plugin.state = PluginState::Loaded;
    
    m_plugins[pluginId] = plugin;
    
    return pluginId;
}

bool PluginLoader::unloadPlugin(int64_t pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return false;
    }
    
    auto& plugin = it->second;
    
    // Shutdown plugin
    if (plugin.shutdownFunc) {
        plugin.shutdownFunc();
    }
    
    // Unload DLL
    if (plugin.hModule) {
        FreeLibrary(static_cast<HMODULE>(plugin.hModule));
    }
    
    m_plugins.erase(it);
    return true;
}

void PluginLoader::unloadAllPlugins() {
    std::vector<int64_t> pluginIds;
    for (const auto& [id, _] : m_plugins) {
        pluginIds.push_back(id);
    }
    
    for (int64_t id : pluginIds) {
        unloadPlugin(id);
    }
}

// ============================================================================
// Hot Reloading
// ============================================================================

bool PluginLoader::reloadPlugin(int64_t pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return false;
    }
    
    std::string path = it->second.info.path;
    
    // Unload
    unloadPlugin(pluginId);
    
    // Reload
    int64_t newId = loadPlugin(path);
    return newId > 0;
}

void PluginLoader::checkForUpdates() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [id, plugin] : m_plugins) {
        if (!std::filesystem::exists(plugin.info.path)) {
            // Plugin file removed
            continue;
        }
        
        auto lastWrite = std::filesystem::last_write_time(plugin.info.path);
        if (lastWrite > plugin.loadTime) {
            // Plugin updated, mark for reload
            // In production, this would queue for reload
        }
    }
}

// ============================================================================
// Dependency Resolution
// ============================================================================

bool PluginLoader::resolveDependencies(const PluginInfo& info) {
    for (const auto& dep : info.dependencies) {
        bool found = false;
        
        // Check loaded plugins
        for (const auto& [id, plugin] : m_plugins) {
            if (plugin.info.name == dep) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Try to load dependency
            std::string depPath = (m_config.pluginDir / dep).string() + ".dll";
            if (std::filesystem::exists(depPath)) {
                int64_t depId = loadPlugin(depPath);
                if (depId < 0) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// Plugin Queries
// ============================================================================

std::vector<PluginInfo> PluginLoader::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<PluginInfo> plugins;
    plugins.reserve(m_plugins.size());
    
    for (const auto& [id, plugin] : m_plugins) {
        plugins.push_back(plugin.info);
    }
    
    return plugins;
}

PluginState PluginLoader::getPluginState(int64_t pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return PluginState::Unknown;
    }
    
    return it->second.state;
}

} // namespace RawrXD::Extensions
