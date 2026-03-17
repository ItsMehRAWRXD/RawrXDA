#include "gguf_parser.hpp"
#include <QDebug>
#include <QDataStream>
#include <cstring>

// Define QK_K for block size calculations
#ifndef QK_K
#define QK_K 256
#endif

GGUFParser::GGUFParser(const QString& filePath)
    : m_file(filePath), m_valid(false), m_version(0), m_tensorCount(0),
      m_metadataKVCount(0), m_tensorDataOffset(0)
{
    if (!m_file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open GGUF file:" << filePath;
        return;
    }
    
    if (!parseHeader()) {
        qWarning() << "Failed to parse GGUF header";
        m_file.close();
        return;
    }
    
    if (!parseMetadata()) {
        qWarning() << "Failed to parse GGUF metadata";
        m_file.close();
        return;
    }
    
    if (!parseTensorInfo()) {
        qWarning() << "Failed to parse GGUF tensor info";
        m_file.close();
        return;
    }
    
    m_valid = true;
    qInfo() << "GGUF parsed successfully:" << m_tensors.size() << "tensors";
}

GGUFParser::~GGUFParser()
{
    if (m_file.isOpen()) {
        m_file.close();
    }
}

bool GGUFParser::parseHeader()
{
    QDataStream stream(&m_file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read magic
    char magic[4];
    if (stream.readRawData(magic, 4) != 4) {
        return false;
    }
    
    if (std::memcmp(magic, "GGUF", 4) != 0) {
        qWarning() << "Invalid GGUF magic:" << QByteArray(magic, 4).toHex();
        return false;
    }
    
    // Read version
    stream >> m_version;
    if (m_version != 3 && m_version != 4) {
        qWarning() << "Unsupported GGUF version:" << m_version << "(expected 3 or 4)";
        return false;
    }
    
    // Read counts
    stream >> m_tensorCount;
    stream >> m_metadataKVCount;
    
    qDebug() << "GGUF v" << m_version << ":" << m_tensorCount << "tensors,"
             << m_metadataKVCount << "metadata entries";
    
    return true;
}

QString GGUFParser::readString(QDataStream& stream)
{
    uint64_t length;
    stream >> length;
    
    if (stream.status() != QDataStream::Ok) {
        qWarning() << "Stream error reading string length:" << stream.status();
        return QString();
    }
    
    // Sanity check: max 1MB strings
    if (length > 1024 * 1024) {
        qWarning() << "String length too large:" << length 
                   << "(file pos:" << m_file.pos() << ")";
        return QString();
    }
    
    if (length == 0) {
        return QString();
    }
    
    QByteArray data(static_cast<int>(length), Qt::Uninitialized);
    int bytesRead = stream.readRawData(data.data(), static_cast<int>(length));
    
    if (bytesRead != static_cast<int>(length)) {
        qWarning() << "Failed to read string: expected" << length << "got" << bytesRead
                   << "stream status:" << stream.status();
        return QString();
    }
    
    if (stream.status() != QDataStream::Ok) {
        qWarning() << "Stream error after reading string data:" << stream.status();
        return QString();
    }
    
    return QString::fromUtf8(data);
}

bool GGUFParser::parseMetadata()
{
    uint64_t keysProcessed = 0;
    uint64_t keysFailed = 0;
    uint64_t skipCount = 0;
    
    for (uint64_t i = 0; i < m_metadataKVCount; ++i) {
        // Read key length (uint64_t, little-endian)
        uint64_t keyLen = 0;
        char lenBuf[8];
        if (m_file.read(lenBuf, 8) != 8) {
            qWarning() << "Failed to read key length at index" << i << "- stopping parsing";
            break;
        }
        
        keyLen = static_cast<uint64_t>(
            (static_cast<uint8_t>(lenBuf[0])) |
            (static_cast<uint8_t>(lenBuf[1]) << 8) |
            (static_cast<uint8_t>(lenBuf[2]) << 16) |
            (static_cast<uint8_t>(lenBuf[3]) << 24) |
            (static_cast<uint64_t>(static_cast<uint8_t>(lenBuf[4])) << 32) |
            (static_cast<uint64_t>(static_cast<uint8_t>(lenBuf[5])) << 40) |
            (static_cast<uint64_t>(static_cast<uint8_t>(lenBuf[6])) << 48) |
            (static_cast<uint64_t>(static_cast<uint8_t>(lenBuf[7])) << 56)
        );
        
        if (keyLen == 0 || keyLen > 512) {
            qWarning() << "Invalid key length at index" << i << ":" << keyLen << "- skipping";
            keysFailed++;
            skipCount++;
            // Don't try to read key/value, just skip to next iteration
            // This will cause file offset corruption but better than crash
            if (skipCount > 5) break;  // Stop after too many skips
            continue;
        }
        
        QByteArray keyData(static_cast<int>(keyLen), Qt::Uninitialized);
        if (m_file.read(keyData.data(), static_cast<int>(keyLen)) != static_cast<int>(keyLen)) {
            qWarning() << "Failed to read key at index" << i;
            keysFailed++;
            continue;
        }
        
        QString key = QString::fromUtf8(keyData);
        
        // Read value type (uint32_t, little-endian)
        char typeBuf[4];
        if (m_file.read(typeBuf, 4) != 4) {
            qWarning() << "Failed to read value type for key:" << key;
            keysFailed++;
            continue;
        }
        
        uint32_t valueType = static_cast<uint32_t>(
            (static_cast<uint8_t>(typeBuf[0])) |
            (static_cast<uint8_t>(typeBuf[1]) << 8) |
            (static_cast<uint8_t>(typeBuf[2]) << 16) |
            (static_cast<uint8_t>(typeBuf[3]) << 24)
        );
        
        bool parseSuccess = false;
        
        // Parse value based on type
        switch (valueType) {
            case 0: case 1: case 7: {  // Uint8, Int8, Bool
                char buf[1];
                parseSuccess = (m_file.read(buf, 1) == 1);
                break;
            }
            case 2: case 3: {  // Uint16, Int16
                char buf[2];
                parseSuccess = (m_file.read(buf, 2) == 2);
                break;
            }
            case 4: {  // Uint32
                char buf[4];
                if (m_file.read(buf, 4) == 4) {
                    uint32_t val = static_cast<uint32_t>(
                        (static_cast<uint8_t>(buf[0])) |
                        (static_cast<uint8_t>(buf[1]) << 8) |
                        (static_cast<uint8_t>(buf[2]) << 16) |
                        (static_cast<uint8_t>(buf[3]) << 24)
                    );
                    if (key.endsWith(".vocab_size")) m_metadata.vocab_size = val;
                    else if (key.endsWith(".embedding_length")) m_metadata.n_embd = val;
                    else if (key.endsWith(".attention.head_count")) m_metadata.n_head = val;
                    else if (key.endsWith(".block_count")) m_metadata.n_layer = val;
                    else if (key.endsWith(".context_length")) m_metadata.n_ctx = val;
                    parseSuccess = true;
                }
                break;
            }
            case 5: case 6: {  // Int32, Float32
                char buf[4];
                parseSuccess = (m_file.read(buf, 4) == 4);
                break;
            }
            case 8: {  // String
                char strLenBuf[8];
                if (m_file.read(strLenBuf, 8) != 8) break;
                
                uint64_t strLen = static_cast<uint64_t>(
                    (static_cast<uint8_t>(strLenBuf[0])) |
                    (static_cast<uint8_t>(strLenBuf[1]) << 8) |
                    (static_cast<uint8_t>(strLenBuf[2]) << 16) |
                    (static_cast<uint8_t>(strLenBuf[3]) << 24) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[4])) << 32) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[5])) << 40) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[6])) << 48) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[7])) << 56)
                );
                
                if (strLen > 512 * 1024) break;
                
                QByteArray strData(static_cast<int>(strLen), Qt::Uninitialized);
                if (m_file.read(strData.data(), static_cast<int>(strLen)) == static_cast<int>(strLen)) {
                    QString strVal = QString::fromUtf8(strData);
                    if (key.endsWith(".architecture")) m_metadata.architecture = strVal;
                    else if (key.endsWith(".tokenizer.model")) m_metadata.tokenizer_model = strVal;
                    else m_metadata.custom_values[key] = strVal;
                    parseSuccess = true;
                }
                break;
            }
            case 9: {  // Array
                char atypeBuf[4];
                if (m_file.read(atypeBuf, 4) != 4) break;
                
                char alenBuf[8];
                if (m_file.read(alenBuf, 8) != 8) break;
                
                uint64_t arrayLen = static_cast<uint64_t>(
                    (static_cast<uint8_t>(alenBuf[0])) |
                    (static_cast<uint8_t>(alenBuf[1]) << 8) |
                    (static_cast<uint8_t>(alenBuf[2]) << 16) |
                    (static_cast<uint8_t>(alenBuf[3]) << 24) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(alenBuf[4])) << 32) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(alenBuf[5])) << 40) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(alenBuf[6])) << 48) |
                    (static_cast<uint64_t>(static_cast<uint8_t>(alenBuf[7])) << 56)
                );
                
                if (arrayLen > 1000000) break;
                
                uint32_t arrayType = static_cast<uint32_t>(
                    (static_cast<uint8_t>(atypeBuf[0])) |
                    (static_cast<uint8_t>(atypeBuf[1]) << 8) |
                    (static_cast<uint8_t>(atypeBuf[2]) << 16) |
                    (static_cast<uint8_t>(atypeBuf[3]) << 24)
                );
                
                // Skip array elements
                parseSuccess = true;
                for (uint64_t j = 0; j < arrayLen && parseSuccess; ++j) {
                    switch (arrayType) {
                        case 0: case 1: case 7: {  // 1-byte types
                            char buf;
                            if (m_file.read(&buf, 1) != 1) parseSuccess = false;
                            break;
                        }
                        case 2: case 3: {  // 2-byte types
                            char buf[2];
                            if (m_file.read(buf, 2) != 2) parseSuccess = false;
                            break;
                        }
                        case 4: case 5: case 6: {  // 4-byte types
                            char buf[4];
                            if (m_file.read(buf, 4) != 4) parseSuccess = false;
                            break;
                        }
                        case 8: {  // STRING type - each element has length prefix!
                            char strLenBuf[8];
                            if (m_file.read(strLenBuf, 8) != 8) {
                                parseSuccess = false;
                                break;
                            }
                            uint64_t elemStrLen = static_cast<uint64_t>(
                                (static_cast<uint8_t>(strLenBuf[0])) |
                                (static_cast<uint8_t>(strLenBuf[1]) << 8) |
                                (static_cast<uint8_t>(strLenBuf[2]) << 16) |
                                (static_cast<uint8_t>(strLenBuf[3]) << 24) |
                                (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[4])) << 32) |
                                (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[5])) << 40) |
                                (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[6])) << 48) |
                                (static_cast<uint64_t>(static_cast<uint8_t>(strLenBuf[7])) << 56)
                            );
                            
                            if (elemStrLen > 10000) {
                                qWarning() << "String array element too large at" << j << "len:" << elemStrLen;
                                parseSuccess = false;
                                break;
                            }
                            
                            if (elemStrLen > 0) {
                                QByteArray elemBuf(static_cast<int>(elemStrLen), Qt::Uninitialized);
                                if (m_file.read(elemBuf.data(), static_cast<int>(elemStrLen)) != static_cast<int>(elemStrLen)) {
                                    parseSuccess = false;
                                }
                            }
                            break;
                        }
                        case 10: case 11: case 12: {  // 8-byte types
                            char buf[8];
                            if (m_file.read(buf, 8) != 8) parseSuccess = false;
                            break;
                        }
                        default:
                            qWarning() << "Unknown array element type:" << arrayType;
                            parseSuccess = false;
                            break;
                    }
                }
                break;
            }
            case 10: case 11: case 12: {  // Uint64, Int64, Float64
                char buf[8];
                parseSuccess = (m_file.read(buf, 8) == 8);
                break;
            }
            default:
                qWarning() << "Unknown metadata value type" << valueType << "for key:" << key;
                parseSuccess = false;
                break;
        }
        
        if (parseSuccess) {
            keysProcessed++;
        } else {
            keysFailed++;
        }
    }
    
    qInfo() << "Metadata: processed=" << keysProcessed << "failed=" << keysFailed
            << "arch=" << m_metadata.architecture << "vocab=" << m_metadata.vocab_size 
            << "layers=" << m_metadata.n_layer
            << "file pos after metadata:" << m_file.pos();
    
    // GGUF v4: Check for hybrid quantization metadata
    if (m_version >= 4) {
        // Check for schema version (indicates v4 support)
        if (m_metadata.custom_values.contains("quantization_schema_version")) {
            bool ok;
            uint32_t schemaVer = m_metadata.custom_values["quantization_schema_version"].toUInt(&ok);
            if (ok && schemaVer >= 2) {
                m_metadata.schema_version = 2;
                qInfo() << "GGUF v4: Schema version" << schemaVer;
                
                // Check for quantization mode
                if (m_metadata.custom_values.contains("quantization_mode")) {
                    QString modeStr = m_metadata.custom_values["quantization_mode"];
                    if (modeStr == "HYBRID" || modeStr == "MIXED") {
                        m_metadata.quantization_mode = QuantizationMode::HYBRID;
                        
                        // Try to read tensor quantization map
                        if (m_metadata.custom_values.contains("quantization_tensor_map_offset") &&
                            m_metadata.custom_values.contains("quantization_tensor_map_count")) {
                            
                            bool ok1, ok2;
                            uint64_t mapOffset = m_metadata.custom_values["quantization_tensor_map_offset"].toULongLong(&ok1);
                            uint32_t mapCount = m_metadata.custom_values["quantization_tensor_map_count"].toUInt(&ok2);
                            
                            if (ok1 && ok2) {
                                qInfo() << "GGUF v4: Reading tensor quant map at offset" << mapOffset 
                                        << "with" << mapCount << "entries";
                                readTensorQuantMap(mapOffset, mapCount);
                            }
                        }
                    }
                }
            }
        }
    }
    
    return keysProcessed > 0 || m_metadataKVCount == 0;
}

