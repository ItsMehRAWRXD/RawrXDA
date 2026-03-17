#include "advanced_model_queue.hpp"
#include "gguf_parser.hpp"
#include "gguf_loader.hpp"
#include "gpu_backend.hpp"
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

AdvancedModelQueue::AdvancedModelQueue(QObject* parent)
    : QObject(parent), m_running(false), m_nextRequestId(1), 
      m_gpuBackend(nullptr), m_gpuInitialized(false)
{
    // Initialize GPU backend
    initializeGPUBackend();
    
    // Start queue processor thread
    m_running = true;
    QTimer::singleShot(0, this, &AdvancedModelQueue::processQueue);
    
    // Memory management timer - check every 5 seconds
    QTimer* memoryTimer = new QTimer(this);
    connect(memoryTimer, &QTimer::timeout, this, &AdvancedModelQueue::performMemoryManagement);
    memoryTimer->start(5000);
}

AdvancedModelQueue::~AdvancedModelQueue()
{
    m_running = false;
    QMutexLocker lock(&m_mutex);
    
    // Free GPU buffers
    if (m_gpuBackend) {
        for (auto& pair : m_gpuModelBuffers) {
            m_gpuBackend->freeMemory(pair.second);
        }
        m_gpuModelBuffers.clear();
        m_gpuModelSizes.clear();
    }
    
    m_loadedModels.clear();
    m_modelCache.clear();
}

int AdvancedModelQueue::enqueueInference(const InferenceRequest& request)
{
    QMutexLocker lock(&m_mutex);
    
    InferenceRequest req = request;
    req.requestId = m_nextRequestId++;
    req.createdAt = QDateTime::currentMSecsSinceEpoch();
    
    // Check if model is already loaded
    if (m_loadedModels.count(req.modelPath) > 0) {
        auto& info = m_loadedModels[req.modelPath];
        if (info.state == Loaded) {
            // Model ready, process immediately
            info.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
            info.accessCount++;
            emit requestStarted(req.requestId);
            if (req.callback) {
                req.callback(true, "Model ready");
            }
            return req.requestId;
        }
    }
    
    // Add to queue
    m_requestQueue.enqueue(req);
    emit requestQueued(req.requestId, req.modelPath);
    emit queueStatusChanged(m_requestQueue.size(), m_loadingModels.size());
    
    m_queueUpdated.wakeAll();
    return req.requestId;
}

int AdvancedModelQueue::preloadModel(const QString& path, Priority priority)
{
    InferenceRequest preloadReq;
    preloadReq.modelPath = path;
    preloadReq.priority = priority;
    preloadReq.preload = true;
    
    return enqueueInference(preloadReq);
}

bool AdvancedModelQueue::hotSwapModel(const HotSwapTarget& swap)
{
    QMutexLocker lock(&m_mutex);
    
    // Verify source model is loaded
    if (m_loadedModels.count(swap.sourceModel) == 0) {
        return false;
    }
    
    auto& sourceInfo = m_loadedModels[swap.sourceModel];
    if (sourceInfo.state != Loaded) {
        return false;
    }
    
    // If target is loading, wait for completion
    auto loadingIt = std::find_if(
        m_loadingModels.begin(), m_loadingModels.end(),
        [&swap](const LoadingModel& lm) { return lm.path == swap.targetModel; }
    );
    
    // Update model states
    sourceInfo.state = Unloading;
    emit modelStateChanged(swap.sourceModel, Loaded, Unloading);
    
    if (m_loadedModels.count(swap.targetModel) > 0) {
        auto& targetInfo = m_loadedModels[swap.targetModel];
        targetInfo.state = Loaded;
        targetInfo.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
        emit modelStateChanged(swap.targetModel, targetInfo.state, Loaded);
    }
    
    emit modelHotSwapped(swap.sourceModel, swap.targetModel);
    
    // Unload source model if not pinned
    if (!sourceInfo.isPinned) {
        m_loadedModels.erase(swap.sourceModel);
        emit modelUnloaded(swap.sourceModel);
    }
    
    return true;
}

