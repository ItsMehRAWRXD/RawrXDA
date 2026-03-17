#include "ModelPoolManager.h"
#include "../inference_engine.hpp"
#include <QDebug>
#include <QUuid>
#include <algorithm>

// ============ ModelInstance Destructor ============

ModelInstance::~ModelInstance() {
    if (engine) {
        delete engine;
        engine = nullptr;
    }
}

QJsonObject ModelInstance::toJson() const {
    QJsonObject obj;
    obj["instanceId"] = instanceId;
    obj["endpointId"] = endpointId;
    obj["modelPath"] = modelPath;
    obj["state"] = static_cast<int>(state);
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["lastUsedAt"] = lastUsedAt.toString(Qt::ISODate);
    obj["requestsHandled"] = requestsHandled;
    obj["totalProcessingTime"] = totalProcessingTime;
    obj["currentConnections"] = currentConnections;
    obj["maxConnections"] = maxConnections;
    
    return obj;
}

// ============ PoolConfiguration Serialization ============

QJsonObject PoolConfiguration::toJson() const {
    QJsonObject obj;
    obj["minInstances"] = minInstances;
    obj["maxInstances"] = maxInstances;
    obj["idleTimeout"] = idleTimeout;
    obj["warmupInstances"] = warmupInstances;
    obj["autoScale"] = autoScale;
    obj["scaleUpThreshold"] = scaleUpThreshold;
    obj["scaleDownThreshold"] = scaleDownThreshold;
    obj["scaleUpCooldown"] = scaleUpCooldown;
    obj["scaleDownCooldown"] = scaleDownCooldown;
    
    return obj;
}

PoolConfiguration PoolConfiguration::fromJson(const QJsonObject& json) {
    PoolConfiguration config;
    config.minInstances = json["minInstances"].toInt(1);
    config.maxInstances = json["maxInstances"].toInt(10);
    config.idleTimeout = json["idleTimeout"].toInt(300000);
    config.warmupInstances = json["warmupInstances"].toInt(2);
    config.autoScale = json["autoScale"].toBool(true);
    config.scaleUpThreshold = json["scaleUpThreshold"].toDouble(0.8);
    config.scaleDownThreshold = json["scaleDownThreshold"].toDouble(0.3);
    config.scaleUpCooldown = json["scaleUpCooldown"].toInt(60000);
    config.scaleDownCooldown = json["scaleDownCooldown"].toInt(120000);
    
    return config;
}

// ============ PoolStatistics Serialization ============

QJsonObject PoolStatistics::toJson() const {
    QJsonObject obj;
    obj["totalInstances"] = totalInstances;
    obj["readyInstances"] = readyInstances;
    obj["busyInstances"] = busyInstances;
    obj["idleInstances"] = idleInstances;
    obj["totalRequestsHandled"] = static_cast<qint64>(totalRequestsHandled);
    obj["averageResponseTime"] = averageResponseTime;
    obj["poolUtilization"] = poolUtilization;
    obj["lastScaleEvent"] = lastScaleEvent.toString(Qt::ISODate);
    
    return obj;
}

// ============ ModelPoolManager Implementation ============

ModelPoolManager::ModelPoolManager(QObject* parent)
    : QObject(parent)
    , m_totalRequestsHandled(0)
    , m_totalInstancesCreated(0)
{
    m_autoScaleTimer = new QTimer(this);
    connect(m_autoScaleTimer, &QTimer::timeout, this, &ModelPoolManager::performAutoScaling);
    
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ModelPoolManager::cleanupIdleInstances);
    m_cleanupTimer->start(60000);  // Check every minute
}

ModelPoolManager::~ModelPoolManager() {
    QMutexLocker locker(&m_mutex);
    
    // Terminate all instances
    for (auto it = m_instances.begin(); it != m_instances.end(); ++it) {
        if (it.value()) {
            locker.unlock();
            terminateInstance(it.key());
            locker.relock();
        }
    }
}

// ============ Pool Configuration ============

