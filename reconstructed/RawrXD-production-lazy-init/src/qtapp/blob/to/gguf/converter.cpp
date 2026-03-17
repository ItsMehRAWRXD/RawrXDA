#include "blob_to_gguf_converter.hpp"
#include "deflate_brutal_qt.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <cstring>
#include <algorithm>

// GGUF magic number (little-endian: "GGUF")
const uint32_t GGUF_MAGIC = 0x46554747;
const uint32_t GGUF_VERSION = 3;

// GGML type enum (must match ggml.h)
enum class ggml_type : int32_t {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15,
};

BlobToGGUFConverter::BlobToGGUFConverter(QObject* parent)
    : QObject(parent), m_isConverting(false), m_cancelRequested(false)
{
    m_metadata.AITrainingArchitecture = "unknown";
}

BlobToGGUFConverter::~BlobToGGUFConverter()
{
    m_blobData.clear();
}

bool BlobToGGUFConverter::loadBlobFile(const QString& blobPath)
{
    QFileInfo fileInfo(blobPath);
    if (!fileInfo.exists()) {
        emit conversionError(QString("Blob file not found: %1").arg(blobPath));
        return false;
    }

    QFile file(blobPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit conversionError(QString("Failed to open blob file: %1").arg(file.errorString()));
        return false;
    }

    m_blobData = file.readAll();
    file.close();

    m_blobPath = blobPath;
    m_totalBlobSize = m_blobData.size();

    qInfo() << "[BlobConverter] Loaded blob file:" << blobPath
            << "Size:" << m_totalBlobSize << "bytes";

    return true;
}

void BlobToGGUFConverter::setMetadata(const BlobGGUFMetadata& metadata)
{
    m_metadata = metadata;
    if (m_metadata.modelName.isEmpty()) {
        m_metadata.modelName = "converted-model";
    }
    qDebug() << "[BlobConverter] Metadata set:" << m_metadata.modelName;
}

bool BlobToGGUFConverter::parseBlob(int estTensorCount)
{
    if (m_blobData.isEmpty()) {
        emit conversionError("No blob data loaded");
        return false;
    }

    try {
        m_tensors = detectTensors();
        m_progress.totalTensors = m_tensors.size();
        m_progress.totalBytes = m_totalBlobSize;

        qInfo() << "[BlobConverter] Detected" << m_tensors.size() << "tensors (" << m_tensors.size() << " chunks)";

        // Emit a metadata preview signal for UI
        emit conversionMetadataPreview(generateMetadataPreview());

        return !m_tensors.isEmpty();
    } catch (const std::exception& e) {
        emit conversionError(QString("Parse error: %1").arg(e.what()));
        return false;
    }
}

QVector<ConversionTensor> BlobToGGUFConverter::detectTensors()
{
    QVector<ConversionTensor> tensors;

    // Simple heuristic: assume blob is a sequence of weight matrices
    // Try to divide blob into equal-sized chunks or use size patterns
    
    if (m_blobData.size() < 100) {
        // Too small, treat as single tensor
        ConversionTensor tensor;
        tensor.name = "weights_0";
        tensor.data = m_blobData;
        tensor.ggmlType = static_cast<int32_t>(ggml_type::F32);
        tensor.shape = {static_cast<int32_t>(m_blobData.size() / 4)};
        tensors.append(tensor);
        return tensors;
    }

    // Try to detect patterns: common layer sizes for LLMs
    // For standard models: embedding, attention, feedforward layers repeat
    qint64 offset = 0;
    int tensorIdx = 0;

    // Heuristic: assume blob contains float32 data in standard chunks
    // Common pattern: embed (d_model*vocab), then layer repeat (4*d_model*d_model etc)
    
    // For now, use a simple strategy: divide into 4KB aligned chunks
    const qint64 chunkSize = 4096;  // 4KB default chunk
    
    while (offset < m_blobData.size() && !m_cancelRequested) {
        qint64 remaining = m_blobData.size() - offset;
        qint64 size = std::min(chunkSize, remaining);

        ConversionTensor tensor;
        tensor.name = QString("tensor_%1").arg(tensorIdx++);
        tensor.data = m_blobData.mid(offset, size);
        tensor.ggmlType = estimateQuantizationType(tensor.data);
        tensor.dataOffset = offset;
        
        // Estimate shape (assume 1D for now)
        tensor.shape = {static_cast<int32_t>(size / 4)};
        
        tensors.append(tensor);
        offset += size;
    }

    return tensors;
}

