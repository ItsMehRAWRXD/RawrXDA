// flash_attn_asm_fallback.cpp — fallback path for MASM build
#include <cstdint>
#include <vector>
#include <cmath>

// Intrinsics-based flash-attention implementation
extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar);

struct BlockQ8_0 {
    float scale;
    int8_t qs[32];
};

extern "C" void flash_attn_asm_avx2_fallback(
    const float* Q, const void* K, const float* V, float* O,
    int seqLen, int headDim, int quantType) {
    (void)quantType;  // quantType reserved for future formats

    const BlockQ8_0* blocks = static_cast<const BlockQ8_0*>(K);
    const int total = seqLen * headDim;
    const int num_blocks = total / 32;

    std::vector<float> k_fp32(total);

    for (int b = 0; b < num_blocks; ++b) {
        float scale = blocks[b].scale;
        for (int i = 0; i < 32; ++i) {
            k_fp32[b * 32 + i] = static_cast<float>(blocks[b].qs[i]) * scale;
        }
    }

    flash_attn_forward(Q, k_fp32.data(), V, O, seqLen, headDim, false);
}
