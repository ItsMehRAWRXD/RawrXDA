#pragma once


#include <vector>
#include <memory>

/**
 * @brief Production-Grade Streaming GGUF Loader with Zone-Based Memory Management
 * 
 * Memory-efficient loader that supports loading large GGUF models by zone
 * rather than loading the entire file into memory at once.
 * 
 * Features:
 * - Zone-based lazy loading (load only required model sections)
 * - Memory-mapped file I/O for efficiency
 * - Tensor index for O(1) tensor lookup
 * - Automatic eviction of unused zones
 * - Structured logging for observability
 * - Latency tracking for zone operations
 * 
 * Production Benefits:
 * - Reduced memory footprint (only active zones loaded)
 * - Faster startup (defer loading until needed)
 * - Support for models larger than available RAM
 * - Resource guard pattern (RAII cleanup)
 */

struct TensorMetadata {
    std::string name;
    uint32_t ndims = 0;
    uint32_t ggml_type = 0;
    uint64_t absolute_offset = 0;
    uint64_t size_bytes = 0;
    std::string zone_id;
};

struct ZoneMemory {
    uint64_t start_offset_in_file = 0;
    uint64_t size_bytes = 0;
    uchar* mapped_data = nullptr;
    std::chrono::system_clock::time_point last_access_time;
    uint64_t access_count = 0;
};

class StreamingGGUFLoader : public void {

public:
    explicit StreamingGGUFLoader(void* parent = nullptr);
    ~StreamingGGUFLoader();

    // Core operations
    bool Open(const std::string& filePath);
    bool BuildTensorIndex();
    void Close();

    // Zone management
    bool LoadZone(const std::string& zoneName);
    void UnloadZone(const std::string& zoneName);
    void UnloadAll();
    
    // Automatic eviction (LRU policy)
    void setMaxLoadedZones(int max) { m_maxLoadedZones = max; }
    void evictLeastRecentlyUsed();

    // Tensor access
    bool GetTensorData(const std::string& tensorName, std::vector<uint8_t>& outData);
    bool HasTensor(const std::string& tensorName) const;
    
    // Metadata
    std::string getModelName() const;
    int64_t getTotalSize() const;
    int getTensorCount() const { return m_tensorIndex.size(); }
    int getLoadedZoneCount() const { return m_loadedZones.size(); }
    
    // Production metrics
    struct LoaderMetrics {
        uint64_t total_zones_loaded = 0;
        uint64_t total_zones_evicted = 0;
        uint64_t total_tensors_accessed = 0;
        double avg_zone_load_time_ms = 0.0;
        uint64_t total_bytes_mapped = 0;
    };
    
    LoaderMetrics getMetrics() const { return m_metrics; }

    void ZoneLoaded(const std::string& zoneName, double load_time_ms);
    void ZoneEvicted(const std::string& zoneName);
    void TensorAccessed(const std::string& tensorName);
    void ErrorOccurred(const std::string& error);

private:
    std::vector<uint8_t> readDataFromFile(int64_t offset, int64_t size);
    void logStructured(const std::string& level, const std::string& event, const void*& data);
    void updateZoneAccessTime(const std::string& zoneName);

    std::fstream m_file;
    std::string m_modelName;
    int64_t m_totalSize = 0;
    int m_maxLoadedZones = 8; // Default limit
    
    std::unordered_map<std::string, TensorMetadata> m_tensorIndex;
    std::unordered_map<std::string, ZoneMemory> m_loadedZones;
    
    LoaderMetrics m_metrics;
};


