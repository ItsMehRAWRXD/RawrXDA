#include "StreamingGGUFLoader.hpp"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <algorithm>

StreamingGGUFLoader::StreamingGGUFLoader(QObject* parent)
    : QObject(parent) {
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "initialized";
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

// Zero-argument constructor for std::make_unique<StreamingGGUFLoader>()
StreamingGGUFLoader::StreamingGGUFLoader()
    : QObject(nullptr) {
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "initialized (zero-arg constructor)";
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    Close();
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "destroyed";
    logEntry["total_zones_loaded"] = (qint64)m_metrics.total_zones_loaded;
    logEntry["total_tensors_accessed"] = (qint64)m_metrics.total_tensors_accessed;
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

bool StreamingGGUFLoader::Open(const QString& filePath) {
    QElapsedTimer timer;
    timer.start();
    
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "open_failed";
        logEntry["file_path"] = filePath;
        logEntry["error"] = m_file.errorString();
        
        qCritical().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        emit ErrorOccurred(QString("Failed to open: %1").arg(m_file.errorString()));
        return false;
    }
    
    m_totalSize = m_file.size();
    m_modelName = QFileInfo(filePath).baseName();
    
    qint64 open_time_ms = timer.elapsed();
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "file_opened";
    logEntry["file_path"] = filePath;
    logEntry["model_name"] = m_modelName;
    logEntry["file_size_bytes"] = m_totalSize;
    logEntry["file_size_mb"] = m_totalSize / (1024.0 * 1024.0);
    logEntry["open_time_ms"] = open_time_ms;
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    return true;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "building_tensor_index";
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    // Real GGUF parsing implementation
    // GGUF Format: magic (4 bytes) + version (4 bytes) + tensor_count (8 bytes) + kv_count (8 bytes) + ...
    
    if (m_totalSize < 24) { // Minimum header size
        qCritical() << "[StreamingGGUFLoader] File too small to be valid GGUF";
        emit ErrorOccurred("File too small to be valid GGUF");
        return false;
    }
    
    // Read GGUF header
    if (!m_file.seek(0)) {
        emit ErrorOccurred("Failed to seek to start of file");
        return false;
    }
    
    QByteArray headerData = m_file.read(24);
    if (headerData.size() < 24) {
        emit ErrorOccurred("Failed to read GGUF header");
        return false;
    }
    
    // Parse magic (4 bytes) - should be "GGUF"
    const char* data = headerData.constData();
    if (memcmp(data, "GGUF", 4) != 0) {
        emit ErrorOccurred("Invalid GGUF magic bytes - not a GGUF file");
        return false;
    }
    
    // Parse version (uint32_t, little-endian)
    uint32_t version = *reinterpret_cast<const uint32_t*>(data + 4);
    if (version < 2 || version > 3) {
        qWarning() << "[StreamingGGUFLoader] Unexpected GGUF version:" << version;
    }
    
    // Parse tensor count (int64_t)
    int64_t tensorCount = *reinterpret_cast<const int64_t*>(data + 8);
    
    // Parse KV pair count (int64_t)
    int64_t kvCount = *reinterpret_cast<const int64_t*>(data + 16);
    
    qInfo() << "[StreamingGGUFLoader] GGUF version:" << version
            << "tensors:" << tensorCount << "kv_pairs:" << kvCount;
    
    // Now we need to skip KV pairs to find tensor info
    // This is complex because strings and arrays have variable lengths
    qint64 currentOffset = 24; // After header
    
    // Skip KV pairs (simplified - read string length + type + value)
    for (int64_t i = 0; i < kvCount && currentOffset < m_totalSize; ++i) {
        if (!m_file.seek(currentOffset)) break;
        
        // Read key string length (uint64_t)
        QByteArray keyLenData = m_file.read(8);
        if (keyLenData.size() < 8) break;
        uint64_t keyLen = *reinterpret_cast<const uint64_t*>(keyLenData.constData());
        currentOffset += 8 + keyLen; // Skip key length + key string
        
        // Read value type (int32_t)
        if (!m_file.seek(currentOffset)) break;
        QByteArray typeData = m_file.read(4);
        if (typeData.size() < 4) break;
        int32_t valueType = *reinterpret_cast<const int32_t*>(typeData.constData());
        currentOffset += 4;
        
        // Skip value based on type
        switch (valueType) {
            case 0: currentOffset += 1; break;  // UINT8
            case 1: currentOffset += 1; break;  // INT8
            case 2: currentOffset += 2; break;  // UINT16
            case 3: currentOffset += 2; break;  // INT16
            case 4: currentOffset += 4; break;  // UINT32
            case 5: currentOffset += 4; break;  // INT32
            case 6: currentOffset += 4; break;  // FLOAT32
            case 7: currentOffset += 1; break;  // BOOL
            case 8: { // STRING
                if (!m_file.seek(currentOffset)) break;
                QByteArray strLenData = m_file.read(8);
                if (strLenData.size() < 8) break;
                uint64_t strLen = *reinterpret_cast<const uint64_t*>(strLenData.constData());
                currentOffset += 8 + strLen;
                break;
            }
            case 9: { // ARRAY (skip for now - complex)
                // Array: type (4 bytes) + count (8 bytes) + elements
                currentOffset += 12; // Simplified - may need full parsing
                break;
            }
            case 10: currentOffset += 8; break; // UINT64
            case 11: currentOffset += 8; break; // INT64
            case 12: currentOffset += 8; break; // FLOAT64
            default:
                qWarning() << "[StreamingGGUFLoader] Unknown KV type:" << valueType;
                break;
        }
    }
    
    // Now parse tensor info entries
    qint64 tensorInfoOffset = currentOffset;
    qint64 totalTensorBytes = 0;
    
    for (int64_t i = 0; i < tensorCount && currentOffset < m_totalSize; ++i) {
        if (!m_file.seek(currentOffset)) break;
        
        // Read tensor name (string: length + chars)
        QByteArray nameLenData = m_file.read(8);
        if (nameLenData.size() < 8) break;
        uint64_t nameLen = *reinterpret_cast<const uint64_t*>(nameLenData.constData());
        currentOffset += 8;
        
        if (!m_file.seek(currentOffset)) break;
        QByteArray nameData = m_file.read(nameLen);
        QString tensorName = QString::fromUtf8(nameData);
        currentOffset += nameLen;
        
        // Read n_dims (uint32_t)
        if (!m_file.seek(currentOffset)) break;
        QByteArray ndimsData = m_file.read(4);
        if (ndimsData.size() < 4) break;
        uint32_t nDims = *reinterpret_cast<const uint32_t*>(ndimsData.constData());
        currentOffset += 4;
        
        // Read dimensions (int64_t each)
        uint64_t tensorElements = 1;
        for (uint32_t d = 0; d < nDims && d < 8; ++d) {
            if (!m_file.seek(currentOffset)) break;
            QByteArray dimData = m_file.read(8);
            if (dimData.size() < 8) break;
            int64_t dimSize = *reinterpret_cast<const int64_t*>(dimData.constData());
            tensorElements *= dimSize;
            currentOffset += 8;
        }
        
        // Read tensor data type (int32_t = ggml_type)
        if (!m_file.seek(currentOffset)) break;
        QByteArray typeData = m_file.read(4);
        if (typeData.size() < 4) break;
        int32_t ggmlType = *reinterpret_cast<const int32_t*>(typeData.constData());
        currentOffset += 4;
        
        // Read tensor data offset (uint64_t)
        if (!m_file.seek(currentOffset)) break;
        QByteArray offsetData = m_file.read(8);
        if (offsetData.size() < 8) break;
        uint64_t dataOffset = *reinterpret_cast<const uint64_t*>(offsetData.constData());
        currentOffset += 8;
        
        // Calculate tensor size based on type and elements
        uint64_t bytesPerElement = 4; // Default F32
        switch (ggmlType) {
            case 0:  bytesPerElement = 4; break;  // F32
            case 1:  bytesPerElement = 2; break;  // F16
            case 2:  bytesPerElement = 1; break;  // Q4_0 (approx)
            case 3:  bytesPerElement = 1; break;  // Q4_1 (approx)
            case 6:  bytesPerElement = 1; break;  // Q5_0
            case 7:  bytesPerElement = 1; break;  // Q5_1
            case 8:  bytesPerElement = 1; break;  // Q8_0
            case 9:  bytesPerElement = 1; break;  // Q8_1
            default: bytesPerElement = 2; break;
        }
        uint64_t tensorSize = tensorElements * bytesPerElement;
        totalTensorBytes += tensorSize;
        
        // Assign to zone based on layer name
        QString zoneId = "default";
        if (tensorName.contains(".attn.")) zoneId = "attention";
        else if (tensorName.contains(".ffn.") || tensorName.contains(".mlp.")) zoneId = "feedforward";
        else if (tensorName.contains("embed") || tensorName.contains("token")) zoneId = "embedding";
        else if (tensorName.contains("output") || tensorName.contains("lm_head")) zoneId = "output";
        
        // Create tensor metadata
        TensorMetadata meta;
        meta.name = tensorName;
        meta.absolute_offset = dataOffset;
        meta.size_bytes = tensorSize;
        meta.zone_id = zoneId;
        meta.ggml_type = ggmlType;
        meta.ndims = nDims;
        
        m_tensorIndex[tensorName] = meta;
    }
    
    qint64 index_time_ms = timer.elapsed();
    
    QJsonObject resultLog;
    resultLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    resultLog["level"] = "INFO";
    resultLog["component"] = "StreamingGGUFLoader";
    resultLog["event"] = "tensor_index_built";
    resultLog["tensor_count"] = m_tensorIndex.size();
    resultLog["total_tensor_bytes_mb"] = totalTensorBytes / (1024.0 * 1024.0);
    resultLog["gguf_version"] = (int)version;
    resultLog["index_time_ms"] = index_time_ms;
    
    qInfo().noquote() << QJsonDocument(resultLog).toJson(QJsonDocument::Compact);
    
    return true;
}

void StreamingGGUFLoader::Close() {
    UnloadAll();
    
    if (m_file.isOpen()) {
        m_file.close();
        
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "file_closed";
        logEntry["model_name"] = m_modelName;
        
        qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    }
}

bool StreamingGGUFLoader::LoadZone(const QString& zoneName) {
    QElapsedTimer timer;
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
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "loading_zone";
    logEntry["zone_name"] = zoneName;
    
    qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    // Calculate zone boundaries from tensor index
    qint64 zoneStartOffset = LLONG_MAX;
    qint64 zoneEndOffset = 0;
    int tensorsInZone = 0;
    
    // Find all tensors belonging to this zone and calculate boundaries
    for (auto it = m_tensorIndex.constBegin(); it != m_tensorIndex.constEnd(); ++it) {
        if (it.value().zone_id == zoneName) {
            qint64 tensorStart = it.value().absolute_offset;
            qint64 tensorEnd = tensorStart + it.value().size_bytes;
            
            if (tensorStart < zoneStartOffset) zoneStartOffset = tensorStart;
            if (tensorEnd > zoneEndOffset) zoneEndOffset = tensorEnd;
            tensorsInZone++;
        }
    }
    
    // If no tensors found for zone, create a minimal zone
    if (tensorsInZone == 0 || zoneStartOffset >= zoneEndOffset) {
        qWarning() << "[StreamingGGUFLoader] No tensors found for zone:" << zoneName;
        // Create a minimal zone for compatibility
        zoneStartOffset = 0;
        zoneEndOffset = qMin(m_totalSize, (qint64)(1024 * 1024)); // 1MB fallback
    }
    
    ZoneMemory zone;
    zone.start_offset_in_file = zoneStartOffset;
    zone.size_bytes = zoneEndOffset - zoneStartOffset;
    zone.last_access_time = std::chrono::system_clock::now();
    zone.access_count = 1;
    
    // Use memory mapping for efficiency
    zone.mapped_data = m_file.map(zone.start_offset_in_file, zone.size_bytes);
    
    if (!zone.mapped_data) {
        QJsonObject errorLog;
        errorLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        errorLog["level"] = "ERROR";
        errorLog["component"] = "StreamingGGUFLoader";
        errorLog["event"] = "zone_map_failed";
        errorLog["zone_name"] = zoneName;
        errorLog["error"] = m_file.errorString();
        
        qCritical().noquote() << QJsonDocument(errorLog).toJson(QJsonDocument::Compact);
        emit ErrorOccurred(QString("Failed to map zone %1: %2").arg(zoneName, m_file.errorString()));
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
    
    QJsonObject resultLog;
    resultLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    resultLog["level"] = "INFO";
    resultLog["component"] = "StreamingGGUFLoader";
    resultLog["event"] = "zone_loaded";
    resultLog["zone_name"] = zoneName;
    resultLog["zone_size_mb"] = zone.size_bytes / (1024.0 * 1024.0);
    resultLog["load_time_ms"] = load_time_ms;
    resultLog["total_loaded_zones"] = m_loadedZones.size();
    resultLog["total_mapped_mb"] = m_metrics.total_bytes_mapped / (1024.0 * 1024.0);
    
    qInfo().noquote() << QJsonDocument(resultLog).toJson(QJsonDocument::Compact);
    
    emit ZoneLoaded(zoneName, load_time_ms);
    return true;
}

void StreamingGGUFLoader::UnloadZone(const QString& zoneName) {
    if (!m_loadedZones.contains(zoneName)) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    ZoneMemory zone = m_loadedZones.take(zoneName);
    
    if (zone.mapped_data) {
        const bool unmapped = m_file.unmap(zone.mapped_data);
        
        if (!unmapped) {
            QJsonObject errorLog;
            errorLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            errorLog["level"] = "WARNING";
            errorLog["component"] = "StreamingGGUFLoader";
            errorLog["event"] = "zone_unmap_failed";
            errorLog["zone_name"] = zoneName;
            
            qWarning().noquote() << QJsonDocument(errorLog).toJson(QJsonDocument::Compact);
        }
        
        m_metrics.total_bytes_mapped -= zone.size_bytes;
    }
    
    m_metrics.total_zones_evicted++;
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "zone_evicted";
    logEntry["zone_name"] = zoneName;
    logEntry["zone_size_mb"] = zone.size_bytes / (1024.0 * 1024.0);
    logEntry["access_count"] = (qint64)zone.access_count;
    logEntry["evict_time_ms"] = timer.elapsed();
    
    qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    emit ZoneEvicted(zoneName);
}

void StreamingGGUFLoader::UnloadAll() {
    const auto keys = m_loadedZones.keys();
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "unloading_all_zones";
    logEntry["zone_count"] = keys.size();
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    for (const auto& k : keys) {
        UnloadZone(k);
    }
}

void StreamingGGUFLoader::evictLeastRecentlyUsed() {
    if (m_loadedZones.isEmpty()) {
        return;
    }
    
    // Find zone with oldest access time
    QString lruZone;
    auto oldestTime = std::chrono::system_clock::now();
    
    for (auto it = m_loadedZones.begin(); it != m_loadedZones.end(); ++it) {
        if (it.value().last_access_time < oldestTime) {
            oldestTime = it.value().last_access_time;
            lruZone = it.key();
        }
    }
    
    if (!lruZone.isEmpty()) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "DEBUG";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "evicting_lru_zone";
        logEntry["zone_name"] = lruZone;
        
        qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        
        UnloadZone(lruZone);
    }
}

bool StreamingGGUFLoader::GetTensorData(const QString& tensorName, std::vector<uint8_t>& outData) {
    QElapsedTimer timer;
    timer.start();
    
    if (!m_tensorIndex.contains(tensorName)) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "WARNING";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "tensor_not_found";
        logEntry["tensor_name"] = tensorName;
        
        qWarning().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        
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
    
    // Copy tensor data from mapped memory to outData
    const ZoneMemory& zone = m_loadedZones[meta.zone_id];
    if (zone.mapped_data) {
        // Calculate offset within the zone
        qint64 offsetInZone = meta.absolute_offset - zone.start_offset_in_file;
        
        if (offsetInZone >= 0 && (offsetInZone + meta.size_bytes) <= zone.size_bytes) {
            // Copy from mapped memory
            outData.resize(meta.size_bytes);
            std::memcpy(outData.data(), zone.mapped_data + offsetInZone, meta.size_bytes);
        } else {
            // Offset out of range - fall back to direct file read
            QByteArray directData = readDataFromFile(meta.absolute_offset, meta.size_bytes);
            outData.resize(directData.size());
            std::memcpy(outData.data(), directData.constData(), directData.size());
        }
    } else {
        // No mapped data available - fall back to direct file read
        QByteArray directData = readDataFromFile(meta.absolute_offset, meta.size_bytes);
        outData.resize(directData.size());
        std::memcpy(outData.data(), directData.constData(), directData.size());
    }
    
    m_metrics.total_tensors_accessed++;
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "StreamingGGUFLoader";
    logEntry["event"] = "tensor_accessed";
    logEntry["tensor_name"] = tensorName;
    logEntry["tensor_size_bytes"] = (qint64)meta.size_bytes;
    logEntry["zone_id"] = meta.zone_id;
    logEntry["access_time_ms"] = timer.elapsed();
    
    qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    emit TensorAccessed(tensorName);
    
    return true;
}

bool StreamingGGUFLoader::HasTensor(const QString& tensorName) const {
    return m_tensorIndex.contains(tensorName);
}

QString StreamingGGUFLoader::getModelName() const { 
    return m_modelName; 
}

qint64 StreamingGGUFLoader::getTotalSize() const { 
    return m_totalSize; 
}

QByteArray StreamingGGUFLoader::readDataFromFile(qint64 offset, qint64 size) {
    if (!m_file.isOpen()) {
        return {};
    }
    
    if (!m_file.seek(offset)) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "StreamingGGUFLoader";
        logEntry["event"] = "seek_failed";
        logEntry["offset"] = offset;
        
        qCritical().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        return {};
    }
    
    return m_file.read(size);
}

void StreamingGGUFLoader::updateZoneAccessTime(const QString& zoneName) {
    if (m_loadedZones.contains(zoneName)) {
        m_loadedZones[zoneName].last_access_time = std::chrono::system_clock::now();
        m_loadedZones[zoneName].access_count++;
    }
}
