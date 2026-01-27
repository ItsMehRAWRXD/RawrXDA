#include "model_queue.hpp"
#include "inference_engine.hpp"
#include <QDebug>
#include <QCoreApplication>
#include <algorithm>

ModelQueue::ModelQueue(QObject* parent)
    : QObject(parent)
{
    m_slots.resize(m_maxConcurrentModels);
    for (int i = 0; i < m_maxConcurrentModels; ++i) {
        m_slots[i].thread = new QThread(this);
        m_slots[i].thread->setObjectName(QString("ModelSlot-%1").arg(i));
    }
}

ModelQueue::~ModelQueue() {
    stop();
    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->deleteLater();
        }
        if (slot.thread) {
            slot.thread->quit();
            slot.thread->wait();
        }
    }
}

qint64 ModelQueue::enqueue(const QString& modelPath, const QString& prompt,
                           int maxTokens, float temperature, Priority priority) {
    QMutexLocker locker(&m_mutex);
    
    Request req;
    req.id = m_nextRequestId++;
    req.modelPath = modelPath;
    req.prompt = prompt;
    req.maxTokens = maxTokens;
    req.temperature = temperature;
    req.priority = priority;
    req.enqueueTime = QDateTime::currentDateTime();
    
    m_queue.enqueue(req);
    
    // Sort queue by priority
    QList<Request> list = m_queue.toList();
    std::sort(list.begin(), list.end());
    m_queue.clear();
    for (const auto& r : list) {
        m_queue.enqueue(r);
    }
    
    qInfo() << "[ModelQueue] Enqueued request" << req.id 
            << "for model" << modelPath << "priority" << priority;
    
    m_condition.wakeOne();
    return req.id;
}

bool ModelQueue::cancelRequest(qint64 requestId) {
    QMutexLocker locker(&m_mutex);
    
    // Remove from pending queue
    QList<Request> list = m_queue.toList();
    auto it = std::remove_if(list.begin(), list.end(),
        [requestId](const Request& r) { return r.id == requestId; });
    
    if (it != list.end()) {
        list.erase(it, list.end());
        m_queue.clear();
        for (const auto& r : list) {
            m_queue.enqueue(r);
        }
        qInfo() << "[ModelQueue] Cancelled pending request" << requestId;
        return true;
    }
    
    // Check if request is active (can't cancel mid-inference)
    if (m_activeRequests.contains(requestId)) {
        qWarning() << "[ModelQueue] Cannot cancel active request" << requestId;
        return false;
    }
    
    return false;
}

int ModelQueue::pendingRequests() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

int ModelQueue::activeModels() const {
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& slot : m_slots) {
        if (slot.busy) ++count;
    }
    return count;
}

void ModelQueue::start() {
    QMutexLocker locker(&m_mutex);
    if (m_running) return;
    
    m_running = true;
    
    // Start processing thread
    m_processingThread = new QThread(this);
    m_processingThread->setObjectName("QueueProcessor");
    
    QObject::connect(m_processingThread, &QThread::started, this, &ModelQueue::processQueue);
    m_processingThread->start();
    
    qInfo() << "[ModelQueue] Started with" << m_maxConcurrentModels << "model slots";
}

void ModelQueue::stop() {
    QMutexLocker locker(&m_mutex);
    if (!m_running) return;
    
    m_running = false;
    m_queue.clear();
    m_activeRequests.clear();
    m_condition.wakeAll();
    
    if (m_processingThread) {
        m_processingThread->quit();
        m_processingThread->wait();
        m_processingThread->deleteLater();
        m_processingThread = nullptr;
    }
    
    qInfo() << "[ModelQueue] Stopped";
}

void ModelQueue::setMaxConcurrentModels(int max) {
    QMutexLocker locker(&m_mutex);
    if (max < 1 || max > 8) {
        qWarning() << "[ModelQueue] Invalid max concurrent models:" << max;
        return;
    }
    
    m_maxConcurrentModels = max;
    m_slots.resize(max);
    for (int i = 0; i < max; ++i) {
        if (!m_slots[i].thread) {
            m_slots[i].thread = new QThread(this);
            m_slots[i].thread->setObjectName(QString("ModelSlot-%1").arg(i));
        }
    }
}

