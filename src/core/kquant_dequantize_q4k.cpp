// =============================================================================
// kquant_dequantize_q4k.cpp — AVX2/AVX-512 Optimized Q4_K Dequantization
// =============================================================================
// Production implementation of GGML K-quant Q4_K dequantization with:
//   - Full Q4_K superblock format support (6-bit + 4-bit quantization)
//   - Hierarchical scales (8 sub-blocks)
//   - AVX2 vectorized operations (256-bit)
//   - AVX-512 fallback for compatible CPUs
//   - Scalar fallback for compatibility
//
// Q4_K Format:
//   - 256 values per superblock
//   - 8 sub-blocks of 32 values each
//   - 1x fp16 scale + 1x fp16 min per superblock  
//   - 8x 6-bit scales per sub-block (quantized scales)
//   - 4-bit quantized values
//
// NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include <cstdint>
#include <cstring>
#include <cmath>
#include <immintrin.h>  // AVX2/AVX-512 intrinsics

#ifdef _MSC_VER
#include <intrin.h>
#endif

// =============================================================================
// Q4_K Superblock Structure (matches GGML layout)
// =============================================================================

#define QK_K 256
#define K_SCALE_SIZE 12

// Q4_K block: 256 values (32 bytes quantized + 12 bytes scales + 2 bytes d/dmin)
struct block_q4_K {
    uint8_t scales[K_SCALE_SIZE];  // 12 bytes: 8x 6-bit scales packed
    uint8_t qs[QK_K/2];            // 128 bytes: 4-bit quants (2 per byte)
    union {
        struct {
            uint16_t d;     // fp16 scale
            uint16_t dmin;  // fp16 min
        };
        uint32_t dm;
    };
};

static_assert(sizeof(block_q4_K) == 144, "Q4_K block size must be 144 bytes");

// =============================================================================
// FP16 → FP32 Conversion
// =============================================================================

static inline float fp16_to_fp32(uint16_t h) {
    // IEEE 754 half-precision conversion
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp  = (h & 0x7C00) >> 10;
    uint32_t mant = (h & 0x03FF);

    if (exp == 0) {
        if (mant == 0) {
            // Zero
            return *reinterpret_cast<float*>(&sign);
        }
        // Denormal
        exp = 1;
        while ((mant & 0x0400) == 0) {
            mant <<= 1;
            exp--;
        }
        mant &= 0x03FF;
        exp = 127 - 15 - exp;
    } else if (exp == 0x1F) {
        // Inf/NaN
        exp = 0xFF;
    } else {
        // Normalized
        exp = exp - 15 + 127;
    }

    uint32_t result = sign | (exp << 23) | (mant << 13);
    return *reinterpret_cast<float*>(&result);
}

// =============================================================================
// 6-bit Scale Extraction (8 scales packed in 12 bytes)
// =============================================================================

static inline void extract_scales_q4k(const uint8_t* scales, uint8_t* out_scales) {
    // Q4_K packs 8x 6-bit scales into 12 bytes (8 * 6 = 48 bits = 6 bytes per 4 scales)
    // Layout: AAAAAABB BBBBCCCC CCDDDDDD ...
    
    out_scales[0] = scales[0] & 0x3F;
    out_scales[1] = (scales[0] >> 6) | ((scales[1] & 0x0F) << 2);
    out_scales[2] = (scales[1] >> 4) | ((scales[2] & 0x03) << 4);
    out_scales[3] = (scales[2] >> 2) & 0x3F;
    
    out_scales[4] = ((scales[2] >> 8) & 0x03) | ((scales[3] & 0x0F) << 2);
    out_scales[5] = (scales[3] >> 4) | ((scales[4] & 0x03) << 4);
    out_scales[6] = (scales[4] >> 2) & 0x3F;
    out_scales[7] = (scales[4] >> 8) | ((scales[5] & 0x03) << 4);
}

// =============================================================================
// Scalar Fallback Dequantization
// =============================================================================