void ModelPoolManager::setConfiguration(const PoolConfiguration& config) {
    QMutexLocker locker(&m_mutex);
    
    m_config = config;
    
    if (m_config.autoScale) {
        locker.unlock();
        enableAutoScaling();
    } else {
        locker.unlock();
        disableAutoScaling();
    }
    
    qDebug() << "Pool configuration updated";
}

PoolConfiguration ModelPoolManager::getConfiguration() const {
    QMutexLocker locker(&m_mutex);
    return m_config;
}

// ============ Instance Management ============

QString ModelPoolManager::createInstance(const QString& endpointId, const QString& modelPath) {
    QMutexLocker locker(&m_mutex);
    
    // Check if we've reached max instances
    int currentCount = 0;
    for (const auto& instance : m_instances) {
        if (instance->endpointId == endpointId) {
            currentCount++;
        }
    }
    
    if (currentCount >= m_config.maxInstances) {
        qWarning() << "Maximum instances reached for endpoint:" << endpointId;
        return QString();
    }
    
    QString instanceId = generateInstanceId(endpointId);
    
    auto instance = std::make_shared<ModelInstance>();
    instance->instanceId = instanceId;
    instance->endpointId = endpointId;
    instance->modelPath = modelPath;
    instance->state = ModelInstanceState::Uninitialized;
    instance->createdAt = QDateTime::currentDateTime();
    instance->lastUsedAt = instance->createdAt;
    
    m_instances[instanceId] = instance;
    m_totalInstancesCreated++;
    
    qDebug() << "Created instance:" << instanceId << "for endpoint:" << endpointId;
    
    return instanceId;
}

bool ModelPoolManager::destroyInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_instances.contains(instanceId)) {
        qWarning() << "Instance not found:" << instanceId;
        return false;
    }
    
    auto instance = m_instances[instanceId];
    QString endpointId = instance->endpointId;
    
    // Remove from available queue
    if (m_availableInstances.contains(endpointId)) {
        m_availableInstances[endpointId].removeAll(instanceId);
    }
    
    // Delete instance
    m_instances.remove(instanceId);
    
    qDebug() << "Destroyed instance:" << instanceId;
    
    return true;
}

ModelInstance* ModelPoolManager::getInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        return m_instances[instanceId].get();
    }
    
    return nullptr;
}

QList<ModelInstance*> ModelPoolManager::getInstancesForEndpoint(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    QList<ModelInstance*> instances;
    
    for (const auto& instance : m_instances) {
        if (instance->endpointId == endpointId) {
            instances.append(instance.get());
        }
    }
    
    return instances;
}

ModelInstance* ModelPoolManager::getAvailableInstance(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_availableInstances.contains(endpointId)) {
        return nullptr;
    }
    
    QQueue<QString>& queue = m_availableInstances[endpointId];
    
    while (!queue.isEmpty()) {
        QString instanceId = queue.dequeue();
        
        if (!m_instances.contains(instanceId)) continue;
        
        auto instance = m_instances[instanceId];
        if (instance->state == ModelInstanceState::Ready) {
            return instance.get();
        }
    }
    
    return nullptr;
}

// ============ Instance Lifecycle ============

bool ModelPoolManager::initializeInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_instances.contains(instanceId)) {
        qWarning() << "Instance not found:" << instanceId;
        return false;
    }
    
    auto instance = m_instances[instanceId];
    
    if (instance->state != ModelInstanceState::Uninitialized) {
        qWarning() << "Instance already initialized:" << instanceId;
        return false;
    }
    
    changeInstanceState(instanceId, ModelInstanceState::Initializing);
    
    // Create inference engine
    instance->engine = new InferenceEngine(instance->modelPath);
    
    // Load model
    locker.unlock();
    bool success = instance->engine->loadModel(instance->modelPath);
    locker.relock();
    
    if (success) {
        changeInstanceState(instanceId, ModelInstanceState::Ready);
        
        // Add to available queue
        if (!m_availableInstances.contains(instance->endpointId)) {
            m_availableInstances[instance->endpointId] = QQueue<QString>();
        }
        m_availableInstances[instance->endpointId].enqueue(instanceId);
        
        m_instanceAvailable.wakeAll();
        
        qDebug() << "Instance initialized successfully:" << instanceId;
        return true;
    } else {
        changeInstanceState(instanceId, ModelInstanceState::Terminated);
        qWarning() << "Failed to initialize instance:" << instanceId;
        return false;
    }
}

