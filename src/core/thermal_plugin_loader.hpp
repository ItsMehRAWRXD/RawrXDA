/**
 * @file thermal_plugin_loader.hpp
 * @brief Hot-injection plugin loader for Thermal Dashboard
 * 
 * Enables runtime loading/unloading of thermal_dashboard.dll
 * without IDE restart. Supports IPC-based commands and auto-discovery.
 */

#pragma once

#include <memory>
#include <functional>

// Forward declare the interface
namespace rawrxd::thermal {
    class IThermalDashboardPlugin;
}

namespace rawrxd::core {

/**
 * @brief Plugin loader status
 */
enum class PluginStatus {
    NotLoaded,
    Loading,
    Loaded,
    Error,
    Unloading
};

/**
 * @brief Hot-injectable plugin loader for thermal dashboard
 * 
 * Features:
 * - Runtime DLL loading via QPluginLoader
 * - Named pipe IPC for external injection commands
 * - File system watching for auto-reload on DLL update
 * - Thread-safe plugin access
 */
class ThermalPluginLoader  {public:
    explicit ThermalPluginLoader( = nullptr);
    ~ThermalPluginLoader() override;

    /**
     * @brief Initialize the plugin loader system
     * @param pluginSearchPaths Directories to search for plugins
     * @return true if initialized successfully
     */
    bool initialize(const std::stringList& pluginSearchPaths = {});

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Load the thermal dashboard plugin
     * @param pluginPath Optional explicit path to DLL
     * @return true if loaded successfully
     */
    bool loadPlugin(const std::string& pluginPath = std::string());

    /**
     * @brief Unload the currently loaded plugin
     * @return true if unloaded successfully
     */
    bool unloadPlugin();

    /**
     * @brief Reload the plugin (unload then load)
     * @return true if reloaded successfully
     */
    bool reloadPlugin();

    /**
     * @brief Check if plugin is currently loaded
     */
    bool isLoaded() const;

    /**
     * @brief Get current plugin status
     */
    PluginStatus status() const { return m_status; }

    /**
     * @brief Get the loaded plugin interface
     * @return Plugin interface or nullptr if not loaded
     */
    rawrxd::thermal::IThermalDashboardPlugin* plugin() const;

    /**
     * @brief Get last error message
     */
    std::string lastError() const { return m_lastError; }

    /**
     * @brief Enable/disable auto-reload on DLL changes
     */
    void setAutoReload(bool enabled);

    /**
     * @brief Enable/disable IPC command server
     */
    void setIpcEnabled(bool enabled);

\npublic:\n    /**
     * @brief Emitted when plugin is loaded
     */
    void pluginLoaded();

    /**
     * @brief Emitted when plugin is unloaded
     */
    void pluginUnloaded();

    /**
     * @brief Emitted when plugin load/unload fails
     */
    void pluginError(const std::string& error);

    /**
     * @brief Emitted when plugin status changes
     */
    void statusChanged(PluginStatus status);

    /**
     * @brief Emitted when auto-reload detects DLL change
     */
    void pluginFileChanged(const std::string& path);

\nprivate:\n    void onFileChanged(const std::string& path);
    void onNewIpcConnection();
    void onIpcReadyRead();
    void onIpcDisconnected();
    void onReloadTimer();

private:
    void setStatus(PluginStatus status);
    std::string findPluginPath() const;
    void setupIpcServer();
    void handleIpcCommand(const void*& cmd, void** socket);

private:
    std::unique_ptr<QPluginLoader> m_loader;
    rawrxd::thermal::IThermalDashboardPlugin* m_plugin;
    
    std::stringList m_searchPaths;
    std::string m_currentPluginPath;
    std::string m_lastError;
    PluginStatus m_status;
    
    // Auto-reload
    std::unique_ptr<// SystemWatcher> m_watcher;
    std::unique_ptr<void> m_reloadTimer;
    bool m_autoReloadEnabled;
    bool m_pendingReload;
    
    // IPC
    std::unique_ptr<void*> m_ipcServer;
    std::vector<void**> m_ipcClients;
    bool m_ipcEnabled;
    
    mutable std::mutex m_mutex;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Inline Implementation
// ═══════════════════════════════════════════════════════════════════════════════

inline ThermalPluginLoader::ThermalPluginLoader()
    