void ModelQueue::processQueue() {
    while (m_running) {
        Request req;
        {
            QMutexLocker locker(&m_mutex);
            
            // Wait for requests
            while (m_queue.isEmpty() && m_running) {
                m_condition.wait(&m_mutex, 100);
            }
            
            if (!m_running) break;
            if (m_queue.isEmpty()) continue;
            
            // Check for available slot
            ModelSlot* slot = allocateSlot(m_queue.head().modelPath);
            if (!slot) {
                // No slots available, wait
                QThread::msleep(50);
                continue;
            }
            
            req = m_queue.dequeue();
            m_activeRequests[req.id] = req;
            slot->busy = true;
        }
        
        emit requestStarted(req.id);
        
        // Get or load model in slot
        InferenceEngine* engine = getOrLoadModel(req.modelPath);
        if (!engine) {
            emit requestFailed(req.id, "Failed to load model");
            QMutexLocker locker(&m_mutex);
            m_activeRequests.remove(req.id);
            continue;
        }
        
        // Connect signals for this request
        connect(engine, &InferenceEngine::inferenceComplete, this,
            [this, requestId = req.id](const QString& reqIdStr, const QString& result) {
                if (reqIdStr == QString::number(requestId)) {
                    onInferenceComplete(requestId, result);
                }
            });
        
        connect(engine, &InferenceEngine::inferenceError, this,
            [this, requestId = req.id](const QString& reqIdStr, const QString& error) {
                if (reqIdStr == QString::number(requestId)) {
                    onInferenceError(requestId, error);
                }
            });
        
        // Start inference (thread-safe via QMetaObject)
        QMetaObject::invokeMethod(engine, "runInference", Qt::QueuedConnection,
            Q_ARG(qint64, req.id),
            Q_ARG(QString, req.prompt),
            Q_ARG(int, req.maxTokens),
            Q_ARG(float, req.temperature));
    }
}

void ModelQueue::onInferenceComplete(qint64 reqId, const QString& result) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeRequests.contains(reqId)) return;
    
    m_activeRequests.remove(reqId);
    
    // Release slot
    for (auto& slot : m_slots) {
        if (slot.busy && slot.engine) {
            slot.busy = false;
            break;
        }
    }
    
    emit requestCompleted(reqId, result);
    
    if (m_queue.isEmpty() && m_activeRequests.isEmpty()) {
        emit queueEmpty();
    }
    
    m_condition.wakeOne();
}

void ModelQueue::onInferenceError(qint64 reqId, const QString& error) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeRequests.contains(reqId)) return;
    
    m_activeRequests.remove(reqId);
    
    // Release slot
    for (auto& slot : m_slots) {
        if (slot.busy) {
            slot.busy = false;
            break;
        }
    }
    
    emit requestFailed(reqId, error);
    m_condition.wakeOne();
}

ModelQueue::ModelSlot* ModelQueue::allocateSlot(const QString& modelPath) {
    // Try to find slot with same model already loaded
    for (auto& slot : m_slots) {
        if (!slot.busy && slot.currentModel == modelPath && slot.engine) {
            return &slot;
        }
    }
    
    // Find any free slot
    for (auto& slot : m_slots) {
        if (!slot.busy) {
            return &slot;
        }
    }
    
    return nullptr;
}

void ModelQueue::releaseSlot(ModelSlot* slot) {
    if (!slot) return;
    slot->busy = false;
}

InferenceEngine* ModelQueue::getOrLoadModel(const QString& modelPath) {
    // Find slot with model
    for (auto& slot : m_slots) {
        if (slot.currentModel == modelPath && slot.engine) {
            return slot.engine;
        }
    }
    
    // Load model in free slot
    for (auto& slot : m_slots) {
        if (!slot.busy) {
            if (slot.engine) {
                slot.engine->deleteLater();
            }
            
            slot.engine = new InferenceEngine(modelPath);
            slot.engine->moveToThread(slot.thread);
            slot.thread->start();
            
            // Load model synchronously
            bool loaded = false;
            QMetaObject::invokeMethod(slot.engine, "loadModel", Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(bool, loaded),
                Q_ARG(QString, modelPath));
            
            if (loaded) {
                slot.currentModel = modelPath;
                emit modelLoaded(modelPath);
                return slot.engine;
            } else {
                slot.engine->deleteLater();
                slot.engine = nullptr;
                return nullptr;
            }
        }
    }
    
    return nullptr;
}
