#pragma once

#include <QString>
#include <QByteArray>
#include <memory>

/**
 * @brief Universal Format Loader - Bridges pure MASM parsers with Qt/C++ pipeline
 * 
 * Supports: SafeTensors, PyTorch, TensorFlow, ONNX, MLX, NumPy, AWQ, GPTQ
 * All parsing done in pure MASM x64 with zero external dependencies
 * 
 * Output: GGUF format compatible with existing inference engine
 */

// Format type constants (from MASM)
enum class UniversalFormat {
    Unknown = 0,
    GGUF = 1,
    HFRepo = 2,
    SafeTensors = 100,
    PyTorch = 101,
    TensorFlow = 102,
    ONNX = 103,
    MLX = 104,
    NumPy = 105
};

/**
 * @brief Tensor metadata extracted from non-GGUF formats
 */
struct TensorMetadata {
    QString name;
    std::vector<uint64_t> shape;
    QString dtype;  // "F32", "F16", "I32", etc.
    uint64_t offset;
    uint64_t size;
    bool isQuantized;
};

/**
 * @brief Universal Format Loader - Pure MASM implementation
 * 
 * Thread-safe: Yes (all allocation per instance)
 * Allocates/deallocates internally
 */
class UniversalFormatLoader {
public:
    UniversalFormatLoader();
    ~UniversalFormatLoader();
    
    /**
     * @brief Detect file format from path
     * @return UniversalFormat enum
     */
    UniversalFormat detectFormat(const QString& filePath);
    
    /**
     * @brief Detect file format from buffer
     * @param buffer Pointer to file data
     * @param size Buffer size in bytes
     * @return UniversalFormat enum
     */
    UniversalFormat detectFormatFromBuffer(const uint8_t* buffer, size_t size);
    
    /**
     * @brief Load and convert SafeTensors file to GGUF
     * @param filePath Path to .safetensors file
     * @return QByteArray containing GGUF data (empty on error)
     */
    QByteArray loadSafeTensors(const QString& filePath);
    
    /**
     * @brief Load and convert PyTorch .pt file to GGUF
     * @param filePath Path to .pt file
     * @return QByteArray containing GGUF data (empty on error)
     */
    QByteArray loadPyTorch(const QString& filePath);
    
    /**
     * @brief Load and convert TensorFlow model to GGUF
     * @param filePath Path to saved_model directory or frozen_pb file
     * @return QByteArray containing GGUF data (empty on error)
     */
    QByteArray loadTensorFlow(const QString& filePath);
    
    /**
     * @brief Load and convert ONNX model to GGUF
     * @param filePath Path to .onnx file
     * @return QByteArray containing GGUF data (empty on error)
     */
    QByteArray loadONNX(const QString& filePath);
    
    /**
     * @brief Unified load: auto-detects format and loads
     * @param filePath Any supported model file
     * @return QByteArray containing GGUF data (empty on error)
     */
    QByteArray load(const QString& filePath);
    
    /**
     * @brief Get last error message
     */
    QString getLastError() const { return m_lastError; }
    
    /**
     * @brief Get detected tensor metadata (for debugging/validation)
     */
    const std::vector<TensorMetadata>& getTensorMetadata() const { return m_tensors; }
    
private:
    // MASM function declarations (external linkage)
    static UniversalFormat detectFormatFromFileASM(const wchar_t* filePath);
    static UniversalFormat detectFormatFromBufferASM(const uint8_t* buffer, size_t size);
    static uint8_t* parseSafeTensorsFileASM(const wchar_t* filePath, size_t* outSize);
    static uint8_t* parsePyTorchFileASM(const wchar_t* filePath, size_t* outSize);
    
    QString m_lastError;
    std::vector<TensorMetadata> m_tensors;
};
