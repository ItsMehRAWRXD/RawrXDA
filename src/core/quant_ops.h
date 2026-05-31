#ifndef RAWRXD_QUANT_OPS_H
#define RAWRXD_QUANT_OPS_H

#include "gguf_format.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
REAL QUANTIZATION OPERATIONS

These functions perform ACTUAL quantization/dequantization.
No simulations. Real math.
═══════════════════════════════════════════════════════════════════════════ */

/* FP16 conversion */
typedef uint16_t half;

/* Convert FP32 to FP16 */
half float_to_half(float f);

/* Convert FP16 to FP32 */
float half_to_float(half h);

/* Dequantize result structure */
typedef struct {
    float* data; /* FP32 output */
    size_t count; /* Number of floats */
    size_t capacity; /* Allocated capacity */
} DequantResult;

/* Quantize result structure */
typedef struct {
    void* data; /* Quantized output */
    size_t size; /* Bytes written */
    size_t capacity; /* Allocated capacity */
} QuantResult;

/* ═══════════════════════════════════════════════════════════════════════════
DEQUANTIZATION (Quantized → FP32)
═══════════════════════════════════════════════════════════════════════════ */

/* Dequantize Q4_0 block to FP32 */
DequantResult dequant_q4_0(const void* blocks, size_t num_blocks);

/* Dequantize Q4_1 block to FP32 */
DequantResult dequant_q4_1(const void* blocks, size_t num_blocks);

/* Dequantize Q5_0 block to FP32 */
DequantResult dequant_q5_0(const void* blocks, size_t num_blocks);

/* Dequantize Q5_1 block to FP32 */
DequantResult dequant_q5_1(const void* blocks, size_t num_blocks);

/* Dequantize Q8_0 block to FP32 */
DequantResult dequant_q8_0(const void* blocks, size_t num_blocks);

/* Dequantize K-quants (Q2_K through Q6_K) */
DequantResult dequant_q2_k(const void* blocks, size_t num_blocks);
DequantResult dequant_q3_k(const void* blocks, size_t num_blocks);
DequantResult dequant_q4_k(const void* blocks, size_t num_blocks);
DequantResult dequant_q5_k(const void* blocks, size_t num_blocks);
DequantResult dequant_q6_k(const void* blocks, size_t num_blocks);

/* Generic dequantize - calls appropriate function based on type */
DequantResult dequant_tensor(
    const void* data,
    GGMLType type,
    uint64_t num_elements
);

/* ═══════════════════════════════════════════════════════════════════════════
QUANTIZATION (FP32 → Quantized)
═══════════════════════════════════════════════════════════════════════════ */

/* Quantize FP32 to Q4_0 */
QuantResult quant_q4_0(const float* data, size_t count);

/* Quantize FP32 to Q4_1 */
QuantResult quant_q4_1(const float* data, size_t count);

/* Quantize FP32 to Q5_0 */
QuantResult quant_q5_0(const float* data, size_t count);

/* Quantize FP32 to Q5_1 */
QuantResult quant_q5_1(const float* data, size_t count);

/* Quantize FP32 to Q8_0 */
QuantResult quant_q8_0(const float* data, size_t count);

/* Quantize FP32 to K-quants */
QuantResult quant_q4_k(const float* data, size_t count);
QuantResult quant_q5_k(const float* data, size_t count);
QuantResult quant_q6_k(const float* data, size_t count);

/* Generic quantize */
QuantResult quant_tensor(
    const float* data,
    size_t count,
    GGMLType target_type
);

/* ═══════════════════════════════════════════════════════════════════════════
RE-QUANTIZATION (Direct Quantized → Quantized)

Optimized path that avoids full FP32 intermediate when possible.
═══════════════════════════════════════════════════════════════════════════ */

/* Re-quantize from one format to another */
QuantResult requantize(
    const void* src_data,
    GGMLType src_type,
    size_t num_elements,
    GGMLType dst_type
);

/* Estimate quality loss from re-quantization */
typedef struct {
    float mse; /* Mean squared error */
    float max_error; /* Maximum absolute error */
    float relative_error; /* Average relative error */
    float snr_db; /* Signal-to-noise ratio in dB */
} QuantQuality;

QuantQuality estimate_quant_quality(
    const float* original,
    const float* reconstructed,
    size_t count
);

/* ═══════════════════════════════════════════════════════════════════════════
MEMORY MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

void dequant_result_free(DequantResult* result);
void quant_result_free(QuantResult* result);

/* ═══════════════════════════════════════════════════════════════════════════
QUANTIZATION UTILITIES
═══════════════════════════════════════════════════════════════════════════ */

/* Calculate quantized block count */
static inline size_t quant_block_count(GGMLType type, size_t num_elements) {
    size_t block_size = GGML_RXD_BLOCK_SIZE[type];
    return (num_elements + block_size - 1) / block_size;
}

/* Get bytes per block */
size_t quant_bytes_per_block(GGMLType type);

/* Get total quantized size */
static inline size_t quant_total_size(GGMLType type, size_t num_elements) {
    return quant_block_count(type, num_elements) * quant_bytes_per_block(type);
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_QUANT_OPS_H */