bool ModelPoolManager::warmUpInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_instances.contains(instanceId)) {
        return false;
    }
    
    auto instance = m_instances[instanceId];
    
    if (instance->state != ModelInstanceState::Ready) {
        return false;
    }
    
    changeInstanceState(instanceId, ModelInstanceState::Warming);
    
    // Perform warm-up (run a dummy inference)
    locker.unlock();
    if (instance->engine) {
        // Generate with empty token vector for warmup
        std::vector<int32_t> emptyTokens;
        instance->engine->generate(emptyTokens, 10);
    }
    locker.relock();
    
    changeInstanceState(instanceId, ModelInstanceState::Ready);
    
    qDebug() << "Instance warmed up:" << instanceId;
    
    return true;
}

void ModelPoolManager::markInstanceBusy(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        auto instance = m_instances[instanceId];
        changeInstanceState(instanceId, ModelInstanceState::Busy);
        instance->currentConnections++;
        instance->lastUsedAt = QDateTime::currentDateTime();
    }
}

void ModelPoolManager::markInstanceReady(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        auto instance = m_instances[instanceId];
        changeInstanceState(instanceId, ModelInstanceState::Ready);
        
        // Add back to available queue
        if (!m_availableInstances.contains(instance->endpointId)) {
            m_availableInstances[instance->endpointId] = QQueue<QString>();
        }
        m_availableInstances[instance->endpointId].enqueue(instanceId);
        
        m_instanceAvailable.wakeAll();
    }
}

void ModelPoolManager::drainInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        changeInstanceState(instanceId, ModelInstanceState::Draining);
        qDebug() << "Instance draining:" << instanceId;
    }
}

void ModelPoolManager::terminateInstance(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        changeInstanceState(instanceId, ModelInstanceState::Terminated);
        locker.unlock();
        destroyInstance(instanceId);
    }
}

// ============ Auto-Scaling ============

void ModelPoolManager::enableAutoScaling() {
    QMutexLocker locker(&m_mutex);
    
    m_config.autoScale = true;
    m_autoScaleTimer->start(30000);  // Check every 30 seconds
    
    qDebug() << "Auto-scaling enabled";
}

void ModelPoolManager::disableAutoScaling() {
    QMutexLocker locker(&m_mutex);
    
    m_config.autoScale = false;
    m_autoScaleTimer->stop();
    
    qDebug() << "Auto-scaling disabled";
}

int ModelPoolManager::scaleUp(const QString& endpointId, int count) {
    int created = 0;
    
    for (int i = 0; i < count; ++i) {
        QString instanceId = createInstance(endpointId, "");  // Model path would come from endpoint
        if (!instanceId.isEmpty()) {
            initializeInstance(instanceId);
            created++;
        }
    }
    
    if (created > 0) {
        QMutexLocker locker(&m_mutex);
        m_lastScaleUp[endpointId] = QDateTime::currentDateTime();
        
        int newCount = getCurrentScale(endpointId);
        emit scalingOccurred(endpointId, newCount - created, newCount);
        
        qDebug() << "Scaled up endpoint" << endpointId << "by" << created << "instances";
    }
    
    return created;
}

