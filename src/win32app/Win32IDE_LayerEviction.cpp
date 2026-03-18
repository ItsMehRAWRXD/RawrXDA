// ============================================================================
// Win32IDE_LayerEviction.cpp — Layer Eviction System Implementation
// ============================================================================
// Provides memory management for large model layers:
//   - Evicts least-recently-used layers to disk
//   - Manages GPU VRAM and system RAM usage
//   - Automatic eviction during high memory pressure
//   - Layer reloading on demand
//   - Performance monitoring and statistics
//
// Architecture:
//   - LayerEvictionManager tracks layer usage and memory
//   - EvictionPolicy decides which layers to evict
//   - StorageBackend handles disk I/O for evicted layers
//   - MemoryMonitor provides real-time memory statistics
// ============================================================================

#include "Win32IDE.h"
#include <memory>
#include <unordered_map>
#include <list>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace RawrXD {

// ============================================================================
// LAYER EVICTION STRUCTURES
// ============================================================================

struct EvictedLayer {
    std::string layerId;
    std::string filePath;
    size_t sizeBytes;
    std::chrono::steady_clock::time_point lastAccess;
    int accessCount;
};

struct LayerInfo {
    std::string id;
    size_t memoryUsage;
    bool isEvicted;
    std::chrono::steady_clock::time_point lastAccess;
    int accessCount;
    std::unique_ptr<EvictedLayer> evictedData;
};

// ============================================================================
// LAYER EVICTION MANAGER
// ============================================================================

class LayerEvictionManager {
public:
    LayerEvictionManager();
    ~LayerEvictionManager();

    bool initialize(size_t maxMemoryBytes, const std::string& cacheDir);
    void shutdown();

    // Layer management
    bool registerLayer(const std::string& layerId, size_t memoryUsage);
    bool unregisterLayer(const std::string& layerId);
    void accessLayer(const std::string& layerId);

    // Eviction control
    void setMaxMemory(size_t maxBytes);
    void setEvictionThreshold(float threshold); // 0.0-1.0
    bool evictLayers(size_t targetFreeBytes = 0);
    bool reloadLayer(const std::string& layerId);

    // Statistics
    size_t getTotalMemoryUsage() const;
    size_t getEvictedMemory() const;
    float getMemoryPressure() const;
    std::vector<std::string> getEvictedLayers() const;

private:
    void evictionThread();
    bool evictSingleLayer(const std::string& layerId);
    bool shouldEvict() const;
    std::string selectLayerToEvict() const;

    std::unordered_map<std::string, LayerInfo> m_layers;
    std::list<std::string> m_accessOrder; // LRU order
    std::string m_cacheDir;
    size_t m_maxMemory;
    float m_evictionThreshold;
    std::thread m_evictionThread;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;

