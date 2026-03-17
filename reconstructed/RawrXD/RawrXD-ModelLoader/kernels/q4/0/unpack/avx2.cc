#include <immintrin.h>
#include <cstdint>

// Unpack Q4_0 quantized values to float using AVX2
// Q4_0 format: each byte contains two 4-bit values (0-15), mapped to int8 range (-8 to 7)
extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
    // For now, use scalar unpacking to ensure correctness
    // TODO: Optimize with AVX2 intrinsics
    for (int i = 0; i < 4096; ++i) {
        int byte_idx = i >> 1;
        bool is_high = (i & 1) != 0;
        uint8_t byte = q4[byte_idx];
        uint8_t nibble = is_high ? (byte >> 4) & 0xF : byte & 0xF;
        int8_t val = static_cast<int8_t>(nibble) - 8; // Map 0-15 to -8 to 7
        fp32[i] = static_cast<float>(val) * scale;
    }
}