#pragma once

#include <cstdint>
#include <vector>
#include <QFile>
#include <QByteArray>

namespace RawrXD {

// GGML tensor type enumeration
enum class GgmlType : uint32_t {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    // Add more as needed
};

// Tensor descriptor (from GGUF metadata)
struct TensorDesc {
    QString name;
    GgmlType type;
    std::vector<uint64_t> shape;
    uint64_t offset;  // File offset to tensor data
};

// Q4_0 block format (32 weights per block)
struct BlockQ4_0 {
    uint16_t d;      // FP16 scale factor
    uint8_t qs[16];  // 32 x 4-bit weights (2 per byte)
};

/**
 * Read Q4_0 quantized tensor from GGUF file
 * 
 * @param f: QFile positioned at tensor data
 * @param d: Tensor descriptor with metadata
 * @param out: Output vector of Q4_0 blocks
 * @return true on success
 */
bool readTensorQ4_0(QFile& f, const TensorDesc& d, std::vector<BlockQ4_0>& out);

/**
 * Read tensor data as raw bytes
 * 
 * @param f: QFile positioned at start of tensor
 * @param offset: File offset to tensor data
 * @param bytes: Number of bytes to read
 * @return QByteArray with raw tensor data
 */
QByteArray readTensorData(QFile& f, uint64_t offset, uint64_t bytes);

/**
 * Dequantize Q4_0 block to FP32 (scalar fallback)
 * 
 * @param block: Input Q4_0 block
 * @param out: Output FP32 array [32 floats]
 */
void dequantize_q4_0_scalar(const BlockQ4_0& block, float* out);

} // namespace RawrXD
