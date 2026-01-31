#include "StreamingGGUFLoader.hpp"


#include <algorithm>

StreamingGGUFLoader::StreamingGGUFLoader(void* parent)
    : void(parent) {
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "initialized";
    
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    Close();
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "destroyed";
    logEntry["total_zones_loaded"] = (int64_t)m_metrics.total_zones_loaded;
    logEntry["total_tensors_accessed"] = (int64_t)m_metrics.total_tensors_accessed;
    
}

bool StreamingGGUFLoader::Open(const std::string& filePath) {
    std::chrono::steady_clock timer;
    timer.start();
    
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "open_failed";
        logEntry["file_path"] = filePath;
        logEntry["error"] = m_file.errorString();
        
        ErrorOccurred(std::string("Failed to open: %1")));
        return false;
    }
    
    m_totalSize = m_file.size();
    m_modelName = std::filesystem::path(filePath).baseName();
    
    int64_t open_time_ms = timer.elapsed();
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "file_opened";
    logEntry["file_path"] = filePath;
    logEntry["model_name"] = m_modelName;
    logEntry["file_size_bytes"] = m_totalSize;
    logEntry["file_size_mb"] = m_totalSize / (1024.0 * 1024.0);
    logEntry["open_time_ms"] = open_time_ms;


    return true;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
    std::chrono::steady_clock timer;
    timer.start();
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "building_tensor_index";


    // TODO: Parse GGUF header and populate m_tensorIndex
    // For now, this is a stub implementation
    // In production, this would:
    // 1. Read GGUF magic number and version
    // 2. Parse metadata section
    // 3. Build tensor index with offsets and zones
    // 4. Assign tensors to zones based on layer/block structure
    
    // Stub: Create dummy tensor entries for demonstration
    // In real implementation, parse actual GGUF format
    
    int64_t index_time_ms = timer.elapsed();
    
    void* resultLog;
    resultLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    resultLog["level"] = "INFO";
    resultLog["component"] = "StreamingGGUFLoader";
    resultLog["event"] = "tensor_index_built";
    resultLog["tensor_count"] = m_tensorIndex.size();
    resultLog["index_time_ms"] = index_time_ms;


    return true;
}

void StreamingGGUFLoader::Close() {
    UnloadAll();
    
    if (m_file.isOpen()) {
        m_file.close();
        
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "file_closed";
        logEntry["model_name"] = m_modelName;
        
    }
}

bool StreamingGGUFLoader::LoadZone(const std::string& zoneName) {
    std::chrono::steady_clock timer;
    timer.start();
    
    if (m_loadedZones.contains(zoneName)) {
        // Zone already loaded, just update access time
        updateZoneAccessTime(zoneName);
        return true;
    }
    
    // Check if we need to evict zones first
    if (m_loadedZones.size() >= m_maxLoadedZones) {
        evictLeastRecentlyUsed();
    }
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "loading_zone";
    logEntry["zone_name"] = zoneName;


    // TODO: Compute zone boundaries from tensor index
    // For now, use stub implementation
    
    ZoneMemory zone;
    zone.start_offset_in_file = 0; // TODO: Calculate from tensor index
    zone.size_bytes = 1024 * 1024; // TODO: Calculate actual size
    zone.last_access_time = std::chrono::system_clock::now();
    zone.access_count = 1;
    
    // Use memory mapping for efficiency
    zone.mapped_data = m_file.map(zone.start_offset_in_file, zone.size_bytes);
    
    if (!zone.mapped_data) {
        void* errorLog;
        errorLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        errorLog["level"] = "ERROR";
        errorLog["component"] = "StreamingGGUFLoader";
        errorLog["event"] = "zone_map_failed";
        errorLog["zone_name"] = zoneName;
        errorLog["error"] = m_file.errorString();
        
        ErrorOccurred(std::string("Failed to map zone %1: %2")));
        return false;
    }
    
    m_loadedZones[zoneName] = zone;
    m_metrics.total_zones_loaded++;
    m_metrics.total_bytes_mapped += zone.size_bytes;
    
    double load_time_ms = timer.elapsed();
    
    // Update average load time
    if (m_metrics.total_zones_loaded > 0) {
        m_metrics.avg_zone_load_time_ms = 
            (m_metrics.avg_zone_load_time_ms * (m_metrics.total_zones_loaded - 1) + load_time_ms) / 
            m_metrics.total_zones_loaded;
    }
    
    void* resultLog;
    resultLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    resultLog["level"] = "INFO";
    resultLog["component"] = "StreamingGGUFLoader";
    resultLog["event"] = "zone_loaded";
    resultLog["zone_name"] = zoneName;
    resultLog["zone_size_mb"] = zone.size_bytes / (1024.0 * 1024.0);
    resultLog["load_time_ms"] = load_time_ms;
    resultLog["total_loaded_zones"] = m_loadedZones.size();
    resultLog["total_mapped_mb"] = m_metrics.total_bytes_mapped / (1024.0 * 1024.0);


    ZoneLoaded(zoneName, load_time_ms);
    return true;
}

