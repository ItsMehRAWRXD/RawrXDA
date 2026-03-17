#include "StreamingGGUFLoader.h"
#include <QDebug>

StreamingGGUFLoader::StreamingGGUFLoader(QObject* parent)
    : QObject(parent) {}

bool StreamingGGUFLoader::Open(const QString& filePath) {
    file_.setFileName(filePath);
    if (!file_.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        qWarning() << "StreamingGGUFLoader: failed to open" << filePath;
        return false;
    }
    totalSize_ = file_.size();
    modelName_ = QFileInfo(filePath).baseName();
    qDebug() << "StreamingGGUFLoader: Opened" << filePath << "size" << totalSize_;
    return true;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
    auto startTime = std::chrono::high_resolution_clock::now();
    qDebug() << "StreamingGGUFLoader: Building tensor index...";
    
    if (!file_.isOpen()) {
        qWarning() << "StreamingGGUFLoader: File not open, cannot build tensor index";
        return false;
    }
    
    // Reset to beginning
    if (!file_.seek(0)) {
        qWarning() << "StreamingGGUFLoader: Failed to seek to file start";
        return false;
    }
    
    // Read GGUF magic number (4 bytes: "GGUF")
    QByteArray magic = file_.read(4);
    if (magic.size() != 4 || magic != "GGUF") {
        qWarning() << "StreamingGGUFLoader: Invalid magic number:" << magic.toHex();
        return false;
    }
    
    // Read version (uint32_t)
    quint32 version = 0;
    if (file_.read(reinterpret_cast<char*>(&version), sizeof(version)) != sizeof(version)) {
        qWarning() << "StreamingGGUFLoader: Failed to read version";
        return false;
    }
    qDebug() << "GGUF version:" << version;
    
    // Read tensor count and metadata count (uint64_t each)
    quint64 tensorCount = 0, metadataCount = 0;
    if (file_.read(reinterpret_cast<char*>(&tensorCount), sizeof(tensorCount)) != sizeof(tensorCount)) {
        qWarning() << "StreamingGGUFLoader: Failed to read tensor count";
        return false;
    }
    if (file_.read(reinterpret_cast<char*>(&metadataCount), sizeof(metadataCount)) != sizeof(metadataCount)) {
        qWarning() << "StreamingGGUFLoader: Failed to read metadata count";
        return false;
    }
    qDebug() << "Tensor count:" << tensorCount << ", Metadata count:" << metadataCount;
    
    // Skip metadata key-value pairs (simplified - full parser would read these)
    // For production, metadata should be parsed to get model info
    for (quint64 i = 0; i < metadataCount; ++i) {
        // Read key length + key string
        quint64 keyLen = 0;
        if (file_.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen)) != sizeof(keyLen)) break;
        file_.seek(file_.pos() + keyLen);
        
        // Read value type and skip value (type-dependent size)
        quint32 valueType = 0;
        if (file_.read(reinterpret_cast<char*>(&valueType), sizeof(valueType)) != sizeof(valueType)) break;
        
        // Simplified: skip 8 bytes for most types (works for int/float, needs refinement for strings/arrays)
        file_.seek(file_.pos() + 8);
    }
    
    // Read tensor metadata
    quint64 currentDataOffset = file_.pos();
    for (quint64 i = 0; i < tensorCount; ++i) {
        TensorMetadata tensor;
        
        // Read tensor name (length + string)
        quint64 nameLen = 0;
        if (file_.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen)) != sizeof(nameLen)) {
            qWarning() << "Failed to read tensor name length at index" << i;
            break;
        }
        QByteArray nameBytes = file_.read(nameLen);
        if (nameBytes.size() != static_cast<int>(nameLen)) {
            qWarning() << "Failed to read tensor name at index" << i;
            break;
        }
        tensor.name = QString::fromUtf8(nameBytes);
        
        // Read number of dimensions
        if (file_.read(reinterpret_cast<char*>(&tensor.ndims), sizeof(tensor.ndims)) != sizeof(tensor.ndims)) {
            qWarning() << "Failed to read ndims for tensor" << tensor.name;
            break;
        }
        
        // Read dimension sizes (ndims * uint64_t)
        std::vector<quint64> dims(tensor.ndims);
        if (file_.read(reinterpret_cast<char*>(dims.data()), tensor.ndims * sizeof(quint64)) != tensor.ndims * sizeof(quint64)) {
            qWarning() << "Failed to read dimensions for tensor" << tensor.name;
            break;
        }
        
        // Read GGML type
        if (file_.read(reinterpret_cast<char*>(&tensor.ggml_type), sizeof(tensor.ggml_type)) != sizeof(tensor.ggml_type)) {
            qWarning() << "Failed to read type for tensor" << tensor.name;
            break;
        }
        
        // Read offset (relative to tensor data section start)
        quint64 relativeOffset = 0;
        if (file_.read(reinterpret_cast<char*>(&relativeOffset), sizeof(relativeOffset)) != sizeof(relativeOffset)) {
            qWarning() << "Failed to read offset for tensor" << tensor.name;
            break;
        }
        
        // Calculate tensor size (simplified - actual calculation depends on GGML type)
        quint64 elementCount = 1;
        for (quint32 d = 0; d < tensor.ndims; ++d) {
            elementCount *= dims[d];
        }
        // Assume 2 bytes per element for simplicity (needs type-specific logic)
        tensor.size_bytes = elementCount * 2;
        
        // Store absolute offset (will be set after header parsing)
        tensor.absolute_offset = currentDataOffset + relativeOffset;
        
        // Assign to zone based on offset (simple strategy: 256MB zones)
        tensor.zone_id = QString("zone_%1").arg(tensor.absolute_offset / (256 * 1024 * 1024));
        
        tensorIndex_[tensor.name] = tensor;
        qDebug() << "Indexed tensor:" << tensor.name << "size:" << tensor.size_bytes << "offset:" << tensor.absolute_offset;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qInfo() << "StreamingGGUFLoader: Indexed" << tensorIndex_.size() << "tensors in" << duration << "ms";
    
    return !tensorIndex_.isEmpty();
}

