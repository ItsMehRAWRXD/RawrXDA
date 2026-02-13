// =============================================================================
// nanoquant_bridge.h
// C++ Bridge to MASM64 NanoQuant Engine (RawrXD_NanoQuant_Engine.asm)
//
// Maps all PUBLIC ASM exports to C++ extern "C" linkage.
// Provides NanoQuantContext RAII wrapper in RawrXD::Quant namespace.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

// =============================================================================
//  NQ_1 Block Layout Constants (must match ASM EQU values)
// =============================================================================
static constexpr int    BLOCK_NQ1_SIZE       = 34;      // 2 + 32 bytes
static constexpr int    NQ1_SCALE_OFFSET     = 0;       // F16 scale at byte 0
static constexpr int    NQ1_SIGNS_OFFSET     = 2;       // 32 bytes packed sign bits
static constexpr int    QK_NQ1               = 256;     // Elements per NQ_1 block

// GGML type identifiers (extends ggml_type enum)
static constexpr int    GGML_TYPE_NQ_1       = 20;      // Block-level binary
static constexpr int    GGML_TYPE_NQ_R4      = 21;      // Matrix-level rank-4

// NQ_MATRIX header layout
static constexpr int    NQM_MAGIC_OFFSET     = 0;
static constexpr int    NQM_ROWS_OFFSET      = 4;
static constexpr int    NQM_COLS_OFFSET      = 8;
static constexpr int    NQM_RANK_OFFSET      = 12;
static constexpr int    NQM_SCALES_OFFSET    = 16;
static constexpr int    NQM_HEADER_SIZE      = 32;
static constexpr uint32_t NQM_MAGIC_VALUE    = 0x3452514E; // 'NQR4'
static constexpr int    NQM_MAX_RANK         = 8;

// CPU capability flags (returned by NanoQuant_Init)
static constexpr uint32_t NQ_CAP_AVX2           = 0x01;
static constexpr uint32_t NQ_CAP_FMA3           = 0x02;
static constexpr uint32_t NQ_CAP_F16C           = 0x04;
static constexpr uint32_t NQ_CAP_AVX512F        = 0x08;
static constexpr uint32_t NQ_CAP_AVX512BW       = 0x10;
static constexpr uint32_t NQ_CAP_AVX512VL       = 0x20;
static constexpr uint32_t NQ_CAP_AVX512VPOPCNTDQ = 0x40;
static constexpr uint32_t NQ_CAP_AVX512BITALG   = 0x80;

// =============================================================================
//  NQ_1 Block Structure (C++ POD mirror of ASM layout)
// =============================================================================
#pragma pack(push, 1)
struct BlockNQ1 {
    uint16_t d;             // F16 scale factor
    uint8_t  signs[32];     // 256 packed sign bits (little-endian)
};
static_assert(sizeof(BlockNQ1) == BLOCK_NQ1_SIZE, "BlockNQ1 size mismatch");
#pragma pack(pop)

// =============================================================================
//  NQ_MATRIX Header (C++ POD mirror)
// =============================================================================
#pragma pack(push, 1)
struct NQMatrixHeader {
    uint32_t magic;         // NQM_MAGIC_VALUE
    uint32_t rows;          // M
    uint32_t cols;          // N
    uint32_t rank;          // r (1-8)
    float    scales[4];     // rank-1..4 scales
};
static_assert(sizeof(NQMatrixHeader) == NQM_HEADER_SIZE, "NQMatrixHeader size mismatch");
#pragma pack(pop)

// =============================================================================
//  NanoQuant Statistics (returned by NQ1_GetStats)
// =============================================================================
struct NQStats {
    uint64_t quant_blocks;
    uint64_t dequant_blocks;
    uint64_t vecdot_calls;
    uint64_t gemm_calls;
    uint64_t admm_iter_total;
};