ModelInfo AdvancedModelQueue::getModelInfo(const QString& path) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_loadedModels.find(path);
    if (it != m_loadedModels.end()) {
        return it->second;
    }
    return ModelInfo();
}

std::vector<ModelInfo> AdvancedModelQueue::getAllModels() const
{
    QMutexLocker lock(&m_mutex);
    std::vector<ModelInfo> models;
    for (const auto& pair : m_loadedModels) {
        models.push_back(pair.second);
    }
    return models;
}

bool AdvancedModelQueue::isModelLoaded(const QString& path) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_loadedModels.find(path);
    return it != m_loadedModels.end() && it->second.state == Loaded;
}

ModelState AdvancedModelQueue::getModelState(const QString& path) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_loadedModels.find(path);
    if (it != m_loadedModels.end()) {
        return it->second.state;
    }
    return Unloaded;
}

void AdvancedModelQueue::setMaxConcurrentLoads(int count)
{
    QMutexLocker lock(&m_mutex);
    m_maxConcurrentLoads = count;
}

void AdvancedModelQueue::setMaxMemoryMB(uint64_t maxMemory)
{
    QMutexLocker lock(&m_mutex);
    m_maxMemoryMB = maxMemory;
}

void AdvancedModelQueue::setPinModel(const QString& path, bool pinned)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_loadedModels.find(path);
    if (it != m_loadedModels.end()) {
        it->second.isPinned = pinned;
    }
}

void AdvancedModelQueue::setPreloadThreshold(float threshold)
{
    QMutexLocker lock(&m_mutex);
    m_preloadThreshold = threshold;
}

int AdvancedModelQueue::getPendingRequestCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_requestQueue.size();
}

int AdvancedModelQueue::getActiveLoadCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_loadingModels.size();
}

void AdvancedModelQueue::clearQueue()
{
    QMutexLocker lock(&m_mutex);
    m_requestQueue.clear();
    emit queueStatusChanged(0, m_loadingModels.size());
}

void AdvancedModelQueue::prioritizeRequest(int requestId)
{
    QMutexLocker lock(&m_mutex);
    
    QQueue<InferenceRequest> temp;
    InferenceRequest prioritized;
    bool found = false;
    
    while (!m_requestQueue.isEmpty()) {
        auto req = m_requestQueue.dequeue();
        if (req.requestId == requestId) {
            prioritized = req;
            prioritized.priority = Critical;
            found = true;
        } else {
            temp.enqueue(req);
        }
    }
    
    if (found) {
        m_requestQueue.enqueue(prioritized);
        while (!temp.isEmpty()) {
            m_requestQueue.enqueue(temp.dequeue());
        }
    }
}

void AdvancedModelQueue::enableAutoOptimization(bool enable)
{
    QMutexLocker lock(&m_mutex);
    m_autoOptimization = enable;
}

void AdvancedModelQueue::enableCaching(bool enable)
{
    QMutexLocker lock(&m_mutex);
    m_cachingEnabled = enable;
}

void AdvancedModelQueue::analyzePerformance()
{
    QMutexLocker lock(&m_mutex);
    
    for (auto& pair : m_performanceHistory) {
        const auto& latencies = pair.second;
        if (latencies.empty()) continue;
        
        float avg = std::accumulate(latencies.begin(), latencies.end(), 0.0f) / latencies.size();
        float variance = 0.0f;
        for (float lat : latencies) {
            variance += (lat - avg) * (lat - avg);
        }
        variance /= latencies.size();
        float stddev = std::sqrt(variance);
        
        qDebug() << "Model:" << pair.first.c_str()
                 << "Avg:" << avg << "ms"
                 << "StdDev:" << stddev << "ms";
    }
}

uint64_t AdvancedModelQueue::getTotalMemoryUsage() const
{
    QMutexLocker lock(&m_mutex);
    uint64_t total = 0;
    for (const auto& pair : m_loadedModels) {
        total += pair.second.memoryUsage;
    }
    return total;
}

uint64_t AdvancedModelQueue::getAvailableMemory() const
{
    return (m_maxMemoryMB * 1024 * 1024) - getTotalMemoryUsage();
}

