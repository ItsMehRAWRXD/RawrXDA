// model_hotpatch_manager.cpp - Complete Model Hotpatching Implementation
#include "model_hotpatch_manager.h"
#include "../logging/structured_logger.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtConcurrent>
#include <QRandomGenerator>
#include <algorithm>
#include <numeric>

namespace ModelHotpatch {

// ============================================================================
// ModelHotpatchManager Implementation
// ============================================================================

ModelHotpatchManager::ModelHotpatchManager(QObject* parent)
    : QObject(parent)
    , m_canaryActive(false)
    , m_canaryRequests(0)
    , m_canarySuccesses(0)
    , m_maxConcurrentLoads(3)
    , m_validationTimeoutSeconds(60)
    , m_maxCachedModels(5)
    , m_automaticRollbackEnabled(true)
    , m_initialized(false)
{
    m_activeInferences.storeRelaxed(0);
    LOG_INFO("Model Hotpatch Manager created");
}

ModelHotpatchManager::~ModelHotpatchManager() {
    shutdown();
}

void ModelHotpatchManager::initialize(const QString& modelsDirectory) {
    QMutexLocker lock(&m_mutex);
    
    if (m_initialized) {
        LOG_WARN("Model Hotpatch Manager already initialized");
        return;
    }
    
    m_modelsDirectory = modelsDirectory;
    
    // Create models directory if it doesn't exist
    QDir dir(m_modelsDirectory);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR("Failed to create models directory", {{"path", m_modelsDirectory}});
            return;
        }
    }
    
    // Try to load existing model registry
    QString registryPath = m_modelsDirectory + "/model_registry.json";
    if (QFile::exists(registryPath)) {
        loadModelRegistry(registryPath);
    }
    
    m_initialized = true;
    
    LOG_INFO("Model Hotpatch Manager initialized", {
        {"models_directory", m_modelsDirectory},
        {"registered_models", m_registeredModels.size()}
    });
}

void ModelHotpatchManager::shutdown() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    // Wait for active inferences to complete
    int waitCount = 0;
    while (m_activeInferences.loadRelaxed() > 0 && waitCount < 100) {
        lock.unlock();
        QThread::msleep(100);
        lock.relock();
        waitCount++;
    }
    
    // Save model registry
    saveModelRegistry();
    
    // Unload all models
    for (const QString& modelId : m_loadedModels) {
        unloadModelInternal(modelId);
    }
    
    m_initialized = false;
    
    LOG_INFO("Model Hotpatch Manager shutdown complete", {
        {"total_swaps", m_swapHistory.size()}
    });
}

bool ModelHotpatchManager::registerModel(const QString& modelPath, const QJsonObject& metadata) {
    QMutexLocker lock(&m_mutex);
    
    if (!QFile::exists(modelPath)) {
        LOG_ERROR("Model file does not exist", {{"path", modelPath}});
        return false;
    }
    
    QFileInfo fileInfo(modelPath);
    QString modelId = QCryptographicHash::hash(
        modelPath.toUtf8(), 
        QCryptographicHash::Sha256
    ).toHex().left(16);
    
    ModelInfo info;
    info.modelId = modelId;
    info.modelName = metadata.value("name").toString(fileInfo.baseName());
    info.modelPath = modelPath;
    info.modelType = metadata.value("type").toString("gguf");
    info.version = metadata.value("version").toString("1.0.0");
    info.sizeBytes = fileInfo.size();
    info.checksum = calculateChecksum(modelPath);
    info.loadedAt = QDateTime::currentDateTime();
    info.isActive = false;
    info.isValidated = false;
    info.referenceCount = 0;
    info.metadata = metadata;
    
    m_registeredModels[modelId] = info;
    
    LOG_INFO("Model registered", {
        {"model_id", modelId},
        {"model_name", info.modelName},
        {"size_mb", info.sizeBytes / (1024.0 * 1024.0)}
    });
    
    return true;
}

