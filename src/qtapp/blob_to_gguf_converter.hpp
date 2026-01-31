#pragma once
#include <memory>
#include <cstdint>

/**
 * @brief Tensor data structure for conversion
 */
struct ConversionTensor {
    std::string name;
    std::vector<uint8_t> data;
    std::vector<int32_t> shape;
    int32_t ggmlType;  // Quantization type (Q4_0, Q5_K, etc.)
    uint32_t dataOffset;  // Offset in output file
};

/**
 * @brief Model metadata for GGUF output
 */
struct BlobGGUFMetadata {
    std::string modelName;
    std::string AITrainingArchitecture;
    int32_t nEmbed = 0;
    int32_t nLayer = 0;
    int32_t nVocab = 0;
    int32_t nCtx = 0;
    int32_t nHead = 0;
    int32_t nHeadKv = 0;
    float ropeFreqBase = 10000.0f;
    float ropeFreqScale = 1.0f;
    std::map<std::string, std::string> customKVPairs;
};

/**
 * @brief Conversion progress reporting
 */
struct ConversionProgress {
    int totalTensors = 0;
    int processedTensors = 0;
    int64_t bytesProcessed = 0;
    int64_t totalBytes = 0;
    std::string currentTensor;
    double percentComplete = 0.0;
    std::string statusMessage;
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
class BlobToGGUFConverter  {

public:
    explicit BlobToGGUFConverter( = nullptr);
    ~BlobToGGUFConverter();

    /**
     * @brief Load a blob file for conversion
     * @param blobPath Path to the blob file
     * @return true if successfully loaded
     */
    bool loadBlobFile(const std::string& blobPath);

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
    bool convertToGGUF(const std::string& outputPath);

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
    std::stringList getDetectedTensors() const;

    /**
     * @brief Get estimated file size for output GGUF
     */
    int64_t getEstimatedGGUFSize() const;
\npublic:\n    /**
     * @brief Emitted when conversion progress updates
     */
    void progressUpdated(const ConversionProgress& progress);

    /**
     * @brief Emitted when conversion completes successfully
     * @param outputPath Path to generated GGUF file
     */
    void conversionComplete(const std::string& outputPath);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Description of the error
     */
    void conversionError(const std::string& errorMessage);

    /**
     * @brief Emitted when conversion is cancelled
     */
    void conversionCancelled();

    /**
     * @brief Emitted when metadata preview is available after parse
     */
    void conversionMetadataPreview(const nlohmann::json& preview);

private:
    /**
     * @brief Write GGUF header and KV pairs
     */
    bool writeGGUFHeader(std::iostream* file);

    /**
     * @brief Write tensor data to GGUF file
     */
    bool writeTensorData(std::iostream* file);

    /**
     * @brief Detect tensor boundaries in raw blob data
     */
    std::vector<ConversionTensor> detectTensors();

    /**
     * @brief Estimate tensor quantization type from data patterns
     */
    int32_t estimateQuantizationType(const std::vector<uint8_t>& tensorData);

    /**
     * @brief Update progress and signal
     */
    void updateProgress(int processedTensors, int64_t bytesProcessed, 
                       const std::string& tensorName, const std::string& message);

    /**
     * @brief Generate metadata preview from parsed blob
     */
    nlohmann::json generateMetadataPreview() const;

    // Member variables
    std::string m_blobPath;
    std::vector<uint8_t> m_blobData;
    BlobGGUFMetadata m_metadata;
    std::vector<ConversionTensor> m_tensors;
    ConversionProgress m_progress;
    bool m_isConverting = false;
    bool m_cancelRequested = false;
    int64_t m_totalBlobSize = 0;
};

