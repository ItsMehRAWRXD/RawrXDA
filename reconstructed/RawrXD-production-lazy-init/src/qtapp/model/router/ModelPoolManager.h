#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <memory>
#include <atomic>

// Forward declarations
class InferenceEngine;
struct ModelEndpoint;

/**
 * @brief Model instance lifecycle state
 */
enum class ModelInstanceState {
    Uninitialized,   // Not created yet
    Initializing,    // Being created
    Ready,           // Available for requests
    Busy,            // Processing request
    Warming,         // Pre-warming
    Idle,            // No recent activity
    Draining,        // Finishing pending requests
    Terminated       // Shut down
};

/**
 * @brief Model instance in the pool
 */
struct ModelInstance {
    QString instanceId;
    QString endpointId;
    QString modelPath;
    ModelInstanceState state;
    InferenceEngine* engine;     // Owned by pool
    QDateTime createdAt;
    QDateTime lastUsedAt;
    int requestsHandled;
    double totalProcessingTime;
    int currentConnections;
    int maxConnections;
    
    ModelInstance() : engine(nullptr), state(ModelInstanceState::Uninitialized),
                     requestsHandled(0), totalProcessingTime(0),
                     currentConnections(0), maxConnections(1) {
        createdAt = QDateTime::currentDateTime();
        lastUsedAt = createdAt;
    }
    
    ~ModelInstance();
    
    QJsonObject toJson() const;
};

/**
 * @brief Pool configuration
 */
struct PoolConfiguration {
    int minInstances;          // Minimum instances to maintain
    int maxInstances;          // Maximum instances allowed
    int idleTimeout;           // Milliseconds before idle instance removed
    int warmupInstances;       // Pre-warm this many instances
    bool autoScale;            // Enable automatic scaling
    double scaleUpThreshold;   // CPU utilization threshold to scale up
    double scaleDownThreshold; // CPU utilization threshold to scale down
    int scaleUpCooldown;       // Cooldown period after scale up (ms)
    int scaleDownCooldown;     // Cooldown period after scale down (ms)
    
    PoolConfiguration() : minInstances(1), maxInstances(10), idleTimeout(300000),
                         warmupInstances(2), autoScale(true),
                         scaleUpThreshold(0.8), scaleDownThreshold(0.3),
                         scaleUpCooldown(60000), scaleDownCooldown(120000) {}
    
    QJsonObject toJson() const;
    static PoolConfiguration fromJson(const QJsonObject& json);
};

/**
 * @brief Pool statistics
 */
struct PoolStatistics {
    int totalInstances;
    int readyInstances;
    int busyInstances;
    int idleInstances;
    qint64 totalRequestsHandled;
    double averageResponseTime;
    double poolUtilization;    // 0.0 - 1.0
    QDateTime lastScaleEvent;
    
    PoolStatistics() : totalInstances(0), readyInstances(0), busyInstances(0),
                      idleInstances(0), totalRequestsHandled(0),
                      averageResponseTime(0), poolUtilization(0) {}
    
    QJsonObject toJson() const;
};

/**
 * @brief Model Pool Manager
 * 
 * Manages a pool of model instances with lifecycle management,
 * automatic scaling, warm-up, and connection pooling.
 */
class ModelPoolManager : public QObject {
    Q_OBJECT
    
public:
    explicit ModelPoolManager(QObject* parent = nullptr);
    ~ModelPoolManager();
    
    // ============ Pool Configuration ============
    
    /**
     * @brief Set pool configuration
     */
    void setConfiguration(const PoolConfiguration& config);
    
    /**
     * @brief Get current configuration
     */
    PoolConfiguration getConfiguration() const;
    
    // ============ Instance Management ============
    
    /**
     * @brief Create model instance
     */
    QString createInstance(const QString& endpointId, const QString& modelPath);
    
    /**
     * @brief Destroy model instance
     */
    bool destroyInstance(const QString& instanceId);
    
    /**
     * @brief Get instance by ID
     */
    ModelInstance* getInstance(const QString& instanceId);
    
    /**
     * @brief Get all instances for endpoint
     */
    QList<ModelInstance*> getInstancesForEndpoint(const QString& endpointId);
    
    /**
     * @brief Get available instance for endpoint
     */
    ModelInstance* getAvailableInstance(const QString& endpointId);
    
    // ============ Instance Lifecycle ============
    