void StreamingGGUFLoader::Close() {
    UnloadAll();
    if (file_.isOpen()) file_.close();
}

bool StreamingGGUFLoader::LoadZone(const QString& zoneName) {
    auto startTime = std::chrono::high_resolution_clock::now();
    qDebug() << "StreamingGGUFLoader: Loading zone" << zoneName;
    
    if (loadedZones_.contains(zoneName)) {
        qDebug() << "Zone" << zoneName << "already loaded";
        return true;
    }
    
    if (!file_.isOpen()) {
        qWarning() << "StreamingGGUFLoader: File not open, cannot load zone";
        return false;
    }
    
    // Find all tensors in this zone
    quint64 minOffset = UINT64_MAX;
    quint64 maxOffset = 0;
    int tensorCount = 0;
    
    for (const auto& tensor : tensorIndex_) {
        if (tensor.zone_id == zoneName) {
            minOffset = qMin(minOffset, tensor.absolute_offset);
            maxOffset = qMax(maxOffset, tensor.absolute_offset + tensor.size_bytes);
            tensorCount++;
        }
    }
    
    if (tensorCount == 0) {
        qWarning() << "No tensors found in zone" << zoneName;
        return false;
    }
    
    // Compute zone boundaries (align to page boundaries for mmap efficiency)
    const quint64 PAGE_SIZE = 4096;
    quint64 alignedStart = (minOffset / PAGE_SIZE) * PAGE_SIZE;
    quint64 alignedSize = ((maxOffset - alignedStart + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    
    // Clamp to file size
    if (alignedStart + alignedSize > static_cast<quint64>(totalSize_)) {
        alignedSize = totalSize_ - alignedStart;
    }
    
    qDebug() << "Zone boundaries: start=" << alignedStart << "size=" << alignedSize << "tensors=" << tensorCount;
    
    // Memory map the zone
    uchar* mappedData = file_.map(alignedStart, alignedSize, QFile::MapPrivateOption);
    if (!mappedData) {
        qWarning() << "StreamingGGUFLoader: Failed to memory map zone" << zoneName;
        qWarning() << "Reason:" << file_.errorString();
        return false;
    }
    
    // Store zone info
    ZoneMemory zone;
    zone.start_offset_in_file = alignedStart;
    zone.size_bytes = alignedSize;
    zone.mapped_data = mappedData;
    loadedZones_[zoneName] = zone;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qInfo() << "StreamingGGUFLoader: Zone" << zoneName << "loaded (" << alignedSize / (1024.0 * 1024.0) << "MB) in" << duration << "ms";
    
    emit ZoneLoaded(zoneName);
    return true;
}

void StreamingGGUFLoader::UnloadZone(const QString& zoneName) {
    if (!loadedZones_.contains(zoneName)) return;
    ZoneMemory zone = loadedZones_.take(zoneName);
    if (zone.mapped_data) {
        const bool unmapped = file_.unmap(zone.mapped_data);
        if (!unmapped) qWarning() << "StreamingGGUFLoader: unmap failed for" << zoneName;
    }
    emit ZoneEvicted(zoneName);
}

void StreamingGGUFLoader::UnloadAll() {
    const auto keys = loadedZones_.keys();
    for (const auto& k : keys) UnloadZone(k);
}

bool StreamingGGUFLoader::GetTensorData(const QString& tensorName, std::vector<uint8_t>& outData) {
    auto startTime = std::chrono::high_resolution_clock::now();
    qDebug() << "StreamingGGUFLoader: Getting tensor data for" << tensorName;
    
    outData.clear();
    
    // Find tensor metadata
    if (!tensorIndex_.contains(tensorName)) {
        qWarning() << "StreamingGGUFLoader: Tensor" << tensorName << "not found in index";
        return false;
    }
    
    const TensorMetadata& tensor = tensorIndex_[tensorName];
    
    // Ensure zone is loaded
    if (!loadedZones_.contains(tensor.zone_id)) {
        qDebug() << "Zone" << tensor.zone_id << "not loaded, loading now...";
        if (!LoadZone(tensor.zone_id)) {
            qWarning() << "Failed to load zone" << tensor.zone_id << "for tensor" << tensorName;
            return false;
        }
    }
    
    const ZoneMemory& zone = loadedZones_[tensor.zone_id];
    
    // Calculate offset within mapped memory
    if (tensor.absolute_offset < zone.start_offset_in_file) {
        qWarning() << "Tensor offset" << tensor.absolute_offset << "before zone start" << zone.start_offset_in_file;
        return false;
    }
    
    quint64 offsetInZone = tensor.absolute_offset - zone.start_offset_in_file;
    
    if (offsetInZone + tensor.size_bytes > zone.size_bytes) {
        qWarning() << "Tensor data exceeds zone boundary";
        return false;
    }
    
    // Copy tensor data from mapped memory
    outData.resize(tensor.size_bytes);
    const uchar* tensorDataPtr = zone.mapped_data + offsetInZone;
    std::memcpy(outData.data(), tensorDataPtr, tensor.size_bytes);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    qDebug() << "StreamingGGUFLoader: Retrieved" << tensor.size_bytes << "bytes for tensor" << tensorName << "in" << duration << "µs";
    
    return true;
}

QString StreamingGGUFLoader::getModelName() const { return modelName_; }
qint64 StreamingGGUFLoader::getTotalSize() const { return totalSize_; }

QByteArray StreamingGGUFLoader::readDataFromFile(qint64 offset, qint64 size) {
    if (!file_.isOpen()) return {};
    if (!file_.seek(offset)) return {};
    return file_.read(size);
}