static void dequantize_q4k_scalar(const block_q4_K* block, float* out) {
    const float d     = fp16_to_fp32(block->d);
    const float dmin  = fp16_to_fp32(block->dmin);
    
    // Extract 8 sub-block scales (6-bit each)
    uint8_t scales[8];
    extract_scales_q4k(block->scales, scales);
    
    const uint8_t* qs = block->qs;
    
    // Dequantize 8 sub-blocks of 32 values each
    for (int i = 0; i < 8; ++i) {
        const float sc = d * static_cast<float>(scales[i]);
        const float m  = dmin * static_cast<float>(scales[i]);
        
        // 32 values = 16 bytes (2 values per byte)
        for (int j = 0; j < 16; ++j) {
            const uint8_t byte = qs[i * 16 + j];
            const uint8_t q0 = byte & 0x0F;
            const uint8_t q1 = byte >> 4;
            
            out[i * 32 + j * 2 + 0] = sc * static_cast<float>(q0) - m;
            out[i * 32 + j * 2 + 1] = sc * static_cast<float>(q1) - m;
        }
    }
}

// =============================================================================
// AVX2 Vectorized Dequantization (256-bit SIMD, 8 floats at a time)
// =============================================================================

#if defined(__AVX2__)

static void dequantize_q4k_avx2(const block_q4_K* block, float* out) {
    const float d    = fp16_to_fp32(block->d);
    const float dmin = fp16_to_fp32(block->dmin);
    
    // Extract scales
    uint8_t scales[8];
    extract_scales_q4k(block->scales, scales);
    
    const uint8_t* qs = block->qs;
    
    // Mask for extracting 4-bit values
    const __m256i mask_lo = _mm256_set1_epi8(0x0F);
    
    // Process 8 sub-blocks
    for (int i = 0; i < 8; ++i) {
        const float sc = d * static_cast<float>(scales[i]);
        const float m  = dmin * static_cast<float>(scales[i]);
        
        const __m256 v_scale = _mm256_set1_ps(sc);
        const __m256 v_min   = _mm256_set1_ps(m);
        
        // Process 32 values in sub-block (16 bytes → 32 nibbles)
        const uint8_t* q_base = &qs[i * 16];
        
        for (int j = 0; j < 4; ++j) {  // 4 iterations × 8 floats = 32 values
            // Load 4 bytes = 8 quantized values
            uint32_t bytes;
            memcpy(&bytes, q_base + j * 4, 4);
            
            // Extract low nibbles (q0, q2, q4, q6, q8, q10, q12, q14)
            __m128i q_bytes = _mm_cvtsi32_si128(bytes);
            __m256i q_256 = _mm256_cvtepu8_epi32(q_bytes);
            __m256i q_lo = _mm256_and_si256(q_256, _mm256_set1_epi32(0x0F));
            __m256 f_lo = _mm256_cvtepi32_ps(q_lo);
            
            // Apply scale and min: f = sc * q - m
            __m256 result_lo = _mm256_fmsub_ps(v_scale, f_lo, v_min);
            _mm256_storeu_ps(&out[i * 32 + j * 8], result_lo);
            
            // Extract high nibbles (q1, q3, q5, q7, q9, q11, q13, q15)
            __m256i q_hi = _mm256_srli_epi32(q_256, 4);
            q_hi = _mm256_and_si256(q_hi, _mm256_set1_epi32(0x0F));
            __m256 f_hi = _mm256_cvtepi32_ps(q_hi);
            
            __m256 result_hi = _mm256_fmsub_ps(v_scale, f_hi, v_min);
            _mm256_storeu_ps(&out[i * 32 + j * 8 + 4], result_hi);
        }
    }
}

#endif  // __AVX2__

// =============================================================================
// AVX-512 Vectorized Dequantization (512-bit SIMD, 16 floats at a time)
// =============================================================================

#if defined(__AVX512F__)