bool GGUFParser::parseTensorInfo()
{
    m_tensors.reserve(m_tensorCount);
    
    for (uint64_t i = 0; i < m_tensorCount; ++i) {
        GGUFTensorInfo info;
        
        // Read tensor name (string: uint64_t length + data)
        char nameLenBuf[8];
        if (m_file.read(nameLenBuf, 8) != 8) {
            qWarning() << "Failed to read tensor name length at index" << i;
            return false;
        }
        
        uint64_t nameLen = static_cast<uint64_t>(
            (static_cast<uint8_t>(nameLenBuf[0])) |
            (static_cast<uint8_t>(nameLenBuf[1]) << 8) |
            (static_cast<uint8_t>(nameLenBuf[2]) << 16) |
            (static_cast<uint8_t>(nameLenBuf[3]) << 24) |
            (static_cast<uint64_t>(static_cast<uint8_t>(nameLenBuf[4])) << 32) |
            (static_cast<uint64_t>(static_cast<uint8_t>(nameLenBuf[5])) << 40) |
            (static_cast<uint64_t>(static_cast<uint8_t>(nameLenBuf[6])) << 48) |
            (static_cast<uint64_t>(static_cast<uint8_t>(nameLenBuf[7])) << 56)
        );
        
        if (nameLen > 256) {
            qWarning() << "Invalid tensor name length at index" << i << ":" << nameLen;
            return false;
        }
        
        QByteArray nameData(static_cast<int>(nameLen), Qt::Uninitialized);
        if (m_file.read(nameData.data(), static_cast<int>(nameLen)) != static_cast<int>(nameLen)) {
            qWarning() << "Failed to read tensor name data at index" << i;
            return false;
        }
        
        info.name = QString::fromUtf8(nameData);
        
        // Read n_dims (uint32_t)
        char dimsBuf[4];
        if (m_file.read(dimsBuf, 4) != 4) {
            qWarning() << "Failed to read n_dims for tensor:" << info.name;
            return false;
        }
        
        info.n_dims = static_cast<uint32_t>(
            (static_cast<uint8_t>(dimsBuf[0])) |
            (static_cast<uint8_t>(dimsBuf[1]) << 8) |
            (static_cast<uint8_t>(dimsBuf[2]) << 16) |
            (static_cast<uint8_t>(dimsBuf[3]) << 24)
        );
        
        if (info.n_dims > 4) {
            qWarning() << "Invalid tensor dimensions:" << info.n_dims << "for:" << info.name;
            return false;
        }
        
        // Read dimensions (uint64_t each)
        info.dimensions.resize(info.n_dims);
        for (uint32_t d = 0; d < info.n_dims; ++d) {
            char dimBuf[8];
            if (m_file.read(dimBuf, 8) != 8) {
                qWarning() << "Failed to read dimension" << d << "for tensor:" << info.name;
                return false;
            }
            
            info.dimensions[d] = static_cast<uint64_t>(
                (static_cast<uint8_t>(dimBuf[0])) |
                (static_cast<uint8_t>(dimBuf[1]) << 8) |
                (static_cast<uint8_t>(dimBuf[2]) << 16) |
                (static_cast<uint8_t>(dimBuf[3]) << 24) |
                (static_cast<uint64_t>(static_cast<uint8_t>(dimBuf[4])) << 32) |
                (static_cast<uint64_t>(static_cast<uint8_t>(dimBuf[5])) << 40) |
                (static_cast<uint64_t>(static_cast<uint8_t>(dimBuf[6])) << 48) |
                (static_cast<uint64_t>(static_cast<uint8_t>(dimBuf[7])) << 56)
            );
        }
        
        // Read type (uint32_t)
        char typeBuf[4];
        if (m_file.read(typeBuf, 4) != 4) {
            qWarning() << "Failed to read tensor type for:" << info.name;
            return false;
        }
        
        uint32_t typeValue = static_cast<uint32_t>(
            (static_cast<uint8_t>(typeBuf[0])) |
            (static_cast<uint8_t>(typeBuf[1]) << 8) |
            (static_cast<uint8_t>(typeBuf[2]) << 16) |
            (static_cast<uint8_t>(typeBuf[3]) << 24)
        );
        info.type = static_cast<GGMLType>(typeValue);
        
        // Read offset (uint64_t)
        char offsetBuf[8];
        if (m_file.read(offsetBuf, 8) != 8) {
            qWarning() << "Failed to read tensor offset for:" << info.name;
            return false;
        }
        
        info.offset = static_cast<uint64_t>(
            (static_cast<uint8_t>(offsetBuf[0])) |
            (static_cast<uint8_t>(offsetBuf[1]) << 8) |
            (static_cast<uint8_t>(offsetBuf[2]) << 16) |
            (static_cast<uint8_t>(offsetBuf[3]) << 24) |
            (static_cast<uint64_t>(static_cast<uint8_t>(offsetBuf[4])) << 32) |
            (static_cast<uint64_t>(static_cast<uint8_t>(offsetBuf[5])) << 40) |
            (static_cast<uint64_t>(static_cast<uint8_t>(offsetBuf[6])) << 48) |
            (static_cast<uint64_t>(static_cast<uint8_t>(offsetBuf[7])) << 56)
        );
        
        // Calculate size based on type and dimensions
        uint64_t numElements = 1;
        for (uint64_t dim : info.dimensions) {
            numElements *= dim;
        }
        
        uint64_t blockSize = typeSize(info.type);
        if (blockSize > 0) {
            // For quantized types, round up to block boundary
            uint64_t blocks = (numElements + QK_K - 1) / QK_K;
            info.size = blocks * blockSize;
        } else {
            // For F32/F16
            info.size = numElements * (info.type == GGMLType::F32 ? 4 : 2);
        }
        
        m_tensors.append(info);
        m_tensorIndex[info.name] = m_tensors.size() - 1;
    }
    
    // Calculate where tensor data starts (alignment to 32 bytes)
    m_tensorDataOffset = m_file.pos();
    uint64_t alignment = 32;
    uint64_t remainder = m_tensorDataOffset % alignment;
    if (remainder != 0) {
        m_tensorDataOffset += (alignment - remainder);
    }
    
    qDebug() << "Tensor info parsed - count:" << m_tensors.size()
             << "data offset:" << m_tensorDataOffset;
    
    return true;
}

