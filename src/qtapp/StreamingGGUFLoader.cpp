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
    
    // ── Parse GGUF binary header ──────────────────────────────────────
    if (m_totalSize < 24) {
        qWarning() << "[StreamingGGUFLoader] File too small for GGUF header";
        return false;
    }
    
    m_file.seek(0);
    
    // Read magic + version
    uint32_t magic = 0, version = 0;
    m_file.read(reinterpret_cast<char*>(&magic), 4);
    m_file.read(reinterpret_cast<char*>(&version), 4);
    
    if (magic != 0x46554747) { // "GGUF" little-endian
        qWarning() << "[StreamingGGUFLoader] Invalid GGUF magic:" << Qt::hex << magic;
        return false;
    }
    
    // Read tensor count and KV count (version 2+: uint64)
    uint64_t nTensors = 0, nKV = 0;
    m_file.read(reinterpret_cast<char*>(&nTensors), 8);
    m_file.read(reinterpret_cast<char*>(&nKV), 8);
    
    qInfo() << "[StreamingGGUFLoader] GGUF v" << version
            << "tensors:" << nTensors << "kv_pairs:" << nKV;
    
    // ── Skip KV metadata section ─────────────────────────────────────
    // Walk KV pairs to find the end of metadata
    for (uint64_t i = 0; i < nKV && m_file.pos() < m_totalSize; ++i) {
        // Key: uint64 len + string bytes
        uint64_t keyLen = 0;
        m_file.read(reinterpret_cast<char*>(&keyLen), 8);
        m_file.seek(m_file.pos() + keyLen);
        
        // Value type: uint32
        uint32_t valueType = 0;
        m_file.read(reinterpret_cast<char*>(&valueType), 4);
        
        // Skip value based on type
        switch (valueType) {
            case 0: m_file.seek(m_file.pos() + 1); break;   // UINT8
            case 1: m_file.seek(m_file.pos() + 1); break;   // INT8
            case 2: m_file.seek(m_file.pos() + 2); break;   // UINT16
            case 3: m_file.seek(m_file.pos() + 2); break;   // INT16
            case 4: m_file.seek(m_file.pos() + 4); break;   // UINT32
            case 5: m_file.seek(m_file.pos() + 4); break;   // INT32
            case 6: m_file.seek(m_file.pos() + 4); break;   // FLOAT32
            case 7: m_file.seek(m_file.pos() + 1); break;   // BOOL
            case 8: { // STRING
                uint64_t sLen = 0;
                m_file.read(reinterpret_cast<char*>(&sLen), 8);
                m_file.seek(m_file.pos() + sLen);
                break;
            }
            case 9: { // ARRAY
                uint32_t arrType = 0;
                uint64_t arrLen = 0;
                m_file.read(reinterpret_cast<char*>(&arrType), 4);
                m_file.read(reinterpret_cast<char*>(&arrLen), 8);
                // Skip array elements (simplified: handle string arrays specially)
                for (uint64_t a = 0; a < arrLen; ++a) {
                    if (arrType == 8) { // string array
                        uint64_t sl = 0;
                        m_file.read(reinterpret_cast<char*>(&sl), 8);
                        m_file.seek(m_file.pos() + sl);
                    } else {
                        // Fixed-size types
                        static const int typeSizes[] = {1,1,2,2,4,4,4,1};
                        if (arrType < 8) m_file.seek(m_file.pos() + typeSizes[arrType]);
                        else m_file.seek(m_file.pos() + 4); // fallback
                    }
                }
                break;
            }
            case 10: m_file.seek(m_file.pos() + 8); break; // UINT64
            case 11: m_file.seek(m_file.pos() + 8); break; // INT64
            case 12: m_file.seek(m_file.pos() + 8); break; // FLOAT64
            default: m_file.seek(m_file.pos() + 4); break;  // unknown, guess 4
        }
    }
    
    qint64 tensorInfoStart = m_file.pos();
    
    // ── Parse tensor info entries ─────────────────────────────────────
    struct RawTensorInfo {
        QString name;
        uint32_t ndims;
        uint32_t type;
        uint64_t offset;
    };
    std::vector<RawTensorInfo> rawTensors;
    rawTensors.reserve(nTensors);
    
    for (uint64_t t = 0; t < nTensors && m_file.pos() < m_totalSize; ++t) {
        RawTensorInfo info;
        
        // Name: uint64 len + bytes
        uint64_t nameLen = 0;
        m_file.read(reinterpret_cast<char*>(&nameLen), 8);
        QByteArray nameBytes = m_file.read(nameLen);
        info.name = QString::fromUtf8(nameBytes);
        
        // ndims: uint32
        m_file.read(reinterpret_cast<char*>(&info.ndims), 4);
        
        // dimensions: ndims * uint64 (skip)
        m_file.seek(m_file.pos() + info.ndims * 8);
        
        // type: uint32
        m_file.read(reinterpret_cast<char*>(&info.type), 4);
        
        // offset from data section start: uint64
        m_file.read(reinterpret_cast<char*>(&info.offset), 8);
        
        rawTensors.push_back(info);
    }
    
    // ── Data section starts after alignment ───────────────────────────
    qint64 dataStart = m_file.pos();
    // GGUF requires 32-byte alignment for data section
    dataStart = (dataStart + 31) & ~31LL;
    
    // ── Build tensor index with zone assignment ──────────────────────
    // Assign tensors to zones based on layer grouping
    const quint64 zoneTargetSize = 64 * 1024 * 1024; // 64 MB per zone
    quint64 currentZoneStart = dataStart;
    quint64 currentZoneSize = 0;
    int zoneIdx = 0;
    QString currentZoneId = QString("zone_%1").arg(zoneIdx);
    
    for (size_t t = 0; t < rawTensors.size(); ++t) {
        const auto& rt = rawTensors[t];
        
        TensorMetadata meta;
        meta.name = rt.name;
        meta.ndims = rt.ndims;
        meta.ggml_type = rt.type;
        meta.absolute_offset = dataStart + rt.offset;
        
        // Estimate tensor size from type and offset delta
        if (t + 1 < rawTensors.size()) {
            meta.size_bytes = rawTensors[t + 1].offset - rt.offset;
        } else {
            meta.size_bytes = m_totalSize - meta.absolute_offset;
        }
        
        // Check if we should start a new zone
        if (currentZoneSize + meta.size_bytes > zoneTargetSize && currentZoneSize > 0) {
            ++zoneIdx;
            currentZoneId = QString("zone_%1").arg(zoneIdx);
            currentZoneStart = meta.absolute_offset;
            currentZoneSize = 0;
        }
        
        meta.zone_id = currentZoneId;
        currentZoneSize += meta.size_bytes;
        
        m_tensorIndex[meta.name] = meta;
    }
    
    qint64 index_time_ms = timer.elapsed();
    
    QJsonObject resultLog;
    resultLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    resultLog["level"] = "INFO";
    resultLog["component"] = "StreamingGGUFLoader";
    resultLog["event"] = "tensor_index_built";
    resultLog["tensor_count"] = m_tensorIndex.size();
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
    
    // Compute zone boundaries from tensor index
    quint64 zoneStart = UINT64_MAX;
    quint64 zoneEnd = 0;
    
    for (auto it = m_tensorIndex.constBegin(); it != m_tensorIndex.constEnd(); ++it) {
        if (it.value().zone_id == zoneName) {
            quint64 tStart = it.value().absolute_offset;
            quint64 tEnd = tStart + it.value().size_bytes;
            if (tStart < zoneStart) zoneStart = tStart;
            if (tEnd > zoneEnd) zoneEnd = tEnd;
        }
    }
    
    if (zoneStart >= zoneEnd || zoneStart == UINT64_MAX) {
        emit ErrorOccurred(QString("Zone %1 has no tensors in index").arg(zoneName));
        return false;
    }
    
    ZoneMemory zone;
    zone.start_offset_in_file = zoneStart;
    zone.size_bytes = zoneEnd - zoneStart;
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
    
    // Copy tensor data from mapped memory region
    const ZoneMemory& zmem = m_loadedZones[meta.zone_id];
    quint64 offsetInZone = meta.absolute_offset - zmem.start_offset_in_file;
    
    if (offsetInZone + meta.size_bytes > zmem.size_bytes || !zmem.mapped_data) {
        qWarning() << "[StreamingGGUFLoader] Tensor" << tensorName
                   << "exceeds zone bounds (offset" << offsetInZone
                   << "+ size" << meta.size_bytes << "> zone" << zmem.size_bytes << ")";
        outData.clear();
        return false;
    }
    
    outData.resize(meta.size_bytes);
    memcpy(outData.data(), zmem.mapped_data + offsetInZone, meta.size_bytes);
    
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
