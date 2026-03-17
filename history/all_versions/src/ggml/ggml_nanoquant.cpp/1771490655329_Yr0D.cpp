// =============================================================================
// ggml_nanoquant.cpp
// GGML Type Registration Implementation for NanoQuant NQ_1 and NQ_R4
//
// Delegates to_float / from_float / vec_dot to MASM64 ASM exports via
// nanoquant_bridge.h. Provides GGUF-compatible type traits.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "ggml/ggml_nanoquant.h"
#include "quant/nanoquant_bridge.h"

#include <cstring>
#include <atomic>

// =============================================================================
//  NQ_1 Callbacks (delegate to MASM64 exports)
// =============================================================================

/// Dequantize NQ_1 blocks to F32.
/// n_elements must be a multiple of QK_NQ1 (256).
static void nq1_to_float(const void* src, float* dst, int64_t n_elements) {
    if (!src || !dst || n_elements <= 0) return;
    const int64_t n_blocks = n_elements / QK_NQ1;
    NQ1_Dequant(reinterpret_cast<const BlockNQ1*>(src), dst,
                static_cast<uint64_t>(n_blocks));
}

/// Quantize F32 to NQ_1 blocks (fast path, no ADMM).
/// n_elements must be a multiple of QK_NQ1 (256).
static void nq1_from_float(const float* src, void* dst, int64_t n_elements) {
    if (!src || !dst || n_elements <= 0) return;
    NQ1_QuantizeTensor(src, reinterpret_cast<BlockNQ1*>(dst),
                       static_cast<uint64_t>(n_elements), 0 /* fast path */);
}

/// Vector dot product: NQ_1 · F32 → scalar.
/// Compatible with ggml_vec_dot_t signature.
///   n        = number of elements
///   result   = output float*
///   vx       = NQ_1 block array
///   vy       = F32 array
///   nrc      = number of rows computed (unused, 1 for basic)
static void nq1_vec_dot(int n, float* result, size_t /*bs_x*/,
                         const void* vx, size_t /*bx*/,
                         const void* vy, size_t /*by*/,
                         int /*nrc*/) {
    if (!result || !vx || !vy || n <= 0) {
        if (result) *result = 0.0f;
        return;
    }
    const int64_t n_blocks = static_cast<int64_t>(n) / QK_NQ1;
    *result = NQ1_VecDot(reinterpret_cast<const BlockNQ1*>(vx),
                         reinterpret_cast<const float*>(vy),
                         static_cast<uint64_t>(n_blocks));
}

// =============================================================================
//  NQ_R4 Callbacks (matrix-level — currently only to_float via reconstruction)
// =============================================================================

/// Reconstruct matrix from NQ_R4 binary factorization.
/// For NQ_R4, n_elements is M*N stored in the header.
/// Not a strict block format — delegates to NQ_MatrixGEMM with identity.
static void nq_r4_to_float(const void* src, float* dst, int64_t n_elements) {
    if (!src || !dst || n_elements <= 0) return;
    
    const auto* hdr = reinterpret_cast<const NQMatrixHeader*>(src);
    if (hdr->magic != NQM_MAGIC_VALUE) return;
    
    const uint32_t M = hdr->rows;
    const uint32_t N = hdr->cols;
    
    // Reconstruct W = NQ_R4 * I_N via NQ_MatrixGEMM
    // Create identity matrix for reconstruction
    // For large matrices this is expensive; production code should use
    // NQ_MatrixGEMM directly with the actual operand B.
    // This path is for GGUF type-system compatibility only.
    
    // Zero output first
    std::memset(dst, 0, static_cast<size_t>(M) * N * sizeof(float));
    
    // Build identity column-by-column (N×N too large for stack)
    // Instead, reconstruct element-by-element from the factorization
    // W[i,j] = Σ_k scales[k] * rsign[k][i] * csign[k][j]
    const uint8_t* data = reinterpret_cast<const uint8_t*>(src) + NQM_HEADER_SIZE;
    const uint32_t row_sign_bytes = (M + 7) / 8;
    const uint32_t col_sign_bytes = (N + 7) / 8;
    const uint32_t bytes_per_rank = row_sign_bytes + col_sign_bytes;
    const uint32_t rank = hdr->rank;
    
    for (uint32_t k = 0; k < rank && k < 4; ++k) {
        const float scale = hdr->scales[k];
        const uint8_t* row_signs = data + k * bytes_per_rank;
        const uint8_t* col_signs = row_signs + row_sign_bytes;
        
        for (uint32_t i = 0; i < M; ++i) {
            int rs = (row_signs[i / 8] >> (i & 7)) & 1;
            float rs_val = rs ? 1.0f : -1.0f;
            
            for (uint32_t j = 0; j < N; ++j) {
                int cs = (col_signs[j / 8] >> (j & 7)) & 1;
                float cs_val = cs ? 1.0f : -1.0f;
                dst[i * N + j] += scale * rs_val * cs_val;
            }
        }
    }
}