GGUFTensorInfo GGUFParser::tensorInfo(const QString& name) const
{
    int idx = m_tensorIndex.value(name, -1);
    if (idx >= 0 && idx < m_tensors.size()) {
        return m_tensors[idx];
    }
    return GGUFTensorInfo();
}

bool GGUFParser::hasTensor(const QString& name) const
{
    return m_tensorIndex.contains(name);
}

QByteArray GGUFParser::readTensorData(const QString& tensorName)
{
    GGUFTensorInfo info = tensorInfo(tensorName);
    if (info.name.isEmpty()) {
        qWarning() << "Tensor not found:" << tensorName;
        return QByteArray();
    }
    return readTensorData(info);
}

QByteArray GGUFParser::readTensorData(const GGUFTensorInfo& info)
{
    if (!m_valid || !m_file.isOpen()) {
        qWarning() << "Parser not valid or file not open";
        return QByteArray();
    }
    
    if (info.size == 0) {
        qWarning() << "Tensor size is zero for:" << info.name;
        return QByteArray();
    }
    
    if (info.size > static_cast<uint64_t>(16) * 1024 * 1024 * 1024) {  // Sanity check: max 16GB
        qWarning() << "Tensor size too large:" << info.size << "for:" << info.name;
        return QByteArray();
    }
    
    uint64_t fileOffset = m_tensorDataOffset + info.offset;
    
    // Verify the offset is reasonable
    if (fileOffset > m_file.size()) {
        qWarning() << "Tensor offset beyond file size:" << fileOffset << "file size:" << m_file.size();
        return QByteArray();
    }
    
    if (!m_file.seek(fileOffset)) {
        qWarning() << "Failed to seek to tensor data:" << info.name 
                   << "offset:" << fileOffset
                   << "error:" << m_file.errorString();
        return QByteArray();
    }
    
    QByteArray data(static_cast<int>(info.size), Qt::Uninitialized);
    qint64 bytesRead = m_file.read(data.data(), static_cast<qint64>(info.size));
    
    if (bytesRead != static_cast<qint64>(info.size)) {
        qWarning() << "Failed to read tensor data:" << info.name
                   << "expected:" << info.size 
                   << "got:" << bytesRead
                   << "error:" << m_file.errorString();
        return QByteArray();
    }
    
    qDebug() << "Successfully read tensor:" << info.name 
             << "size:" << bytesRead << "bytes"
             << "type:" << typeName(info.type);
    
    return data;
}

