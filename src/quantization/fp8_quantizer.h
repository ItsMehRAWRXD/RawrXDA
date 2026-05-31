#pragma once
// ============================================================================
// RawrXD Sovereign FP8 Quantizer
// Hardware-agnostic KV-cache quantization bypassing vendor telemetry
// ============================================================================
// This implementation uses direct bit-manipulation to convert FP32 -> FP8
// without vendor-specific library calls that may trigger telemetry.
//
// Supports:
//   - E4M3 (4-bit exponent, 3-bit mantissa): Higher precision, smaller range
//   - E5M2 (5-bit exponent, 2-bit mantissa): Lower precision, larger range
//
// Target: RX 7800 XT (RDNA3) - uses native FP8 support when available,
//         falls back to custom bit-hacking for true sovereignty.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>


namespace RawrXD
{
namespace Quantization
{

// FP8 format configuration
enum class FP8Format
{
    E4M3,  // 4-bit exponent, 3-bit mantissa, bias=7
    E5M2   // 5-bit exponent, 2-bit mantissa, bias=15
};

// FP8 constants
constexpr float FP8_E4M3_MAX = 448.0f;    // Max representable value
constexpr float FP8_E5M2_MAX = 57344.0f;  // Max representable value (FP8 E5M2 ~= FP16)
constexpr int FP8_E4M3_BIAS = 7;
constexpr int FP8_E5M2_BIAS = 15;

// OCP wire patterns (avoid scattered 0x7F literals in encode/decode)
inline constexpr uint8_t FP8_E4M3_U8_NAN = 0x7F;
inline constexpr uint8_t FP8_E4M3_U8_INF_POS = 0x7E;
inline constexpr uint8_t FP8_E4M3_U8_INF_NEG = 0xFE;
inline constexpr uint8_t FP8_E5M2_U8_NAN = 0x7F;
inline constexpr uint8_t FP8_E5M2_U8_INF_POS = 0x7B;
inline constexpr uint8_t FP8_E5M2_U8_INF_NEG = 0xFB;

static_assert(sizeof(float) == 4u && sizeof(uint8_t) == 1u,
              "SovereignFP8: nominal compression assumes 4-byte float and 1-byte FP8 storage");

// ============================================================================
// Host-side FP32 -> FP8 conversion (scalar fallback)
// ============================================================================
// Semantics:
//   - quantize(): optional stochastic dither (see enableStochasticRounding) then E4M3/E5M2 encode.
//   - quantizeBatch() / dequantizeBatch(): deterministic; no dither so round-trips match absmax scaling
//     (same encode/decode paths as scalar floatToE4M3 / e4m3ToFloat). Suitable for KV tensors and tests.
// ============================================================================
class SovereignFP8Quantizer
{
  public:
    explicit SovereignFP8Quantizer(FP8Format format = FP8Format::E4M3);

    // Quantize a single float to FP8
    uint8_t quantize(float value) const;

    // Dequantize FP8 back to float
    float dequantize(uint8_t fp8) const;

    // Batch quantization with per-tensor scaling (deterministic; ignores stochastic flag)
    void quantizeBatch(const float* input, uint8_t* output, size_t count, float scale = 1.0f);
    void dequantizeBatch(const uint8_t* input, float* output, size_t count, float scale = 1.0f) const;

    // Absmax scale: max_repr / max(|x|) so scaled values fit representable FP8 range (OCP-style)
    float computeScale(const float* data, size_t count) const;

    // Alias for older call sites / tests
    float estimateScale(const float* data, size_t count) const { return computeScale(data, count); }

    // Stochastic rounding for better precision
    void enableStochasticRounding(bool enable) { m_stochastic = enable; }

    /// XORShift state for dither (call before a run for reproducible `quantize()` traces).
    void setDitherSeed(uint32_t seed) { m_randState = seed; }

    // Get current format
    FP8Format getFormat() const { return m_format; }

    // Get format name
    const char* getFormatName() const;

