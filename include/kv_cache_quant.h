#pragma once
// kv_cache_quant.h — Block-wise KV-cache quantization interface
// Formats: FP16 (lossless), Q8_0 (8-bit signed), Q4_0 (4-bit unsigned nibble)

#include <stddef.h>
#include <stdint.h>

#define KV_BLOCK_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KVQuantFormat {
    KV_QUANT_FP16  = 0,   // IEEE 754 half-precision, 2 bytes/element
    KV_QUANT_Q8_0  = 1,   // int8 + float32 scale per KV_BLOCK_SIZE elements
    KV_QUANT_Q4_0  = 2,   // nibble (0-15) + float32 scale per KV_BLOCK_SIZE elements
} KVQuantFormat;

// Compress pSrc[count] FP32 → packed quantized pDst.  Returns 0 on success.
int    KVPage_Quantize      (const float* pSrc, uint8_t* pDst, size_t count, KVQuantFormat fmt);

// Expand packed quantized pSrc → pDst[count] FP32.  Returns 0 on success.
int    KVPage_Dequantize    (const uint8_t* pSrc, float* pDst, size_t count, KVQuantFormat fmt);

// Query byte size required for KVPage_Quantize output.
size_t KVPage_QuantizedSize (size_t count, KVQuantFormat fmt);

#ifdef __cplusplus
}
#endif
