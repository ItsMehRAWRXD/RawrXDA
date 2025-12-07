#pragma once

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QString>
#include <QDateTime>
#include <memory>

class InferenceEngine;

/**
 * @brief Multi-model queue system for concurrent model management
 * 
 * Features:
 * - Priority-based scheduling (HIGH, NORMAL, LOW)
 * - Concurrent model loading (up to 2+ models)
 * - Memory-aware queue management
 * - Request throttling and backpressure
 * - Hot model swapping without blocking
 */
class ModelQueue : public QObject {
    Q_OBJECT

public:
    enum Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2
    };

    struct Request {
        qint64 id;
        QString modelPath;
        QString prompt;
        int maxTokens;
        float temperature;
        Priority priority;
        QDateTime enqueueTime;
        
        bool operator<(const Request& other) const {
            if (priority != other.priority) {
                return priority > other.priority; // Higher priority first
            }
            return enqueueTime < other.enqueueTime; // FIFO for same priority
        }
    };

    explicit ModelQueue(QObject* parent = nullptr);
    ~ModelQueue();

    /**
     * @brief Enqueue an inference request
     * @param modelPath Path to GGUF model
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     * @param temperature Sampling temperature
     * @param priority Request priority
     * @return Request ID for tracking
     */
    qint64 enqueue(const QString& modelPath, const QString& prompt, 
                   int maxTokens = 256, float temperature = 0.7f,
                   Priority priority = NORMAL);

    /**
     * @brief Cancel a pending request
     */
    bool cancelRequest(qint64 requestId);

    /**
     * @brief Get queue status
     */
    int pendingRequests() const;
    int activeModels() const;
    
    /**
     * @brief Start processing queue
     */
    void start();
    
    /**
     * @brief Stop processing and clear queue
     */
    void stop();

    /**
     * @brief Set maximum concurrent models (default: 2)
     */
    void setMaxConcurrentModels(int max);

signals:
    void requestStarted(qint64 requestId);
    void requestCompleted(qint64 requestId, const QString& result);
    void requestFailed(qint64 requestId, const QString& error);
    void queueEmpty();
    void modelLoaded(const QString& modelPath);
    void modelUnloaded(const QString& modelPath);

private slots:
    void processQueue();
    void onInferenceComplete(qint64 reqId, const QString& result);
    void onInferenceError(qint64 reqId, const QString& error);

private:
    struct ModelSlot {
        QString currentModel;
        InferenceEngine* engine = nullptr;
        bool busy = false;
        QThread* thread = nullptr;
    };

    ModelSlot* allocateSlot(const QString& modelPath);
    void releaseSlot(ModelSlot* slot);
    InferenceEngine* getOrLoadModel(const QString& modelPath);

    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    QQueue<Request> m_queue;
    QHash<qint64, Request> m_activeRequests;
    QVector<ModelSlot> m_slots;
    
    qint64 m_nextRequestId = 1;
    int m_maxConcurrentModels = 2;
    bool m_running = false;
    
    QThread* m_processingThread = nullptr;
};