QString GGUFParser::typeName(GGMLType type)
{
    switch (type) {
        case GGMLType::F32: return "F32";
        case GGMLType::F16: return "F16";
        case GGMLType::Q4_0: return "Q4_0";
        case GGMLType::Q4_1: return "Q4_1";
        case GGMLType::Q5_0: return "Q5_0";
        case GGMLType::Q5_1: return "Q5_1";
        case GGMLType::Q8_0: return "Q8_0";
        case GGMLType::Q8_1: return "Q8_1";
        case GGMLType::Q2_K: return "Q2_K";
        case GGMLType::Q3_K: return "Q3_K";
        case GGMLType::Q4_K: return "Q4_K";
        case GGMLType::Q5_K: return "Q5_K";
        case GGMLType::Q6_K: return "Q6_K";
        case GGMLType::Q8_K: return "Q8_K";
        default: return "Unknown";
    }
}

uint64_t GGUFParser::typeSize(GGMLType type)
{
    // Return block size in bytes for quantized types
    // QK_K = 256 elements per block
    switch (type) {
        case GGMLType::F32: return 0;  // Not block-based
        case GGMLType::F16: return 0;  // Not block-based
        case GGMLType::Q4_0: return 18;   // 2 + 16 (16 values * 0.5 bytes)
        case GGMLType::Q4_1: return 20;   // 4 + 16
        case GGMLType::Q5_0: return 22;
        case GGMLType::Q5_1: return 24;
        case GGMLType::Q8_0: return 34;   // 2 + 32
        case GGMLType::Q8_1: return 36;
        case GGMLType::Q2_K: return 84;   // 16 + 64 + 2 + 2
        case GGMLType::Q3_K: return 110;  // 32 + 64 + 12 + 2
        case GGMLType::Q4_K: return 144;
        case GGMLType::Q5_K: return 176;
        case GGMLType::Q6_K: return 210;
        case GGMLType::Q8_K: return 292;
        default: return 0;
    }
}

