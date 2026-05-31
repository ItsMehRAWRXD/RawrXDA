// ============================================================================
// Plugin Manager — Extensible Plugin System
// Dynamic plugin loading and lifecycle management
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <dlfcn.h>

namespace RawrXD::Plugins {

enum class PluginState {
    UNLOADED,
    LOADING,
    LOADED,
    RUNNING,
    ERROR,
    UNLOADING
};

enum class PluginType {
    EXTENSION,
    THEME,
    LANGUAGE_SUPPORT,
    TOOL_INTEGRATION,
    CUSTOM
};

struct PluginInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    PluginType type;
    std::vector<std::string> dependencies;
    std::vector<std::string> permissions;
    std::map<std::string, std::string> settings;
};

struct PluginInstance {
    PluginInfo info;
    PluginState state;
    void* handle;
    std::chrono::system_clock::time_point loadedAt;
    std::chrono::system_clock::time_point startedAt;
    std::string errorMessage;
    std::map<std::string, std::string> runtimeData;
};

struct PluginHook {
    std::string name;
    std::vector<std::string> pluginIds;
    std::function<void(void*)> callback;
};

class PluginManager {
public:
    explicit PluginManager(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    ~PluginManager() {
        // Unload all plugins
        for (const auto& [id, instance] : m_plugins) {
            if (instance.state == PluginState::RUNNING) {
                UnloadPlugin(id);
            }
        }
    }

    bool LoadPlugin(const std::string& pluginPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Load plugin library
        void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
        if (!handle) {
            return false;
        }
        
        // Get plugin info function
        auto getInfo = (PluginInfo(*)())dlsym(handle, "GetPluginInfo");
        if (!getInfo) {
            dlclose(handle);
            return false;
        }
        
        PluginInfo info = getInfo();
        
        // Check dependencies
        if (!CheckDependencies(info)) {
            dlclose(handle);
            return false;
        }
        
        // Create instance
        PluginInstance instance;
        instance.info = info;
        instance.state = PluginState::LOADED;
        instance.handle = handle;
        instance.loadedAt = std::chrono::system_clock::now();
        
        m_plugins[info.id] = instance;
        
        // Initialize plugin
        auto init = (bool(*)())dlsym(handle, "Initialize");
        if (init) {
            if (!init()) {
                instance.state = PluginState::ERROR;
                instance.errorMessage = "Initialization failed";
                return false;
            }
        }
        
        instance.state = PluginState::RUNNING;
        instance.startedAt = std::chrono::system_clock::now();
        
        // Register hooks
        RegisterPluginHooks(info.id);
        
        return true;
    }

    bool UnloadPlugin(const std::string& pluginId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it == m_plugins.end()) {
            return false;
        }
        
        auto& instance = it->second;
        
        // Shutdown plugin
        if (instance.handle) {
            auto shutdown = (void(*)())dlsym(instance.handle, "Shutdown");
            if (shutdown) {
                shutdown();
            }
            
            dlclose(instance.handle);
        }
        
        // Unregister hooks
        UnregisterPluginHooks(pluginId);
        
        instance.state = PluginState::UNLOADED;
        m_plugins.erase(it);
        
        return true;
    }

    bool EnablePlugin(const std::string& pluginId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it == m_plugins.end()) {
            return false;
        }
        
        if (it->second.state == PluginState::LOADED) {
            auto activate = (bool(*)())dlsym(it->second.handle, "Activate");
            if (activate) {
                if (activate()) {
                    it->second.state = PluginState::RUNNING;
                    return true;
                }
            }
        }
        
