#include "ModelMetadataParser.h"
#include <QFile>
#include <QDebug>
#include <cstring>

uint32_t ModelMetadataParser::readU32(const QByteArray& data, size_t& offset) {
    if (offset + 4 > data.size()) return 0;
    uint32_t value = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;
    return value;
}

uint64_t ModelMetadataParser::readU64(const QByteArray& data, size_t& offset) {
    if (offset + 8 > data.size()) return 0;
    uint64_t value = *reinterpret_cast<const uint64_t*>(data.data() + offset);
    offset += 8;
    return value;
}

QString ModelMetadataParser::readString(const QByteArray& data, size_t& offset) {
    if (offset + 4 > data.size()) return QString();
    uint32_t len = readU32(data, offset);
    if (offset + len > data.size()) return QString();
    QString str = QString::fromUtf8(data.data() + offset, len);
    offset += len;
    return str;
}

ModelMetadataParser::GGUFHeader ModelMetadataParser::parseHeader(const QString& filePath) {
    GGUFHeader header{};
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return header;
    }
    
    QByteArray data = file.read(1024 * 1024); // Read first 1MB
    file.close();
    
    size_t offset = 0;
    header.magic = readU32(data, offset);
    
    if (header.magic != 0x46554747) { // 'GGUF'
        qWarning() << "Invalid GGUF magic:" << QString("0x%1").arg(header.magic, 0, 16);
        return header;
    }
    
    header.version = readU32(data, offset);
    uint64_t tensorCount = readU64(data, offset);
    uint64_t metadataCount = readU64(data, offset);
    
    // Parse metadata key-value pairs
    for (uint64_t i = 0; i < metadataCount && offset < data.size(); i++) {
        QString key = readString(data, offset);
        uint32_t valueType = readU32(data, offset);
        
        // Parse value based on type (simplified)
        if (valueType == 0) { // uint8
            offset += 1;
        } else if (valueType == 1) { // int8
            offset += 1;
        } else if (valueType == 2) { // uint16
            offset += 2;
        } else if (valueType == 3) { // int16
            offset += 2;
        } else if (valueType == 4) { // uint32
            uint32_t value = readU32(data, offset);
            header.metadata[key] = (int)value;
        } else if (valueType == 5) { // int32
            int32_t value = readU32(data, offset);
            header.metadata[key] = value;
        } else if (valueType == 8) { // string
            QString value = readString(data, offset);
            header.metadata[key] = value;
        }
    }
    
    header.tensorDataOffset = file.size();
    return header;
}

QVector<ModelMetadataParser::TensorInfo> ModelMetadataParser::parseTensors(const QString& filePath) {
    QVector<TensorInfo> tensors;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return tensors;
    }
    
    QByteArray data = file.read(10 * 1024 * 1024); // Read first 10MB for tensor metadata
    file.close();
    
    size_t offset = 0;
    uint32_t magic = readU32(data, offset);
    if (magic != 0x46554747) return tensors;
    
    uint32_t version = readU32(data, offset);
    uint64_t tensorCount = readU64(data, offset);
    uint64_t metadataCount = readU64(data, offset);
    
    // Skip metadata
    for (uint64_t i = 0; i < metadataCount && offset < data.size(); i++) {
        readString(data, offset);
        uint32_t valueType = readU32(data, offset);
        // Skip values (simplified)
        offset += 8; // Approximate
    }
    
    // Parse tensors
    for (uint64_t i = 0; i < tensorCount && offset < data.size(); i++) {
        TensorInfo tensor;
        tensor.name = readString(data, offset);
        
        uint32_t dims = readU32(data, offset);
        for (uint32_t j = 0; j < dims && offset < data.size(); j++) {
            tensor.shape.append(readU32(data, offset));
        }
        
        tensor.type = readU32(data, offset);
        tensor.offset = readU64(data, offset);
        
        // Calculate size
        tensor.sizeBytes = 1;
        for (uint32_t dim : tensor.shape) {
            tensor.sizeBytes *= dim;
        }
        
        tensors.append(tensor);
    }
    
    return tensors;
}

ModelMetadataParser::ModelInfo ModelMetadataParser::parseModelInfo(const QString& filePath) {
    ModelInfo info{};
    
    auto header = parseHeader(filePath);
    
    // Extract model info from metadata
    if (header.metadata.contains("general.name")) {
        info.name = header.metadata["general.name"].toString();
    }
    
    if (header.metadata.contains("general.architecture")) {
        info.architecture = header.metadata["general.architecture"].toString();
    }
    
    if (header.metadata.contains("llama.context_length")) {
        info.contextSize = header.metadata["llama.context_length"].toInt();
    }
    
    if (header.metadata.contains("llama.embedding_length")) {
        info.embeddingSize = header.metadata["llama.embedding_length"].toInt();
    }
    
    if (header.metadata.contains("llama.block_count")) {
        info.numLayers = header.metadata["llama.block_count"].toInt();
    }
    
    info.quantization = "Unknown";
    if (header.metadata.contains("general.quantization_version")) {
        info.quantization = "GGUF v" + QString::number(header.metadata["general.quantization_version"].toInt());
    }
    
    return info;
}

bool ModelMetadataParser::validateFile(const QString& filePath, QString& errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = "Cannot open file: " + filePath;
        return false;
    }
    
    QByteArray header = file.read(4);
    file.close();
    
    uint32_t magic = *reinterpret_cast<const uint32_t*>(header.data());
    if (magic != 0x46554747) { // 'GGUF'
        errorMessage = QString("Invalid GGUF magic: 0x%1").arg(magic, 0, 16);
        return false;
    }
    
    return true;
}
