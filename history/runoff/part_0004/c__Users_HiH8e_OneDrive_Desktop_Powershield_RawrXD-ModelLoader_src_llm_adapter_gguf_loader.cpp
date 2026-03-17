// GGUF tensor loader implementation
#include "gguf_loader.h"
#include <cstring>

namespace RawrXD {

// Convert FP16 to FP32
static float half_to_float(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        exp = 1;
        while (!(mant & 0x400)) { mant <<= 1; exp--; }
        mant &= 0x3FF;
    } else if (exp == 31) {
        return sign ? -INFINITY : INFINITY;
    }
    
    uint32_t f32_exp = exp - 15 + 127;
    uint32_t f32_mant = mant << 13;
    uint32_t bits = (sign << 31) | (f32_exp << 23) | f32_mant;
    
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

// Read raw tensor data
QByteArray readTensorData(QFile& f, uint64_t offset, uint64_t bytes) {
    if (!f.seek(offset)) {
        return QByteArray();
    }
    return f.read(bytes);
}

// Read Q4_0 tensor
bool readTensorQ4_0(QFile& f, const TensorDesc& d, std::vector<BlockQ4_0>& out) {
    // Calculate total elements
    uint64_t total_elements = 1;
    for (auto dim : d.shape) {
        total_elements *= dim;
    }
    
    // Q4_0 stores 32 weights per block
    uint64_t num_blocks = (total_elements + 31) / 32;
    uint64_t bytes = num_blocks * sizeof(BlockQ4_0);
    
    // Read raw data
    QByteArray raw = readTensorData(f, d.offset, bytes);
    if (raw.isEmpty() || static_cast<uint64_t>(raw.size()) != bytes) {
        return false;
    }
    
    // Copy to output vector
    out.resize(num_blocks);
    std::memcpy(out.data(), raw.constData(), bytes);
    return true;
}

// Dequantize Q4_0 block to FP32 (scalar)
void dequantize_q4_0_scalar(const BlockQ4_0& block, float* out) {
    float scale = half_to_float(block.d);
    
    for (int i = 0; i < 16; ++i) {
        uint8_t byte = block.qs[i];
        
        // Extract two 4-bit values
        int q0 = byte & 0xF;
        int q1 = (byte >> 4) & 0xF;
        
        // Dequantize: map [0, 15] to [-8, 7] then scale
        out[i * 2]     = ((float)q0 - 8.0f) * scale;
        out[i * 2 + 1] = ((float)q1 - 8.0f) * scale;
    }
}

} // namespace RawrXD
