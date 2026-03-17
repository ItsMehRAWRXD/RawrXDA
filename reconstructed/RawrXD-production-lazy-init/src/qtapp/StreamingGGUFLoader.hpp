#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QElapsedTimer>
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
    QString name;
    uint32_t ndims = 0;
    uint32_t ggml_type = 0;
    quint64 absolute_offset = 0;
    quint64 size_bytes = 0;
    QString zone_id;
};

struct ZoneMemory {
    quint64 start_offset_in_file = 0;
    quint64 size_bytes = 0;
    uchar* mapped_data = nullptr;
    std::chrono::system_clock::time_point last_access_time;
    quint64 access_count = 0;
};

class StreamingGGUFLoader : public QObject {
    Q_OBJECT
    
public:
    explicit StreamingGGUFLoader(QObject* parent = nullptr);
    StreamingGGUFLoader();  // Zero-argument constructor for std::make_unique<StreamingGGUFLoader>()
    ~StreamingGGUFLoader();

    // Core operations
    bool Open(const QString& filePath);
    bool BuildTensorIndex();
    void Close();

    // Zone management
    bool LoadZone(const QString& zoneName);
    void UnloadZone(const QString& zoneName);
    void UnloadAll();
    
    // Automatic eviction (LRU policy)
    void setMaxLoadedZones(int max) { m_maxLoadedZones = max; }
    void evictLeastRecentlyUsed();

    // Tensor access
    bool GetTensorData(const QString& tensorName, std::vector<uint8_t>& outData);
    bool HasTensor(const QString& tensorName) const;
    
    // Metadata
    QString getModelName() const;
    qint64 getTotalSize() const;
    int getTensorCount() const { return m_tensorIndex.size(); }
    int getLoadedZoneCount() const { return m_loadedZones.size(); }
    
    // Production metrics
    struct LoaderMetrics {
        quint64 total_zones_loaded = 0;
        quint64 total_zones_evicted = 0;
        quint64 total_tensors_accessed = 0;
        double avg_zone_load_time_ms = 0.0;
        quint64 total_bytes_mapped = 0;
    };
    
    LoaderMetrics getMetrics() const { return m_metrics; }

signals:
    void ZoneLoaded(const QString& zoneName, double load_time_ms);
    void ZoneEvicted(const QString& zoneName);
    void TensorAccessed(const QString& tensorName);
    void ErrorOccurred(const QString& error);

private:
    QByteArray readDataFromFile(qint64 offset, qint64 size);
    void logStructured(const QString& level, const QString& event, const QJsonObject& data);
    void updateZoneAccessTime(const QString& zoneName);

    QFile m_file;
    QString m_modelName;
    qint64 m_totalSize = 0;
    int m_maxLoadedZones = 8; // Default limit
    
    QHash<QString, TensorMetadata> m_tensorIndex;
    QHash<QString, ZoneMemory> m_loadedZones;
    
    LoaderMetrics m_metrics;
};
