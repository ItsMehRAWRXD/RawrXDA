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
    
    // TODO: Parse GGUF header and populate m_tensorIndex
    // For now, this is a stub implementation
    // In production, this would:
    // 1. Read GGUF magic number and version
    // 2. Parse metadata section
    // 3. Build tensor index with offsets and zones
    // 4. Assign tensors to zones based on layer/block structure
    
    // Stub: Create dummy tensor entries for demonstration
    // In real implementation, parse actual GGUF format
    
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
    
    // TODO: Copy tensor data from mapped memory to outData
    // For now, stub implementation
    outData.resize(meta.size_bytes);
    
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