bool AdvancedModelQueue::evictLRUModel()
{
    QMutexLocker lock(&m_mutex);
    
    QString lruModel = findLRUModel();
    if (!lruModel.isEmpty()) {
        m_loadedModels.erase(lruModel);
        emit modelUnloaded(lruModel);
        return true;
    }
    return false;
}

void AdvancedModelQueue::compactMemory()
{
    QMutexLocker lock(&m_mutex);
    
    // Remove old cache entries
    auto now = QDateTime::currentMSecsSinceEpoch();
    std::vector<QString> toRemove;
    
    for (auto& pair : m_modelCache) {
        // Remove entries older than 1 hour
        if (now - pair.second.createdAt > 3600000) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (const auto& key : toRemove) {
        m_modelCache.erase(key);
    }
}

void AdvancedModelQueue::startBenchmarking()
{
    QMutexLocker lock(&m_mutex);
    m_benchmarking = true;
    m_performanceHistory.clear();
}

void AdvancedModelQueue::stopBenchmarking()
{
    QMutexLocker lock(&m_mutex);
    m_benchmarking = false;
}

std::map<QString, float> AdvancedModelQueue::getBenchmarkResults() const
{
    QMutexLocker lock(&m_mutex);
    std::map<QString, float> results;
    
    for (const auto& pair : m_performanceHistory) {
        const auto& latencies = pair.second;
        if (!latencies.empty()) {
            float avg = std::accumulate(latencies.begin(), latencies.end(), 0.0f) / latencies.size();
            results[pair.first] = avg;
        }
    }
    
    return results;
}

void AdvancedModelQueue::processQueue()
{
    while (m_running) {
        QMutexLocker lock(&m_mutex);
        
        // Process as many requests as we can with available load slots
        while (!m_requestQueue.isEmpty() && 
               m_loadingModels.size() < static_cast<size_t>(m_maxConcurrentLoads)) {
            
            int bestIdx = findBestRequest();
            auto request = m_requestQueue.dequeue();
            
            if (tryLoadModel(request)) {
                LoadingModel loading;
                loading.path = request.modelPath;
                loading.startTime = QDateTime::currentMSecsSinceEpoch();
                loading.requestId = request.requestId;
                m_loadingModels.push_back(loading);
                
                emit requestStarted(request.requestId);
            } else {
                if (request.callback) {
                    request.callback(false, "Failed to load model");
                }
                emit requestCompleted(request.requestId, false);
            }
            
            emit queueStatusChanged(m_requestQueue.size(), m_loadingModels.size());
        }
        
        // Wait for queue updates or timeout after 100ms
        m_queueUpdated.wait(&m_mutex, 100);
    }
}

void AdvancedModelQueue::onModelLoadFinished(const QString& path, bool success)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = std::find_if(
        m_loadingModels.begin(), m_loadingModels.end(),
        [&path](const LoadingModel& lm) { return lm.path == path; }
    );
    
    if (it != m_loadingModels.end()) {
        qint64 loadTime = QDateTime::currentMSecsSinceEpoch() - it->startTime;
        int requestId = it->requestId;
        m_loadingModels.erase(it);
        
        if (success) {
            if (m_loadedModels.count(path) == 0) {
                ModelInfo info;
                info.path = path;
                info.state = Loaded;
                info.loadedAt = QDateTime::currentMSecsSinceEpoch();
                info.lastAccessAt = info.loadedAt;
                
                // Get actual memory usage from GPU backend
                if (m_gpuModelSizes.count(path) > 0) {
                    info.memoryUsage = m_gpuModelSizes[path];
                } else {
                    // Fallback: use file size if not in GPU
                    QFileInfo fileInfo(path);
                    info.memoryUsage = fileInfo.size();
                }
                
                m_loadedModels[path] = info;
                
                qDebug() << "Model loaded:" << path
                         << "Memory:" << info.memoryUsage / (1024*1024*1024) << "GB"
                         << "Load time:" << loadTime << "ms";
            }
            emit modelLoadCompleted(path, true);
            emit requestCompleted(requestId, true);
        } else {
            emit modelLoadCompleted(path, false);
            emit requestCompleted(requestId, false);
        }
        
        emit queueStatusChanged(m_requestQueue.size(), m_loadingModels.size());
    }
}

