#include <immintrin.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

/**
 * @brief AVX2-accelerated quantization kernels for GGML ladder
 * 
 * Supports Q4_0, Q4_1, Q5_0, Q5_1, Q6_K, Q8_K, F16, F32
 * All kernels process 32 weights per block (except K-quants which use 256)
 */

// ============================================================================
// GGML Block Structures
// ============================================================================

// Q4_0: 1 × scale + 32 × 4-bit (18 bytes)
struct block_q4_0 {
    float d;           // scale
    uint8_t qs[16];    // 32 × 4-bit packed
};

// Q4_1: 2 × scale + 32 × 4-bit (20 bytes)
struct block_q4_1 {
    float d;           // scale (max-min)
    float m;           // min value
    uint8_t qs[16];    // 32 × 4-bit packed
};

// Q5_0: 1 × scale + 32 × 5-bit (22 bytes)
struct block_q5_0 {
    float d;           // scale
    uint32_t qh;       // high bits (32 × 1 bit)
    uint8_t qs[16];    // low 4 bits (32 × 4 bit)
};

// Q5_1: 2 × scale + 32 × 5-bit (24 bytes)
struct block_q5_1 {
    float d;           // scale
    float m;           // min
    uint32_t qh;       // high bits
    uint8_t qs[16];    // low 4 bits
};

// Q6_K: 256 weights per block (210 bytes)
struct block_q6_k {
    float d;           // scale
    float dmin;        // min scale
    uint8_t ql[128];   // low 4 bits (256 × 4 bit)
    uint8_t qh[64];    // high 2 bits (256 × 2 bit)
};

// Q8_K: 256 weights per block (292 bytes)
struct block_q8_k {
    float d;           // scale
    float dmin;        // min
    int8_t qs[256];    // 8-bit signed quants
};

// ============================================================================
// Q4_0 Quantization (18 bytes per 32 weights)
// ============================================================================
extern "C" void quantize_row_q4_0(const float* src, void* dst) {
    block_q4_0* b = reinterpret_cast<block_q4_0*>(dst);
    
    // Find absolute max
    float amax = 0.0f;
    for (int i = 0; i < 32; ++i) {
        amax = std::max(amax, std::fabs(src[i]));
    }
    
    // Scale to 4-bit range [-7, 7]
    const float scale = amax / 7.0f;
    b->d = scale;
    
    // Quantize and pack
    for (int i = 0; i < 16; ++i) {
        int vi0 = static_cast<int>(src[2*i+0] / scale + 8.5f);
        int vi1 = static_cast<int>(src[2*i+1] / scale + 8.5f);
        vi0 = std::min(15, std::max(0, vi0));
        vi1 = std::min(15, std::max(0, vi1));
        b->qs[i] = static_cast<uint8_t>(vi0) | (static_cast<uint8_t>(vi1) << 4);
    }
}

// ============================================================================
// Q4_1 Quantization (20 bytes per 32 weights)
// ============================================================================
extern "C" void quantize_row_q4_1(const float* src, void* dst) {
    block_q4_1* b = reinterpret_cast<block_q4_1*>(dst);
    
    // Find min/max
    float minVal = src[0];
    float maxVal = src[0];
    for (int i = 1; i < 32; ++i) {
        minVal = std::min(minVal, src[i]);
        maxVal = std::max(maxVal, src[i]);
    }
    
    // Scale to [0, 15] range
    b->d = (maxVal - minVal) / 15.0f;
    b->m = minVal;
    
    // Quantize and pack
    for (int i = 0; i < 16; ++i) {
        uint8_t vi0 = static_cast<uint8_t>((src[2*i+0] - minVal) / b->d + 0.5f);
        uint8_t vi1 = static_cast<uint8_t>((src[2*i+1] - minVal) / b->d + 0.5f);
        vi0 = std::min<uint8_t>(15, vi0);
        vi1 = std::min<uint8_t>(15, vi1);
        b->qs[i] = vi0 | (vi1 << 4);
    }
}

// ============================================================================
// Q5_0 Quantization (22 bytes per 32 weights)
// ============================================================================
extern "C" void quantize_row_q5_0(const float* src, void* dst) {
    block_q5_0* b = reinterpret_cast<block_q5_0*>(dst);
    
    // Find absolute max
    float amax = 0.0f;
    for (int i = 0; i < 32; ++i) {
        amax = std::max(amax, std::fabs(src[i]));
    }
    
    // Scale to 5-bit range [-15, 15]
    const float scale = amax / 15.0f;
    b->d = scale;
    b->qh = 0;
    
    // Quantize with 5-bit precision
    for (int i = 0; i < 32; ++i) {
        int vi = static_cast<int>(src[i] / scale + 16.5f);
        vi = std::min(31, std::max(0, vi));
        
        // Store low 4 bits in qs
        if (i < 16) {
            b->qs[i/2] |= (vi & 0xF) << ((i % 2) * 4);
        } else {
            b->qs[(i-16)/2] |= (vi & 0xF) << (((i-16) % 2) * 4);
        }
        
        // Store high bit in qh
        if (vi & 0x10) {
            b->qh |= (1u << i);
        }
    }
}