    // Statistics
    std::atomic<size_t> m_totalMemoryUsage;
    std::atomic<size_t> m_evictedMemory;
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

LayerEvictionManager::LayerEvictionManager()
    : m_maxMemory(0), m_evictionThreshold(0.8f), m_running(false),
      m_totalMemoryUsage(0), m_evictedMemory(0) {
}

LayerEvictionManager::~LayerEvictionManager() {
    shutdown();
}

bool LayerEvictionManager::initialize(size_t maxMemoryBytes, const std::string& cacheDir) {
    m_maxMemory = maxMemoryBytes;
    m_cacheDir = cacheDir;

    // Create cache directory
    try {
        std::filesystem::create_directories(cacheDir);
    } catch (...) {
        return false;
    }

    m_running = true;
    m_evictionThread = std::thread(&LayerEvictionManager::evictionThread, this);

    return true;
}

void LayerEvictionManager::shutdown() {
    m_running = false;
    if (m_evictionThread.joinable()) {
        m_evictionThread.join();
    }

    // Clean up evicted files
    for (const auto& pair : m_layers) {
        if (pair.second.evictedData) {
            try {
                std::filesystem::remove(pair.second.evictedData->filePath);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
    }

    m_layers.clear();
    m_accessOrder.clear();
}

bool LayerEvictionManager::registerLayer(const std::string& layerId, size_t memoryUsage) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_layers.count(layerId)) {
        return false; // Already registered
    }

    LayerInfo info;
    info.id = layerId;
    info.memoryUsage = memoryUsage;
    info.isEvicted = false;
    info.lastAccess = std::chrono::steady_clock::now();
    info.accessCount = 0;

    m_layers[layerId] = std::move(info);
    m_totalMemoryUsage += memoryUsage;

    // Check if eviction is needed
    if (shouldEvict()) {
        evictLayers();
    }

    return true;
}

bool LayerEvictionManager::unregisterLayer(const std::string& layerId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_layers.find(layerId);
    if (it == m_layers.end()) {
        return false;
    }

    if (!it->second.isEvicted) {
        m_totalMemoryUsage -= it->second.memoryUsage;
    } else {
        m_evictedMemory -= it->second.memoryUsage;
    }

    // Remove evicted file if exists
    if (it->second.evictedData) {
        try {
            std::filesystem::remove(it->second.evictedData->filePath);
        } catch (...) {
            // Ignore
        }
    }

    m_layers.erase(it);
    m_accessOrder.remove(layerId);

    return true;
}

void LayerEvictionManager::accessLayer(const std::string& layerId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_layers.find(layerId);
    if (it == m_layers.end()) {
        return;
    }

    it->second.lastAccess = std::chrono::steady_clock::now();
    it->second.accessCount++;

    // Move to front of LRU list
    m_accessOrder.remove(layerId);
    m_accessOrder.push_front(layerId);
}

void LayerEvictionManager::setMaxMemory(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxMemory = maxBytes;

    if (shouldEvict()) {
        evictLayers();
    }
}

void LayerEvictionManager::setEvictionThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evictionThreshold = threshold;
}

bool LayerEvictionManager::evictLayers(size_t targetFreeBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t targetMemory = m_maxMemory - targetFreeBytes;
    if (m_totalMemoryUsage <= targetMemory) {
        return true; // No eviction needed
    }

    // Evict layers until we're under the limit
    while (m_totalMemoryUsage > targetMemory && !m_accessOrder.empty()) {
        std::string layerId = selectLayerToEvict();
        if (layerId.empty()) {
            break; // No more layers to evict
        }

        if (!evictSingleLayer(layerId)) {
            break; // Eviction failed
        }
    }

    return m_totalMemoryUsage <= targetMemory;
}

bool LayerEvictionManager::reloadLayer(const std::string& layerId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_layers.find(layerId);
    if (it == m_layers.end() || !it->second.isEvicted) {
        return false;
    }

    // In a real implementation, this would load the layer data from disk
    // For now, just mark as not evicted
    it->second.isEvicted = false;
    m_totalMemoryUsage += it->second.memoryUsage;
    m_evictedMemory -= it->second.memoryUsage;

    // Remove evicted data
    it->second.evictedData.reset();

    return true;
}

size_t LayerEvictionManager::getTotalMemoryUsage() const {
    return m_totalMemoryUsage;
}

size_t LayerEvictionManager::getEvictedMemory() const {
    return m_evictedMemory;
}

float LayerEvictionManager::getMemoryPressure() const {
    if (m_maxMemory == 0) return 0.0f;
    return static_cast<float>(m_totalMemoryUsage) / static_cast<float>(m_maxMemory);
}

std::vector<std::string> LayerEvictionManager::getEvictedLayers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;

    for (const auto& pair : m_layers) {
        if (pair.second.isEvicted) {
            result.push_back(pair.first);
        }
    }

    return result;
}

void LayerEvictionManager::evictionThread() {
    while (m_running) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (shouldEvict()) {
                evictLayers();
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5)); // Check every 5 seconds
    }
}

