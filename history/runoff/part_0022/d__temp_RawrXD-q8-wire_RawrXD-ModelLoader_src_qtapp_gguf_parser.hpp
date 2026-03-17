#pragma once
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QVector>
#include <QFile>
#include <cstdint>

// GGML quantization types
enum class GGMLType : uint32_t {
    F32     = 0,
    F16     = 1,
    Q4_0    = 2,
    Q4_1    = 3,
    Q5_0    = 6,
    Q5_1    = 7,
    Q8_0    = 8,
    Q8_1    = 9,
    Q2_K    = 10,
    Q3_K    = 11,
    Q4_K    = 12,
    Q5_K    = 13,
    Q6_K    = 14,
    Q8_K    = 15,
    IQ2_XXS = 16,
    IQ2_XS  = 17,
    IQ3_XXS = 18,
    IQ1_S   = 19,
    IQ4_NL  = 20,
    IQ3_S   = 21,
    IQ2_S   = 22,
    IQ4_XS  = 23,
    I8      = 24,
    I16     = 25,
    I32     = 26,
    I64     = 27,
    F64     = 28,
    IQ1_M   = 29,
    Unknown = 0xFFFFFFFF
};

// Quantization mode for model (GGUF v4 extension)
enum class QuantizationMode : uint8_t {
    UNIFORM = 0,
    HYBRID  = 1,
    MIXED   = 2
};

// GGUF metadata value types
enum class GGUFValueType : uint32_t {
    Uint8   = 0,
    Int8    = 1,
    Uint16  = 2,
    Int16   = 3,
    Uint32  = 4,
    Int32   = 5,
    Float32 = 6,
    Bool    = 7,
    String  = 8,
    Array   = 9,
    Uint64  = 10,
    Int64   = 11,
    Float64 = 12
};

struct GGUFTensorInfo {
    QString name;
    uint32_t n_dims;
    QVector<uint64_t> dimensions;
    GGMLType type;
    uint64_t offset;        // Offset in file
    uint64_t size;          // Size in bytes
};

// Per-tensor quantization metadata (GGUF v4)
struct TensorQuantMeta {
    uint32_t name_hash;           // CRC32 hash of tensor name
    GGMLType quantization_type;   // Quantization format for this tensor
    uint16_t flags;               // Bit flags (reserved for future)
};

struct GGUFMetadata {
    QString architecture;
    uint32_t vocab_size;
    uint32_t n_embd;
    uint32_t n_head;
    uint32_t n_layer;
    uint32_t n_ctx;
    QString tokenizer_model;
    QHash<QString, QString> custom_values;
    QVector<int> eos_tokens;  // End-of-sequence token IDs
    
    // GGUF v4 hybrid quantization support
    QuantizationMode quantization_mode{QuantizationMode::UNIFORM};
    uint32_t schema_version{1};  // 1=v3, 2=v4
};

/**
 * @brief Parser for GGUF v3 format files
 * 
 * Properly parses GGUF headers, metadata, and tensor information
 * with support for Q2_K, Q3_K, and other quantization formats.
 */
class GGUFParser {
public:
    explicit GGUFParser(const QString& filePath);
    ~GGUFParser();
    
    bool isValid() const { return m_valid; }
    uint32_t version() const { return m_version; }
    
    const GGUFMetadata& metadata() const { return m_metadata; }
    const QVector<GGUFTensorInfo>& tensors() const { return m_tensors; }
    
    GGUFTensorInfo tensorInfo(const QString& name) const;
    bool hasTensor(const QString& name) const;
    
    // Read raw tensor data from file
    QByteArray readTensorData(const QString& tensorName);
    
    // GGUF v4 hybrid quantization support
    bool isHybridQuantized() const { return m_metadata.quantization_mode == QuantizationMode::HYBRID; }
    QuantizationMode getQuantizationMode() const { return m_metadata.quantization_mode; }
    uint32_t getSchemaVersion() const { return m_metadata.schema_version; }
    
    // Get quantization type for specific tensor (with fallback to name inference)
    GGMLType getTensorQuantType(const QString& tensorName) const;
    
    // Get all tensor quantization metadata
    const QHash<uint32_t, TensorQuantMeta>& getTensorQuantMap() const;
    QByteArray readTensorData(const GGUFTensorInfo& info);
    
    // Get quantization type name
    static QString typeName(GGMLType type);
    static uint64_t typeSize(GGMLType type);  // Block size in bytes
    
private:
    bool parseHeader();
    bool parseMetadata();
    bool parseTensorInfo();
    
    QString readString(QDataStream& stream);
    
    // GGUF v4 hybrid quantization parsing
    bool readTensorQuantMap(uint64_t offset, uint32_t count);
    uint32_t hashTensorName(const QString& tensorName) const;
    GGMLType inferQuantTypeFromName(const QString& tensorName) const;
    
    QFile m_file;
    bool m_valid;
    uint32_t m_version;
    uint64_t m_tensorCount;
    uint64_t m_metadataKVCount;
    uint64_t m_tensorDataOffset;  // Where tensor data starts
    
    GGUFMetadata m_metadata;
    QVector<GGUFTensorInfo> m_tensors;
    QHash<QString, int> m_tensorIndex;
    QHash<uint32_t, TensorQuantMeta> m_tensorQuantMap;  // GGUF v4: per-tensor quant metadata
};
