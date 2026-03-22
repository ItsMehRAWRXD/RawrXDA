#pragma once

#include <cstddef>
#include <cstdint>

namespace RawrXD
{
namespace GgufTensorBytes
{
/// GGML payload size on disk for contiguous `nElements` (ggml row layout).
bool payloadBytes(uint32_t ggmlTypeU32, size_t nElements, size_t& outBytes);
/// Decompress full tensor payload to float32 (row-major, one contiguous vector).
bool dequantizeToFloat(uint32_t ggmlTypeU32, const uint8_t* src, size_t nElements, float* dst);
}  // namespace GgufTensorBytes
}  // namespace RawrXD