bool LayerEvictionManager::evictSingleLayer(const std::string& layerId) {
    auto it = m_layers.find(layerId);
    if (it == m_layers.end() || it->second.isEvicted) {
        return false;
    }

    // Create evicted layer data
    auto evicted = std::make_unique<EvictedLayer>();
    evicted->layerId = layerId;
    evicted->sizeBytes = it->second.memoryUsage;
    evicted->lastAccess = it->second.lastAccess;
    evicted->accessCount = it->second.accessCount;
    evicted->filePath = m_cacheDir + "\\" + layerId + ".evicted";

    // In a real implementation, serialize layer data to disk here
    // For now, just create an empty file as placeholder
    try {
        std::ofstream file(evicted->filePath, std::ios::binary);
        if (!file) {
            return false;
        }
        // Write placeholder data
        file.write(reinterpret_cast<const char*>(&evicted->sizeBytes), sizeof(size_t));
    } catch (...) {
        return false;
    }

    // Update layer info
    it->second.isEvicted = true;
    it->second.evictedData = std::move(evicted);
    m_totalMemoryUsage -= it->second.memoryUsage;
    m_evictedMemory += it->second.memoryUsage;

    return true;
}

bool LayerEvictionManager::shouldEvict() const {
    if (m_maxMemory == 0) return false;
    return getMemoryPressure() > m_evictionThreshold;
}

std::string LayerEvictionManager::selectLayerToEvict() const {
    // Simple LRU: evict the least recently used layer
    for (auto it = m_accessOrder.rbegin(); it != m_accessOrder.rend(); ++it) {
        const std::string& layerId = *it;
        auto layerIt = m_layers.find(layerId);
        if (layerIt != m_layers.end() && !layerIt->second.isEvicted) {
            return layerId;
        }
    }

    return ""; // No evictable layers
}

} // namespace RawrXD

void LayerEvictionManagerDeleter::operator()(RawrXD::LayerEvictionManager* ptr) noexcept {
    delete ptr;
}

// ============================================================================
// WIN32IDE INTEGRATION
// ============================================================================

void Win32IDE::initLayerEviction() {
    if (!m_layerEvictionManager) {
        m_layerEvictionManager.reset(new RawrXD::LayerEvictionManager());
    }

    // Initialize with 8GB max memory, cache in temp directory
    std::string cacheDir = std::filesystem::temp_directory_path().string() + "\\RawrXD_LayerCache";
    size_t maxMemory = 8LL * 1024LL * 1024LL * 1024LL; // 8GB

    if (m_layerEvictionManager->initialize(maxMemory, cacheDir)) {
        LOG_INFO("Layer Eviction System initialized (max: 8GB, cache: " + cacheDir + ")");
    } else {
        LOG_ERROR("Failed to initialize Layer Eviction System");
    }
}

void Win32IDE::shutdownLayerEviction() {
    if (m_layerEvictionManager) {
        m_layerEvictionManager->shutdown();
        m_layerEvictionManager.reset();
    }
}

bool Win32IDE::registerModelLayer(const std::string& layerId, size_t memoryUsage) {
    if (!m_layerEvictionManager) return false;
    return m_layerEvictionManager->registerLayer(layerId, memoryUsage);
}

void Win32IDE::accessModelLayer(const std::string& layerId) {
    if (m_layerEvictionManager) {
        m_layerEvictionManager->accessLayer(layerId);
    }
}

void Win32IDE::showLayerEvictionStats() {
    if (!m_layerEvictionManager) {
        appendToOutput("Layer Eviction System not initialized", "System", OutputSeverity::Warning);
        return;
    }

    size_t totalUsage = m_layerEvictionManager->getTotalMemoryUsage();
    size_t evicted = m_layerEvictionManager->getEvictedMemory();
    float pressure = m_layerEvictionManager->getMemoryPressure();
    auto evictedLayers = m_layerEvictionManager->getEvictedLayers();

    std::string stats = "Layer Eviction Stats:\n\n";
    stats += "Total Memory Usage: " + std::to_string(totalUsage / (1024*1024)) + " MB\n";
    stats += "Evicted Memory: " + std::to_string(evicted / (1024*1024)) + " MB\n";
    stats += "Memory Pressure: " + std::to_string(pressure * 100.0f) + "%\n";
    stats += "Evicted Layers: " + std::to_string(evictedLayers.size()) + "\n";

    if (!evictedLayers.empty()) {
        stats += "\nEvicted Layer IDs:\n";
        for (const auto& layer : evictedLayers) {
            stats += "  - " + layer + "\n";
        }
    }

    appendToOutput(stats, "System", OutputSeverity::Info);
}
