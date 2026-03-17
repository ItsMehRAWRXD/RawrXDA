/**
 * @file plugin_loader.hpp
 * @brief Hot-injection loader for thermal dashboard plugin
 *
 * Allows loading/unloading thermal_dashboard.dll at runtime
 * without restarting the IDE. Pure Win32 LoadLibrary implementation.
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <memory>
#include <filesystem>
#include <mutex>
#include <cstdio>

namespace rawrxd::thermal {

/**
 * @brief Runtime plugin loader for hot-injection (Win32 LoadLibrary)
 */
class ThermalPluginLoader {
public:
    static ThermalPluginLoader& instance() {
        static ThermalPluginLoader s_instance;
        return s_instance;
    }

    /**
     * @brief Load thermal dashboard plugin from DLL
     * @param pluginPath Path to thermal_dashboard.dll
     * @return true if loaded successfully
     */
    bool loadPlugin(const std::string& pluginPath = std::string()) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_plugin) {
            return true;  // Already loaded
        }

        std::string path = pluginPath;
        if (path.empty()) {
            path = getDefaultPluginPath();
        }

        // Load DLL via Win32
        m_hModule = LoadLibraryA(path.c_str());
        if (!m_hModule) {
            fprintf(stderr, "[ThermalPlugin] LoadLibrary failed: %s (error %lu)\n",
                    path.c_str(), GetLastError());
            return false;
        }

        // Get factory function
        auto createFunc = reinterpret_cast<CreateThermalPluginFunc>(
            GetProcAddress(m_hModule, "CreateThermalPlugin")
        );
        if (!createFunc) {
            fprintf(stderr, "[ThermalPlugin] CreateThermalPlugin export not found\n");
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
            return false;
        }

        // Create plugin instance
        m_plugin = createFunc();
        if (!m_plugin) {
            fprintf(stderr, "[ThermalPlugin] CreateThermalPlugin returned null\n");
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
            return false;
        }

        // Initialize plugin
        if (!m_plugin->initialize()) {
            fprintf(stderr, "[ThermalPlugin] Plugin initialization failed\n");
            m_plugin = nullptr;
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
            return false;
        }

        m_currentPath = path;
        fprintf(stdout, "[ThermalPlugin] Loaded: %s v%s\n",
                m_plugin->pluginName().c_str(),
                m_plugin->pluginVersion().c_str());

        return true;
    }

    /**
     * @brief Unload plugin (hot-unload)
     */
    void unloadPlugin() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_plugin) {
            m_plugin->shutdown();
            m_plugin = nullptr;
        }

        if (m_hModule) {
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
        }

        m_currentPath.clear();
    }

    /**
     * @brief Reload plugin (hot-swap)
     */
    bool reloadPlugin(const std::string& pluginPath = std::string()) {
        std::string savedPath;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            savedPath = m_currentPath;
        }
        unloadPlugin();
        Sleep(50);  // Brief delay to ensure DLL file handle is released
        return loadPlugin(pluginPath.empty() ? savedPath : pluginPath);
    }

    /**
     * @brief Check if plugin is loaded
     */
    bool isLoaded() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_plugin != nullptr;
    }

    /**
     * @brief Get plugin interface
     */
    IThermalDashboardPlugin* plugin() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_plugin;
    }

    /**
     * @brief Create dashboard widget (convenience)
     */
    void* createDashboard(void* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_plugin) return nullptr;
        return m_plugin->createDashboardWidget(parent);
    }

    /**
     * @brief Create compact widget (convenience)
     */
    void* createCompactWidget(void* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_plugin) return nullptr;
        return m_plugin->createCompactWidget(parent);
    }

private:
    ThermalPluginLoader() = default;
    ~ThermalPluginLoader() { unloadPlugin(); }

    ThermalPluginLoader(const ThermalPluginLoader&) = delete;
    ThermalPluginLoader& operator=(const ThermalPluginLoader&) = delete;

    std::string getDefaultPluginPath() const {
        // Get directory of current executable
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Search common locations
        const std::filesystem::path candidates[] = {
            exeDir / "plugins" / "thermal_dashboard.dll",
            exeDir / "thermal_dashboard.dll",
            std::filesystem::path("D:/rawrxd/build/bin/thermal_dashboard.dll"),
            std::filesystem::path("D:/rawrxd/build/src/thermal/Release/thermal_dashboard.dll"),
        };

        for (const auto& p : candidates) {
            if (std::filesystem::exists(p)) {
                return p.string();
            }
        }

        // Fall back to default name (LoadLibrary will use system search)
        return "thermal_dashboard.dll";
    }

    HMODULE m_hModule = nullptr;
    IThermalDashboardPlugin* m_plugin = nullptr;
    std::string m_currentPath;
    mutable std::mutex m_mutex;
};

/**
 * @brief Convenience macros for IDE integration
 */
#define THERMAL_LOAD()       rawrxd::thermal::ThermalPluginLoader::instance().loadPlugin()
#define THERMAL_UNLOAD()     rawrxd::thermal::ThermalPluginLoader::instance().unloadPlugin()
#define THERMAL_RELOAD()     rawrxd::thermal::ThermalPluginLoader::instance().reloadPlugin()
#define THERMAL_PLUGIN()     rawrxd::thermal::ThermalPluginLoader::instance().plugin()
#define THERMAL_DASHBOARD(p) rawrxd::thermal::ThermalPluginLoader::instance().createDashboard(p)
#define THERMAL_COMPACT(p)   rawrxd::thermal::ThermalPluginLoader::instance().createCompactWidget(p)

} // namespace rawrxd::thermal


