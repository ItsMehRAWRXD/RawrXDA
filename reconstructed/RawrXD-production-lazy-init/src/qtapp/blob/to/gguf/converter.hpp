#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>
#include <QHash>
#include <QObject>
#include <QJsonObject>
#include <memory>
#include <cstdint>

/**
 * @brief Tensor data structure for conversion
 */
struct ConversionTensor {
    QString name;
    QByteArray data;
    QVector<int32_t> shape;
    int32_t ggmlType;  // Quantization type (Q4_0, Q5_K, etc.)
    uint32_t dataOffset;  // Offset in output file
};

/**
 * @brief Model metadata for GGUF output
 */
struct BlobGGUFMetadata {
    QString modelName;
    QString AITrainingArchitecture;
    int32_t nEmbed = 0;
    int32_t nLayer = 0;
    int32_t nVocab = 0;
    int32_t nCtx = 0;
    int32_t nHead = 0;
    int32_t nHeadKv = 0;
    float ropeFreqBase = 10000.0f;
    float ropeFreqScale = 1.0f;
    QHash<QString, QString> customKVPairs;
};

/**
 * @brief Conversion progress reporting
 */
struct ConversionProgress {
    int totalTensors = 0;
    int processedTensors = 0;
    qint64 bytesProcessed = 0;
    qint64 totalBytes = 0;
    QString currentTensor;
    double percentComplete = 0.0;
    QString statusMessage;
    double lastCompressionRatio = 0.0; // Track compression performance
};

/**
 * @class BlobToGGUFConverter
 * @brief Core engine for converting blob model files to GGUF format
 * 
 * This class handles:
 * - Blob file parsing (raw model weights)
 * - Metadata injection
 * - Tensor quantization and encoding
 * - GGUF serialization
 * - Progress tracking and error handling
 */
class BlobToGGUFConverter : public QObject {
    Q_OBJECT

public:
    explicit BlobToGGUFConverter(QObject* parent = nullptr);
    ~BlobToGGUFConverter();

    /**
     * @brief Load a blob file for conversion
     * @param blobPath Path to the blob file
     * @return true if successfully loaded
     */
    bool loadBlobFile(const QString& blobPath);

    /**
     * @brief Set model metadata for the output GGUF
     */
    void setMetadata(const BlobGGUFMetadata& metadata);

    /**
     * @brief Get current metadata
     */
    BlobGGUFMetadata getMetadata() const { return m_metadata; }

    /**
     * @brief Parse blob file to extract tensor information
     * @param estTensorCount Estimated number of tensors in the blob
     * @return true if parsing succeeded
     */
    bool parseBlob(int estTensorCount);

    /**
     * @brief Convert blob to GGUF format
     * @param outputPath Output GGUF file path
     * @return true if conversion succeeded
     */
    bool convertToGGUF(const QString& outputPath);

    /**
     * @brief Get current conversion progress
     */
    ConversionProgress getProgress() const { return m_progress; }

    /**
     * @brief Check if conversion is running
     */
    bool isConverting() const { return m_isConverting; }

    /**
     * @brief Cancel ongoing conversion
     */
    void cancelConversion() { m_cancelRequested = true; }

    /**
     * @brief Get list of detected tensors in blob
     */
    QStringList getDetectedTensors() const;

    /**
     * @brief Get estimated file size for output GGUF
     */
    qint64 getEstimatedGGUFSize() const;

signals:
    /**
     * @brief Emitted when conversion progress updates
     */
    void progressUpdated(const ConversionProgress& progress);

    /**
     * @brief Emitted when conversion completes successfully
     * @param outputPath Path to generated GGUF file
     */
    void conversionComplete(const QString& outputPath);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Description of the error
     */
    void conversionError(const QString& errorMessage);

    /**
     * @brief Emitted when conversion is cancelled
     */
    void conversionCancelled();

    /**
     * @brief Emitted when metadata preview is available after parse
     */
    void conversionMetadataPreview(const QJsonObject& preview);

private:
    /**
     * @brief Write GGUF header and KV pairs
     */
    bool writeGGUFHeader(QIODevice* file);

    /**
     * @brief Write tensor data to GGUF file
     */
    bool writeTensorData(QIODevice* file);

    /**
     * @brief Detect tensor boundaries in raw blob data
     */
    QVector<ConversionTensor> detectTensors();

    /**
     * @brief Estimate tensor quantization type from data patterns
     */
    int32_t estimateQuantizationType(const QByteArray& tensorData);

    /**
     * @brief Update progress and emit signal
     */
    void updateProgress(int processedTensors, qint64 bytesProcessed, 
                       const QString& tensorName, const QString& message);

    /**
     * @brief Generate metadata preview from parsed blob
     */
    QJsonObject generateMetadataPreview() const;

    // Member variables
    QString m_blobPath;
    QByteArray m_blobData;
    BlobGGUFMetadata m_metadata;
    QVector<ConversionTensor> m_tensors;
    ConversionProgress m_progress;
    bool m_isConverting = false;
    bool m_cancelRequested = false;
    qint64 m_totalBlobSize = 0;
};
