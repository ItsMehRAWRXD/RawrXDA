// kernels/q4_0_unpack_avx2.cc
// Batch dequantization: unpack Q4_0 to fp32 scratch buffer, then use fast fp32 GEMM

#include <immintrin.h>
#include <cstdint>
#include <cstring>

#ifdef __AVX2__

// Unpack 8 nibbles (4 bytes) to 8 fp32 values with scale
static inline void unpack_8_nibbles(__m256* out, const uint8_t* q4_4bytes, float scale) {
    // Load 4 bytes = 8 nibbles
    uint32_t bytes4;
    std::memcpy(&bytes4, q4_4bytes, 4);
    
    // Extract all 8 nibbles
    uint8_t nibbles[8];
    nibbles[0] = (bytes4 >>  0) & 0xF;
    nibbles[1] = (bytes4 >>  4) & 0xF;
    nibbles[2] = (bytes4 >>  8) & 0xF;
    nibbles[3] = (bytes4 >> 12) & 0xF;
    nibbles[4] = (bytes4 >> 16) & 0xF;
    nibbles[5] = (bytes4 >> 20) & 0xF;
    nibbles[6] = (bytes4 >> 24) & 0xF;
    nibbles[7] = (bytes4 >> 28) & 0xF;
    
    // Convert to int32, subtract 8 (for signed range), convert to float, scale
    __m256i nib_i32 = _mm256_cvtepu8_epi32(_mm_loadl_epi64((__m128i*)nibbles));
    __m256 nib_f32 = _mm256_cvtepi32_ps(nib_i32);
    __m256 offset = _mm256_set1_ps(-8.0f);
    __m256 scale_vec = _mm256_set1_ps(scale);
    
    *out = _mm256_mul_ps(_mm256_add_ps(nib_f32, offset), scale_vec);
}

// Unpack Q4_0 block (32 weights = 16 bytes) to 32 fp32 values
extern "C" void q4_0_unpack_32(const uint8_t* q4_16bytes, float* fp32_32, float scale) {
    __m256 w0, w1, w2, w3;
    unpack_8_nibbles(&w0, q4_16bytes + 0, scale);
    unpack_8_nibbles(&w1, q4_16bytes + 4, scale);
    unpack_8_nibbles(&w2, q4_16bytes + 8, scale);
    unpack_8_nibbles(&w3, q4_16bytes + 12, scale);
    
    _mm256_storeu_ps(fp32_32 + 0,  w0);
    _mm256_storeu_ps(fp32_32 + 8,  w1);
    _mm256_storeu_ps(fp32_32 + 16, w2);
    _mm256_storeu_ps(fp32_32 + 24, w3);
}

// Unpack 64×64 tile of Q4_0 weights (4096 weights = 2048 bytes) to fp32 (16 KB)
extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
    // 64×64 = 4096 weights = 2048 bytes Q4_0
    // Process 32 weights (16 bytes) at a time = 128 iterations
    for (int i = 0; i < 128; ++i) {
        q4_0_unpack_32(q4 + i * 16, fp32 + i * 32, scale);
    }
}

// Optimized version: unroll 4× for better throughput
extern "C" void q4_0_unpack_64x64_fast(const uint8_t* q4, float* fp32, float scale) {
    __m256 scale_vec = _mm256_set1_ps(scale);
    __m256 offset = _mm256_set1_ps(-8.0f);
    
    for (int i = 0; i < 2048; i += 16) {  // Process 16 bytes = 32 nibbles per iteration
        // Load 16 bytes
        __m128i bytes16 = _mm_loadu_si128((__m128i*)(q4 + i));
        
        // Extract nibbles in chunks of 8
        for (int j = 0; j < 4; ++j) {  // 4 chunks × 8 nibbles = 32 nibbles
            uint32_t bytes4;
            std::memcpy(&bytes4, &q4[i + j * 4], 4);
            
            // Extract 8 nibbles
            alignas(16) uint8_t nibbles[8];
            nibbles[0] = (bytes4 >>  0) & 0xF;
            nibbles[1] = (bytes4 >>  4) & 0xF;
            nibbles[2] = (bytes4 >>  8) & 0xF;
            nibbles[3] = (bytes4 >> 12) & 0xF;
            nibbles[4] = (bytes4 >> 16) & 0xF;
            nibbles[5] = (bytes4 >> 20) & 0xF;
            nibbles[6] = (bytes4 >> 24) & 0xF;
            nibbles[7] = (bytes4 >> 28) & 0xF;
            
            // Convert and store
            __m256i nib_i32 = _mm256_cvtepu8_epi32(_mm_loadl_epi64((__m128i*)nibbles));
            __m256 nib_f32 = _mm256_cvtepi32_ps(nib_i32);
            __m256 weights = _mm256_mul_ps(_mm256_add_ps(nib_f32, offset), scale_vec);
            
            _mm256_storeu_ps(fp32 + (i * 2) + (j * 8), weights);
        }
    }
}

#else

extern "C" void q4_0_unpack_32(const uint8_t* q4_16bytes, float* fp32_32, float scale) {
    for (int i = 0; i < 16; ++i) {
        uint8_t byte = q4_16bytes[i];
        fp32_32[i*2 + 0] = ((float)((int8_t)(byte & 0xF) - 8)) * scale;
        fp32_32[i*2 + 1] = ((float)((int8_t)(byte >> 4) - 8)) * scale;
    }
}

extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
    for (int i = 0; i < 128; ++i) {
        q4_0_unpack_32(q4 + i * 16, fp32 + i * 32, scale);
    }
}

extern "C" void q4_0_unpack_64x64_fast(const uint8_t* q4, float* fp32, float scale) {
    q4_0_unpack_64x64(q4, fp32, scale);  // Fallback
}

#endif
