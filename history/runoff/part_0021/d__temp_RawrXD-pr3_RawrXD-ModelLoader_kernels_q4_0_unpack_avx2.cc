#include <cstdint>
#include <immintrin.h>

extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
    // Unpack a 64x64 Q4_0 tile (two 4-bit values per byte) into fp32 with scale.
    // Layout is row-major: 64 rows (K) x 64 cols (N).
    // q4 index: idx = k * 64 + n; byte = idx >> 1; lo/hi nibble holds value.
    const int TN = 64;
    const int TK = 64;

    for (int k = 0; k < TK; ++k) {
        const int row_off = k * TN;
        for (int n = 0; n < TN; ++n) {
            int idx = row_off + n;
            int byte_idx = idx >> 1;
            bool hi = (idx & 1) != 0;
            uint8_t byte = q4[byte_idx];
            int v4 = hi ? ((byte >> 4) & 0xF) : (byte & 0xF);
            float v = (float)(v4 - 8) * scale; // symmetric Q4_0 in [-8..7]
            fp32[row_off + n] = v;
        }
    }
}