        return false;
    }

    bool DisablePlugin(const std::string& pluginId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it == m_plugins.end()) {
            return false;
        }
        
        if (it->second.state == PluginState::RUNNING) {
            auto deactivate = (bool(*)())dlsym(it->second.handle, "Deactivate");
            if (deactivate) {
                if (deactivate()) {
                    it->second.state = PluginState::LOADED;
                    return true;
                }
            }
        }
        
        return false;
    }

    std::vector<PluginInstance> GetLoadedPlugins() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<PluginInstance> plugins;
        for (const auto& [id, instance] : m_plugins) {
            plugins.push_back(instance);
        }
        return plugins;
    }

    PluginState GetPluginState(const std::string& pluginId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it != m_plugins.end()) {
            return it->second.state;
        }
        return PluginState::UNLOADED;
    }

    void RegisterHook(const std::string& hookName, const std::string& pluginId,
                     std::function<void(void*)> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_hooks[hookName].pluginIds.push_back(pluginId);
        m_hooks[hookName].callback = callback;
    }

    void ExecuteHook(const std::string& hookName, void* data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_hooks.find(hookName);
        if (it != m_hooks.end()) {
            for (const auto& pluginId : it->second.pluginIds) {
                auto pluginIt = m_plugins.find(pluginId);
                if (pluginIt != m_plugins.end() && 
                    pluginIt->second.state == PluginState::RUNNING) {
                    it->second.callback(data);
                }
            }
        }
    }

    std::string GetPluginSetting(const std::string& pluginId, 
                                const std::string& key) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it != m_plugins.end()) {
            auto settingIt = it->second.info.settings.find(key);
            if (settingIt != it->second.info.settings.end()) {
                return settingIt->second;
            }
        }
        return "";
    }

    void SetPluginSetting(const std::string& pluginId, const std::string& key,
                         const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plugins.find(pluginId);
        if (it != m_plugins.end()) {
            it->second.info.settings[key] = value;
            
            // Notify plugin of setting change
            auto onSettingChanged = (void(*)(const char*, const char*))dlsym(
                it->second.handle, "OnSettingChanged");
            if (onSettingChanged) {
                onSettingChanged(key.c_str(), value.c_str());
            }
        }
    }

    std::string GeneratePluginReport() {
        std::ostringstream report;
        report << "# Plugin Report\n\n";
        
        auto plugins = GetLoadedPlugins();
        report << "## Loaded Plugins (" << plugins.size() << ")\n\n";
        
        for (const auto& instance : plugins) {
            report << "### " << instance.info.name << "\n";
            report << "- **ID:** " << instance.info.id << "\n";
            report << "- **Version:** " << instance.info.version << "\n";
            report << "- **Author:** " << instance.info.author << "\n";
            report << "- **Type:** " << TypeToString(instance.info.type) << "\n";
            report << "- **State:** " << StateToString(instance.state) << "\n";
            report << "- **Loaded:** " << FormatTime(instance.loadedAt) << "\n\n";
            
            if (!instance.info.dependencies.empty()) {
                report << "**Dependencies:**\n";
                for (const auto& dep : instance.info.dependencies) {
                    report << "- " << dep << "\n";
                }
                report << "\n";
            }
            
            if (!instance.errorMessage.empty()) {
                report << "**Error:** " << instance.errorMessage << "\n\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, PluginInstance> m_plugins;
    std::map<std::string, PluginHook> m_hooks;

    bool CheckDependencies(const PluginInfo& info) {
        for (const auto& dep : info.dependencies) {
            if (m_plugins.find(dep) == m_plugins.end()) {
                return false;
            }
        }
        return true;
    }

    void RegisterPluginHooks(const std::string& pluginId) {
        // Register plugin-specific hooks
    }

    void UnregisterPluginHooks(const std::string& pluginId) {
        // Unregister plugin-specific hooks
        for (auto& [name, hook] : m_hooks) {
            hook.pluginIds.erase(
                std::remove(hook.pluginIds.begin(), hook.pluginIds.end(), pluginId),
                hook.pluginIds.end()
            );
        }
    }

    std::string StateToString(PluginState state) {
        switch (state) {
            case PluginState::UNLOADED: return "Unloaded";
            case PluginState::LOADING: return "Loading";
            case PluginState::LOADED: return "Loaded";
            case PluginState::RUNNING: return "Running";
            case PluginState::ERROR: return "Error";
            case PluginState::UNLOADING: return "Unloading";
            default: return "Unknown";
        }
    }

    std::string TypeToString(PluginType type) {
        switch (type) {
            case PluginType::EXTENSION: return "Extension";
            case PluginType::THEME: return "Theme";
            case PluginType::LANGUAGE_SUPPORT: return "Language Support";
            case PluginType::TOOL_INTEGRATION: return "Tool Integration";
            case PluginType::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Plugins