static void dequantize_q4k_avx512(const block_q4_K* block, float* out) {
    const float d    = fp16_to_fp32(block->d);
    const float dmin = fp16_to_fp32(block->dmin);
    
    // Extract scales
    uint8_t scales[8];
    extract_scales_q4k(block->scales, scales);
    
    const uint8_t* qs = block->qs;
    
    // Process 8 sub-blocks
    for (int i = 0; i < 8; ++i) {
        const float sc = d * static_cast<float>(scales[i]);
        const float m  = dmin * static_cast<float>(scales[i]);
        
        const __m512 v_scale = _mm512_set1_ps(sc);
        const __m512 v_min   = _mm512_set1_ps(m);
        
        // Process 32 values in sub-block (16 bytes → 32 nibbles)
        const uint8_t* q_base = &qs[i * 16];
        
        for (int j = 0; j < 2; ++j) {  // 2 iterations × 16 floats = 32 values
            // Load 8 bytes = 16 quantized values
            uint64_t bytes;
            memcpy(&bytes, q_base + j * 8, 8);
            
            // Extract nibbles and convert to fp32
            __m128i q_bytes = _mm_cvtsi64_si128(bytes);
            __m512i q_512 = _mm512_cvtepu8_epi32(q_bytes);
            
            // Low nibbles
            __m512i q_lo = _mm512_and_si512(q_512, _mm512_set1_epi32(0x0F));
            __m512 f_lo = _mm512_cvtepi32_ps(q_lo);
            __m512 result_lo = _mm512_fmsub_ps(v_scale, f_lo, v_min);
            _mm512_storeu_ps(&out[i * 32 + j * 16], result_lo);
            
            // High nibbles
            __m512i q_hi = _mm512_srli_epi32(q_512, 4);
            q_hi = _mm512_and_si512(q_hi, _mm512_set1_epi32(0x0F));
            __m512 f_hi = _mm512_cvtepi32_ps(q_hi);
            __m512 result_hi = _mm512_fmsub_ps(v_scale, f_hi, v_min);
            _mm512_storeu_ps(&out[i * 32 + j * 16 + 8], result_hi);
        }
    }
}

#endif  // __AVX512F__

// =============================================================================
// Runtime Dispatch Based on CPU Capabilities
// =============================================================================

static bool has_avx2() {
#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 5)) != 0;  // EBX bit 5
#else
    return __builtin_cpu_supports("avx2");
#endif
}

static bool has_avx512f() {
#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 16)) != 0;  // EBX bit 16
#else
    return __builtin_cpu_supports("avx512f");
#endif
}

// =============================================================================
// Public API: KQuant_DequantizeQ4_K
// =============================================================================

extern "C" {

/**
 * @brief Dequantize Q4_K quantized tensor to fp32
 * 
 * @param blocks    Input blocks (Q4_K format, 144 bytes each)
 * @param n_blocks  Number of blocks (total elements = n_blocks * 256)
 * @param out       Output fp32 array (must hold n_blocks * 256 floats)
 */
void KQuant_DequantizeQ4_K(const void* blocks, size_t n_blocks, float* out) {
    if (!blocks || !out || n_blocks == 0) return;
    
    const block_q4_K* block_ptr = static_cast<const block_q4_K*>(blocks);
    
    // Runtime dispatch based on CPU features
    static int dispatch_mode = -1;
    if (dispatch_mode == -1) {
#if defined(__AVX512F__)
        if (has_avx512f()) {
            dispatch_mode = 2;  // AVX-512
        } else 
#endif
#if defined(__AVX2__)
        if (has_avx2()) {
            dispatch_mode = 1;  // AVX2
        } else 
#endif
        {
            dispatch_mode = 0;  // Scalar
        }
    }
    
    // Process all blocks
    for (size_t i = 0; i < n_blocks; ++i) {
        float* out_block = out + i * QK_K;
        const block_q4_K* block = &block_ptr[i];
        
        switch (dispatch_mode) {
#if defined(__AVX512F__)
            case 2:
                dequantize_q4k_avx512(block, out_block);
                break;
#endif
#if defined(__AVX2__)
            case 1:
                dequantize_q4k_avx2(block, out_block);
                break;
#endif
            default:
                dequantize_q4k_scalar(block, out_block);
                break;
        }
    }
}

/**
 * @brief Get dispatch mode being used (for diagnostics)
 * @return 0=scalar, 1=AVX2, 2=AVX-512
 */
int KQuant_GetDispatchMode() {
#if defined(__AVX512F__)
    if (has_avx512f()) return 2;
#endif
#if defined(__AVX2__)
    if (has_avx2()) return 1;
#endif
    return 0;
}

/**
 * @brief Get Q4_K block size (always 144 bytes)
 */
size_t KQuant_Q4K_BlockSize() {
    return sizeof(block_q4_K);
}

/**
 * @brief Get elements per Q4_K block (always 256)
 */
size_t KQuant_Q4K_ElementsPerBlock() {
    return QK_K;
}

} // extern "C"