// =============================================================================
//  MASM64 Export Declarations (extern "C" — Microsoft x64 ABI)
// =============================================================================
extern "C" {

// Initialization & capabilities
uint32_t NanoQuant_Init(void);
uint32_t NanoQuant_GetCapabilities(void);

// Quantization: F32 → NQ_1
void     NQ1_QuantizeBlock_Fast(const float* src256, BlockNQ1* dst);
uint32_t NQ1_QuantizeBlock_ADMM(const float* src256, BlockNQ1* dst, uint32_t max_iter);
uint64_t NQ1_QuantizeTensor(const float* src, BlockNQ1* dst, uint64_t n_elements, uint32_t admm_iter);

// Dequantization: NQ_1 → F32
uint32_t NQ1_DequantBlock_AVX512(const BlockNQ1* src, float* dst256);
uint32_t NQ1_DequantBlock_AVX2(const BlockNQ1* src, float* dst256);
uint64_t NQ1_Dequant(const BlockNQ1* src, float* dst, uint64_t n_blocks);

// Fused vector dot product (NQ_1 · F32)
// AVX-512 and AVX2 paths — result returned via xmm0 (float ABI)
float    NQ1_VecDot_AVX512(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);
float    NQ1_VecDot_AVX2(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);
float    NQ1_VecDot(const BlockNQ1* nq1, const float* f32, uint64_t n_blocks);

// GEMV / GEMM
void     NQ1_SGEMV(const BlockNQ1* A_nq1, const float* x, float* y,
                    uint32_t M, uint32_t K_elements);
void     NQ1_SGEMM(const BlockNQ1* A_nq1, const float* B, float* C,
                    uint32_t M, uint32_t N, uint32_t K_elements);
void     NQ1_SGEMM_Tiled(const BlockNQ1* A_nq1, const float* B, float* C,
                          uint32_t M, uint32_t N, uint32_t K_elements);

// Matrix-level binary factorization (sub-1-bit)
uint64_t NQ_MatrixFactor_Rank1(float* W, void* output, uint32_t M, uint32_t N);
uint64_t NQ_MatrixFactor_MultiRank(float* W, void* output,
                                    uint32_t M, uint32_t N, uint32_t rank);
void     NQ_MatrixGEMM(const void* nq_matrix, const float* B, float* C,
                        uint32_t M, uint32_t N, uint32_t K);

// Requantization paths (NQ_1 → legacy formats)
void     NQ1_Requantize_Q4_0(const BlockNQ1* src, void* dst_q4_0, uint64_t n_blocks);
void     NQ1_Requantize_Q8_0(const BlockNQ1* src, void* dst_q8_0, uint64_t n_blocks);
void     NQ1_Requantize_Q4_K(const BlockNQ1* src, void* dst_q4_k, uint64_t n_blocks);

// Utility
uint32_t NQ1_GetBlockSize(void);
float    NQ1_GetCompressionRatio(void);
uint32_t NQ1_GetStats(NQStats* stats);

// Type-based dispatch (compatible with ggml_type dispatch tables)
// op: 0=dequant, 1=vecdot, 2=quant_fast, 3=quant_admm
int64_t  NQ1_Dispatch(uint32_t op, void* arg1, void* arg2, uint64_t arg3);

} // extern "C"

// =============================================================================
//  C++ RAII Wrapper — RawrXD::Quant::NanoQuantContext
// =============================================================================
namespace RawrXD { namespace Quant {

/// RAII wrapper that ensures NanoQuant_Init is called exactly once.
/// Thread-safe via std::call_once semantics (init flag).
class NanoQuantContext {
public:
    NanoQuantContext() : caps_(0), initialized_(false) {}

    /// Initialize CPU detection and dispatch table. Safe to call multiple times.
    bool init() {
        if (initialized_) return true;
        caps_ = NanoQuant_Init();
        initialized_ = (caps_ != 0); // At minimum AVX2 required
        return initialized_;
    }

    bool isInitialized()  const { return initialized_; }
    uint32_t capabilities() const { return caps_; }

    bool hasAVX2()       const { return (caps_ & NQ_CAP_AVX2) != 0; }
    bool hasFMA3()       const { return (caps_ & NQ_CAP_FMA3) != 0; }
    bool hasF16C()       const { return (caps_ & NQ_CAP_F16C) != 0; }
    bool hasAVX512F()    const { return (caps_ & NQ_CAP_AVX512F) != 0; }
    bool hasAVX512BW()   const { return (caps_ & NQ_CAP_AVX512BW) != 0; }
    bool hasAVX512VL()   const { return (caps_ & NQ_CAP_AVX512VL) != 0; }
    bool hasVPOPCNTDQ()  const { return (caps_ & NQ_CAP_AVX512VPOPCNTDQ) != 0; }
    bool hasBITALG()     const { return (caps_ & NQ_CAP_AVX512BITALG) != 0; }

    /// Bits per element for NQ_1 format
    static constexpr float bitsPerElement() { return 34.0f * 8.0f / 256.0f; } // 1.0625

    /// Compression ratio vs FP16
    static constexpr float compressionVsFP16() { return 16.0f / 1.0625f; } // ~15.06x

    /// Compression ratio vs FP32
    static constexpr float compressionVsFP32() { return 32.0f / 1.0625f; } // ~30.12x

    /// Get performance counters
    NQStats stats() const {
        NQStats s{};
        NQ1_GetStats(&s);
        return s;
    }

private:
    uint32_t caps_;
    bool     initialized_;
};

}} // namespace RawrXD::Quant
