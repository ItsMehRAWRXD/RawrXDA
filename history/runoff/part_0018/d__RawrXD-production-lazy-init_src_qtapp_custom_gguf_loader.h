#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <QVector>
#include <memory>
#include <cstdint>

/**
 * @brief GGUF model metadata
 */
struct GGUFModelMetadata {
    QString modelName;
    QString modelFamily;
    int nLayer;
    int nEmbd;
    int nHead;
    int nHeadKV;
    int nRot;
    int nCtx;
    float ropeFreqBase;
    float ropeFreqScale;
    QString tokenModel;
    int vocabSize;
    
    GGUFModelMetadata() : nLayer(0), nEmbd(0), nHead(0), nHeadKV(0), 
                         nRot(0), nCtx(0), ropeFreqBase(10000), 
                         ropeFreqScale(1.0), vocabSize(0) {}
};

/**
 * @brief GGUF tensor metadata
 */
struct GGUFTensorInfo {
    QString name;
    int ndim;
    QVector<uint64_t> ne;    // dimensions
    uint32_t quantType;      // quantization type
    uint64_t offset;         // file offset
    size_t computedSize;     // decompressed size
};

/**
 * @brief Pure GGUF loader - No external dependencies
 * 
 * This custom loader reads GGUF files without depending on llama.cpp or ollama.
 * It provides:
 * - Direct GGUF file parsing
 * - Tensor metadata extraction
 * - Quantization type detection
 * - Model parameter reading
 * - Optimized tensor loading for inference
 */
class CustomGGUFLoader {
public:
    /**
     * @brief Open and parse a GGUF file
     */
    explicit CustomGGUFLoader(const QString& filePath);
    ~CustomGGUFLoader();
    
    /**
     * @brief Check if file was successfully opened
     */
    bool isOpen() const { return m_isOpen; }
    
    /**
     * @brief Get model metadata
     */
    GGUFModelMetadata getMetadata() const { return m_metadata; }
    
    /**
     * @brief Get tensor count
     */
    int getTensorCount() const { return m_tensors.size(); }
    
    /**
     * @brief Get tensor by name
     */
    GGUFTensorInfo getTensorInfo(const QString& name) const;
    
    /**
     * @brief Get all tensor names
     */
    QStringList getTensorNames() const;
    
    /**
     * @brief Load tensor data (decompressed)
     * @param name Tensor name
     * @return Tensor data as QByteArray
     */
    QByteArray loadTensorData(const QString& name);
    
    /**
     * @brief Get tensor shape
     */
    QVector<uint64_t> getTensorShape(const QString& name) const;
    
    /**
     * @brief Get quantization type for tensor
     */
    uint32_t getQuantizationType(const QString& name) const;
    
    /**
     * @brief Get file size in bytes
     */
    uint64_t getFileSize() const { return m_fileSize; }
    
    /**
     * @brief Get estimated decompressed size
     */
    uint64_t getEstimatedUncompressedSize() const { return m_estimatedUncompressedSize; }
    
    /**
     * @brief Check if model has unsupported quantization types
     */
    bool hasUnsupportedQuantization() const;
    
    /**
     * @brief Get unsupported quantization types
     */
    QMap<uint32_t, QString> getUnsupportedQuantizationTypes() const;
    
    /**
     * @brief Get recommended conversion type
     */
    QString getRecommendedConversion() const;
    
    /**
     * @brief Get model statistics for optimization
     */
    QMap<QString, QVariant> getOptimizationStats() const;
    
private:
    // File operations
    bool parseGGUFFile();
    bool readHeader();
    bool readMetadata();
    bool readTensorIndex();
    
    // Quantization support
    bool isQuantizationSupported(uint32_t quantType) const;
    QString quantTypeToString(uint32_t quantType) const;
    size_t getQuantizationElementSize(uint32_t quantType) const;
    
    // Tensor decompression
    QByteArray decompressTensor(const GGUFTensorInfo& info);
    
    // Member variables
    QString m_filePath;
    bool m_isOpen;
    uint64_t m_fileSize;
    uint64_t m_estimatedUncompressedSize;
    
    GGUFModelMetadata m_metadata;
    QMap<QString, GGUFTensorInfo> m_tensors;
    
    // File handle for lazy loading
    FILE* m_fileHandle;
};


