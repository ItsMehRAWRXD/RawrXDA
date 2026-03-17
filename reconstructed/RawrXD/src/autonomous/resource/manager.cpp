#include "autonomous_resource_manager.h"
#include <QTimer>
#include <QDebug>
#include <QStorageInfo>
#include <QDir>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#endif

AutonomousResourceManager::AutonomousResourceManager(QObject* parent)
    : QObject(parent)
    , monitoring_timer_(nullptr)
    , monitoring_active_(false)
{
    // Initialize with current resources
    updateResources();
}

AutonomousResourceManager::~AutonomousResourceManager()
{
    stopMonitoring();
}

AutonomousResourceManager::SystemResources AutonomousResourceManager::getCurrentResources() const
{
    return current_resources_;
}

bool AutonomousResourceManager::canLoadModel(const QString& modelPath, const SystemResources& resources) const
{
    SystemResources res = resources;
    if (res.total_memory_bytes == 0) {
        res = current_resources_;
    }

    // Check if file exists and get size
    QFileInfo fileInfo(modelPath);
    if (!fileInfo.exists()) {
        qWarning() << "[ResourceManager] Model file does not exist:" << modelPath;
        return false;
    }

    qint64 modelSize = fileInfo.size();
    
    // Rule 1: Need at least 2x model size in available memory (for decompression + working space)
    uint64_t requiredMemory = modelSize * 2;
    if (res.available_memory_bytes < requiredMemory) {
        qWarning() << "[ResourceManager] Insufficient memory: need" << (requiredMemory / (1024*1024)) << "MB, have" << (res.available_memory_bytes / (1024*1024)) << "MB";
        return false;
    }

    // Rule 2: Need disk space for temporary decompression (if needed)
    uint64_t requiredDisk = modelSize; // Assume 1x for temp files
    if (res.disk_space_available_bytes < requiredDisk) {
        qWarning() << "[ResourceManager] Insufficient disk space: need" << (requiredDisk / (1024*1024)) << "MB, have" << (res.disk_space_available_bytes / (1024*1024)) << "MB";
        return false;
    }

    // Rule 3: Don't load if memory usage is critical
    if (res.isMemoryCritical()) {
        qWarning() << "[ResourceManager] Memory usage critical:" << res.getMemoryUsagePercent() << "%";
        return false;
    }

    // Rule 4: Don't load if CPU is maxed out (unless GPU available)
    if (res.cpu_usage_percent > 90 && !res.gpu_available) {
        qWarning() << "[ResourceManager] CPU usage too high:" << res.cpu_usage_percent << "%";
        return false;
    }

    qInfo() << "[ResourceManager] Model can be loaded:" << modelPath 
            << "(" << (modelSize / (1024*1024)) << "MB)";
    return true;
}

uint32_t AutonomousResourceManager::getOptimalThreadCount(const SystemResources& resources) const
{
    SystemResources res = resources;
    if (res.total_memory_bytes == 0) {
        res = current_resources_;
    }

    // Get CPU core count
    uint32_t cpuCores = QThread::idealThreadCount();
    if (cpuCores == 0) cpuCores = 4; // Fallback

    // Adjust based on CPU usage
    if (res.cpu_usage_percent > 80) {
        // High CPU usage - use fewer threads
        return std::max(1u, cpuCores / 2);
    } else if (res.cpu_usage_percent > 50) {
        // Medium CPU usage - use 75% of cores
        return std::max(1u, (cpuCores * 3) / 4);
    } else {
        // Low CPU usage - use all cores
        return cpuCores;
    }
}

bool AutonomousResourceManager::shouldUseCompression(const SystemResources& resources) const
{
    SystemResources res = resources;
    if (res.total_memory_bytes == 0) {
        res = current_resources_;
    }

    // Use compression if:
    // 1. Memory is low (compression saves memory)
    // 2. Disk space is limited (compression saves disk)
    // 3. CPU is not maxed out (compression uses CPU)

    if (res.isMemoryLow()) {
        qInfo() << "[ResourceManager] Using compression: memory low";
        return true;
    }

    if (res.disk_space_available_bytes < 10ULL * 1024 * 1024 * 1024) { // < 10GB
        qInfo() << "[ResourceManager] Using compression: disk space limited";
        return true;
    }

    if (res.cpu_usage_percent < 70) {
        qInfo() << "[ResourceManager] Using compression: CPU available";
        return true;
    }

    // Don't use compression if CPU is maxed out
    qInfo() << "[ResourceManager] Not using compression: CPU high, memory OK";
    return false;
}

