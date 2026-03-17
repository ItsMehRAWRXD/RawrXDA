/**
 * @file plugin_loader.hpp
 * @brief Hot-injection loader for thermal dashboard plugin
 * 
 * Allows loading/unloading thermal_dashboard.dll at runtime
 * without restarting the IDE.
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"
#include <QPluginLoader>
#include <QDir>
#include <QDebug>
#include <memory>

namespace rawrxd::thermal {

/**
 * @brief Runtime plugin loader for hot-injection
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
    bool loadPlugin(const QString& pluginPath = QString()) {
        if (m_plugin) {
            qWarning() << "[PluginLoader] Plugin already loaded";
            return true;
        }
        
        QString path = pluginPath;
        if (path.isEmpty()) {
            // Default: plugins directory next to executable
            path = QDir(QCoreApplication::applicationDirPath())
                       .filePath("plugins/thermal_dashboard.dll");
        }
        
        qDebug() << "[PluginLoader] Loading plugin from:" << path;
        
        m_loader = std::make_unique<QPluginLoader>(path);
        
        if (!m_loader->load()) {
            qWarning() << "[PluginLoader] Failed to load:" << m_loader->errorString();
            m_loader.reset();
            return false;
        }
        
        QObject* instance = m_loader->instance();
        if (!instance) {
            qWarning() << "[PluginLoader] Failed to get plugin instance";
            m_loader->unload();
            m_loader.reset();
            return false;
        }
        
        m_plugin = qobject_cast<IThermalDashboardPlugin*>(instance);
        if (!m_plugin) {
            qWarning() << "[PluginLoader] Plugin does not implement IThermalDashboardPlugin";
            m_loader->unload();
            m_loader.reset();
            return false;
        }
        
        // Initialize plugin
        if (!m_plugin->initialize()) {
            qWarning() << "[PluginLoader] Plugin initialization failed";
            m_loader->unload();
            m_loader.reset();
            m_plugin = nullptr;
            return false;
        }
        
        qDebug() << "[PluginLoader] Plugin loaded successfully:"
                 << m_plugin->pluginName() << "v" << m_plugin->pluginVersion();
        
        return true;
    }
    
    /**
     * @brief Unload plugin (hot-unload)
     */
    void unloadPlugin() {
        if (!m_plugin) return;
        
        qDebug() << "[PluginLoader] Unloading plugin...";
        
        m_plugin->shutdown();
        m_plugin = nullptr;
        
        if (m_loader) {
            m_loader->unload();
            m_loader.reset();
        }
        
        qDebug() << "[PluginLoader] Plugin unloaded";
    }
    
    /**
     * @brief Reload plugin (hot-swap)
     */
    bool reloadPlugin(const QString& pluginPath = QString()) {
        unloadPlugin();
        return loadPlugin(pluginPath);
    }
    
    /**
     * @brief Check if plugin is loaded
     */
    bool isLoaded() const {
        return m_plugin != nullptr;
    }
    
    /**
     * @brief Get plugin interface
     */
    IThermalDashboardPlugin* plugin() {
        return m_plugin;
    }
    
    /**
     * @brief Create dashboard widget (convenience)
     */
    QWidget* createDashboard(QWidget* parent = nullptr) {
        if (!m_plugin) return nullptr;
        return m_plugin->createDashboardWidget(parent);
    }
    
    /**
     * @brief Create compact widget (convenience)
     */
    QWidget* createCompactWidget(QWidget* parent = nullptr) {
        if (!m_plugin) return nullptr;
        return m_plugin->createCompactWidget(parent);
    }

private:
    ThermalPluginLoader() = default;
    ~ThermalPluginLoader() { unloadPlugin(); }
    
    ThermalPluginLoader(const ThermalPluginLoader&) = delete;
    ThermalPluginLoader& operator=(const ThermalPluginLoader&) = delete;
    
    std::unique_ptr<QPluginLoader> m_loader;
    IThermalDashboardPlugin* m_plugin = nullptr;
};

/**
 * @brief Convenience macros for IDE integration
 */
#define THERMAL_LOAD()      rawrxd::thermal::ThermalPluginLoader::instance().loadPlugin()
#define THERMAL_UNLOAD()    rawrxd::thermal::ThermalPluginLoader::instance().unloadPlugin()
#define THERMAL_RELOAD()    rawrxd::thermal::ThermalPluginLoader::instance().reloadPlugin()
#define THERMAL_PLUGIN()    rawrxd::thermal::ThermalPluginLoader::instance().plugin()
#define THERMAL_DASHBOARD(p) rawrxd::thermal::ThermalPluginLoader::instance().createDashboard(p)
#define THERMAL_COMPACT(p)   rawrxd::thermal::ThermalPluginLoader::instance().createCompactWidget(p)

} // namespace rawrxd::thermal