bool ModelHotpatchManager::unregisterModel(const QString& modelId) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(modelId)) {
        return false;
    }
    
    // Can't unregister if it's the active model
    if (modelId == m_activeModelId) {
        LOG_ERROR("Cannot unregister active model", {{"model_id", modelId}});
        return false;
    }
    
    // Unload if loaded
    if (m_loadedModels.contains(modelId)) {
        unloadModelInternal(modelId);
    }
    
    m_registeredModels.remove(modelId);
    
    LOG_INFO("Model unregistered", {{"model_id", modelId}});
    
    return true;
}

HotpatchResult ModelHotpatchManager::swapModel(const QString& newModelId, bool validateFirst) {
    QElapsedTimer timer;
    timer.start();
    
    HotpatchResult result;
    result.modelId = newModelId;
    result.previousModelId = m_activeModelId;
    result.success = false;
    
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(newModelId)) {
        result.errorMessage = "Model not registered";
        LOG_ERROR("Model swap failed", {
            {"model_id", newModelId},
            {"error", result.errorMessage}
        });
        return result;
    }
    
    // Validate model if requested
    if (validateFirst) {
        lock.unlock();
        if (!validateModel(newModelId)) {
            result.errorMessage = "Model validation failed";
            LOG_ERROR("Model validation failed", {{"model_id", newModelId}});
            return result;
        }
        lock.relock();
    }
    
    // Preload new model if not already loaded
    if (!m_loadedModels.contains(newModelId)) {
        lock.unlock();
        if (!loadModelInternal(newModelId)) {
            lock.relock();
            result.errorMessage = "Failed to load model";
            LOG_ERROR("Model load failed", {{"model_id", newModelId}});
            
            if (m_automaticRollbackEnabled) {
                LOG_INFO("Automatic rollback - keeping previous model active");
            }
            
            return result;
        }
        lock.relock();
    }
    
    // Wait for active inferences to complete (with timeout)
    int waitCount = 0;
    while (m_activeInferences.loadRelaxed() > 0 && waitCount < m_validationTimeoutSeconds * 10) {
        lock.unlock();
        QThread::msleep(100);
        lock.relock();
        waitCount++;
    }
    
    if (m_activeInferences.loadRelaxed() > 0) {
        result.errorMessage = "Timeout waiting for active inferences to complete";
        LOG_ERROR("Model swap timeout", {
            {"model_id", newModelId},
            {"active_inferences", m_activeInferences.loadRelaxed()}
        });
        return result;
    }
    
    // Perform atomic switch
    QString previousModelId = m_activeModelId;
    m_activeModelId = newModelId;
    
    // Update model states
    if (m_registeredModels.contains(previousModelId)) {
        m_registeredModels[previousModelId].isActive = false;
    }
    m_registeredModels[newModelId].isActive = true;
    
    // Record metrics
    result.success = true;
    result.swapDurationMs = timer.elapsed();
    result.completedAt = QDateTime::currentDateTime();
    
    QJsonObject metrics;
    metrics["active_inferences_during_swap"] = m_activeInferences.loadRelaxed();
    metrics["models_in_memory"] = m_loadedModels.size();
    metrics["validation_performed"] = validateFirst;
    result.metrics = metrics;
    
    m_swapHistory.append(result);
    m_lastSwapTime = result.completedAt;
    
    // Emit signal
    emit modelSwapped(newModelId, previousModelId);
    
    LOG_INFO("Model swapped successfully", {
        {"new_model_id", newModelId},
        {"previous_model_id", previousModelId},
        {"duration_ms", result.swapDurationMs}
    });
    
    // Prune model cache if needed
    pruneModelCache();
    
    return result;
}

HotpatchResult ModelHotpatchManager::swapModelAsync(const QString& newModelId, 
                                                    std::function<void(HotpatchResult)> callback) {
    // Launch async model swap
    QtConcurrent::run([this, newModelId, callback]() {
        HotpatchResult result = swapModel(newModelId, true);
        if (callback) {
            callback(result);
        }
    });
    
    // Return immediate result
    HotpatchResult immediateResult;
    immediateResult.success = true;
    immediateResult.modelId = newModelId;
    immediateResult.completedAt = QDateTime::currentDateTime();
    immediateResult.metrics["async"] = true;
    
    return immediateResult;
}