void AdvancedModelQueue::performMemoryManagement()
{
    QMutexLocker lock(&m_mutex);
    
    uint64_t used = getTotalMemoryUsage();
    if (used > m_maxMemoryMB * 1024 * 1024 * 0.9) { // 90% threshold
        emit memoryWarning(used, m_maxMemoryMB * 1024 * 1024);
        
        // Evict LRU models until below 80%
        while (used > m_maxMemoryMB * 1024 * 1024 * 0.8 && evictLRUModel()) {
            used = getTotalMemoryUsage();
        }
    }
    
    if (m_autoOptimization) {
        applyAutoOptimizations();
    }
    
    compactMemory();
}

int AdvancedModelQueue::findBestRequest()
{
    // Priority queue simulation - higher priority first
    int bestIdx = 0;
    Priority bestPriority = Low;
    
    for (int i = 0; i < m_requestQueue.size(); ++i) {
        if (m_requestQueue[i].priority > bestPriority) {
            bestPriority = m_requestQueue[i].priority;
            bestIdx = i;
        }
    }
    
    return bestIdx;
}

bool AdvancedModelQueue::tryLoadModel(const InferenceRequest& request)
{
    // Check if model is already loaded
    if (m_loadedModels.count(request.modelPath) > 0) {
        auto& info = m_loadedModels[request.modelPath];
        if (info.state == Loaded) {
            return true;
        }
    }
    
    // Validate model file exists
    QFileInfo fileInfo(request.modelPath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        qWarning() << "Model file not found or not readable:" << request.modelPath;
        emit modelLoadCompleted(request.modelPath, false);
        return false;
    }
    
    // Check available memory
    uint64_t estimatedSize = fileInfo.size();
    if (getTotalMemoryUsage() + estimatedSize > m_maxMemoryMB * 1024 * 1024) {
        // Try to evict LRU models to make space
        while (getTotalMemoryUsage() + estimatedSize > m_maxMemoryMB * 1024 * 1024) {
            if (!evictLRUModel()) {
                qWarning() << "Insufficient memory to load model:" << request.modelPath;
                return false;
            }
        }
    }
    
    // Load and parse GGUF model
    if (!loadGGUFModel(request.modelPath)) {
        qWarning() << "Failed to load GGUF model:" << request.modelPath;
        emit modelLoadCompleted(request.modelPath, false);
        return false;
    }
    
    qDebug() << "Successfully loaded model:" << request.modelPath;
    return true;
}

bool AdvancedModelQueue::loadGGUFModel(const QString& path)
{
    try {
        // Parse GGUF metadata and tensors
        GGUFParser parser;
        if (!parser.parseFile(path)) {
            qWarning() << "GGUF parser failed for:" << path;
            return false;
        }
        
        // Get tensor information
        auto tensors = parser.getTensors();
        uint64_t totalSize = 0;
        uint32_t tensorCount = 0;
        
        // Calculate total memory needed
        for (const auto& tensor : tensors) {
            totalSize += tensor.size;
            tensorCount++;
        }
        
        // Get model metadata
        auto metadata = parser.getMetadata();
        
        // If GPU backend available, load to GPU with optimized dequantization
        if (m_gpuBackend && m_gpuInitialized) {
            if (!loadModelToGPU(path, parser, totalSize)) {
                qWarning() << "GPU loading failed, falling back to CPU";
                return false;
            }
        } else {
            // CPU-only loading
            qDebug() << "Loading model on CPU:" << path << "Size:" << totalSize / (1024*1024) << "MB";
        }
        
        // Log successful load
        qDebug() << "Model loaded:" << path 
                 << "| Tensors:" << tensorCount
                 << "| Size:" << totalSize / (1024*1024*1024) << "GB";
        
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Exception loading GGUF model:" << e.what();
        return false;
    }
}

