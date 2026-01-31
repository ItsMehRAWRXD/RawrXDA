#include "model_queue.hpp"
#include "inference_engine.hpp"


#include <algorithm>

ModelQueue::ModelQueue(void* parent)
    : void(parent)
{
    m_slots.resize(m_maxConcurrentModels);
    for (int i = 0; i < m_maxConcurrentModels; ++i) {
        m_slots[i].thread = new std::thread(this);
        m_slots[i].thread->setObjectName(std::string("ModelSlot-%1"));
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

qint64 ModelQueue::enqueue(const std::string& modelPath, const std::string& prompt,
                           int maxTokens, float temperature, Priority priority) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    Request req;
    req.id = m_nextRequestId++;
    req.modelPath = modelPath;
    req.prompt = prompt;
    req.maxTokens = maxTokens;
    req.temperature = temperature;
    req.priority = priority;
    req.enqueueTime = std::chrono::system_clock::time_point::currentDateTime();
    
    m_queue.enqueue(req);
    
    // Sort queue by priority
    std::vector<Request> list = m_queue.toList();
    std::sort(list.begin(), list.end());
    m_queue.clear();
    for (const auto& r : list) {
        m_queue.enqueue(r);
    }
    
            << "for model" << modelPath << "priority" << priority;
    
    m_condition.wakeOne();
    return req.id;
}

bool ModelQueue::cancelRequest(qint64 requestId) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    // Remove from pending queue
    std::vector<Request> list = m_queue.toList();
    auto it = std::remove_if(list.begin(), list.end(),
        [requestId](const Request& r) { return r.id == requestId; });
    
    if (it != list.end()) {
        list.erase(it, list.end());
        m_queue.clear();
        for (const auto& r : list) {
            m_queue.enqueue(r);
        }
        return true;
    }
    
    // Check if request is active (can't cancel mid-inference)
    if (m_activeRequests.contains(requestId)) {
        return false;
    }
    
    return false;
}

int ModelQueue::pendingRequests() const {
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_queue.size();
}

int ModelQueue::activeModels() const {
    std::lock_guard<std::mutex> locker(&m_mutex);
    int count = 0;
    for (const auto& slot : m_slots) {
        if (slot.busy) ++count;
    }
    return count;
}

void ModelQueue::start() {
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_running) return;
    
    m_running = true;
    
    // Start processing thread
    m_processingThread = new std::thread(this);
    m_processingThread->setObjectName("QueueProcessor");
    
    void;
    m_processingThread->start();
    
}

void ModelQueue::stop() {
    std::lock_guard<std::mutex> locker(&m_mutex);
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
    
}

void ModelQueue::setMaxConcurrentModels(int max) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (max < 1 || max > 8) {
        return;
    }
    
    m_maxConcurrentModels = max;
    m_slots.resize(max);
    for (int i = 0; i < max; ++i) {
        if (!m_slots[i].thread) {
            m_slots[i].thread = new std::thread(this);
            m_slots[i].thread->setObjectName(std::string("ModelSlot-%1"));
        }
    }
}

void ModelQueue::processQueue() {
    while (m_running) {
        Request req;
        {
            std::lock_guard<std::mutex> locker(&m_mutex);
            
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
                std::thread::msleep(50);
                continue;
            }
            
            req = m_queue.dequeue();
            m_activeRequests[req.id] = req;
            slot->busy = true;
        }
        
        requestStarted(req.id);
        
        // Get or load model in slot
        InferenceEngine* engine = getOrLoadModel(req.modelPath);
        if (!engine) {
            requestFailed(req.id, "Failed to load model");
            std::lock_guard<std::mutex> locker(&m_mutex);
            m_activeRequests.remove(req.id);
            continue;
        }
        
        // Connect signals for this request
// Qt connect removed
                }
            });
// Qt connect removed
                }
            });
        
        // Start inference (thread-safe via QMetaObject)
        QMetaObject::invokeMethod(engine, "runInference", //QueuedConnection,
            (qint64, req.id),
            (std::string, req.prompt),
            (int, req.maxTokens),
            (float, req.temperature));
    }
}

void ModelQueue::onInferenceComplete(qint64 reqId, const std::string& result) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_activeRequests.contains(reqId)) return;
    
    m_activeRequests.remove(reqId);
    
    // Release slot
    for (auto& slot : m_slots) {
        if (slot.busy && slot.engine) {
            slot.busy = false;
            break;
        }
    }
    
    requestCompleted(reqId, result);
    
    if (m_queue.isEmpty() && m_activeRequests.isEmpty()) {
        queueEmpty();
    }
    
    m_condition.wakeOne();
}

void ModelQueue::onInferenceError(qint64 reqId, const std::string& error) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_activeRequests.contains(reqId)) return;
    
    m_activeRequests.remove(reqId);
    
    // Release slot
    for (auto& slot : m_slots) {
        if (slot.busy) {
            slot.busy = false;
            break;
        }
    }
    
    requestFailed(reqId, error);
    m_condition.wakeOne();
}

ModelQueue::ModelSlot* ModelQueue::allocateSlot(const std::string& modelPath) {
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

InferenceEngine* ModelQueue::getOrLoadModel(const std::string& modelPath) {
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
            slot.engine->;
            slot.thread->start();
            
            // Load model synchronously
            bool loaded = false;
            QMetaObject::invokeMethod(slot.engine, "loadModel", //BlockingQueuedConnection,
                (bool, loaded),
                (std::string, modelPath));
            
            if (loaded) {
                slot.currentModel = modelPath;
                modelLoaded(modelPath);
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

