#pragma once

#include <cstddef>
#include <cstdint>

// NQ_1 block layout constants (must match RawrXD_NanoQuant_Engine.asm)
static constexpr int BLOCK_NQ1_SIZE = 34;
static constexpr int NQ1_SCALE_OFFSET = 0;
static constexpr int NQ1_SIGNS_OFFSET = 2;
static constexpr int QK_NQ1 = 256;

// GGML type identifiers
#ifndef GGML_TYPE_NQ_1
#define GGML_TYPE_NQ_1 20
#endif

#ifndef GGML_TYPE_NQ_R4
#define GGML_TYPE_NQ_R4 21
#endif

// NQ_MATRIX layout constants
static constexpr int NQM_MAGIC_OFFSET = 0;
static constexpr int NQM_ROWS_OFFSET = 4;
static constexpr int NQM_COLS_OFFSET = 8;
static constexpr int NQM_RANK_OFFSET = 12;
static constexpr int NQM_SCALES_OFFSET = 16;
static constexpr int NQM_HEADER_SIZE = 32;
static constexpr uint32_t NQM_MAGIC_VALUE = 0x3452514E;
static constexpr int NQM_MAX_RANK = 8;

// CPU capability flags returned by NanoQuant_Init
static constexpr uint32_t NQ_CAP_AVX2 = 0x01;
static constexpr uint32_t NQ_CAP_FMA3 = 0x02;
static constexpr uint32_t NQ_CAP_F16C = 0x04;
static constexpr uint32_t NQ_CAP_AVX512F = 0x08;
static constexpr uint32_t NQ_CAP_AVX512BW = 0x10;
static constexpr uint32_t NQ_CAP_AVX512VL = 0x20;
static constexpr uint32_t NQ_CAP_AVX512VPOPCNTDQ = 0x40;
static constexpr uint32_t NQ_CAP_AVX512BITALG = 0x80;

// Useful constants for telemetry and reporting
static constexpr float NQ1_BITS_PER_ELEMENT = 34.0f * 8.0f / 256.0f;
static constexpr float NQ1_COMPRESSION_VS_FP16 = 16.0f / NQ1_BITS_PER_ELEMENT;
static constexpr float NQ1_COMPRESSION_VS_FP32 = 32.0f / NQ1_BITS_PER_ELEMENT;

#pragma pack(push, 1)
struct BlockNQ1 {
    uint16_t d;
    uint8_t signs[32];
};
static_assert(sizeof(BlockNQ1) == BLOCK_NQ1_SIZE, "BlockNQ1 size mismatch");

struct NQMatrixHeader {
    uint32_t magic;
    uint32_t rows;
    uint32_t cols;
    uint32_t rank;
    float scales[4];
};
static_assert(sizeof(NQMatrixHeader) == NQM_HEADER_SIZE, "NQMatrixHeader size mismatch");
#pragma pack(pop)

struct NQStats {
    uint64_t quant_blocks;
    uint64_t dequant_blocks;
    uint64_t vecdot_calls;
    uint64_t gemm_calls;
    uint64_t admm_iter_total;
};

extern "C" {
uint32_t NanoQuant_Init(void);
uint32_t NanoQuant_GetCapabilities(void);

void NQ1_QuantizeBlock_Fast(const float* src256, BlockNQ1* dst);
uint32_t NQ1_QuantizeBlock_ADMM(const float* src256, BlockNQ1* dst, uint32_t max_iter);
uint64_t NQ1_QuantizeTensor(const float* src, BlockNQ1* dst, uint64_t n_elements, uint32_t admm_iter);

uint32_t NQ1_DequantBlock_AVX512(const BlockNQ1* src, float* dst256);
uint32_t NQ1_DequantBlock_AVX2(const BlockNQ1* src, float* dst256);
uint64_t NQ1_Dequant(const BlockNQ1* src, float* dst, uint64_t n_blocks);

float NQ1_VecDot_AVX512(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);
float NQ1_VecDot_AVX2(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);
float NQ1_VecDot(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);

void NQ1_SGEMV(const BlockNQ1* A_nq1, const float* x, float* y, uint32_t M, uint32_t K_elements);
void NQ1_SGEMM(const BlockNQ1* A_nq1, const float* B, float* C, uint32_t M, uint32_t N, uint32_t K_elements);
void NQ1_SGEMM_Tiled(const BlockNQ1* A_nq1, const float* B, float* C, uint32_t M, uint32_t N, uint32_t K_elements);

uint64_t NQ_MatrixFactor_Rank1(float* W, void* output, uint32_t M, uint32_t N);
uint64_t NQ_MatrixFactor_MultiRank(float* W, void* output, uint32_t M, uint32_t N, uint32_t rank);
void NQ_MatrixGEMM(const void* nq_matrix, const float* B, float* C, uint32_t M, uint32_t N, uint32_t K);

void NQ1_Requantize_Q4_0(const BlockNQ1* src, void* dst_q4_0, uint64_t n_blocks);
void NQ1_Requantize_Q8_0(const BlockNQ1* src, void* dst_q8_0, uint64_t n_blocks);
void NQ1_Requantize_Q4_K(const BlockNQ1* src, void* dst_q4_k, uint64_t n_blocks);

uint32_t NQ1_GetBlockSize(void);
float NQ1_GetCompressionRatio(void);
uint32_t NQ1_GetStats(NQStats* stats);
int64_t NQ1_Dispatch(uint32_t op, void* arg1, void* arg2, uint64_t arg3);
}
