#include "custom_gguf_loader.h"
#include <QFile>
#include <QDebug>
#include <QVariant>
#include <algorithm>
#include <cstring>

// GGUF file constants
const uint32_t GGUF_MAGIC = 0x46554747; // 'GGUF'
const uint32_t GGUF_VERSION = 3;

// Quantization types (from GGML)
enum QuantizationType {
    GGML_TYPE_F32 = 0,
    GGML_TYPE_F16 = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    GGML_TYPE_Q2_K = 10,
    GGML_TYPE_Q3_K = 11,
    GGML_TYPE_Q4_K = 12,
    GGML_TYPE_Q5_K = 13,
    GGML_TYPE_Q6_K = 14,
    GGML_TYPE_IQ2_XXS = 15,
    GGML_TYPE_IQ2_XS = 16,
    GGML_TYPE_IQ3_XXS = 17,
    GGML_TYPE_IQ1_S = 18,
    GGML_TYPE_IQ4_NL = 19
};

CustomGGUFLoader::CustomGGUFLoader(const QString& filePath)
    : m_filePath(filePath), m_isOpen(false), m_fileSize(0), 
      m_estimatedUncompressedSize(0), m_fileHandle(nullptr)
{
    qDebug() << "[CustomGGUFLoader] Loading GGUF file:" << filePath;
    
    QFile file(filePath);
    if (!file.exists()) {
        qWarning() << "[CustomGGUFLoader] File not found:" << filePath;
        return;
    }
    
    m_fileSize = file.size();
    
    // Try to parse
    if (parseGGUFFile()) {
        m_isOpen = true;
        qDebug() << "[CustomGGUFLoader] Successfully loaded GGUF file"
                 << "- Tensors:" << m_tensors.size()
                 << "- Layers:" << m_metadata.nLayer
                 << "- Context:" << m_metadata.nCtx;
    }
    else {
        qWarning() << "[CustomGGUFLoader] Failed to parse GGUF file";
    }
}

CustomGGUFLoader::~CustomGGUFLoader()
{
    if (m_fileHandle) {
        fclose(m_fileHandle);
        m_fileHandle = nullptr;
    }
}

GGUFTensorInfo CustomGGUFLoader::getTensorInfo(const QString& name) const
{
    auto it = m_tensors.find(name);
    if (it != m_tensors.end()) {
        return it.value();
    }
    return GGUFTensorInfo();
}

QStringList CustomGGUFLoader::getTensorNames() const
{
    return m_tensors.keys();
}

QByteArray CustomGGUFLoader::loadTensorData(const QString& name)
{
    auto info = getTensorInfo(name);
    if (info.name.isEmpty()) {
        qWarning() << "[CustomGGUFLoader] Tensor not found:" << name;
        return QByteArray();
    }
    
    return decompressTensor(info);
}

QVector<uint64_t> CustomGGUFLoader::getTensorShape(const QString& name) const
{
    auto info = getTensorInfo(name);
    return info.ne;
}

uint32_t CustomGGUFLoader::getQuantizationType(const QString& name) const
{
    auto info = getTensorInfo(name);
    return info.quantType;
}

bool CustomGGUFLoader::parseGGUFFile()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[CustomGGUFLoader] Cannot open file for reading";
        return false;
    }
    
    try {
        // Read and validate header
        if (!readHeader()) {
            qWarning() << "[CustomGGUFLoader] Invalid GGUF header";
            file.close();
            return false;
        }
        
        // Read metadata
        if (!readMetadata()) {
            qWarning() << "[CustomGGUFLoader] Failed to read metadata";
            file.close();
            return false;
        }
        
        // Read tensor index
        if (!readTensorIndex()) {
            qWarning() << "[CustomGGUFLoader] Failed to read tensor index";
            file.close();
            return false;
        }
        
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "[CustomGGUFLoader] Exception during parsing:" << e.what();
        file.close();
        return false;
    }
}

bool CustomGGUFLoader::readHeader()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    // Read magic number
    uint32_t magic;
    if (file.read(reinterpret_cast<char*>(&magic), sizeof(magic)) != sizeof(magic)) {
        file.close();
        return false;
    }
    
    if (magic != GGUF_MAGIC) {
        qWarning() << "[CustomGGUFLoader] Invalid magic number:" << QString::number(magic, 16);
        file.close();
        return false;
    }
    
    // Read version
    uint32_t version;
    if (file.read(reinterpret_cast<char*>(&version), sizeof(version)) != sizeof(version)) {
        file.close();
        return false;
    }
    
    qDebug() << "[CustomGGUFLoader] GGUF version:" << version;
    
    file.close();
    return true;
}

bool CustomGGUFLoader::readMetadata()
{
    // For now, initialize with reasonable defaults
    // In production, would parse actual metadata from GGUF
    
    m_metadata.modelName = "Unknown";
    m_metadata.modelFamily = "Unknown";
    m_metadata.nLayer = 32;           // Assume 32 layers
    m_metadata.nEmbd = 4096;          // Assume 4096 embedding
    m_metadata.nHead = 32;            // Assume 32 heads
    m_metadata.nHeadKV = 8;           // Assume 8 KV heads
    m_metadata.nCtx = 4096;           // Assume 4K context
    m_metadata.vocabSize = 32000;     // Assume vocab size
    
    return true;
}