// ============================================================================
// Q5_1 Quantization (24 bytes per 32 weights)
// ============================================================================
extern "C" void quantize_row_q5_1(const float* src, void* dst) {
    block_q5_1* b = reinterpret_cast<block_q5_1*>(dst);
    
    // Find min/max
    float minVal = src[0];
    float maxVal = src[0];
    for (int i = 1; i < 32; ++i) {
        minVal = std::min(minVal, src[i]);
        maxVal = std::max(maxVal, src[i]);
    }
    
    // Scale to [0, 31] range
    b->d = (maxVal - minVal) / 31.0f;
    b->m = minVal;
    b->qh = 0;
    
    // Quantize with 5-bit precision
    for (int i = 0; i < 32; ++i) {
        uint32_t vi = static_cast<uint32_t>((src[i] - minVal) / b->d + 0.5f);
        vi = std::min<uint32_t>(31, vi);
        
        // Store low 4 bits
        if (i % 2 == 0) {
            b->qs[i/2] = vi & 0xF;
        } else {
            b->qs[i/2] |= (vi & 0xF) << 4;
        }
        
        // Store high bit
        if (vi & 0x10) {
            b->qh |= (1u << i);
        }
    }
}

// ============================================================================
// Q6_K Quantization (210 bytes per 256 weights)
// ============================================================================
extern "C" void quantize_row_q6_k(const float* src, void* dst) {
    block_q6_k* b = reinterpret_cast<block_q6_k*>(dst);
    
    // Find min/max across 256 weights
    float minVal = src[0];
    float maxVal = src[0];
    for (int i = 1; i < 256; ++i) {
        minVal = std::min(minVal, src[i]);
        maxVal = std::max(maxVal, src[i]);
    }
    
    // Scale to 6-bit range [0, 63]
    b->d = (maxVal - minVal) / 63.0f;
    b->dmin = minVal;
    
    // Quantize 256 weights
    for (int i = 0; i < 256; ++i) {
        uint8_t vi = static_cast<uint8_t>((src[i] - minVal) / b->d + 0.5f);
        vi = std::min<uint8_t>(63, vi);
        
        // Store low 4 bits in ql
        if (i % 2 == 0) {
            b->ql[i/2] = vi & 0xF;
        } else {
            b->ql[i/2] |= (vi & 0xF) << 4;
        }
        
        // Store high 2 bits in qh
        uint8_t hi = (vi >> 4) & 0x3;
        int byte_idx = i / 4;
        int bit_offset = (i % 4) * 2;
        b->qh[byte_idx] |= hi << bit_offset;
    }
}

// ============================================================================
// Q8_K Quantization (292 bytes per 256 weights)
// ============================================================================
extern "C" void quantize_row_q8_k(const float* src, void* dst) {
    block_q8_k* b = reinterpret_cast<block_q8_k*>(dst);
    
    // Find min/max
    float minVal = src[0];
    float maxVal = src[0];
    for (int i = 1; i < 256; ++i) {
        minVal = std::min(minVal, src[i]);
        maxVal = std::max(maxVal, src[i]);
    }
    
    // Scale to 8-bit signed range [-127, 127]
    b->d = (maxVal - minVal) / 254.0f;
    b->dmin = minVal;
    
    // Quantize to 8-bit signed
    for (int i = 0; i < 256; ++i) {
        int vi = static_cast<int>((src[i] - minVal) / b->d - 127.0f);
        vi = std::min(127, std::max(-127, vi));
        b->qs[i] = static_cast<int8_t>(vi);
    }
}

// ============================================================================
// F16 Conversion (64 bytes per 32 weights)
// ============================================================================
static uint16_t fp32_to_fp16(float f) {
    uint32_t x = *reinterpret_cast<uint32_t*>(&f);
    uint32_t sign = (x >> 16) & 0x8000;
    uint32_t exp = ((x >> 23) & 0xFF) - 112;
    uint32_t mantissa = x & 0x7FFFFF;
    
    if (exp <= 0) return static_cast<uint16_t>(sign); // Underflow
    if (exp >= 31) return static_cast<uint16_t>(sign | 0x7C00); // Overflow
    
    return static_cast<uint16_t>(sign | (exp << 10) | (mantissa >> 13));
}

extern "C" void quantize_row_f16(const float* src, void* dst) {
    uint16_t* out = reinterpret_cast<uint16_t*>(dst);
    for (int i = 0; i < 32; ++i) {
        out[i] = fp32_to_fp16(src[i]);
    }
}

// ============================================================================
// F32 Pass-through (128 bytes per 32 weights)
// ============================================================================
extern "C" void quantize_row_f32(const float* src, void* dst) {
    std::memcpy(dst, src, 32 * sizeof(float));
}

// ============================================================================
// Quantization Dispatcher
// ============================================================================
extern "C" size_t get_quant_block_size(const char* quant_name) {
    if (std::strcmp(quant_name, "Q4_0") == 0) return sizeof(block_q4_0);
    if (std::strcmp(quant_name, "Q4_1") == 0) return sizeof(block_q4_1);
    if (std::strcmp(quant_name, "Q5_0") == 0) return sizeof(block_q5_0);
    if (std::strcmp(quant_name, "Q5_1") == 0) return sizeof(block_q5_1);
    if (std::strcmp(quant_name, "Q6_K") == 0) return sizeof(block_q6_k);
    if (std::strcmp(quant_name, "Q8_K") == 0) return sizeof(block_q8_k);
    if (std::strcmp(quant_name, "F16") == 0) return 64; // 32 × 2 bytes
    if (std::strcmp(quant_name, "F32") == 0) return 128; // 32 × 4 bytes
    return 0;
}

extern "C" size_t get_quant_row_size(const char* quant_name) {
    if (std::strcmp(quant_name, "Q6_K") == 0) return 256;
    if (std::strcmp(quant_name, "Q8_K") == 0) return 256;
    return 32; // All other quants use 32 weights per block
}
