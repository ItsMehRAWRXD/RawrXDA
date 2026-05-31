#pragma once

#define GGML_RXD_COMMON_DECL_C
#include "ggml-common.h"

#include "ggml.h"

// GGML internal header

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: these functions are defined as GGML_RXD_API because they used by the CPU backend

// Quantization
GGML_RXD_API void quantize_row_q4_0_ref(const float * GGML_RXD_RESTRICT x, block_q4_0 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q4_1_ref(const float * GGML_RXD_RESTRICT x, block_q4_1 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q5_0_ref(const float * GGML_RXD_RESTRICT x, block_q5_0 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q5_1_ref(const float * GGML_RXD_RESTRICT x, block_q5_1 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q8_0_ref(const float * GGML_RXD_RESTRICT x, block_q8_0 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q8_1_ref(const float * GGML_RXD_RESTRICT x, block_q8_1 * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void quantize_row_mxfp4_ref(const float * GGML_RXD_RESTRICT x, block_mxfp4 * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void quantize_row_q2_K_ref(const float * GGML_RXD_RESTRICT x, block_q2_K * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q3_K_ref(const float * GGML_RXD_RESTRICT x, block_q3_K * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q4_K_ref(const float * GGML_RXD_RESTRICT x, block_q4_K * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q5_K_ref(const float * GGML_RXD_RESTRICT x, block_q5_K * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q6_K_ref(const float * GGML_RXD_RESTRICT x, block_q6_K * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_q8_K_ref(const float * GGML_RXD_RESTRICT x, block_q8_K * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void quantize_row_tq1_0_ref(const float * GGML_RXD_RESTRICT x, block_tq1_0 * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_tq2_0_ref(const float * GGML_RXD_RESTRICT x, block_tq2_0 * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void quantize_row_iq3_xxs_ref(const float * GGML_RXD_RESTRICT x, block_iq3_xxs * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_iq4_nl_ref (const float * GGML_RXD_RESTRICT x, block_iq4_nl  * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_iq4_xs_ref (const float * GGML_RXD_RESTRICT x, block_iq4_xs  * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_iq3_s_ref  (const float * GGML_RXD_RESTRICT x, block_iq3_s   * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void quantize_row_iq2_s_ref  (const float * GGML_RXD_RESTRICT x, block_iq2_s   * GGML_RXD_RESTRICT y, int64_t k);

// Dequantization
GGML_RXD_API void dequantize_row_q4_0(const block_q4_0 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q4_1(const block_q4_1 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q5_0(const block_q5_0 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q5_1(const block_q5_1 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q8_0(const block_q8_0 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
//GGML_RXD_API void dequantize_row_q8_1(const block_q8_1 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void dequantize_row_mxfp4(const block_mxfp4 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void dequantize_row_q2_K(const block_q2_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q3_K(const block_q3_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q4_K(const block_q4_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q5_K(const block_q5_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q6_K(const block_q6_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_q8_K(const block_q8_K * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void dequantize_row_tq1_0(const block_tq1_0 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_tq2_0(const block_tq2_0 * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);

GGML_RXD_API void dequantize_row_iq2_xxs(const block_iq2_xxs * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq2_xs (const block_iq2_xs  * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq2_s  (const block_iq2_s   * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq3_xxs(const block_iq3_xxs * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq1_s  (const block_iq1_s   * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq1_m  (const block_iq1_m   * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq4_nl (const block_iq4_nl  * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq4_xs (const block_iq4_xs  * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
GGML_RXD_API void dequantize_row_iq3_s  (const block_iq3_s   * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);

// Quantization utilizing an importance matrix (a.k.a. "Activation aWare Quantization")
GGML_RXD_API size_t quantize_iq2_xxs(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq2_xs (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq2_s  (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq3_xxs(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq1_s  (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq1_m  (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq4_nl (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq4_xs (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_iq3_s  (const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);

GGML_RXD_API size_t quantize_tq1_0(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_tq2_0(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);

GGML_RXD_API size_t quantize_q2_K(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q3_K(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q4_K(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q5_K(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q6_K(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q4_0(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q4_1(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q5_0(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q5_1(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
GGML_RXD_API size_t quantize_q8_0(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);

GGML_RXD_API size_t quantize_mxfp4(const float * GGML_RXD_RESTRICT src, void * GGML_RXD_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix);

GGML_RXD_API void iq2xs_init_impl(enum ggml_rxd_type type);
GGML_RXD_API void iq2xs_free_impl(enum ggml_rxd_type type);
GGML_RXD_API void iq3xs_init_impl(int grid_size);
GGML_RXD_API void iq3xs_free_impl(int grid_size);

#ifdef __cplusplus
}
#endif