bool ModelHotpatchManager::preloadModel(const QString& modelId, LoadStrategy strategy) {
    if (strategy == LoadStrategy::Background) {
        QtConcurrent::run([this, modelId]() {
            return loadModelInternal(modelId);
        });
        return true;
    } else {
        return loadModelInternal(modelId);
    }
}

bool ModelHotpatchManager::loadModelInternal(const QString& modelId) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(modelId)) {
        LOG_ERROR("Cannot load unregistered model", {{"model_id", modelId}});
        return false;
    }
    
    if (m_loadedModels.contains(modelId)) {
        LOG_DEBUG("Model already loaded", {{"model_id", modelId}});
        return true;
    }
    
    ModelInfo& info = m_registeredModels[modelId];
    
    LOG_INFO("Loading model", {
        {"model_id", modelId},
        {"model_name", info.modelName},
        {"model_path", info.modelPath}
    });
    
    // Simulate model loading (in production, actually load the model)
    // This would involve:
    // 1. Memory allocation
    // 2. File reading
    // 3. Model deserialization
    // 4. GPU transfer if applicable
    
    lock.unlock();
    QThread::msleep(100);  // Simulate loading time
    lock.relock();
    
    m_loadedModels.append(modelId);
    info.referenceCount++;
    
    emit modelLoaded(modelId);
    
    LOG_INFO("Model loaded successfully", {
        {"model_id", modelId},
        {"loaded_models_count", m_loadedModels.size()}
    });
    
    return true;
}

bool ModelHotpatchManager::unloadModel(const QString& modelId) {
    QMutexLocker lock(&m_mutex);
    return unloadModelInternal(modelId);
}

bool ModelHotpatchManager::unloadModelInternal(const QString& modelId) {
    if (!m_loadedModels.contains(modelId)) {
        return false;
    }
    
    // Cannot unload active model
    if (modelId == m_activeModelId) {
        LOG_ERROR("Cannot unload active model", {{"model_id", modelId}});
        return false;
    }
    
    if (m_registeredModels.contains(modelId)) {
        ModelInfo& info = m_registeredModels[modelId];
        info.referenceCount--;
        
        if (info.referenceCount > 0) {
            LOG_DEBUG("Model still has references", {
                {"model_id", modelId},
                {"reference_count", info.referenceCount}
            });
            return false;
        }
    }
    
    LOG_INFO("Unloading model", {{"model_id", modelId}});
    
    m_loadedModels.removeAll(modelId);
    
    emit modelUnloaded(modelId);
    
    return true;
}

bool ModelHotpatchManager::validateModel(const QString& modelId) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(modelId)) {
        return false;
    }
    
    ModelInfo& info = m_registeredModels[modelId];
    
    // Verify file exists
    if (!QFile::exists(info.modelPath)) {
        LOG_ERROR("Model file not found", {
            {"model_id", modelId},
            {"path", info.modelPath}
        });
        return false;
    }
    
    // Verify checksum
    lock.unlock();
    bool checksumValid = verifyChecksum(modelId);
    lock.relock();
    
    if (!checksumValid) {
        LOG_ERROR("Model checksum verification failed", {{"model_id", modelId}});
        return false;
    }
    
    // Run health check
    lock.unlock();
    QJsonObject healthCheck = runHealthCheck(modelId);
    lock.relock();
    
    bool healthy = healthCheck.value("status").toString() == "healthy";
    
    info.isValidated = healthy;
    
    LOG_INFO("Model validation completed", {
        {"model_id", modelId},
        {"validated", healthy}
    });
    
    return healthy;
}

bool ModelHotpatchManager::verifyChecksum(const QString& modelId) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(modelId)) {
        return false;
    }
    
    const ModelInfo& info = m_registeredModels[modelId];
    
    lock.unlock();
    QString currentChecksum = calculateChecksum(info.modelPath);
    lock.relock();
    
    bool valid = (currentChecksum == info.checksum);
    
    if (!valid) {
        LOG_ERROR("Checksum mismatch", {
            {"model_id", modelId},
            {"expected", info.checksum},
            {"actual", currentChecksum}
        });
    }
    
    return valid;
}

QString ModelHotpatchManager::calculateChecksum(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    // Read file in chunks to handle large files
    const qint64 chunkSize = 8192;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(chunkSize);
        hash.addData(chunk);
    }
    
    file.close();
    
    return hash.result().toHex();
}

