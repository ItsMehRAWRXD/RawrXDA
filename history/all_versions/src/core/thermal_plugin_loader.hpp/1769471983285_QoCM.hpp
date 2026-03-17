/**
 * @file thermal_plugin_loader.hpp
 * @brief Hot-injection plugin loader for Thermal Dashboard
 * 
 * Enables runtime loading/unloading of thermal_dashboard.dll
 * without IDE restart. Supports IPC-based commands and auto-discovery.
 */

#pragma once

#include <QObject>
#include <QPluginLoader>
#include <QFileSystemWatcher>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
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
class ThermalPluginLoader : public QObject {
    Q_OBJECT

public:
    explicit ThermalPluginLoader(QObject* parent = nullptr);
    ~ThermalPluginLoader() override;

    /**
     * @brief Initialize the plugin loader system
     * @param pluginSearchPaths Directories to search for plugins
     * @return true if initialized successfully
     */
    bool initialize(const QStringList& pluginSearchPaths = {});

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Load the thermal dashboard plugin
     * @param pluginPath Optional explicit path to DLL
     * @return true if loaded successfully
     */
    bool loadPlugin(const QString& pluginPath = QString());

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
    QString lastError() const { return m_lastError; }

    /**
     * @brief Enable/disable auto-reload on DLL changes
     */
    void setAutoReload(bool enabled);

    /**
     * @brief Enable/disable IPC command server
     */
    void setIpcEnabled(bool enabled);

signals:
    /**
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
    void pluginError(const QString& error);

    /**
     * @brief Emitted when plugin status changes
     */
    void statusChanged(PluginStatus status);

    /**
     * @brief Emitted when auto-reload detects DLL change
     */
    void pluginFileChanged(const QString& path);

private slots:
    void onFileChanged(const QString& path);
    void onNewIpcConnection();
    void onIpcReadyRead();
    void onIpcDisconnected();
    void onReloadTimer();

private:
    void setStatus(PluginStatus status);
    QString findPluginPath() const;
    void setupIpcServer();
    void handleIpcCommand(const QJsonObject& cmd, QLocalSocket* socket);

private:
    std::unique_ptr<QPluginLoader> m_loader;
    rawrxd::thermal::IThermalDashboardPlugin* m_plugin;
    
    QStringList m_searchPaths;
    QString m_currentPluginPath;
    QString m_lastError;
    PluginStatus m_status;
    
    // Auto-reload
    std::unique_ptr<QFileSystemWatcher> m_watcher;
    std::unique_ptr<QTimer> m_reloadTimer;
    bool m_autoReloadEnabled;
    bool m_pendingReload;
    
    // IPC
    std::unique_ptr<QLocalServer> m_ipcServer;
    QList<QLocalSocket*> m_ipcClients;
    bool m_ipcEnabled;
    
    mutable QMutex m_mutex;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Inline Implementation
// ═══════════════════════════════════════════════════════════════════════════════

inline ThermalPluginLoader::ThermalPluginLoader(QObject* parent)
    : QObject(parent)
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

inline bool ThermalPluginLoader::initialize(const QStringList& pluginSearchPaths)
{
    QMutexLocker locker(&m_mutex);
    
    m_searchPaths = pluginSearchPaths;
    if (m_searchPaths.isEmpty()) {
        // Default search paths
        m_searchPaths << QCoreApplication::applicationDirPath()
                      << QCoreApplication::applicationDirPath() + "/plugins"
                      << "D:/rawrxd/build/bin"
                      << "D:/rawrxd/build/src/thermal/Release";
    }
    
    // Setup file watcher for auto-reload
    m_watcher = std::make_unique<QFileSystemWatcher>(this);
    connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
            this, &ThermalPluginLoader::onFileChanged);
    
    // Setup reload debounce timer
    m_reloadTimer = std::make_unique<QTimer>(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(500);  // 500ms debounce
    connect(m_reloadTimer.get(), &QTimer::timeout,
            this, &ThermalPluginLoader::onReloadTimer);
    
    // Setup IPC server
    if (m_ipcEnabled) {
        setupIpcServer();
    }
    
    qDebug() << "[ThermalPluginLoader] Initialized with search paths:" << m_searchPaths;
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

inline bool ThermalPluginLoader::loadPlugin(const QString& pluginPath)
{
    QMutexLocker locker(&m_mutex);
    
    // Unload existing plugin
    if (m_loader && m_loader->isLoaded()) {
        locker.unlock();
        unloadPlugin();
        locker.relock();
    }
    
    setStatus(PluginStatus::Loading);
    
    // Find plugin path
    QString path = pluginPath.isEmpty() ? findPluginPath() : pluginPath;
    if (path.isEmpty()) {
        m_lastError = "Could not find thermal_dashboard.dll";
        setStatus(PluginStatus::Error);
        emit pluginError(m_lastError);
        return false;
    }
    
    qDebug() << "[ThermalPluginLoader] Loading plugin from:" << path;
    
    // Create loader and load
    m_loader = std::make_unique<QPluginLoader>(path);
    
    if (!m_loader->load()) {
        m_lastError = m_loader->errorString();
        qWarning() << "[ThermalPluginLoader] Load failed:" << m_lastError;
        setStatus(PluginStatus::Error);
        emit pluginError(m_lastError);
        return false;
    }
    
    // Get plugin interface
    QObject* instance = m_loader->instance();
    m_plugin = qobject_cast<rawrxd::thermal::IThermalDashboardPlugin*>(instance);
    
    if (!m_plugin) {
        m_lastError = "Plugin does not implement IThermalDashboardPlugin";
        m_loader->unload();
        setStatus(PluginStatus::Error);
        emit pluginError(m_lastError);
        return false;
    }
    
    // Initialize plugin
    if (!m_plugin->initialize()) {
        m_lastError = "Plugin initialization failed";
        m_loader->unload();
        m_plugin = nullptr;
        setStatus(PluginStatus::Error);
        emit pluginError(m_lastError);
        return false;
    }
    
    m_currentPluginPath = path;
    
    // Watch for changes
    if (m_autoReloadEnabled && m_watcher) {
        m_watcher->addPath(path);
    }
    
    setStatus(PluginStatus::Loaded);
    qDebug() << "[ThermalPluginLoader] Plugin loaded successfully:"
             << m_plugin->pluginName() << m_plugin->pluginVersion();
    
    emit pluginLoaded();
    return true;
}

inline bool ThermalPluginLoader::unloadPlugin()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_loader || !m_loader->isLoaded()) {
        return true;
    }
    
