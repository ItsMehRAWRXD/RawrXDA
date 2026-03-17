#pragma once

/**
 * @file InstanceManager.h
 * @brief Multi-instance management for RawrXD IDE
 * 
 * Provides instance detection, port allocation, and inter-process communication
 * for running multiple IDE instances safely without conflicts.
 * 
 * Features:
 * - Unique instance identification (PID + timestamp)
 * - Dynamic port allocation (11434-11534 range)
 * - QSharedMemory-based instance registry
 * - Instance-specific settings paths
 * - IPC for instance discovery and communication
 */

#include <QObject>
#include <QString>
#include <QMap>
#include <QSharedMemory>
#include <QLocalServer>
#include <QDateTime>
#include <cstdint>

namespace RawrXD {

/**
 * @brief Information about a single IDE instance
 */
struct InstanceInfo {
    qint64 pid;              ///< Process ID
    uint16_t port;           ///< API server port
    QString instanceId;      ///< Unique instance identifier (PID_timestamp)
    QDateTime startTime;     ///< When this instance was started
    QString workingDir;      ///< Current working directory
    bool isPrimary;          ///< Is this the primary instance?
    
    InstanceInfo() : pid(0), port(0), isPrimary(false) {}
    
    QString toString() const {
        return QString("Instance[id=%1, pid=%2, port=%3, dir=%4]")
            .arg(instanceId)
            .arg(pid)
            .arg(port)
            .arg(workingDir);
    }
};

/**
 * @brief Manages multiple IDE instances and their coordination
 * 
 * This class handles:
 * - Instance registration and discovery
 * - Port allocation without conflicts
 * - Inter-process communication
 * - Resource sharing coordination
 */
class InstanceManager : public QObject
{
    Q_OBJECT
    
public:
    explicit InstanceManager(QObject* parent = nullptr);
    ~InstanceManager() override;
    
    /**
     * @brief Initialize this instance and register with the instance registry
     * @param requestedPort Optional specific port to request (0 = auto-allocate)
     * @return true if initialization succeeded
     */
    bool initialize(uint16_t requestedPort = 0);
    
    /**
     * @brief Get the unique instance ID for this process
     * @return Instance ID string (e.g., "12345_20260115_143022")
     */
    QString instanceId() const { return m_instanceInfo.instanceId; }
    
    /**
     * @brief Get the allocated port for this instance
     * @return Port number (11434-11534 range)
     */
    uint16_t port() const { return m_instanceInfo.port; }
    
    /**
     * @brief Get the process ID for this instance
     */
    qint64 pid() const { return m_instanceInfo.pid; }
    
    /**
     * @brief Check if this is the primary instance
     */
    bool isPrimary() const { return m_instanceInfo.isPrimary; }
    
    /**
     * @brief Get information about this instance
     */
    const InstanceInfo& info() const { return m_instanceInfo; }
    
    /**
     * @brief Get list of all running instances
     * @return Map of instance ID -> InstanceInfo
     */
    QMap<QString, InstanceInfo> getRunningInstances() const;
    
    /**
     * @brief Find an available port in the configured range
     * @param startPort Starting port to search from (default: 11434)
     * @param endPort Ending port of search range (default: 11534)
     * @return Available port number, or 0 if none available
     */
    static uint16_t findAvailablePort(uint16_t startPort = 11434, uint16_t endPort = 11534);
    
    /**
     * @brief Check if a specific port is available
     * @param port Port number to check
     * @return true if port is available
     */
    static bool isPortAvailable(uint16_t port);
    
    /**
     * @brief Generate instance-specific settings path
     * @param basePath Base settings directory
     * @return Full path including instance ID (e.g., "settings_12345.json")
     */
    QString getInstanceSettingsPath(const QString& basePath) const;
    
    /**
     * @brief Send a message to another instance
     * @param targetInstanceId Instance to send to
     * @param message Message content
     * @return true if message was sent successfully
     */
    bool sendMessageToInstance(const QString& targetInstanceId, const QString& message);
    
    /**
     * @brief Launch a new IDE instance
     * @param workingDir Optional working directory for new instance
     * @return PID of new instance, or 0 on failure
     */
    qint64 launchNewInstance(const QString& workingDir = QString());

signals:
    /**
     * @brief Emitted when a new instance is detected
     */
    void instanceAdded(const InstanceInfo& info);
    
    /**
     * @brief Emitted when an instance terminates
     */
    void instanceRemoved(const QString& instanceId);
    
    /**
     * @brief Emitted when a message is received from another instance
     */
    void messageReceived(const QString& fromInstanceId, const QString& message);
    
    /**
     * @brief Emitted when port allocation succeeds
     */
    void portAllocated(uint16_t port);

private slots:
    void onIncomingConnection();
    void onClientMessage();

private:
    /**
     * @brief Register this instance in shared memory
     */
    bool registerInstance();
    
    /**
     * @brief Unregister this instance from shared memory
     */
    void unregisterInstance();
    
    /**
     * @brief Update the instance registry from shared memory
     */
    void updateInstanceRegistry();
    
    /**
     * @brief Setup IPC server for receiving messages
     */
    bool setupIPCServer();

    InstanceInfo m_instanceInfo;              ///< Info about this instance
    QSharedMemory* m_sharedMemory;            ///< Shared memory for instance registry
    QLocalServer* m_ipcServer;                ///< IPC server for messages
    
    static constexpr int MAX_INSTANCES = 32;  ///< Maximum concurrent instances
    static constexpr int REGISTRY_SIZE = 8192; ///< Shared memory size
};

} // namespace RawrXD