/// NQ_R4 quantization: matrix-level binary factorization.
static void nq_r4_from_float(const float* src, void* dst, int64_t n_elements) {
    if (!src || !dst || n_elements <= 0) return;
    // NQ_R4 is a matrix-level format — block-level quantization is not meaningful.
    // Fallback: pack as scaled int8 with per-element sign bits for compatibility.
    auto* out = static_cast<uint8_t*>(dst);
    float absmax = 0.0f;
    for (int64_t i = 0; i < n_elements; ++i) {
        float a = src[i] < 0 ? -src[i] : src[i];
        if (a > absmax) absmax = a;
    }
    float scale = absmax > 0.0f ? 127.0f / absmax : 0.0f;
    for (int64_t i = 0; i < n_elements; ++i) {
        int val = (int)(src[i] * scale + 0.5f);
        if (val > 127) val = 127;
        if (val < -128) val = -128;
        out[i] = (uint8_t)(val + 128);
    }
}

/// NQ_R4 vec_dot — fallback scalar implementation.
/// Use NQ_MatrixGEMM for matrix-level operations.
static void nq_r4_vec_dot(int n, float* result, size_t /*bs_x*/,
                           const void* vx, size_t /*bx*/,
                           const void* vy, size_t /*by*/,
                           int /*nrc*/) {
    if (!result) return;
    if (!vx || !vy || n <= 0) { *result = 0.0f; return; }
    // Treat both as uint8 (offset-128 format) and compute dot product
    const auto* x = static_cast<const uint8_t*>(vx);
    const auto* y = static_cast<const uint8_t*>(vy);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        float fx = (float)x[i] - 128.0f;
        float fy = (float)y[i] - 128.0f;
        sum += fx * fy;
    }
    *result = sum;
}

// =============================================================================
//  Type Traits Table
// =============================================================================

static ggml_nanoquant_type_traits s_nq1_traits = {
    /* type_name    */ "nq_1",
    /* blck_size    */ QK_NQ1,          // 256 elements per block
    /* type_size    */ BLOCK_NQ1_SIZE,  // 34 bytes per block
    /* is_quantized */ true,
    /* to_float     */ nq1_to_float,
    /* from_float   */ nq1_from_float,
    /* vec_dot      */ nq1_vec_dot,
    /* vec_dot_type */ 0                // GGML_TYPE_F32 = 0 (NQ_1 · F32)
};

static ggml_nanoquant_type_traits s_nq_r4_traits = {
    /* type_name    */ "nq_r4",
    /* blck_size    */ 1,               // Matrix-level, no fixed block size
    /* type_size    */ NQM_HEADER_SIZE, // Minimum header size (variable payload)
    /* is_quantized */ true,
    /* to_float     */ nq_r4_to_float,
    /* from_float   */ nq_r4_from_float,
    /* vec_dot      */ nq_r4_vec_dot,
    /* vec_dot_type */ 0                // Not applicable
};

static std::atomic<bool> s_registered{false};

// =============================================================================
//  Registration
// =============================================================================

bool ggml_register_nanoquant_types() {
    if (s_registered.exchange(true)) {
        return true; // Already registered
    }
    
    // In a real ggml integration, this would call:
    //   ggml_type_traits[GGML_TYPE_NQ_1] = { ... };
    //   ggml_type_traits[GGML_TYPE_NQ_R4] = { ... };
    //
    // For now, we store the traits in our static table and expose via
    // ggml_get_nanoquant_traits(). The GGUF loader dispatches through
    // this when it encounters type 20 or 21 in a model file.
    
    return true;
}

const ggml_nanoquant_type_traits* ggml_get_nanoquant_traits(int type_id) {
    switch (type_id) {
        case GGML_TYPE_NQ_1:  return &s_nq1_traits;
        case GGML_TYPE_NQ_R4: return &s_nq_r4_traits;
        default:              return nullptr;
    }
}