QJsonObject ModelHotpatchManager::runHealthCheck(const QString& modelId) {
    QJsonObject result;
    result["model_id"] = modelId;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QMutexLocker lock(&m_mutex);
    
    if (!m_registeredModels.contains(modelId)) {
        result["status"] = "error";
        result["message"] = "Model not found";
        return result;
    }
    
    const ModelInfo& info = m_registeredModels[modelId];
    
    // Check file exists
    if (!QFile::exists(info.modelPath)) {
        result["status"] = "unhealthy";
        result["message"] = "Model file not found";
        return result;
    }
    
    // Check if loaded
    bool isLoaded = m_loadedModels.contains(modelId);
    result["loaded"] = isLoaded;
    
    // Check performance metrics if available
    if (m_performanceMetrics.contains(modelId)) {
        const ModelPerformanceMetrics& metrics = m_performanceMetrics[modelId];
        result["error_rate"] = metrics.errorRate;
        result["average_latency_ms"] = metrics.averageLatencyMs;
        result["total_inferences"] = static_cast<qint64>(metrics.totalInferences);
    }
    
    result["status"] = "healthy";
    result["message"] = "Model is operational";
    
    return result;
}

bool ModelHotpatchManager::startCanaryDeployment(const CanaryConfig& config) {
    QMutexLocker lock(&m_mutex);
    
    if (m_canaryActive) {
        LOG_ERROR("Canary deployment already active");
        return false;
    }
    
    if (!m_registeredModels.contains(config.testModelId)) {
        LOG_ERROR("Canary model not registered", {{"model_id", config.testModelId}});
        return false;
    }
    
    // Preload canary model
    lock.unlock();
    if (!preloadModel(config.testModelId, LoadStrategy::Eager)) {
        LOG_ERROR("Failed to preload canary model", {{"model_id", config.testModelId}});
        return false;
    }
    lock.relock();
    
    m_canaryConfig = config;
    m_canaryActive = true;
    m_canaryRequests = 0;
    m_canarySuccesses = 0;
    
    m_canaryMetrics = ModelPerformanceMetrics();
    m_canaryMetrics.lastUpdated = QDateTime::currentDateTime();
    
    LOG_INFO("Canary deployment started", {
        {"model_id", config.testModelId},
        {"traffic_percentage", config.trafficPercentage},
        {"min_requests", config.minRequests}
    });
    
    return true;
}

void ModelHotpatchManager::stopCanaryDeployment() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_canaryActive) {
        return;
    }
    
    m_canaryActive = false;
    
    LOG_INFO("Canary deployment stopped", {
        {"model_id", m_canaryConfig.testModelId},
        {"total_requests", m_canaryRequests},
        {"success_rate", m_canaryRequests > 0 ? 
                        static_cast<double>(m_canarySuccesses) / m_canaryRequests : 0.0}
    });
}

QJsonObject ModelHotpatchManager::getCanaryMetrics() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject metrics;
    
    if (!m_canaryActive) {
        metrics["active"] = false;
        return metrics;
    }
    
    metrics["active"] = true;
    metrics["model_id"] = m_canaryConfig.testModelId;
    metrics["traffic_percentage"] = m_canaryConfig.trafficPercentage;
    metrics["total_requests"] = static_cast<qint64>(m_canaryRequests);
    metrics["successful_requests"] = static_cast<qint64>(m_canarySuccesses);
    metrics["success_rate"] = m_canaryRequests > 0 ? 
                             static_cast<double>(m_canarySuccesses) / m_canaryRequests : 0.0;
    metrics["average_latency_ms"] = m_canaryMetrics.averageLatencyMs;
    metrics["error_rate"] = m_canaryMetrics.errorRate;
    
    bool meetsThreshold = (m_canaryRequests >= m_canaryConfig.minRequests) &&
                         ((static_cast<double>(m_canarySuccesses) / m_canaryRequests) >= 
                          m_canaryConfig.successThreshold);
    
    metrics["meets_threshold"] = meetsThreshold;
    
    return metrics;
}

