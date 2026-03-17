// =============================================================================
// ggml_nanoquant.h
// GGML Type Registration for NanoQuant NQ_1 and NQ_R4 formats
//
// Defines type traits (block size, type size, to_float, from_float, vec_dot)
// and provides ggml_register_nanoquant_types() for GGUF backend integration.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

// GGML NanoQuant type identifiers
#ifndef GGML_TYPE_NQ_1
#define GGML_TYPE_NQ_1   20   // Block-level binary (34B / 256 elements = 1.0625 bpe)
#endif

#ifndef GGML_TYPE_NQ_R4
#define GGML_TYPE_NQ_R4  21   // Matrix-level rank-4 binary factorization
#endif

// =============================================================================
//  GGML Type Traits for NanoQuant (compatible with ggml_type_traits_t)
// =============================================================================

/// Type trait structure compatible with ggml's ggml_type_traits_t.
/// Each NQ type provides: type_name, blck_size, type_size, is_quantized,
/// to_float, from_float, vec_dot, vec_dot_type.
struct ggml_nanoquant_type_traits {
    const char* type_name;
    int         blck_size;      // Elements per block
    size_t      type_size;      // Bytes per block
    bool        is_quantized;
    
    // Conversion callbacks
    void (*to_float)(const void* src, float* dst, int64_t n_elements);
    void (*from_float)(const float* src, void* dst, int64_t n_elements);
    
    // Vector dot product callback
    void (*vec_dot)(int n, float* result, size_t bs_x,
                    const void* vx, size_t bx,
                    const void* vy, size_t by,
                    int nrc);
    
    // Type of vec_dot partner (e.g., GGML_TYPE_F32 for NQ_1 · F32)
    int vec_dot_type;
};

// =============================================================================
//  Registration API
// =============================================================================

/// Register NQ_1 and NQ_R4 type traits into the GGML type system.
/// Must be called once at startup (after NanoQuant_Init).
/// Returns true on success.
bool ggml_register_nanoquant_types();

/// Retrieve type traits for a NanoQuant type.
/// Returns nullptr if type_id is not NQ_1 or NQ_R4.
const ggml_nanoquant_type_traits* ggml_get_nanoquant_traits(int type_id);

/// Check if a GGML type ID is a NanoQuant type.
inline bool ggml_is_nanoquant_type(int type_id) {
    return type_id == GGML_TYPE_NQ_1 || type_id == GGML_TYPE_NQ_R4;
}