int ModelPoolManager::scaleDown(const QString& endpointId, int count) {
    QMutexLocker locker(&m_mutex);
    
    QList<QString> candidates;
    
    // Find idle instances to remove
    for (const auto& instance : m_instances) {
        if (instance->endpointId == endpointId && 
            instance->state == ModelInstanceState::Idle &&
            instance->currentConnections == 0) {
            candidates.append(instance->instanceId);
        }
    }
    
    int removed = 0;
    int toRemove = std::min(count, static_cast<int>(candidates.size()));
    
    for (int i = 0; i < toRemove; ++i) {
        locker.unlock();
        terminateInstance(candidates[i]);
        locker.relock();
        removed++;
    }
    
    if (removed > 0) {
        m_lastScaleDown[endpointId] = QDateTime::currentDateTime();
        
        int newCount = getCurrentScale(endpointId);
        emit scalingOccurred(endpointId, newCount + removed, newCount);
        
        qDebug() << "Scaled down endpoint" << endpointId << "by" << removed << "instances";
    }
    
    return removed;
}

int ModelPoolManager::getCurrentScale(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const auto& instance : m_instances) {
        if (instance->endpointId == endpointId) {
            count++;
        }
    }
    
    return count;
}

// ============ Warm-Up Management ============

void ModelPoolManager::preWarmInstances(const QString& endpointId, int count) {
    QList<ModelInstance*> instances = getInstancesForEndpoint(endpointId);
    
    int warmed = 0;
    for (ModelInstance* instance : instances) {
        if (warmed >= count) break;
        
        if (instance->state == ModelInstanceState::Ready) {
            warmUpInstance(instance->instanceId);
            warmed++;
        }
    }
    
    qDebug() << "Pre-warmed" << warmed << "instances for" << endpointId;
}

void ModelPoolManager::scheduleWarmUp(const QString& endpointId, const QDateTime& when, int count) {
    qint64 msecs = QDateTime::currentDateTime().msecsTo(when);
    
    if (msecs > 0) {
        QTimer::singleShot(msecs, this, [this, endpointId, count]() {
            preWarmInstances(endpointId, count);
        });
        
        qDebug() << "Scheduled warm-up for" << endpointId << "at" << when.toString();
    }
}

// ============ Request Handling ============

ModelInstance* ModelPoolManager::acquireInstance(const QString& endpointId, int timeoutMs) {
    QMutexLocker locker(&m_mutex);
    
    QDateTime deadline = QDateTime::currentDateTime().addMSecs(timeoutMs);
    
    while (true) {
        // Try to get available instance
        ModelInstance* instance = getAvailableInstance(endpointId);
        if (instance) {
            locker.unlock();
            markInstanceBusy(instance->instanceId);
            return instance;
        }
        
        // Check if we can create a new instance
        int currentCount = getCurrentScale(endpointId);
        if (currentCount < m_config.maxInstances) {
            QString instanceId = createInstance(endpointId, "");
            if (!instanceId.isEmpty()) {
                locker.unlock();
                initializeInstance(instanceId);
                return getInstance(instanceId);
            }
        }
        
        // Wait for available instance
        if (timeoutMs <= 0) {
            return nullptr;
        }
        
        qint64 remaining = QDateTime::currentDateTime().msecsTo(deadline);
        if (remaining <= 0) {
            qWarning() << "Timeout waiting for instance";
            return nullptr;
        }
        
        m_instanceAvailable.wait(&m_mutex, remaining);
    }
}

void ModelPoolManager::releaseInstance(const QString& instanceId, double processingTime) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        auto instance = m_instances[instanceId];
        instance->currentConnections--;
        instance->totalProcessingTime += processingTime;
        instance->lastUsedAt = QDateTime::currentDateTime();
        
        locker.unlock();
        markInstanceReady(instanceId);
    }
}

void ModelPoolManager::recordRequestComplete(const QString& instanceId, bool success, double responseTime) {
    QMutexLocker locker(&m_mutex);
    
    if (m_instances.contains(instanceId)) {
        auto instance = m_instances[instanceId];
        instance->requestsHandled++;
        
        m_totalRequestsHandled++;
    }
}

// ============ Pool Statistics ============