    setStatus(PluginStatus::Unloading);
    
    // Stop watching
    if (m_watcher && !m_currentPluginPath.isEmpty()) {
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
        qWarning() << "[ThermalPluginLoader] Unload failed:" << m_lastError;
        setStatus(PluginStatus::Error);
        return false;
    }
    
    m_loader.reset();
    m_currentPluginPath.clear();
    
    setStatus(PluginStatus::NotLoaded);
    qDebug() << "[ThermalPluginLoader] Plugin unloaded";
    
    emit pluginUnloaded();
    return true;
}

inline bool ThermalPluginLoader::reloadPlugin()
{
    QString path = m_currentPluginPath;
    if (!unloadPlugin()) {
        return false;
    }
    
    // Small delay to ensure file is released
    QThread::msleep(100);
    
    return loadPlugin(path);
}

inline bool ThermalPluginLoader::isLoaded() const
{
    QMutexLocker locker(&m_mutex);
    return m_loader && m_loader->isLoaded() && m_plugin != nullptr;
}

inline rawrxd::thermal::IThermalDashboardPlugin* ThermalPluginLoader::plugin() const
{
    QMutexLocker locker(&m_mutex);
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
        emit statusChanged(status);
    }
}

inline QString ThermalPluginLoader::findPluginPath() const
{
    const QString pluginName = "thermal_dashboard.dll";
    
    for (const QString& dir : m_searchPaths) {
        QString path = QDir(dir).filePath(pluginName);
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QString();
}

inline void ThermalPluginLoader::setupIpcServer()
{
    // Create named pipe server for external injection commands
    QString pipeName = QString("RawrXD_PluginLoader_%1").arg(QCoreApplication::applicationPid());
    
    m_ipcServer = std::make_unique<QLocalServer>(this);
    m_ipcServer->setSocketOptions(QLocalServer::WorldAccessOption);
    
    // Remove any stale socket
    QLocalServer::removeServer(pipeName);
    
    if (!m_ipcServer->listen(pipeName)) {
        qWarning() << "[ThermalPluginLoader] Failed to start IPC server:" << m_ipcServer->errorString();
        return;
    }
    
    connect(m_ipcServer.get(), &QLocalServer::newConnection,
            this, &ThermalPluginLoader::onNewIpcConnection);
    
    qDebug() << "[ThermalPluginLoader] IPC server listening on:" << pipeName;
}

inline void ThermalPluginLoader::onFileChanged(const QString& path)
{
    qDebug() << "[ThermalPluginLoader] Plugin file changed:" << path;
    emit pluginFileChanged(path);
    
    if (m_autoReloadEnabled) {
        m_pendingReload = true;
        m_reloadTimer->start();  // Debounce
    }
}

inline void ThermalPluginLoader::onReloadTimer()
{
    if (m_pendingReload) {
        m_pendingReload = false;
        qDebug() << "[ThermalPluginLoader] Auto-reloading plugin...";
        reloadPlugin();
    }
}

inline void ThermalPluginLoader::onNewIpcConnection()
{
    while (m_ipcServer->hasPendingConnections()) {
        QLocalSocket* socket = m_ipcServer->nextPendingConnection();
        m_ipcClients.append(socket);
        
        connect(socket, &QLocalSocket::readyRead,
                this, &ThermalPluginLoader::onIpcReadyRead);
        connect(socket, &QLocalSocket::disconnected,
                this, &ThermalPluginLoader::onIpcDisconnected);
        
        qDebug() << "[ThermalPluginLoader] IPC client connected";
    }
}

inline void ThermalPluginLoader::onIpcReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    
    QByteArray data = socket->readLine();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isObject()) {
        handleIpcCommand(doc.object(), socket);
    }
}

inline void ThermalPluginLoader::onIpcDisconnected()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        m_ipcClients.removeAll(socket);
        socket->deleteLater();
        qDebug() << "[ThermalPluginLoader] IPC client disconnected";
    }
}

inline void ThermalPluginLoader::handleIpcCommand(const QJsonObject& cmd, QLocalSocket* socket)
{
    QString action = cmd["action"].toString();
    QJsonObject response;
    
    if (action == "LOAD_PLUGIN") {
        QString path = cmd["path"].toString();
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
    
    QJsonDocument respDoc(response);
    socket->write(respDoc.toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
}

} // namespace rawrxd::core
