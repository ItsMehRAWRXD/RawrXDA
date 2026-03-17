#pragma once

/**
 * @file WorkerThread.h
 * @brief Reusable worker thread abstraction with task queue system
 * 
 * Provides a general-purpose worker thread framework for CPU-intensive
 * background operations like model loading, code analysis, and file processing.
 * 
 * Features:
 * - Task queue with priority support
 * - Progress reporting via signals
 * - Cancellation support
 * - Thread pool management
 * - Lock-free telemetry collection
 */

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QAtomicInt>
#include <functional>
#include <memory>

namespace RawrXD {

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Generic background task
 */
class WorkerTask {
public:
    using TaskFunction = std::function<void(WorkerTask*)>;
    
    WorkerTask(const QString& name, TaskFunction func, TaskPriority priority = TaskPriority::Normal)
        : m_name(name)
        , m_function(func)
        , m_priority(priority)
        , m_progress(0)
        , m_cancelled(false)
    {
        m_taskId = QString("%1_%2").arg(name).arg(reinterpret_cast<quintptr>(this));
    }
    
    QString taskId() const { return m_taskId; }
    QString name() const { return m_name; }
    TaskPriority priority() const { return m_priority; }
    
    /**
     * @brief Execute the task
     */
    void execute() {
        if (m_function && !m_cancelled.load()) {
            m_function(this);
        }
    }
    
    /**
     * @brief Set task progress (0-100)
     */
    void setProgress(int progress) {
        m_progress.store(progress);
    }
    
    /**
     * @brief Get current progress
     */
    int progress() const {
        return m_progress.load();
    }
    
    /**
     * @brief Cancel this task
     */
    void cancel() {
        m_cancelled.store(true);
    }
    
    /**
     * @brief Check if task is cancelled
     */
    bool isCancelled() const {
        return m_cancelled.load();
    }
    
private:
    QString m_taskId;
    QString m_name;
    TaskFunction m_function;
    TaskPriority m_priority;
    QAtomicInt m_progress;
    QAtomicInt m_cancelled;
};

/**
 * @brief Worker thread for executing background tasks
 */
class WorkerThread : public QThread
{
    Q_OBJECT
    
public:
    explicit WorkerThread(QObject* parent = nullptr);
    ~WorkerThread() override;
    
    /**
     * @brief Add a task to the queue
     * @param task Task to execute (takes ownership)
     */
    void addTask(WorkerTask* task);
    
    /**
     * @brief Stop the worker thread gracefully
     * @param waitForCompletion If true, wait for current task to finish
     */
    void stop(bool waitForCompletion = true);
    
    /**
     * @brief Get number of pending tasks
     */
    int pendingTaskCount() const;
    
    /**
     * @brief Get currently executing task ID
     */
    QString currentTaskId() const;
    
    /**
     * @brief Check if thread is idle (no tasks)
     */
    bool isIdle() const;

signals:
    /**
     * @brief Emitted when a task starts
     */
    void taskStarted(const QString& taskId, const QString& taskName);
    
    /**
     * @brief Emitted when a task completes
     */
    void taskCompleted(const QString& taskId);
    
    /**
     * @brief Emitted when a task fails
     */
    void taskFailed(const QString& taskId, const QString& error);
    
    /**
     * @brief Emitted when task progress changes
     */
    void taskProgress(const QString& taskId, int progress);

protected:
    void run() override;

private:
    struct TaskEntry {
        WorkerTask* task;
        
        bool operator<(const TaskEntry& other) const {
            return static_cast<int>(task->priority()) < static_cast<int>(other.task->priority());
        }
    };
    
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    QQueue<TaskEntry> m_tasks;
    QAtomicInt m_running;
    QString m_currentTaskId;
};

/**
 * @brief Thread pool manager for worker threads
 */
class WorkerThreadPool : public QObject
{
    Q_OBJECT
    
public:
    explicit WorkerThreadPool(int numThreads = -1, QObject* parent = nullptr);
    ~WorkerThreadPool() override;
    
    /**
     * @brief Submit a task to the pool
     * @param task Task to execute
     * @return Task ID for tracking
     */
    QString submitTask(WorkerTask* task);
    
    /**
     * @brief Cancel a specific task
     * @param taskId Task to cancel
     */
    void cancelTask(const QString& taskId);
    
    /**
     * @brief Cancel all pending tasks
     */
    void cancelAll();
    
    /**
     * @brief Get pool statistics
     */
    QString getStatistics() const;
    
    /**
     * @brief Get number of active threads
     */
    int activeThreadCount() const;
    
    /**
     * @brief Get total number of threads in pool
     */
    int threadCount() const { return m_threads.size(); }
    
    /**
     * @brief Wait for all tasks to complete
     * @param timeoutMs Maximum time to wait (-1 = infinite)
     */
    bool waitForDone(int timeoutMs = -1);

signals:
    void taskStarted(const QString& taskId);
    void taskCompleted(const QString& taskId);
    void taskFailed(const QString& taskId, const QString& error);
    void taskProgress(const QString& taskId, int progress);

private:
    /**
     * @brief Find least busy thread
     */
    WorkerThread* getLeastBusyThread();
    
    QVector<WorkerThread*> m_threads;
    mutable QMutex m_mutex;
    QMap<QString, WorkerThread*> m_taskThreadMap;
};

/**
 * @brief Lock-free telemetry buffer for background metrics collection
 */
class TelemetryBuffer {
public:
    struct Entry {
        qint64 timestamp;
        QString category;
        QString event;
        double value;
    };
    
    TelemetryBuffer(int capacity = 10000);
    ~TelemetryBuffer();
    
    /**
     * @brief Record a telemetry event (lock-free)
     */
    void record(const QString& category, const QString& event, double value = 0.0);
    
    /**
     * @brief Flush buffered entries to storage
     * @return Number of entries flushed
     */
    int flush(QVector<Entry>& output);
    
    /**
     * @brief Get current buffer usage
     */
    int size() const { return m_writeIndex.load() - m_readIndex.load(); }
    
    /**
     * @brief Check if buffer is full
     */
    bool isFull() const { return size() >= m_capacity; }

private:
    Entry* m_buffer;
    int m_capacity;
    QAtomicInt m_writeIndex;
    QAtomicInt m_readIndex;
};

} // namespace RawrXD