    , m_plugin(nullptr)
    , m_status(PluginStatus::NotLoaded)
    , m_autoReloadEnabled(true)
    , m_pendingReload(false)
    , m_ipcEnabled(true)
{
}

inline ThermalPluginLoader::~ThermalPluginLoader()
{
    shutdown();
}

inline bool ThermalPluginLoader::initialize(const std::stringList& pluginSearchPaths)
{
    std::mutexLocker locker(&m_mutex);
    
    m_searchPaths = pluginSearchPaths;
    if (m_searchPaths.empty()) {
        // Default search paths
        m_searchPaths << QCoreApplication::applicationDirPath()
                      << QCoreApplication::applicationDirPath() + "/plugins"
                      << "D:/rawrxd/build/bin"
                      << "D:/rawrxd/build/src/thermal/Release";
    }
    
    // Setup file watcher for auto-reload
    m_watcher = std::make_unique<// SystemWatcher>(this);  // Signal connection removed\n// Setup reload debounce timer
    m_reloadTimer = std::make_unique<std::chrono::system_clock::time_pointr>(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(500);  // 500ms debounce  // Signal connection removed\n// Setup IPC server
    if (m_ipcEnabled) {
        setupIpcServer();
    }
    
    return true;
}

inline void ThermalPluginLoader::shutdown()
{
    unloadPlugin();
    
    if (m_ipcServer) {
        m_ipcServer->close();
        m_ipcServer.reset();
    }
    
    m_watcher.reset();
    m_reloadTimer.reset();
}

inline bool ThermalPluginLoader::loadPlugin(const std::string& pluginPath)
{
    std::mutexLocker locker(&m_mutex);
    
    // Unload existing plugin
    if (m_loader && m_loader->isLoaded()) {
        locker.unlock();
        unloadPlugin();
        locker.relock();
    }
    
    setStatus(PluginStatus::Loading);
    
    // Find plugin path
    std::string path = pluginPath.empty() ? findPluginPath() : pluginPath;
    if (path.empty()) {
        m_lastError = "Could not find thermal_dashboard.dll";
        setStatus(PluginStatus::Error);
        pluginError(m_lastError);
        return false;
    }


    // Create loader and load
    m_loader = std::make_unique<QPluginLoader>(path);
    
    if (!m_loader->load()) {
        m_lastError = m_loader->errorString();
        setStatus(PluginStatus::Error);
        pluginError(m_lastError);
        return false;
    }
    
    // Get plugin interface
    void* instance = m_loader->instance();
// REMOVED_QT:     m_plugin = qobject_cast<rawrxd::thermal::IThermalDashboardPlugin*>(instance);
    
    if (!m_plugin) {
        m_lastError = "Plugin does not implement IThermalDashboardPlugin";
        m_loader->unload();
        setStatus(PluginStatus::Error);
        pluginError(m_lastError);
        return false;
    }
    
    // Initialize plugin
    if (!m_plugin->initialize()) {
        m_lastError = "Plugin initialization failed";
        m_loader->unload();
        m_plugin = nullptr;
        setStatus(PluginStatus::Error);
        pluginError(m_lastError);
        return false;
    }
    
    m_currentPluginPath = path;
    
    // Watch for changes
    if (m_autoReloadEnabled && m_watcher) {
        m_watcher->addPath(path);
    }
    
    setStatus(PluginStatus::Loaded);
             << m_plugin->pluginName() << m_plugin->pluginVersion();
    
    pluginLoaded();
    return true;
}

inline bool ThermalPluginLoader::unloadPlugin()
{
    std::mutexLocker locker(&m_mutex);
    
    if (!m_loader || !m_loader->isLoaded()) {
        return true;
    }
    
    setStatus(PluginStatus::Unloading);
    
    // Stop watching
    if (m_watcher && !m_currentPluginPath.empty()) {
        m_watcher->removePath(m_currentPluginPath);
    }
    
    // Shutdown plugin
    if (m_plugin) {
        m_plugin->shutdown();
        m_plugin = nullptr;
    }
    
    // Unload DLL
    if (!m_loader->unload()) {
        m_lastError = m_loader->errorString();
        setStatus(PluginStatus::Error);
        return false;
    }
    
    m_loader.reset();
    m_currentPluginPath.clear();
    
    setStatus(PluginStatus::NotLoaded);
    
    pluginUnloaded();
    return true;
}

inline bool ThermalPluginLoader::reloadPlugin()
{
    std::string path = m_currentPluginPath;
    if (!unloadPlugin()) {
        return false;
    }
    
    // Small delay to ensure file is released
    std::thread::msleep(100);
    
    return loadPlugin(path);
}

inline bool ThermalPluginLoader::isLoaded() const
{
    std::mutexLocker locker(&m_mutex);
    return m_loader && m_loader->isLoaded() && m_plugin != nullptr;
}

inline rawrxd::thermal::IThermalDashboardPlugin* ThermalPluginLoader::plugin() const
{
    std::mutexLocker locker(&m_mutex);
    return m_plugin;
}

inline void ThermalPluginLoader::setAutoReload(bool enabled)
{
    m_autoReloadEnabled = enabled;
}

inline void ThermalPluginLoader::setIpcEnabled(bool enabled)
{
    m_ipcEnabled = enabled;
    if (enabled && !m_ipcServer) {
        setupIpcServer();
    } else if (!enabled && m_ipcServer) {
        m_ipcServer->close();
        m_ipcServer.reset();
    }
}

inline void ThermalPluginLoader::setStatus(PluginStatus status)
{
    if (m_status != status) {
        m_status = status;
        statusChanged(status);
    }
}

inline std::string ThermalPluginLoader::findPluginPath() const
{
    const std::string pluginName = "thermal_dashboard.dll";
    
    for (const std::string& dir : m_searchPaths) {
        std::string path = // (dir).filePath(pluginName);
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    return std::string();
}

inline void ThermalPluginLoader::setupIpcServer()
{
    // Create named pipe server for external injection commands
    std::string pipeName = std::string("RawrXD_PluginLoader_%1"));
    
    m_ipcServer = std::make_unique<void*>(this);
    m_ipcServer->setSocketOptions(void*::WorldAccessOption);
    
    // Remove any stale socket
    void*::removeServer(pipeName);
    
    if (!m_ipcServer->listen(pipeName)) {
        return;
}

inline void ThermalPluginLoader::onFileChanged(const std::string& path)
{
    pluginFileChanged(path);
    
    if (m_autoReloadEnabled) {
        m_pendingReload = true;
        m_reloadTimer->start();  // Debounce
    }
}

inline void ThermalPluginLoader::onReloadTimer()
{
    if (m_pendingReload) {
        m_pendingReload = false;
        reloadPlugin();
    }
}

inline void ThermalPluginLoader::onNewIpcConnection()
{
    while (m_ipcServer->hasPendingConnections()) {
        void** socket = m_ipcServer->nextPendingConnection();
    }
}

inline void ThermalPluginLoader::onIpcReadyRead()
{
// REMOVED_QT:     void** socket = qobject_cast<void**>(sender());
    if (!socket) return;
    
    std::vector<uint8_t> data = socket->readLine();
    void* doc = void*::fromJson(data);
    
    if (doc.isObject()) {
        handleIpcCommand(doc.object(), socket);
    }
}

inline void ThermalPluginLoader::onIpcDisconnected()
{
// REMOVED_QT:     void** socket = qobject_cast<void**>(sender());
    if (socket) {
        m_ipcClients.removeAll(socket);
        socket->deleteLater();
    }
}

inline void ThermalPluginLoader::handleIpcCommand(const void*& cmd, void** socket)
{
    std::string action = cmd["action"].toString();
    void* response;
    
    if (action == "LOAD_PLUGIN") {
        std::string path = cmd["path"].toString();
        bool success = loadPlugin(path);
        response["success"] = success;
        if (!success) {
            response["error"] = m_lastError;
        }
    }
    else if (action == "UNLOAD_PLUGIN") {
        bool success = unloadPlugin();
        response["success"] = success;
    }
    else if (action == "RELOAD_PLUGIN") {
        bool success = reloadPlugin();
        response["success"] = success;
        if (!success) {
            response["error"] = m_lastError;
        }
    }
    else if (action == "STATUS") {
        response["success"] = true;
        response["loaded"] = isLoaded();
        response["status"] = static_cast<int>(m_status);
        if (m_plugin) {
            response["name"] = m_plugin->pluginName();
            response["version"] = m_plugin->pluginVersion();
        }
    }
    else {
        response["success"] = false;
        response["error"] = "Unknown action: " + action;
    }
    
    void* respDoc(response);
    socket->write(respDoc.toJson(void*::Compact) + "\n");
    socket->flush();
}

} // namespace rawrxd::core