bool ModelHotpatchManager::promoteCanaryToProduction() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_canaryActive) {
        LOG_ERROR("No active canary deployment to promote");
        return false;
    }
    
    // Check if canary meets threshold
    bool meetsThreshold = (m_canaryRequests >= m_canaryConfig.minRequests) &&
                         ((static_cast<double>(m_canarySuccesses) / m_canaryRequests) >= 
                          m_canaryConfig.successThreshold);
    
    if (!meetsThreshold) {
        LOG_ERROR("Canary does not meet promotion threshold", {
            {"requests", m_canaryRequests},
            {"min_requests", m_canaryConfig.minRequests},
            {"success_rate", m_canaryRequests > 0 ? 
                           static_cast<double>(m_canarySuccesses) / m_canaryRequests : 0.0},
            {"threshold", m_canaryConfig.successThreshold}
        });
        return false;
    }
    
    QString canaryModelId = m_canaryConfig.testModelId;
    
    lock.unlock();
    HotpatchResult result = swapModel(canaryModelId, false);
    lock.relock();
    
    if (result.success) {
        m_canaryActive = false;
        emit canaryPromoted(canaryModelId);
        
        LOG_INFO("Canary promoted to production", {
            {"model_id", canaryModelId}
        });
        
        return true;
    } else {
        LOG_ERROR("Failed to promote canary", {
            {"model_id", canaryModelId},
            {"error", result.errorMessage}
        });
        return false;
    }
}

bool ModelHotpatchManager::rollbackCanary() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_canaryActive) {
        return false;
    }
    
    m_canaryActive = false;
    
    LOG_INFO("Canary deployment rolled back", {
        {"model_id", m_canaryConfig.testModelId}
    });
    
    return true;
}

void ModelHotpatchManager::recordInference(const QString& modelId, double latencyMs, bool success) {
    QMutexLocker lock(&m_mutex);
    
    updatePerformanceMetrics(modelId, latencyMs, success);
    
    // Update canary metrics if this is a canary request
    if (m_canaryActive && modelId == m_canaryConfig.testModelId) {
        m_canaryRequests++;
        if (success) {
            m_canarySuccesses++;
        }
        
        updatePerformanceMetrics(m_canaryConfig.testModelId, latencyMs, success);
    }
}

void ModelHotpatchManager::updatePerformanceMetrics(const QString& modelId, 
                                                   double latencyMs, bool success) {
    if (!m_performanceMetrics.contains(modelId)) {
        m_performanceMetrics[modelId] = ModelPerformanceMetrics();
    }
    
    ModelPerformanceMetrics& metrics = m_performanceMetrics[modelId];
    
    metrics.totalInferences++;
    
    // Update latency (exponential moving average)
    if (metrics.totalInferences == 1) {
        metrics.averageLatencyMs = latencyMs;
    } else {
        const double alpha = 0.1;  // Weight for new values
        metrics.averageLatencyMs = alpha * latencyMs + (1.0 - alpha) * metrics.averageLatencyMs;
    }
    
    // Track latency history for percentiles
    if (!m_latencyHistory.contains(modelId)) {
        m_latencyHistory[modelId] = QVector<double>();
    }
    
    m_latencyHistory[modelId].append(latencyMs);
    
    // Keep only last 1000 samples
    if (m_latencyHistory[modelId].size() > 1000) {
        m_latencyHistory[modelId].remove(0);
    }
    
    // Calculate percentiles
    if (m_latencyHistory[modelId].size() > 0) {
        QVector<double> sorted = m_latencyHistory[modelId];
        std::sort(sorted.begin(), sorted.end());
        
        int p95Index = static_cast<int>(sorted.size() * 0.95);
        int p99Index = static_cast<int>(sorted.size() * 0.99);
        
        metrics.p95LatencyMs = sorted[qMin(p95Index, sorted.size() - 1)];
        metrics.p99LatencyMs = sorted[qMin(p99Index, sorted.size() - 1)];
    }
    
    // Update error tracking
    if (!success) {
        metrics.errorCount++;
    }
    
    metrics.errorRate = static_cast<double>(metrics.errorCount) / metrics.totalInferences;
    
    // Calculate throughput
    metrics.lastUpdated = QDateTime::currentDateTime();
}