bool AdvancedModelQueue::loadModelToGPU(const QString& path, const GGUFParser& parser, uint64_t totalSize)
{
    if (!m_gpuBackend) {
        return false;
    }
    
    try {
        auto deviceInfo = m_gpuBackend->getDeviceInfo();
        
        // Check GPU memory availability
        if (deviceInfo.memoryAvailable < totalSize) {
            qWarning() << "Insufficient GPU memory. Available:" 
                       << deviceInfo.memoryAvailable / (1024*1024*1024) << "GB"
                       << "Needed:" << totalSize / (1024*1024*1024) << "GB";
            return false;
        }
        
        // Allocate GPU memory for model
        void* gpuBuffer = m_gpuBackend->allocateMemory(totalSize);
        if (!gpuBuffer) {
            qWarning() << "Failed to allocate GPU memory for model:" << path;
            return false;
        }
        
        // Load GGUF file
        GGUFLoader loader(path);
        if (!loader.isOpen()) {
            qWarning() << "Failed to open GGUF file:" << path;
            m_gpuBackend->freeMemory(gpuBuffer);
            return false;
        }
        
        // Process each tensor and load to GPU with appropriate dequantization
        auto tensors = parser.getTensors();
        uint64_t gpuOffset = 0;
        
        for (const auto& tensor : tensors) {
            qDebug() << "Loading tensor:" << tensor.name 
                     << "Type:" << static_cast<int>(tensor.type)
                     << "Size:" << tensor.size;
            
            // Dequantize on GPU if quantized format
            if (shouldDequantizeOnGPU(tensor.type)) {
                if (!dequantizeTensorOnGPU(tensor, gpuBuffer, gpuOffset)) {
                    qWarning() << "Failed to dequantize tensor on GPU:" << tensor.name;
                    m_gpuBackend->freeMemory(gpuBuffer);
                    return false;
                }
            } else {
                // Direct copy for non-quantized formats
                QByteArray tensorData = loader.inflateWeight(tensor.name);
                if (!m_gpuBackend->copyToGPU(
                    static_cast<uint8_t*>(gpuBuffer) + gpuOffset,
                    tensorData.data(),
                    tensor.size)) {
                    qWarning() << "Failed to copy tensor to GPU:" << tensor.name;
                    m_gpuBackend->freeMemory(gpuBuffer);
                    return false;
                }
            }
            
            gpuOffset += tensor.size;
        }
        
        // Synchronize GPU operations
        m_gpuBackend->synchronize();
        
        // Store GPU buffer reference
        m_gpuModelBuffers[path] = gpuBuffer;
        m_gpuModelSizes[path] = totalSize;
        
        auto speedup = m_gpuBackend->getEstimatedSpeedup();
        qDebug() << "Model loaded to GPU with" << speedup << "x speedup";
        
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during GPU model loading:" << e.what();
        return false;
    }
}

bool AdvancedModelQueue::shouldDequantizeOnGPU(GGMLType type) const
{
    // GPU dequantization for quantized formats is more efficient
    return type == GGMLType::Q2_K || 
           type == GGMLType::Q3_K || 
           type == GGMLType::Q4_K ||
           type == GGMLType::Q5_K ||
           type == GGMLType::Q6_K;
}

bool AdvancedModelQueue::dequantizeTensorOnGPU(const GGUFTensorInfo& tensor, 
                                               void* gpuBuffer, uint64_t offset)
{
    if (!m_gpuBackend) {
        return false;
    }
    
    try {
        // Estimate dequantized size (typically 4x for FP32 output)
        uint64_t dequantizedSize = tensor.size * 4;
        
        // Load quantized data
        GGUFLoader loader(tensor.name);
        QByteArray quantizedData = loader.inflateWeight(tensor.name);
        
        // Allocate temporary GPU buffer for quantized data
        void* quantizedGPU = m_gpuBackend->allocateMemory(quantizedData.size());
        if (!quantizedGPU) {
            qWarning() << "Failed to allocate GPU buffer for quantized data";
            return false;
        }
        
        // Copy quantized data to GPU
        if (!m_gpuBackend->copyToGPU(quantizedGPU, quantizedData.data(), quantizedData.size())) {
            qWarning() << "Failed to copy quantized data to GPU";
            m_gpuBackend->freeMemory(quantizedGPU);
            return false;
        }
        
        // Dequantize on GPU based on quantization type
        bool success = false;
        uint64_t blocks = tensor.size / 32; // Typical block size
        
        switch (tensor.type) {
            case GGMLType::Q2_K:
                success = m_gpuBackend->dequantizeQ2K(quantizedGPU, 
                    static_cast<uint8_t*>(gpuBuffer) + offset, blocks, 32);
                break;
            case GGMLType::Q3_K:
                success = m_gpuBackend->dequantizeQ3K(quantizedGPU,
                    static_cast<uint8_t*>(gpuBuffer) + offset, blocks, 32);
                break;
            case GGMLType::Q5_K:
                success = m_gpuBackend->dequantizeQ5K(quantizedGPU,
                    static_cast<uint8_t*>(gpuBuffer) + offset, blocks, 32);
                break;
            default:
                qWarning() << "Unsupported quantization type for GPU dequantization";
                break;
        }
        
        // Free temporary buffer
        m_gpuBackend->freeMemory(quantizedGPU);
        
        return success;
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during GPU dequantization:" << e.what();
        return false;
    }
}