int32_t BlobToGGUFConverter::estimateQuantizationType(const QByteArray& tensorData)
{
    // Simple heuristic: if data looks like float32 (many values between 0.0 and 1.0),
    // use F32; otherwise try Q4_0 for compression

    if (tensorData.size() < 4) {
        return static_cast<int32_t>(ggml_type::F32);
    }

    // Check first float value to infer type
    float firstValue;
    std::memcpy(&firstValue, tensorData.data(), sizeof(float));

    // If value is reasonable (not extreme), keep as F32
    if (std::abs(firstValue) < 100.0f) {
        return static_cast<int32_t>(ggml_type::F32);
    }

    // Otherwise use Q4_0 for compression
    return static_cast<int32_t>(ggml_type::Q4_0);
}

bool BlobToGGUFConverter::convertToGGUF(const QString& outputPath)
{
    if (m_tensors.isEmpty()) {
        emit conversionError("No tensors to convert. Run parseBlob() first.");
        return false;
    }

    m_isConverting = true;
    m_cancelRequested = false;

    try {
        QFile outputFile(outputPath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            emit conversionError(QString("Failed to open output file: %1").arg(outputFile.errorString()));
            m_isConverting = false;
            return false;
        }

        // Write GGUF header and KV pairs
        if (!writeGGUFHeader(&outputFile)) {
            outputFile.close();
            QFile::remove(outputPath);
            m_isConverting = false;
            return false;
        }

        // Write tensor data
        if (!writeTensorData(&outputFile)) {
            outputFile.close();
            QFile::remove(outputPath);
            m_isConverting = false;
            return false;
        }

        outputFile.close();
        m_isConverting = false;

        qInfo() << "[BlobConverter] Conversion complete:" << outputPath;
        emit conversionComplete(outputPath);
        return true;

    } catch (const std::exception& e) {
        m_isConverting = false;
        emit conversionError(QString("Conversion error: %1").arg(e.what()));
        QFile::remove(outputPath);
        return false;
    }
}

bool BlobToGGUFConverter::writeGGUFHeader(QIODevice* file)
{
    try {
        // Write magic number
        uint32_t magic = GGUF_MAGIC;
        file->write(reinterpret_cast<const char*>(&magic), sizeof(magic));

        // Write version
        uint32_t version = GGUF_VERSION;
        file->write(reinterpret_cast<const char*>(&version), sizeof(version));

        // Write tensor count and KV count
        uint64_t tensorCount = m_tensors.size();
        uint64_t kvCount = 5 + m_metadata.customKVPairs.size();  // Metadata + custom KVs

        file->write(reinterpret_cast<const char*>(&tensorCount), sizeof(tensorCount));
        file->write(reinterpret_cast<const char*>(&kvCount), sizeof(kvCount));

        // Write KV pairs
        auto writeString = [file](const QString& str) {
            QByteArray utf8 = str.toUtf8();
            uint64_t len = utf8.length();
            file->write(reinterpret_cast<const char*>(&len), sizeof(len));
            file->write(utf8);
        };

        // Model name
        writeString("general.name");
        uint32_t type = 8;  // String type in GGUF
        file->write(reinterpret_cast<const char*>(&type), sizeof(type));
        writeString(m_metadata.modelName);

        // Architecture
        writeString("general.architecture");
        file->write(reinterpret_cast<const char*>(&type), sizeof(type));
        writeString(m_metadata.AITrainingArchitecture);

        // BRUTAL MASM COMPRESSION METADATA
        writeString("rawrxd.brutal_masm_compression");
        file->write(reinterpret_cast<const char*>(&type), sizeof(type));
        writeString("enabled");

        writeString("rawrxd.brutal_kernel");
        file->write(reinterpret_cast<const char*>(&type), sizeof(type));
        writeString("deflate_brutal_masm");

        // Model parameters
        writeString("llama.embedding_length");
        uint32_t uintType = 5;  // uint32 in GGUF
        file->write(reinterpret_cast<const char*>(&uintType), sizeof(uintType));
        uint32_t nEmbed = m_metadata.nEmbed;
        file->write(reinterpret_cast<const char*>(&nEmbed), sizeof(nEmbed));

        writeString("llama.block_count");
        file->write(reinterpret_cast<const char*>(&uintType), sizeof(uintType));
        uint32_t nLayer = m_metadata.nLayer;
        file->write(reinterpret_cast<const char*>(&nLayer), sizeof(nLayer));

        writeString("llama.context_length");
        file->write(reinterpret_cast<const char*>(&uintType), sizeof(uintType));
        uint32_t nCtx = m_metadata.nCtx;
        file->write(reinterpret_cast<const char*>(&nCtx), sizeof(nCtx));

        // Custom KV pairs
        for (auto it = m_metadata.customKVPairs.begin(); it != m_metadata.customKVPairs.end(); ++it) {
            writeString(it.key());
            file->write(reinterpret_cast<const char*>(&type), sizeof(type));
            writeString(it.value());
        }

        qInfo() << "[BlobConverter] GGUF header written successfully";
        return true;

    } catch (const std::exception& e) {
        emit conversionError(QString("Header write error: %1").arg(e.what()));
        return false;
    }
}