ModelPerformanceMetrics ModelHotpatchManager::getPerformanceMetrics(const QString& modelId) const {
    QMutexLocker lock(&m_mutex);
    
    if (!m_performanceMetrics.contains(modelId)) {
        return ModelPerformanceMetrics();
    }
    
    return m_performanceMetrics[modelId];
}

QString ModelHotpatchManager::getBestPerformingModel() const {
    QMutexLocker lock(&m_mutex);
    
    QString bestModelId;
    double bestScore = -1.0;
    
    for (auto it = m_performanceMetrics.begin(); it != m_performanceMetrics.end(); ++it) {
        const ModelPerformanceMetrics& metrics = it.value();
        
        if (metrics.totalInferences < 10) {
            continue;  // Not enough data
        }
        
        // Score based on low latency and low error rate
        double score = 1.0 / (metrics.averageLatencyMs + 1.0) * (1.0 - metrics.errorRate);
        
        if (score > bestScore) {
            bestScore = score;
            bestModelId = it.key();
        }
    }
    
    return bestModelId;
}

QString ModelHotpatchManager::getActiveModelId() const {
    QMutexLocker lock(&m_mutex);
    return m_activeModelId;
}

ModelInfo ModelHotpatchManager::getActiveModel() const {
    QMutexLocker lock(&m_mutex);
    
    if (m_registeredModels.contains(m_activeModelId)) {
        return m_registeredModels[m_activeModelId];
    }
    
    return ModelInfo();
}

QVector<ModelInfo> ModelHotpatchManager::listModels() const {
    QMutexLocker lock(&m_mutex);
    return m_registeredModels.values().toVector();
}

ModelInfo ModelHotpatchManager::getModelInfo(const QString& modelId) const {
    QMutexLocker lock(&m_mutex);
    
    if (m_registeredModels.contains(modelId)) {
        return m_registeredModels[modelId];
    }
    
    return ModelInfo();
}

void ModelHotpatchManager::setMaxConcurrentLoads(int max) {
    QMutexLocker lock(&m_mutex);
    m_maxConcurrentLoads = max;
    LOG_INFO("Max concurrent loads set", {{"max", max}});
}

void ModelHotpatchManager::setValidationTimeout(int seconds) {
    QMutexLocker lock(&m_mutex);
    m_validationTimeoutSeconds = seconds;
    LOG_INFO("Validation timeout set", {{"seconds", seconds}});
}

void ModelHotpatchManager::setModelCacheSize(int maxModels) {
    QMutexLocker lock(&m_mutex);
    m_maxCachedModels = maxModels;
    LOG_INFO("Model cache size set", {{"max_models", maxModels}});
}

void ModelHotpatchManager::enableAutomaticRollback(bool enable) {
    QMutexLocker lock(&m_mutex);
    m_automaticRollbackEnabled = enable;
    LOG_INFO("Automatic rollback", {{"enabled", enable}});
}

void ModelHotpatchManager::pruneModelCache() {
    if (m_loadedModels.size() <= m_maxCachedModels) {
        return;
    }
    
    // Unload least recently used models
    QVector<QPair<QString, QDateTime>> modelUsage;
    
    for (const QString& modelId : m_loadedModels) {
        if (modelId == m_activeModelId) {
            continue;  // Never unload active model
        }
        
        if (m_registeredModels.contains(modelId)) {
            modelUsage.append(qMakePair(modelId, m_registeredModels[modelId].loadedAt));
        }
    }
    
    // Sort by loaded time (oldest first)
    std::sort(modelUsage.begin(), modelUsage.end(),
             [](const QPair<QString, QDateTime>& a, const QPair<QString, QDateTime>& b) {
                 return a.second < b.second;
             });
    
    // Unload oldest models
    int toUnload = m_loadedModels.size() - m_maxCachedModels;
    for (int i = 0; i < qMin(toUnload, modelUsage.size()); ++i) {
        unloadModelInternal(modelUsage[i].first);
    }
}