void StreamingGGUFLoader::UnloadZone(const std::string& zoneName) {
    if (!m_loadedZones.contains(zoneName)) {
        return;
    }
    
    std::chrono::steady_clock timer;
    timer.start();
    
    ZoneMemory zone = m_loadedZones.take(zoneName);
    
    if (zone.mapped_data) {
        const bool unmapped = m_file.unmap(zone.mapped_data);
        
        if (!unmapped) {
            void* errorLog;
            errorLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
            errorLog["level"] = "WARNING";
            errorLog["component"] = "StreamingGGUFLoader";
            errorLog["event"] = "zone_unmap_failed";
            errorLog["zone_name"] = zoneName;
            
        }
        
        m_metrics.total_bytes_mapped -= zone.size_bytes;
    }
    
    m_metrics.total_zones_evicted++;
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "zone_evicted";
    logEntry["zone_name"] = zoneName;
    logEntry["zone_size_mb"] = zone.size_bytes / (1024.0 * 1024.0);
    logEntry["access_count"] = (int64_t)zone.access_count;
    logEntry["evict_time_ms"] = timer.elapsed();


    ZoneEvicted(zoneName);
}

void StreamingGGUFLoader::UnloadAll() {
    const auto keys = m_loadedZones.keys();
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "unloading_all_zones";
    logEntry["zone_count"] = keys.size();


    for (const auto& k : keys) {
        UnloadZone(k);
    }
}

void StreamingGGUFLoader::evictLeastRecentlyUsed() {
    if (m_loadedZones.empty()) {
        return;
    }
    
    // Find zone with oldest access time
    std::string lruZone;
    auto oldestTime = std::chrono::system_clock::now();
    
    for (auto it = m_loadedZones.begin(); it != m_loadedZones.end(); ++it) {
        if (it.value().last_access_time < oldestTime) {
            oldestTime = it.value().last_access_time;
            lruZone = it.key();
        }
    }
    
    if (!lruZone.empty()) {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "DEBUG";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "evicting_lru_zone";
        logEntry["zone_name"] = lruZone;


        UnloadZone(lruZone);
    }
}

bool StreamingGGUFLoader::GetTensorData(const std::string& tensorName, std::vector<uint8_t>& outData) {
    std::chrono::steady_clock timer;
    timer.start();
    
    if (!m_tensorIndex.contains(tensorName)) {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "WARNING";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "tensor_not_found";
        logEntry["tensor_name"] = tensorName;


        outData.clear();
        return false;
    }
    
    const TensorMetadata& meta = m_tensorIndex[tensorName];
    
    // Ensure the zone containing this tensor is loaded
    if (!m_loadedZones.contains(meta.zone_id)) {
        if (!LoadZone(meta.zone_id)) {
            outData.clear();
            return false;
        }
    }
    
    // Update zone access time
    updateZoneAccessTime(meta.zone_id);
    
    // TODO: Copy tensor data from mapped memory to outData
    // For now, stub implementation
    outData.resize(meta.size_bytes);
    
    m_metrics.total_tensors_accessed++;
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "tensor_accessed";
    logEntry["tensor_name"] = tensorName;
    logEntry["tensor_size_bytes"] = (int64_t)meta.size_bytes;
    logEntry["zone_id"] = meta.zone_id;
    logEntry["access_time_ms"] = timer.elapsed();


    TensorAccessed(tensorName);
    
    return true;
}

bool StreamingGGUFLoader::HasTensor(const std::string& tensorName) const {
    return m_tensorIndex.contains(tensorName);
}

std::string StreamingGGUFLoader::getModelName() const { 
    return m_modelName; 
}

int64_t StreamingGGUFLoader::getTotalSize() const { 
    return m_totalSize; 
}

std::vector<uint8_t> StreamingGGUFLoader::readDataFromFile(int64_t offset, int64_t size) {
    if (!m_file.isOpen()) {
        return {};
    }
    
    if (!m_file.seek(offset)) {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "seek_failed";
        logEntry["offset"] = offset;
        
        return {};
    }
    
    return m_file.read(size);
}

void StreamingGGUFLoader::updateZoneAccessTime(const std::string& zoneName) {
    if (m_loadedZones.contains(zoneName)) {
        m_loadedZones[zoneName].last_access_time = std::chrono::system_clock::now();
        m_loadedZones[zoneName].access_count++;
    }
}



