#pragma once
#include <QObject>
#include <QQueue>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <functional>

/**
 * @class ModelLoadRequest
 * @brief Represents a queued model load/unload request
 */
struct ModelLoadRequest {
    enum Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };
    
    QString modelPath;
    Priority priority = Normal;
    std::function<void(bool, const QString&)> callback; // success, message
    int requestId = -1;
    qint64 createdAt = 0; // timestamp
    bool isUnload = false;
};

/**
 * @class ModelQueue
 * @brief Manages concurrent model loading/unloading with priority scheduling
 * 
 * Features:
 * - Priority-based request queuing (Critical, High, Normal, Low)
 * - Concurrent model loading with throttling
 * - Model state tracking and validation
 * - Automatic cleanup on unload
 * - Request callback system
 */
class ModelQueue : public QObject {
    Q_OBJECT
    
public:
    explicit ModelQueue(QObject* parent = nullptr);
    ~ModelQueue();
    
    /**
     * @brief Enqueue a model load request
     * @param path Model file path
     * @param priority Request priority
     * @param callback Completion callback(success, message)
     * @return Request ID for tracking
     */
    int enqueueLoad(const QString& path, ModelLoadRequest::Priority priority = ModelLoadRequest::Normal,
                    std::function<void(bool, const QString&)> callback = nullptr);
    
    /**
     * @brief Enqueue a model unload request
     * @param path Model file path to unload
     * @return Request ID for tracking
     */
    int enqueueUnload(const QString& path);
    
    /**
     * @brief Get number of pending requests
     */
    int pendingRequestCount() const;
    
    /**
     * @brief Get currently loaded models
     */
    QStringList loadedModels() const;
    
    /**
     * @brief Check if model is loaded
     */
    bool isModelLoaded(const QString& path) const;
    
    /**
     * @brief Get model metadata without loading
     */
    QHash<QString, QVariant> getModelMetadata(const QString& path);
    
    /**
     * @brief Set maximum concurrent loads (default: 1)
     */
    void setMaxConcurrentLoads(int count);
    
    /**
     * @brief Clear all pending requests
     */
    void clearQueue();
    
    /**
     * @brief Start processing queue
     */
    void start();
    
    /**
     * @brief Stop processing queue
     */
    void stop();
    
signals:
    void loadStarted(int requestId, const QString& path);
    void loadCompleted(int requestId, bool success, const QString& message);
    void unloadCompleted(const QString& path);
    void queueStatusChanged(int pending, int active);
    void modelStatsUpdated(const QString& path, int cachedTensors, qint64 memoryUsage);
    
private slots:
    void processQueue();
    void onLoadFinished(bool success, const QString& message);
    
private:
    struct LoadedModelInfo {
        QString path;
        QHash<QString, QVariant> metadata;
        int cachedTensors = 0;
        qint64 memoryUsage = 0;
        qint64 loadedAt = 0;
    };
    
    struct ActiveLoad {
        int requestId;
        QString path;
        qint64 startTime;
    };
    
    QQueue<ModelLoadRequest> requestQueue;
    QHash<QString, LoadedModelInfo> loadedModels_;
    QVector<ActiveLoad> activeLoads;
    
    mutable QMutex mutex;
    QWaitCondition queueUpdated;
    
    int nextRequestId = 1;
    int maxConcurrentLoads = 1;
    bool isRunning = false;
    
    int findBestRequest();
    bool tryLoad(const ModelLoadRequest& request);
    void notifyQueueStatus();
};