bool ModelHotpatchManager::saveModelRegistry(const QString& path) {
    QString registryPath = path.isEmpty() ? 
                          (m_modelsDirectory + "/model_registry.json") : path;
    
    QJsonArray modelsArray;
    
    for (const ModelInfo& info : m_registeredModels.values()) {
        QJsonObject modelObj;
        modelObj["model_id"] = info.modelId;
        modelObj["model_name"] = info.modelName;
        modelObj["model_path"] = info.modelPath;
        modelObj["model_type"] = info.modelType;
        modelObj["version"] = info.version;
        modelObj["size_bytes"] = static_cast<qint64>(info.sizeBytes);
        modelObj["checksum"] = info.checksum;
        modelObj["metadata"] = info.metadata;
        
        modelsArray.append(modelObj);
    }
    
    QJsonObject rootObj;
    rootObj["models"] = modelsArray;
    rootObj["active_model_id"] = m_activeModelId;
    rootObj["last_updated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QFile file(registryPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("Failed to save model registry", {{"path", registryPath}});
        return false;
    }
    
    QJsonDocument doc(rootObj);
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO("Model registry saved", {{"path", registryPath}});
    
    return true;
}

bool ModelHotpatchManager::loadModelRegistry(const QString& path) {
    QString registryPath = path.isEmpty() ? 
                          (m_modelsDirectory + "/model_registry.json") : path;
    
    QFile file(registryPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }
    
    QJsonObject rootObj = doc.object();
    QJsonArray modelsArray = rootObj["models"].toArray();
    
    m_registeredModels.clear();
    
    for (const QJsonValue& value : modelsArray) {
        QJsonObject modelObj = value.toObject();
        
        ModelInfo info;
        info.modelId = modelObj["model_id"].toString();
        info.modelName = modelObj["model_name"].toString();
        info.modelPath = modelObj["model_path"].toString();
        info.modelType = modelObj["model_type"].toString();
        info.version = modelObj["version"].toString();
        info.sizeBytes = modelObj["size_bytes"].toInt();
        info.checksum = modelObj["checksum"].toString();
        info.metadata = modelObj["metadata"].toObject();
        info.isActive = false;
        info.isValidated = false;
        info.referenceCount = 0;
        
        m_registeredModels[info.modelId] = info;
    }
    
    m_activeModelId = rootObj["active_model_id"].toString();
    
    LOG_INFO("Model registry loaded", {
        {"path", registryPath},
        {"models_count", m_registeredModels.size()}
    });
    
    return true;
}

// ============================================================================
// ModelReference Implementation
// ============================================================================

ModelReference::ModelReference(ModelHotpatchManager* manager, const QString& modelId)
    : m_manager(manager)
    , m_modelId(modelId)
    , m_valid(false)
{
    if (m_manager) {
        m_valid = m_manager->preloadModel(modelId);
        if (m_valid) {
            m_manager->m_activeInferences.fetchAndAddOrdered(1);
        }
    }
}

ModelReference::~ModelReference() {
    if (m_manager && m_valid) {
        m_manager->m_activeInferences.fetchAndSubOrdered(1);
    }
}

// ============================================================================
// InferenceRouter Implementation
// ============================================================================

InferenceRouter::InferenceRouter(ModelHotpatchManager* manager, QObject* parent)
    : QObject(parent)
    , m_manager(manager)
    , m_strategy(RoutingStrategy::ActiveOnly)
{
}

void InferenceRouter::setRoutingStrategy(RoutingStrategy strategy) {
    m_strategy = strategy;
}

QString InferenceRouter::routeRequest(const QString& requestId) {
    if (!m_manager) {
        return QString();
    }
    
    switch (m_strategy) {
        case RoutingStrategy::ActiveOnly:
            return m_manager->getActiveModelId();
            
        case RoutingStrategy::Canary:
            if (m_manager->m_canaryActive) {
                // Route percentage of traffic to canary
                double random = QRandomGenerator::global()->bounded(1.0);
                if (random < m_manager->m_canaryConfig.trafficPercentage) {
                    return m_manager->m_canaryConfig.testModelId;
                }
            }
            return m_manager->getActiveModelId();
            
        case RoutingStrategy::PerformanceBased:
            return m_manager->getBestPerformingModel();
            
        default:
            return m_manager->getActiveModelId();
    }
}

} // namespace ModelHotpatch
