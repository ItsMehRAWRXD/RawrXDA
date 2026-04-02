// transcoder.h - RawrXD Custom N-bit Transcoder Header
// IEEE 754 Half-Precision Float Transcoding for Hardware Acceleration

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Custom N-bit to FP16 Inflation
// Inflates compressed N-bit data to IEEE 754 half-precision floats
// pSource: Pointer to N-bit compressed data
// pDest: Pointer to FP16 destination buffer
// elementCount: Number of elements to inflate
// bitDepth: Bit depth (1-8 bits)
// Returns: 1 on success, 0 on failure
uint8_t Custom_Inflate_Nbit_to_FP16(
    const uint8_t* pSource,
    uint16_t* pDest,
    size_t elementCount,
    uint8_t bitDepth
);

// Custom FP16 to N-bit Deflation
// Deflates IEEE 754 half-precision floats to N-bit compressed data
// pSource: Pointer to FP16 source data
// pDest: Pointer to N-bit destination buffer
// elementCount: Number of elements to deflate
// bitDepth: Bit depth (1-8 bits)
// Returns: 1 on success, 0 on failure
uint8_t Custom_Deflate_FP16_to_Nbit(
    const uint16_t* pSource,
    uint8_t* pDest,
    size_t elementCount,
    uint8_t bitDepth
);

// Set Custom Bit Depth Configuration
// Configures the transcoder for a specific bit depth
// bitDepth: Bit depth (1-8 bits)
// Returns: 1 on success, 0 on failure
uint8_t Set_Custom_Bit_Depth(uint8_t bitDepth);

#ifdef __cplusplus
}
#endif