    /**
     * @brief Initialize instance (load model)
     */
    bool initializeInstance(const QString& instanceId);
    
    /**
     * @brief Warm up instance (pre-process)
     */
    bool warmUpInstance(const QString& instanceId);
    
    /**
     * @brief Mark instance as busy
     */
    void markInstanceBusy(const QString& instanceId);
    
    /**
     * @brief Mark instance as ready
     */
    void markInstanceReady(const QString& instanceId);
    
    /**
     * @brief Drain instance (stop accepting new requests)
     */
    void drainInstance(const QString& instanceId);
    
    /**
     * @brief Terminate instance
     */
    void terminateInstance(const QString& instanceId);
    
    // ============ Auto-Scaling ============
    
    /**
     * @brief Enable auto-scaling
     */
    void enableAutoScaling();
    
    /**
     * @brief Disable auto-scaling
     */
    void disableAutoScaling();
    
    /**
     * @brief Scale up (add instances)
     */
    int scaleUp(const QString& endpointId, int count = 1);
    
    /**
     * @brief Scale down (remove instances)
     */
    int scaleDown(const QString& endpointId, int count = 1);
    
    /**
     * @brief Get current scale for endpoint
     */
    int getCurrentScale(const QString& endpointId) const;
    
    // ============ Warm-Up Management ============
    
    /**
     * @brief Pre-warm instances for endpoint
     */
    void preWarmInstances(const QString& endpointId, int count);
    
    /**
     * @brief Schedule warm-up
     */
    void scheduleWarmUp(const QString& endpointId, const QDateTime& when, int count);
    
    // ============ Request Handling ============
    
    /**
     * @brief Acquire instance for request
     */
    ModelInstance* acquireInstance(const QString& endpointId, int timeoutMs = 5000);
    
    /**
     * @brief Release instance after request
     */
    void releaseInstance(const QString& instanceId, double processingTime);
    
    /**
     * @brief Record request completion
     */
    void recordRequestComplete(const QString& instanceId, bool success, double responseTime);
    
    // ============ Pool Statistics ============
    
    /**
     * @brief Get pool statistics
     */
    PoolStatistics getStatistics() const;
    
    /**
     * @brief Get statistics for endpoint
     */
    PoolStatistics getEndpointStatistics(const QString& endpointId) const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();
    
    // ============ Health Management ============
    
    /**
     * @brief Check instance health
     */
    bool checkInstanceHealth(const QString& instanceId);
    
    /**
     * @brief Get unhealthy instances
     */
    QList<QString> getUnhealthyInstances() const;
    
    /**
     * @brief Restart unhealthy instances
     */
    void restartUnhealthyInstances();
    
    // ============ Configuration Persistence ============
    
    /**
     * @brief Save configuration
     */
    QJsonObject saveConfiguration() const;
    
    /**
     * @brief Load configuration
     */
    bool loadConfiguration(const QJsonObject& config);
    
signals:
    /**
     * @brief Emitted when instance state changes
     */
    void instanceStateChanged(const QString& instanceId, ModelInstanceState oldState, ModelInstanceState newState);
    
    /**
     * @brief Emitted when scaling occurs
     */
    void scalingOccurred(const QString& endpointId, int oldCount, int newCount);
    
    /**
     * @brief Emitted when pool utilization changes
     */
    void utilizationChanged(double utilization);
    
private slots:
    void performAutoScaling();
    void cleanupIdleInstances();
    
private:
    // Helper functions
    QString generateInstanceId(const QString& endpointId);
    double calculateUtilization(const QString& endpointId) const;
    bool shouldScaleUp(const QString& endpointId) const;
    bool shouldScaleDown(const QString& endpointId) const;
    void changeInstanceState(const QString& instanceId, ModelInstanceState newState);
    
    // Data members
    QMap<QString, std::shared_ptr<ModelInstance>> m_instances;
    QMap<QString, QQueue<QString>> m_availableInstances;  // endpointId -> instance IDs
    
    PoolConfiguration m_config;
    
    QTimer* m_autoScaleTimer;
    QTimer* m_cleanupTimer;
    
    QMap<QString, QDateTime> m_lastScaleUp;
    QMap<QString, QDateTime> m_lastScaleDown;
    
    std::atomic<qint64> m_totalRequestsHandled;
    std::atomic<qint64> m_totalInstancesCreated;
    
    mutable QMutex m_mutex;
    QWaitCondition m_instanceAvailable;
};