PoolStatistics ModelPoolManager::getStatistics() const {
    QMutexLocker locker(&m_mutex);
    
    PoolStatistics stats;
    stats.totalInstances = m_instances.size();
    
    double totalResponseTime = 0;
    int responseCount = 0;
    
    for (const auto& instance : m_instances) {
        switch (instance->state) {
            case ModelInstanceState::Ready:
                stats.readyInstances++;
                break;
            case ModelInstanceState::Busy:
                stats.busyInstances++;
                break;
            case ModelInstanceState::Idle:
                stats.idleInstances++;
                break;
            default:
                break;
        }
        
        if (instance->requestsHandled > 0) {
            totalResponseTime += instance->totalProcessingTime;
            responseCount += instance->requestsHandled;
        }
    }
    
    stats.totalRequestsHandled = m_totalRequestsHandled;
    stats.averageResponseTime = responseCount > 0 ? totalResponseTime / responseCount : 0;
    stats.poolUtilization = stats.totalInstances > 0 ? 
        static_cast<double>(stats.busyInstances) / stats.totalInstances : 0;
    
    return stats;
}

PoolStatistics ModelPoolManager::getEndpointStatistics(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    
    PoolStatistics stats;
    
    double totalResponseTime = 0;
    int responseCount = 0;
    
    for (const auto& instance : m_instances) {
        if (instance->endpointId != endpointId) continue;
        
        stats.totalInstances++;
        
        switch (instance->state) {
            case ModelInstanceState::Ready:
                stats.readyInstances++;
                break;
            case ModelInstanceState::Busy:
                stats.busyInstances++;
                break;
            case ModelInstanceState::Idle:
                stats.idleInstances++;
                break;
            default:
                break;
        }
        
        if (instance->requestsHandled > 0) {
            totalResponseTime += instance->totalProcessingTime;
            responseCount += instance->requestsHandled;
        }
    }
    
    stats.averageResponseTime = responseCount > 0 ? totalResponseTime / responseCount : 0;
    stats.poolUtilization = stats.totalInstances > 0 ? 
        static_cast<double>(stats.busyInstances) / stats.totalInstances : 0;
    
    return stats;
}

void ModelPoolManager::resetStatistics() {
    QMutexLocker locker(&m_mutex);
    
    m_totalRequestsHandled = 0;
    
    for (auto& instance : m_instances) {
        instance->requestsHandled = 0;
        instance->totalProcessingTime = 0;
    }
    
    qDebug() << "Statistics reset";
}

// ============ Health Management ============

bool ModelPoolManager::checkInstanceHealth(const QString& instanceId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_instances.contains(instanceId)) {
        return false;
    }
    
    auto instance = m_instances[instanceId];
    
    // Simple health check: has engine and is in valid state
    bool healthy = (instance->engine != nullptr) && 
                   (instance->state == ModelInstanceState::Ready || 
                    instance->state == ModelInstanceState::Busy ||
                    instance->state == ModelInstanceState::Idle);
    
    return healthy;
}

QList<QString> ModelPoolManager::getUnhealthyInstances() const {
    QMutexLocker locker(&m_mutex);
    
    QList<QString> unhealthy;
    
    for (const auto& instance : m_instances) {
        if (!instance->engine || instance->state == ModelInstanceState::Terminated) {
            unhealthy.append(instance->instanceId);
        }
    }
    
    return unhealthy;
}

void ModelPoolManager::restartUnhealthyInstances() {
    QList<QString> unhealthy = getUnhealthyInstances();
    
    for (const QString& instanceId : unhealthy) {
        ModelInstance* instance = getInstance(instanceId);
        if (instance) {
            QString endpointId = instance->endpointId;
            QString modelPath = instance->modelPath;
            
            terminateInstance(instanceId);
            QString newId = createInstance(endpointId, modelPath);
            initializeInstance(newId);
            
            qDebug() << "Restarted unhealthy instance:" << instanceId << "as" << newId;
        }
    }
}

// ============ Configuration Persistence ============

QJsonObject ModelPoolManager::saveConfiguration() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject config;
    config["poolConfiguration"] = m_config.toJson();
    config["totalInstancesCreated"] = static_cast<qint64>(m_totalInstancesCreated);
    
    return config;
}