  private:
    FP8Format m_format;
    bool m_stochastic;
    uint32_t m_randState;

    // Encode finite FP32 to FP8 bits (stochastic applied only in quantize(), not here)
    uint8_t quantizeFinite(float value) const;

    // Bit manipulation helpers
    uint8_t floatToE4M3(float value) const;
    uint8_t floatToE5M2(float value) const;
    float e4m3ToFloat(uint8_t fp8) const;
    float e5m2ToFloat(uint8_t fp8) const;

    // Stochastic rounding helper
    float stochasticRound(float value) const;
    uint32_t xorshift32() const;
};

// ============================================================================
// GPU kernel dispatch (Vulkan compute shaders)
// ============================================================================
namespace GPU
{

// FP8 quantization compute shader dispatch
// Returns true if GPU execution succeeded
bool dispatchQuantizeFP8(const float* deviceInput, uint8_t* deviceOutput, size_t elementCount, float scale,
                         FP8Format format);

// FP8 dequantization compute shader dispatch
bool dispatchDequantizeFP8(const uint8_t* deviceInput, float* deviceOutput, size_t elementCount, float scale,
                           FP8Format format);

// Async batch processing with command buffer batching
struct FP8BatchJob
{
    const float* input;
    uint8_t* output;
    size_t count;
    float scale;
    FP8Format format;
    bool completed;
};

class FP8BatchProcessor
{
  public:
    void submitJob(const FP8BatchJob& job);
    void flushBatch();  // Submit all pending jobs as single command buffer
    void waitForCompletion();

  private:
    std::vector<FP8BatchJob> m_pendingJobs;
    static constexpr size_t MAX_BATCH_SIZE = 16;  // Jobs per command buffer
};

}  // namespace GPU

// ============================================================================
// KV-cache specific integration
// ============================================================================
struct KVCacheFP8Config
{
    FP8Format format = FP8Format::E4M3;
    bool useGPU = true;
    /// Reserved for callers / future paths. `SovereignFP8Quantizer` CPU batch uses a
    /// deterministic scalar encoder (`quantizeFinite`); it does not branch on this flag.
    bool useAVX512 = true;
    bool stochasticRounding = true;
    float defaultScale = 1.0f;
};

// Quantized KV cache block
struct QuantizedKVBlock
{
    std::vector<uint8_t> k_data;  // FP8 quantized K
    std::vector<uint8_t> v_data;  // FP8 quantized V
    float k_scale = 1.0f;         // Per-block scale factor
    float v_scale = 1.0f;
    uint32_t token_count = 0;

    void allocate(size_t numElements);
    void dequantizeTo(float* k_out, float* v_out, const SovereignFP8Quantizer& quantizer);
};

// High-level KV cache quantizer
class KVCacheFP8Manager
{
  public:
    explicit KVCacheFP8Manager(const KVCacheFP8Config& config);

    // Quantize and store K/V tensors
    void quantizeAndStore(int blockId, const float* k_tensor, const float* v_tensor, size_t numElements);

    // Retrieve and dequantize K/V tensors
    void retrieveAndDequantize(int blockId, float* k_out, float* v_out, size_t numElements);

    /// Nominal payload ratio: FP32 bytes / FP8 bytes (ignores per-block `k_scale` / `v_scale`).
    double getCompressionRatio() const
    {
        return static_cast<double>(sizeof(float)) / static_cast<double>(sizeof(uint8_t));
    }

    /// Effective ratio when accounting for two `float` scales per KV block (K + V).
    static double getCompressionRatioWithScaleOverhead(size_t numFp8Scalars, size_t numKvBlocks);

    // Memory savings in bytes
    size_t getMemorySavings(size_t originalBytes) const { return originalBytes * 3 / 4; }

  private:
    KVCacheFP8Config m_config;
    SovereignFP8Quantizer m_quantizer;
    std::vector<std::unique_ptr<QuantizedKVBlock>> m_blocks;
};

}  // namespace Quantization
}  // namespace RawrXD