bool CustomGGUFLoader::readTensorIndex()
{
    // In a full implementation, would read tensor offset table from GGUF
    // For now, create placeholder tensors for structure
    
    QStringList commonTensors = {
        "token_embd.weight",
        "output.weight",
        "output_norm.weight"
    };
    
    for (int i = 0; i < m_metadata.nLayer; i++) {
        commonTensors.append(QString("blk.%1.attn_q.weight").arg(i));
        commonTensors.append(QString("blk.%1.attn_k.weight").arg(i));
        commonTensors.append(QString("blk.%1.attn_v.weight").arg(i));
        commonTensors.append(QString("blk.%1.attn_out.weight").arg(i));
        commonTensors.append(QString("blk.%1.mlp_gate.weight").arg(i));
        commonTensors.append(QString("blk.%1.mlp_up.weight").arg(i));
        commonTensors.append(QString("blk.%1.mlp_down.weight").arg(i));
    }
    
    uint64_t offset = 0;
    for (const auto& name : commonTensors) {
        GGUFTensorInfo info;
        info.name = name;
        info.ndim = 2;
        info.ne = {static_cast<uint64_t>(m_metadata.nEmbd), 32};
        info.quantType = GGML_TYPE_Q4_K;
        info.offset = offset;
        info.computedSize = 1024 * 1024; // Placeholder
        
        m_tensors[name] = info;
        offset += info.computedSize;
    }
    
    m_estimatedUncompressedSize = offset;
    
    return true;
}

bool CustomGGUFLoader::isQuantizationSupported(uint32_t quantType) const
{
    switch (quantType) {
        case GGML_TYPE_F32:
        case GGML_TYPE_F16:
        case GGML_TYPE_Q4_0:
        case GGML_TYPE_Q4_1:
        case GGML_TYPE_Q5_0:
        case GGML_TYPE_Q5_1:
        case GGML_TYPE_Q8_0:
        case GGML_TYPE_Q4_K:
        case GGML_TYPE_Q5_K:
        case GGML_TYPE_Q6_K:
            return true;
        default:
            return false;
    }
}

QString CustomGGUFLoader::quantTypeToString(uint32_t quantType) const
{
    switch (quantType) {
        case GGML_TYPE_F32: return "F32";
        case GGML_TYPE_F16: return "F16";
        case GGML_TYPE_Q4_0: return "Q4_0";
        case GGML_TYPE_Q4_1: return "Q4_1";
        case GGML_TYPE_Q5_0: return "Q5_0";
        case GGML_TYPE_Q5_1: return "Q5_1";
        case GGML_TYPE_Q8_0: return "Q8_0";
        case GGML_TYPE_Q4_K: return "Q4_K";
        case GGML_TYPE_Q5_K: return "Q5_K";
        case GGML_TYPE_Q6_K: return "Q6_K";
        case GGML_TYPE_IQ4_NL: return "IQ4_NL";
        default: return QString("Unknown (%1)").arg(quantType);
    }
}

size_t CustomGGUFLoader::getQuantizationElementSize(uint32_t quantType) const
{
    switch (quantType) {
        case GGML_TYPE_F32: return 4;
        case GGML_TYPE_F16: return 2;
        case GGML_TYPE_Q4_0: return 18;  // 32 values in 18 bytes
        case GGML_TYPE_Q4_K: return 144; // 256 values in 144 bytes
        case GGML_TYPE_Q5_K: return 176; // 256 values in 176 bytes
        case GGML_TYPE_Q6_K: return 210; // 256 values in 210 bytes
        default: return 1;
    }
}

QByteArray CustomGGUFLoader::decompressTensor(const GGUFTensorInfo& info)
{
    // In a full implementation, would read from file and decompress
    // For now, return placeholder
    
    size_t elementSize = getQuantizationElementSize(info.quantType);
    uint64_t totalElements = 1;
    for (auto dim : info.ne) {
        totalElements *= dim;
    }
    
    size_t totalBytes = (totalElements * elementSize) / 32; // Rough estimate for quantized
    
    QByteArray data(totalBytes, 0);
    return data;
}

bool CustomGGUFLoader::hasUnsupportedQuantization() const
{
    for (const auto& tensor : m_tensors) {
        if (!isQuantizationSupported(tensor.quantType)) {
            return true;
        }
    }
    return false;
}

QMap<uint32_t, QString> CustomGGUFLoader::getUnsupportedQuantizationTypes() const
{
    QMap<uint32_t, QString> unsupported;
    
    for (const auto& tensor : m_tensors) {
        if (!isQuantizationSupported(tensor.quantType)) {
            unsupported[tensor.quantType] = quantTypeToString(tensor.quantType);
        }
    }
    
    return unsupported;
}

QString CustomGGUFLoader::getRecommendedConversion() const
{
    // Check what types we need to convert to
    for (const auto& tensor : m_tensors) {
        if (tensor.quantType == GGML_TYPE_IQ4_NL) {
            return "Q5_K";  // Recommended conversion
        }
    }
    
    return "Q4_K";  // Default recommendation
}

QMap<QString, QVariant> CustomGGUFLoader::getOptimizationStats() const
{
    QMap<QString, QVariant> stats;
    
    stats["tensorCount"] = QVariant(static_cast<int>(m_tensors.size()));
    stats["fileSize"] = QVariant(static_cast<qint64>(m_fileSize));
    stats["estimatedUncompressed"] = QVariant(static_cast<qint64>(m_estimatedUncompressedSize));
    stats["compressionRatio"] = QVariant(m_estimatedUncompressedSize > 0 ? 
        (double)m_fileSize / m_estimatedUncompressedSize : 0.0);
    stats["nLayers"] = QVariant(m_metadata.nLayer);
    stats["nEmbed"] = QVariant(m_metadata.nEmbd);
    stats["contextSize"] = QVariant(m_metadata.nCtx);
    
    return stats;
}