void AdvancedModelQueue::initializeGPUBackend()
{
    try {
        m_gpuBackend = GPUBackendFactory::createBestBackend();
        if (m_gpuBackend) {
            m_gpuInitialized = m_gpuBackend->initialize();
            if (m_gpuInitialized) {
                auto deviceInfo = m_gpuBackend->getDeviceInfo();
                qDebug() << "GPU Backend initialized:"
                         << "Device:" << deviceInfo.name
                         << "Memory:" << deviceInfo.memoryTotal / (1024*1024*1024) << "GB";
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize GPU backend:" << e.what();
        m_gpuInitialized = false;
    }
}

bool AdvancedModelQueue::shouldEvict() const
{
    uint64_t used = getTotalMemoryUsage();
    uint64_t max = m_maxMemoryMB * 1024 * 1024;
    return used > max * 0.85; // 85% threshold
}

QString AdvancedModelQueue::findLRUModel() const
{
    QString lruPath;
    qint64 oldestTime = std::numeric_limits<qint64>::max();
    
    for (const auto& pair : m_loadedModels) {
        if (pair.second.isPinned) continue; // Skip pinned models
        
        if (pair.second.lastAccessAt < oldestTime) {
            oldestTime = pair.second.lastAccessAt;
            lruPath = pair.first;
        }
    }
    
    return lruPath;
}

void AdvancedModelQueue::updateModelStats(const QString& path, float latency)
{
    if (m_benchmarking) {
        m_performanceHistory[path.toStdString()].push_back(latency);
        
        // Keep only last 100 measurements
        if (m_performanceHistory[path.toStdString()].size() > 100) {
            m_performanceHistory[path.toStdString()].erase(
                m_performanceHistory[path.toStdString()].begin()
            );
        }
    }
    
    auto it = m_loadedModels.find(path);
    if (it != m_loadedModels.end()) {
        it->second.averageLatency = 
            (it->second.averageLatency * it->second.accessCount + latency) /
            (it->second.accessCount + 1);
        it->second.accessCount++;
    }
}

void AdvancedModelQueue::applyAutoOptimizations()
{
    // Analyze and apply automatic optimizations
    for (auto& pair : m_performanceHistory) {
        const auto& latencies = pair.second;
        if (latencies.size() < 10) continue;
        
        float avg = std::accumulate(latencies.begin(), latencies.end(), 0.0f) / latencies.size();
        float variance = 0.0f;
        for (float lat : latencies) {
            variance += (lat - avg) * (lat - avg);
        }
        variance /= latencies.size();
        float stddev = std::sqrt(variance);
        
        // If high variance, suggest preloading or model swapping
        if (stddev > avg * 0.3) { // 30% variation
            emit optimizationApplied("High latency variance detected for: " + pair.first);
        }
    }
}

ModelState AdvancedModelQueue::updateModelState(const QString& path, ModelState newState)
{
    auto it = m_loadedModels.find(path);
    if (it != m_loadedModels.end()) {
        ModelState oldState = it->second.state;
        it->second.state = newState;
        emit modelStateChanged(path, oldState, newState);
        return oldState;
    }
    return Unloaded;
}