// ============ GGUF v4 Hybrid Quantization Support ============

uint32_t GGUFParser::hashTensorName(const QString& tensorName) const
{
    // CRC32 hash for fast O(1) tensor lookup
    uint32_t crc = 0xFFFFFFFF;
    QByteArray nameBytes = tensorName.toUtf8();
    
    for (unsigned char c : nameBytes) {
        crc ^= c;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool GGUFParser::readTensorQuantMap(uint64_t offset, uint32_t count)
{
    // Seek to tensor quantization map
    if (!m_file.seek(offset)) {
        qWarning() << "Failed to seek to tensor quantization map at offset" << offset;
        return false;
    }
    
    // Verify magic bytes
    char magic[4];
    if (m_file.read(magic, 4) != 4 || std::memcmp(magic, "TQMP", 4) != 0) {
        qWarning() << "Invalid tensor quantization map magic (expected TQMP)";
        return false;
    }
    
    // Read and verify entry count
    char countBuf[4];
    if (m_file.read(countBuf, 4) != 4) {
        qWarning() << "Failed to read tensor quantization map count";
        return false;
    }
    
    uint32_t actualCount = static_cast<uint32_t>(
        (static_cast<uint8_t>(countBuf[0])) |
        (static_cast<uint8_t>(countBuf[1]) << 8) |
        (static_cast<uint8_t>(countBuf[2]) << 16) |
        (static_cast<uint8_t>(countBuf[3]) << 24)
    );
    
    if (actualCount != count) {
        qWarning() << "Tensor quantization map count mismatch: expected" << count 
                   << "got" << actualCount;
        return false;
    }
    
    // Read all entries (16 bytes each: 4 hash + 4 type + 2 flags + 6 reserved)
    for (uint32_t i = 0; i < count; ++i) {
        TensorQuantMeta meta;
        
        // name_hash (uint32)
        char hashBuf[4];
        if (m_file.read(hashBuf, 4) != 4) {
            qWarning() << "Failed to read tensor hash at index" << i;
            return false;
        }
        meta.name_hash = static_cast<uint32_t>(
            (static_cast<uint8_t>(hashBuf[0])) |
            (static_cast<uint8_t>(hashBuf[1]) << 8) |
            (static_cast<uint8_t>(hashBuf[2]) << 16) |
            (static_cast<uint8_t>(hashBuf[3]) << 24)
        );
        
        // quantization_type (uint8)
        char typeBuf[1];
        if (m_file.read(typeBuf, 1) != 1) {
            qWarning() << "Failed to read quantization type at index" << i;
            return false;
        }
        meta.quantization_type = static_cast<GGMLType>(static_cast<uint8_t>(typeBuf[0]));
        
        // reserved (uint8)
        char reserved;
        if (m_file.read(&reserved, 1) != 1) {
            qWarning() << "Failed to read reserved byte at index" << i;
            return false;
        }
        
        // flags (uint16)
        char flagsBuf[2];
        if (m_file.read(flagsBuf, 2) != 2) {
            qWarning() << "Failed to read flags at index" << i;
            return false;
        }
        meta.flags = static_cast<uint16_t>(
            (static_cast<uint8_t>(flagsBuf[0])) |
            (static_cast<uint8_t>(flagsBuf[1]) << 8)
        );
        
        // Store in map
        m_tensorQuantMap[meta.name_hash] = meta;
    }
    
    qInfo() << "Successfully loaded" << count << "tensor quantization map entries";
    return true;
}

GGMLType GGUFParser::getTensorQuantType(const QString& tensorName) const
{
    // First try to find in explicit tensor map (GGUF v4 hybrid)
    uint32_t hash = hashTensorName(tensorName);
    auto it = m_tensorQuantMap.find(hash);
    
    if (it != m_tensorQuantMap.end()) {
        return it.value().quantization_type;
    }
    
    // Fallback: infer from name patterns (for compatibility)
    return inferQuantTypeFromName(tensorName);
}

GGMLType GGUFParser::inferQuantTypeFromName(const QString& tensorName) const
{
    // Layer pattern matching for automatic inference
    QString lower = tensorName.toLower();
    
    // Attention layers → Q4_K (critical path, needs precision)
    if (lower.contains("attn") || lower.contains("attention") ||
        lower.contains("q_proj") || lower.contains("k_proj") ||
        lower.contains("v_proj") || lower.contains("out_proj")) {
        return GGMLType::Q4_K;
    }
    
    // FFN layers → Q2_K (less sensitive to quantization)
    if (lower.contains("ffn") || lower.contains("feed_forward") ||
        lower.contains("mlp") || lower.contains("gate_proj") ||
        lower.contains("up_proj") || lower.contains("down_proj")) {
        return GGMLType::Q2_K;
    }
    
    // Embeddings → Q2_K (rarely accessed during inference)
    if (lower.contains("embed")) {
        return GGMLType::Q2_K;
    }
    
    // Normalization → Q2_K (low precision sufficient for scales)
    if (lower.contains("norm") || lower.contains("rms") ||
        lower.contains("layer_norm")) {
        return GGMLType::Q2_K;
    }
    
    // Default: assume uniform quantization
    // For v3 files, this is encoded in metadata
    // For v4 uniform files, all tensors use same format
    return GGMLType::Q4_K;
}

const QHash<uint32_t, TensorQuantMeta>& GGUFParser::getTensorQuantMap() const
{
    return m_tensorQuantMap;
}