bool ModelPoolManager::loadConfiguration(const QJsonObject& config) {
    QMutexLocker locker(&m_mutex);
    
    m_config = PoolConfiguration::fromJson(config["poolConfiguration"].toObject());
    
    if (m_config.autoScale) {
        locker.unlock();
        enableAutoScaling();
    }
    
    qDebug() << "Configuration loaded";
    
    return true;
}

// ============ Private Slots ============

void ModelPoolManager::performAutoScaling() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_config.autoScale) return;
    
    // Get unique endpoints
    QSet<QString> endpoints;
    for (const auto& instance : m_instances) {
        endpoints.insert(instance->endpointId);
    }
    
    for (const QString& endpointId : endpoints) {
        locker.unlock();
        
        if (shouldScaleUp(endpointId)) {
            scaleUp(endpointId, 1);
        } else if (shouldScaleDown(endpointId)) {
            scaleDown(endpointId, 1);
        }
        
        locker.relock();
    }
}

void ModelPoolManager::cleanupIdleInstances() {
    QMutexLocker locker(&m_mutex);
    
    QList<QString> toRemove;
    QDateTime now = QDateTime::currentDateTime();
    
    for (const auto& instance : m_instances) {
        qint64 idleTime = instance->lastUsedAt.msecsTo(now);
        
        if (idleTime > m_config.idleTimeout && 
            instance->state == ModelInstanceState::Ready &&
            instance->currentConnections == 0) {
            
            // Don't remove if below minimum
            int currentCount = getCurrentScale(instance->endpointId);
            if (currentCount > m_config.minInstances) {
                toRemove.append(instance->instanceId);
            }
        }
    }
    
    for (const QString& instanceId : toRemove) {
        locker.unlock();
        terminateInstance(instanceId);
        locker.relock();
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "Cleaned up" << toRemove.size() << "idle instances";
    }
}

// ============ Private Helper Functions ============

QString ModelPoolManager::generateInstanceId(const QString& endpointId) {
    return QString("%1-%2").arg(endpointId).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

double ModelPoolManager::calculateUtilization(const QString& endpointId) const {
    int total = 0;
    int busy = 0;
    
    for (const auto& instance : m_instances) {
        if (instance->endpointId == endpointId) {
            total++;
            if (instance->state == ModelInstanceState::Busy) {
                busy++;
            }
        }
    }
    
    return total > 0 ? static_cast<double>(busy) / total : 0.0;
}

bool ModelPoolManager::shouldScaleUp(const QString& endpointId) const {
    // Check cooldown
    if (m_lastScaleUp.contains(endpointId)) {
        qint64 elapsed = m_lastScaleUp[endpointId].msecsTo(QDateTime::currentDateTime());
        if (elapsed < m_config.scaleUpCooldown) {
            return false;
        }
    }
    
    double utilization = calculateUtilization(endpointId);
    int currentCount = getCurrentScale(endpointId);
    
    return utilization > m_config.scaleUpThreshold && currentCount < m_config.maxInstances;
}

bool ModelPoolManager::shouldScaleDown(const QString& endpointId) const {
    // Check cooldown
    if (m_lastScaleDown.contains(endpointId)) {
        qint64 elapsed = m_lastScaleDown[endpointId].msecsTo(QDateTime::currentDateTime());
        if (elapsed < m_config.scaleDownCooldown) {
            return false;
        }
    }
    
    double utilization = calculateUtilization(endpointId);
    int currentCount = getCurrentScale(endpointId);
    
    return utilization < m_config.scaleDownThreshold && currentCount > m_config.minInstances;
}

void ModelPoolManager::changeInstanceState(const QString& instanceId, ModelInstanceState newState) {
    if (!m_instances.contains(instanceId)) return;
    
    auto instance = m_instances[instanceId];
    ModelInstanceState oldState = instance->state;
    instance->state = newState;
    
    emit instanceStateChanged(instanceId, oldState, newState);
}
