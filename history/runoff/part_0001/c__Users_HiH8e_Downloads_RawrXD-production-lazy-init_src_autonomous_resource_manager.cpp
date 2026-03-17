#include "autonomous_resource_manager.h"
#include <QFileInfo>
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

AutonomousResourceManager::AutonomousResourceManager(QObject* parent)
    : QObject(parent) {
}

AutonomousResourceManager::SystemResources AutonomousResourceManager::getCurrentResources() {
    SystemResources resources;
    
    // Simplified resource monitoring (would use platform-specific APIs in production)
    resources.available_memory = 8ULL * 1024 * 1024 * 1024; // Assume 8GB available
    resources.cpu_usage_percent = 25; // Assume moderate usage
    resources.gpu_usage_percent = 10; // Assume low GPU usage
    resources.disk_space_available = 100ULL * 1024 * 1024 * 1024; // Assume 100GB available
    
    m_lastResources = resources;
    return resources;
}

bool AutonomousResourceManager::canLoadModel(const QString& modelPath, const SystemResources& resources) {
    QFileInfo fileInfo(modelPath);
    if (!fileInfo.exists()) {
        return false;
    }
    
    qint64 modelSize = fileInfo.size();
    
    // Autonomous decision: need at least 2x model size in available memory
    uint64_t requiredMemory = static_cast<uint64_t>(modelSize) * 2;
    
    bool canLoad = resources.available_memory >= requiredMemory;
    
    qInfo() << "[AutonomousResourceManager] Model size:" << modelSize 
            << "Required memory:" << requiredMemory 
            << "Available:" << resources.available_memory
            << "Can load:" << canLoad;
    
    if (!canLoad) {
        emit resourcesLow(resources);
    }
    
    return canLoad;
}

uint32_t AutonomousResourceManager::getOptimalThreadCount() {
    SystemResources resources = getCurrentResources();
    
    // Autonomous decision based on CPU usage
    uint32_t maxThreads = QThread::idealThreadCount();
    
    if (resources.cpu_usage_percent > 80) {
        // High CPU usage - use fewer threads
        return std::max(1u, maxThreads / 4);
    } else if (resources.cpu_usage_percent > 50) {
        // Moderate CPU usage - use half threads
        return std::max(1u, maxThreads / 2);
    } else {
        // Low CPU usage - use most threads
        return maxThreads;
    }
}

bool AutonomousResourceManager::shouldUseCompression(const SystemResources& resources) const {
    // Autonomous decision: use compression if memory is limited
    const uint64_t MEMORY_THRESHOLD = 4ULL * 1024 * 1024 * 1024; // 4GB
    
    bool useCompression = resources.available_memory < MEMORY_THRESHOLD;
    
    qInfo() << "[AutonomousResourceManager] Should use compression:" << useCompression
            << "(available memory:" << resources.available_memory << ")";
    
    return useCompression;
}

bool AutonomousResourceManager::shouldUseCompression() const {
    return shouldUseCompression(const_cast<AutonomousResourceManager*>(this)->getCurrentResources());
}

AutonomousResourceManager* AutonomousResourceManager::instance() {
    static AutonomousResourceManager* inst = new AutonomousResourceManager();
    return inst;
}