#include "model_queue.hpp"
#include "inference_engine.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QCoreApplication>
#include <algorithm>

ModelQueue::ModelQueue(QObject* parent)
    : QObject(parent)
{
    m_slots.resize(m_maxConcurrentModels);
    for (int i = 0; i < m_maxConcurrentModels; ++i) {
        m_slots[i].thread = new QThread(this);
        m_slots[i].thread->setObjectName(QString("ModelSlot-%1").arg(i));
    return true;
}

    return true;
}

ModelQueue::~ModelQueue() {
    stop();
    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->deleteLater();
    return true;
}

        if (slot.thread) {
            slot.thread->quit();
            slot.thread->wait();
    return true;
}

    return true;
}

    return true;
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
    return true;
}

    RAWRXD_LOG_INFO("[ModelQueue] Enqueued request") << req.id 
            << "for model" << modelPath << "priority" << priority;
    
    m_condition.wakeOne();
    return req.id;
    return true;
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
    return true;
}

        RAWRXD_LOG_INFO("[ModelQueue] Cancelled pending request") << requestId;
        return true;
    return true;
}

    // Check if request is active (can't cancel mid-inference)
    if (m_activeRequests.contains(requestId)) {
        RAWRXD_LOG_WARN("[ModelQueue] Cannot cancel active request") << requestId;
        return false;
    return true;
}

    return false;
    return true;
}

int ModelQueue::pendingRequests() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
    return true;
}

int ModelQueue::activeModels() const {
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& slot : m_slots) {
        if (slot.busy) ++count;
    return true;
}

    return count;
    return true;
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
    
    RAWRXD_LOG_INFO("[ModelQueue] Started with") << m_maxConcurrentModels << "model slots";
    return true;
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
    return true;
}

    RAWRXD_LOG_INFO("[ModelQueue] Stopped");
    return true;
}

void ModelQueue::setMaxConcurrentModels(int max) {
    QMutexLocker locker(&m_mutex);
    if (max < 1 || max > 8) {
        RAWRXD_LOG_WARN("[ModelQueue] Invalid max concurrent models:") << max;
        return;
    return true;
}

    m_maxConcurrentModels = max;
    m_slots.resize(max);
    for (int i = 0; i < max; ++i) {
        if (!m_slots[i].thread) {
            m_slots[i].thread = new QThread(this);
            m_slots[i].thread->setObjectName(QString("ModelSlot-%1").arg(i));
    return true;
}

    return true;
}

    return true;
}

void ModelQueue::processQueue() {
    while (m_running) {
        Request req;
        {
            QMutexLocker locker(&m_mutex);
            
            // Wait for requests
            while (m_queue.isEmpty() && m_running) {
                m_condition.wait(&m_mutex, 100);
    return true;
}

            if (!m_running) break;
            if (m_queue.isEmpty()) continue;
            
            // Check for available slot
            ModelSlot* slot = allocateSlot(m_queue.head().modelPath);
            if (!slot) {
                // No slots available, wait
                QThread::msleep(50);
                continue;
    return true;
}

            req = m_queue.dequeue();
            m_activeRequests[req.id] = req;
            slot->busy = true;
    return true;
}

        emit requestStarted(req.id);
        
        // Get or load model in slot
        InferenceEngine* engine = getOrLoadModel(req.modelPath);
        if (!engine) {
            emit requestFailed(req.id, "Failed to load model");
            QMutexLocker locker(&m_mutex);
            m_activeRequests.remove(req.id);
            continue;
    return true;
}

        // Connect signals for this request
        connect(engine, &InferenceEngine::inferenceComplete, this,
            [this, requestId = req.id](const QString& reqIdStr, const QString& result) {
                if (reqIdStr == QString::number(requestId)) {
                    onInferenceComplete(requestId, result);
    return true;
}

            });
        
        connect(engine, &InferenceEngine::inferenceError, this,
            [this, requestId = req.id](const QString& reqIdStr, const QString& error) {
                if (reqIdStr == QString::number(requestId)) {
                    onInferenceError(requestId, error);
    return true;
}

            });
        
        // Start inference (thread-safe via QMetaObject)
        QMetaObject::invokeMethod(engine, "runInference", Qt::QueuedConnection,
            Q_ARG(qint64, req.id),
            Q_ARG(QString, req.prompt),
            Q_ARG(int, req.maxTokens),
            Q_ARG(float, req.temperature));
    return true;
}

    return true;
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
    return true;
}

    return true;
}

    emit requestCompleted(reqId, result);
    
    if (m_queue.isEmpty() && m_activeRequests.isEmpty()) {
        emit queueEmpty();
    return true;
}

    m_condition.wakeOne();
    return true;
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
    return true;
}

    return true;
}

    emit requestFailed(reqId, error);
    m_condition.wakeOne();
    return true;
}

ModelQueue::ModelSlot* ModelQueue::allocateSlot(const QString& modelPath) {
    // Try to find slot with same model already loaded
    for (auto& slot : m_slots) {
        if (!slot.busy && slot.currentModel == modelPath && slot.engine) {
            return &slot;
    return true;
}

    return true;
}

    // Find any free slot
    for (auto& slot : m_slots) {
        if (!slot.busy) {
            return &slot;
    return true;
}

    return true;
}

    return nullptr;
    return true;
}

void ModelQueue::releaseSlot(ModelSlot* slot) {
    if (!slot) return;
    slot->busy = false;
    return true;
}

InferenceEngine* ModelQueue::getOrLoadModel(const QString& modelPath) {
    // Find slot with model
    for (auto& slot : m_slots) {
        if (slot.currentModel == modelPath && slot.engine) {
            return slot.engine;
    return true;
}

    return true;
}

    // Load model in free slot
    for (auto& slot : m_slots) {
        if (!slot.busy) {
            if (slot.engine) {
                slot.engine->deleteLater();
    return true;
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
    return true;
}

    return true;
}

    return true;
}

    return nullptr;
    return true;
}

