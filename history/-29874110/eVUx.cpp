#include "model_queue.hpp"
#include <QDebug>
#include <QDateTime>
#include <algorithm>

ModelQueue::ModelQueue(QObject* parent)
    : QObject(parent), nextRequestId(1), maxConcurrentLoads(1), isRunning(false)
{
}

ModelQueue::~ModelQueue()
{
    stop();
}

int ModelQueue::enqueueLoad(const QString& path, ModelLoadRequest::Priority priority,
                            std::function<void(bool, const QString&)> callback)
{
    QMutexLocker lock(&mutex);
    
    // Check if already loaded
    if (loadedModels_.contains(path)) {
        qDebug() << "Model already loaded:" << path;
        if (callback) callback(true, "Model already loaded");
        return -1;
    }
    
    ModelLoadRequest req;
    req.modelPath = path;
    req.priority = priority;
    req.callback = callback;
    req.requestId = nextRequestId++;
    req.createdAt = QDateTime::currentMSecsSinceEpoch();
    
    requestQueue.enqueue(req);
    
    qDebug() << "Enqueued load request:" << req.requestId << "priority:" << priority << "path:" << path;
    emit loadStarted(req.requestId, path);
    
    notifyQueueStatus();
    queueUpdated.wakeAll();
    
    return req.requestId;
}

int ModelQueue::enqueueUnload(const QString& path)
{
    QMutexLocker lock(&mutex);
    
    if (!loadedModels_.contains(path)) {
        qWarning() << "Model not loaded:" << path;
        return -1;
    }
    
    ModelLoadRequest req;
    req.modelPath = path;
    req.priority = ModelLoadRequest::Normal;
    req.requestId = nextRequestId++;
    req.isUnload = true;
    req.createdAt = QDateTime::currentMSecsSinceEpoch();
    
    requestQueue.enqueue(req);
    
    qDebug() << "Enqueued unload request:" << req.requestId << "path:" << path;
    notifyQueueStatus();
    queueUpdated.wakeAll();
    
    return req.requestId;
}

int ModelQueue::pendingRequestCount() const
{
    QMutexLocker lock(&mutex);
    return requestQueue.size() + activeLoads.size();
}

QStringList ModelQueue::loadedModels() const
{
    QMutexLocker lock(&mutex);
    return loadedModels_.keys();
}

bool ModelQueue::isModelLoaded(const QString& path) const
{
    QMutexLocker lock(&mutex);
    return loadedModels_.contains(path);
}

QHash<QString, QVariant> ModelQueue::getModelMetadata(const QString& path)
{
    QMutexLocker lock(&mutex);
    if (loadedModels_.contains(path)) {
        return loadedModels_[path].metadata;
    }
    return QHash<QString, QVariant>();
}

void ModelQueue::setMaxConcurrentLoads(int count)
{
    QMutexLocker lock(&mutex);
    maxConcurrentLoads = std::max(1, count);
    qDebug() << "Max concurrent loads set to:" << maxConcurrentLoads;
    queueUpdated.wakeAll();
}

void ModelQueue::clearQueue()
{
    QMutexLocker lock(&mutex);
    requestQueue.clear();
    qDebug() << "Queue cleared";
    notifyQueueStatus();
}

void ModelQueue::start()
{
    QMutexLocker lock(&mutex);
    isRunning = true;
    qDebug() << "ModelQueue started";
    queueUpdated.wakeAll();
}

void ModelQueue::stop()
{
    QMutexLocker lock(&mutex);
    isRunning = false;
    qDebug() << "ModelQueue stopped";
}

int ModelQueue::findBestRequest()
{
    if (requestQueue.isEmpty()) return -1;
    
    // Find highest priority request
    int bestIdx = 0;
    int bestPriority = requestQueue[0].priority;
    
    for (int i = 1; i < requestQueue.size(); ++i) {
        if (requestQueue[i].priority > bestPriority) {
            bestIdx = i;
            bestPriority = requestQueue[i].priority;
        }
    }
    
    return bestIdx;
}

bool ModelQueue::tryLoad(const ModelLoadRequest& request)
{
    if (request.isUnload) {
        // Handle unload
        loadedModels_.remove(request.modelPath);
        qDebug() << "Model unloaded:" << request.modelPath;
        emit unloadCompleted(request.modelPath);
        return true;
    }
    
    // TODO: Integrate with actual InferenceEngine for model loading
    // This is a placeholder for the actual model loading logic
    
    qDebug() << "Loading model:" << request.modelPath;
    
    // Simulate loading (in real implementation, call InferenceEngine::loadModel)
    LoadedModelInfo info;
    info.path = request.modelPath;
    info.loadedAt = QDateTime::currentMSecsSinceEpoch();
    info.metadata["loaded"] = true;
    
    loadedModels_[request.modelPath] = info;
    
    if (request.callback) {
        request.callback(true, "Model loaded successfully");
    }
    
    emit loadCompleted(request.requestId, true, "Success");
    return true;
}

void ModelQueue::processQueue()
{
    while (isRunning) {
        QMutexLocker lock(&mutex);
        
        // Process requests if we have capacity
        while (activeLoads.size() < maxConcurrentLoads && !requestQueue.isEmpty()) {
            int bestIdx = findBestRequest();
            if (bestIdx < 0) break;
            
            // Extract best request
            ModelLoadRequest request = requestQueue.takeAt(bestIdx);
            
            // Try to load
            if (tryLoad(request)) {
                // Remove from active loads after completion (for sync loading)
                // In async version, we'd add to activeLoads
                notifyQueueStatus();
            }
        }
        
        // Wait for queue update
        if (requestQueue.isEmpty()) {
            queueUpdated.wait(&mutex, 100);
        }
    }
}

void ModelQueue::onLoadFinished(bool success, const QString& message)
{
    QMutexLocker lock(&mutex);
    if (!activeLoads.isEmpty()) {
        activeLoads.removeFirst();
        notifyQueueStatus();
        queueUpdated.wakeAll();
    }
}

void ModelQueue::notifyQueueStatus()
{
    int pending = requestQueue.size();
    int active = activeLoads.size();
    qDebug() << "Queue status - Pending:" << pending << "Active:" << active;
    emit queueStatusChanged(pending, active);
}
