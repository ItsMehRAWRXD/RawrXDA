// q4_0_unpack_neon.cc — ARM-NEON Q4_0 unpacking kernel
// Phase 5: Port AVX2 Q4_0 kernel to ARM64 for Apple Silicon parity
// Target: ≥5× speedup on M1/M2/M3 vs scalar

#include <cstdint>
#include <arm_neon.h>

extern "C" void q4_0_unpack_64x64_neon(const uint8_t* q4, float* fp32, float scale) {
    // Unpack a 64×64 Q4_0 tile (two 4-bit values per byte) into fp32 with scale.
    // Layout: row-major 64 rows (K) × 64 cols (N)
    // Q4_0 format: symmetric 4-bit in [-8..7], stored as nibbles
    
    const int TN = 64;
    const int TK = 64;
    const float32x4_t vscale = vdupq_n_f32(scale);
    const float32x4_t voffset = vdupq_n_f32(-8.0f);  // Q4_0 bias
    
    for (int k = 0; k < TK; ++k) {
        const int row_off = k * TN;
        const uint8_t* q4_row = q4 + (row_off >> 1);  // 32 bytes per row
        float* fp32_row = fp32 + row_off;
        
        // Process 16 nibbles (8 bytes) at a time → 16 float outputs
        // NEON strategy: load 8 bytes → expand to 16×uint8 → convert to fp32
        for (int n = 0; n < TN; n += 16) {
            // Load 8 bytes (16 nibbles)
            uint8x8_t bytes = vld1_u8(q4_row + (n >> 1));
            
            // Extract low and high nibbles
            // Low nibbles: bytes & 0x0F
            uint8x8_t lo_nib = vand_u8(bytes, vdup_n_u8(0x0F));
            // High nibbles: (bytes >> 4) & 0x0F
            uint8x8_t hi_nib = vshr_n_u8(bytes, 4);
            
            // Interleave to get sequential order: [lo0, hi0, lo1, hi1, ...]
            uint8x8x2_t interleaved = vzip_u8(lo_nib, hi_nib);
            
            // Expand to 16×uint8 then to 16×uint16 then to 4×float32x4
            uint8x16_t nibs = vcombine_u8(interleaved.val[0], interleaved.val[1]);
            uint16x8_t nibs_lo = vmovl_u8(vget_low_u8(nibs));
            uint16x8_t nibs_hi = vmovl_u8(vget_high_u8(nibs));
            
            // Convert to float and apply scale/offset
            // nibs[0..3] → float
            uint32x4_t u32_0 = vmovl_u16(vget_low_u16(nibs_lo));
            float32x4_t f0 = vcvtq_f32_u32(u32_0);
            f0 = vaddq_f32(f0, voffset);  // subtract 8
            f0 = vmulq_f32(f0, vscale);
            vst1q_f32(fp32_row + n + 0, f0);
            
            // nibs[4..7] → float
            uint32x4_t u32_1 = vmovl_u16(vget_high_u16(nibs_lo));
            float32x4_t f1 = vcvtq_f32_u32(u32_1);
            f1 = vaddq_f32(f1, voffset);
            f1 = vmulq_f32(f1, vscale);
            vst1q_f32(fp32_row + n + 4, f1);
            
            // nibs[8..11] → float
            uint32x4_t u32_2 = vmovl_u16(vget_low_u16(nibs_hi));
            float32x4_t f2 = vcvtq_f32_u32(u32_2);
            f2 = vaddq_f32(f2, voffset);
            f2 = vmulq_f32(f2, vscale);
            vst1q_f32(fp32_row + n + 8, f2);
            
            // nibs[12..15] → float
            uint32x4_t u32_3 = vmovl_u16(vget_high_u16(nibs_hi));
            float32x4_t f3 = vcvtq_f32_u32(u32_3);
            f3 = vaddq_f32(f3, voffset);
            f3 = vmulq_f32(f3, vscale);
            vst1q_f32(fp32_row + n + 12, f3);
        }
    }
}