uint32_t AutonomousResourceManager::getRecommendedCompressionLevel(const SystemResources& resources) const
{
    SystemResources res = resources;
    if (res.total_memory_bytes == 0) {
        res = current_resources_;
    }

    // Compression level selection:
    // - Level 1-3: Fast (low CPU, lower compression)
    // - Level 4-6: Balanced (medium CPU, good compression)
    // - Level 7-9: Best (high CPU, best compression)

    if (res.cpu_usage_percent > 80) {
        // High CPU - use fast compression
        return 3;
    } else if (res.cpu_usage_percent > 50) {
        // Medium CPU - use balanced compression
        return 6;
    } else if (res.isMemoryLow()) {
        // Low memory - prioritize compression ratio
        return 9;
    } else {
        // Normal conditions - balanced
        return 6;
    }
}

void AutonomousResourceManager::startMonitoring(int intervalMs)
{
    if (monitoring_active_) {
        stopMonitoring();
    }

    monitoring_timer_ = new QTimer(this);
    connect(monitoring_timer_, &QTimer::timeout, this, &AutonomousResourceManager::onMonitoringTimer);
    monitoring_timer_->start(intervalMs);
    monitoring_active_ = true;

    qInfo() << "[ResourceManager] Started monitoring with interval" << intervalMs << "ms";
}

void AutonomousResourceManager::stopMonitoring()
{
    if (monitoring_timer_) {
        monitoring_timer_->stop();
        monitoring_timer_->deleteLater();
        monitoring_timer_ = nullptr;
    }
    monitoring_active_ = false;
    qInfo() << "[ResourceManager] Stopped monitoring";
}

void AutonomousResourceManager::updateResources()
{
    SystemResources newResources = gatherResources();
    
    // Emit signals based on state changes
    if (newResources.isMemoryCritical()) {
        emit resourcesCritical(newResources);
    } else if (newResources.isMemoryLow()) {
        emit resourcesLow(newResources);
    } else if (newResources.getMemoryUsagePercent() < 50.0 && newResources.cpu_usage_percent < 50) {
        emit resourcesOptimal(newResources);
    }

    current_resources_ = newResources;
    emit resourcesUpdated(newResources);
}

void AutonomousResourceManager::onMonitoringTimer()
{
    updateResources();
}

AutonomousResourceManager::SystemResources AutonomousResourceManager::gatherResources() const
{
    SystemResources res;

    res.available_memory_bytes = getAvailableMemory();
    res.total_memory_bytes = getTotalMemory();
    res.cpu_usage_percent = getCpuUsage();
    getGpuInfo(res.gpu_usage_percent, res.gpu_available, res.gpu_name);
    res.disk_space_available_bytes = getAvailableDiskSpace();
    res.memory_usage_percent = static_cast<uint32_t>(res.getMemoryUsagePercent());

    return res;
}

#ifdef _WIN32
uint64_t AutonomousResourceManager::getAvailableMemory() const
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullAvailPhys;
    }
    return 0;
}

uint64_t AutonomousResourceManager::getTotalMemory() const
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullTotalPhys;
    }
    return 0;
}

uint32_t AutonomousResourceManager::getCpuUsage() const
{
    // Simplified CPU usage - in production, use PDH or WMI
    // For now, return a placeholder
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    
    if (!query) {
        PdhOpenQuery(nullptr, 0, &query);
        PdhAddCounter(query, L"\\Processor(_Total)\\% Processor Time", 0, &counter);
    }
    
    if (query && counter) {
        PdhCollectQueryData(query);
        Sleep(100); // Wait for sample
        PdhCollectQueryData(query);
        
        PDH_FMT_COUNTERVALUE value;
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
            return static_cast<uint32_t>(value.doubleValue);
        }
    }
    
    return 0; // Fallback
}

void AutonomousResourceManager::getGpuInfo(uint32_t& usage, bool& available, QString& name) const
{
    // Simplified GPU detection - in production, use NVML, ADL, or WMI
    // For now, check for common GPU vendors via WMI
    available = false;
    usage = 0;
    name = "Unknown";
    
    // TODO: Implement proper GPU detection using WMI or vendor APIs
    // This is a placeholder
}
#else
uint64_t AutonomousResourceManager::getAvailableMemory() const
{
    // Linux/Mac implementation
    return 0;
}

uint64_t AutonomousResourceManager::getTotalMemory() const
{
    // Linux/Mac implementation
    return 0;
}

uint32_t AutonomousResourceManager::getCpuUsage() const
{
    // Linux/Mac implementation
    return 0;
}

void AutonomousResourceManager::getGpuInfo(uint32_t& usage, bool& available, QString& name) const
{
    // Linux/Mac implementation
    available = false;
    usage = 0;
    name = "Unknown";
}
#endif

uint64_t AutonomousResourceManager::getAvailableDiskSpace(const QString& path) const
{
    QString targetPath = path.isEmpty() ? QDir::currentPath() : path;
    QStorageInfo storage(targetPath);
    storage.refresh();
    return storage.bytesAvailable();
}