bool BlobToGGUFConverter::writeTensorData(QIODevice* file)
{
    try {
        for (int i = 0; i < m_tensors.size(); ++i) {
            if (m_cancelRequested) {
                emit conversionCancelled();
                return false;
            }

            const ConversionTensor& tensor = m_tensors[i];

            // Write tensor header
            QByteArray nameUtf8 = tensor.name.toUtf8();
            uint64_t nameLen = nameUtf8.length();
            file->write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
            file->write(nameUtf8);

            // Write shape
            uint32_t ndim = tensor.shape.size();
            file->write(reinterpret_cast<const char*>(&ndim), sizeof(ndim));
            for (int32_t dim : tensor.shape) {
                file->write(reinterpret_cast<const char*>(&dim), sizeof(dim));
            }

            // Write quantization type
            file->write(reinterpret_cast<const char*>(&tensor.ggmlType), sizeof(tensor.ggmlType));

            // Write data offset in file
            uint64_t offset = file->pos() + 8;  // Account for offset field itself
            file->write(reinterpret_cast<const char*>(&offset), sizeof(offset));

            // Write tensor data
            // Compress using brutal MASM for maximum efficiency
            QByteArray compressed = brutal::compress(tensor.data);
            if (!compressed.isEmpty()) {
                qDebug() << "[BlobConverter] Compressed" << tensor.name 
                         << "from" << tensor.data.size() << "to" << compressed.size() << "bytes";
                file->write(compressed);
                // Track compression ratio
                double ratio = 100.0 * (1.0 - static_cast<double>(compressed.size()) / tensor.data.size());
                m_progress.lastCompressionRatio = ratio;
            } else {
                // Fallback to uncompressed if compression fails
                qWarning() << "[BlobConverter] Compression failed for" << tensor.name << ", writing uncompressed";
                file->write(tensor.data);
            }

            // Update progress
            updateProgress(i + 1, file->pos(), tensor.name, "Writing tensors...");
        }

        qInfo() << "[BlobConverter] All tensor data written";
        return true;

    } catch (const std::exception& e) {
        emit conversionError(QString("Tensor write error: %1").arg(e.what()));
        return false;
    }
}

QStringList BlobToGGUFConverter::getDetectedTensors() const
{
    QStringList names;
    for (const auto& tensor : m_tensors) {
        names.append(tensor.name);
    }
    return names;
}

qint64 BlobToGGUFConverter::getEstimatedGGUFSize() const
{
    // Rough estimate: header (1KB) + all tensor data + metadata
    qint64 headerSize = 1024;
    qint64 metadataSize = 512;
    qint64 tensorDataSize = 0;

    for (const auto& tensor : m_tensors) {
        tensorDataSize += tensor.data.size();
    }

    return headerSize + metadataSize + tensorDataSize;
}

QJsonObject BlobToGGUFConverter::generateMetadataPreview() const
{
    QJsonObject preview;
    preview["tensors"] = m_tensors.size();
    preview["tensor_bytes"] = m_blobData.size();
    // First tensor sample
    if (!m_tensors.isEmpty()) {
        preview["first_tensor"] = m_tensors.first().name;
        preview["first_tensor_bytes"] = m_tensors.first().data.size();
        preview["first_ggml_type"] = m_tensors.first().ggmlType;
    }
    return preview;
}

void BlobToGGUFConverter::updateProgress(int processedTensors, qint64 bytesProcessed,
                                         const QString& tensorName, const QString& message)
{
    m_progress.processedTensors = processedTensors;
    m_progress.bytesProcessed = bytesProcessed;
    m_progress.currentTensor = tensorName;
    m_progress.statusMessage = message;
    m_progress.percentComplete = m_progress.totalBytes > 0 
        ? (100.0 * bytesProcessed / m_progress.totalBytes)
        : 0.0;

    emit progressUpdated(m_progress);
}
