// ============================================================================
// kv_cache_fp8_quantizer.cpp — P0: KV-Cache FP8 Quantization Implementation
// ============================================================================
// Sovereign bit-manipulation implementation — no vendor telemetry.
// Direct FP32<->FP8 conversion using IEEE-754 bit hacking.
// ============================================================================

#include "kv_cache_fp8_quantizer.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <immintrin.h>

namespace RawrXD {
namespace KV {

// FP8 constants
static constexpr float E4M3_MAX = 448.0f;
static constexpr int E4M3_BIAS = 7;
static constexpr int E5M2_BIAS = 15;
static constexpr float E5M2_MAX = 57344.0f;

// ============================================================================
// Construction / Destruction
// ============================================================================
KVCacheFP8Quantizer::KVCacheFP8Quantizer() = default;

KVCacheFP8Quantizer::~KVCacheFP8Quantizer() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================
bool KVCacheFP8Quantizer::initialize(uint32_t num_layers, uint32_t num_heads,
                                     uint32_t head_dim, uint32_t max_seq_len,
                                     FP8Format format) {
    if (initialized_) {
        shutdown();
    }

    num_layers_ = num_layers;
    num_heads_ = num_heads;
    head_dim_ = head_dim;
    max_seq_len_ = max_seq_len;
    format_ = format;

    // Pre-allocate layer caches
    k_caches_.resize(num_layers);
    v_caches_.resize(num_layers);

    for (uint32_t i = 0; i < num_layers; ++i) {
        if (!allocateLayerCache(i)) {
            shutdown();
            return false;
        }
    }

    initialized_ = true;
    return true;
}

void KVCacheFP8Quantizer::shutdown() {
    if (!initialized_) return;

    for (uint32_t i = 0; i < num_layers_; ++i) {
        freeLayerCache(i);
    }

    k_caches_.clear();
    v_caches_.clear();

    total_bytes_used_.store(0);
    total_bytes_saved_.store(0);
    quantize_ops_.store(0);
    dequantize_ops_.store(0);

    initialized_ = false;
}

// ============================================================================
// Memory Management
// ============================================================================
bool KVCacheFP8Quantizer::allocateLayerCache(uint32_t layer_idx) {
    const uint64_t tokens_per_layer = max_seq_len_;
    const uint64_t elements_per_layer = tokens_per_layer * num_heads_ * head_dim_;
    const uint64_t fp8_bytes = elements_per_layer;  // 1 byte per element
    const uint64_t fp32_bytes = elements_per_layer * sizeof(float);

    // Allocate K cache
    auto k_entry = std::make_unique<KVCacheEntry>();
    k_entry->fp8_data = new uint8_t[fp8_bytes];
    k_entry->fp32_data = new float[elements_per_layer];  // FP32 cache for active tokens
    k_entry->params = {1.0f, 1.0f, format_, {0, 0, 0}};
    k_entry->num_tokens = 0;
    k_entry->num_heads = num_heads_;
    k_entry->head_dim = head_dim_;
    k_entry->seq_len = 0;
    k_entry->is_quantized = false;

    // Allocate V cache
    auto v_entry = std::make_unique<KVCacheEntry>();
    v_entry->fp8_data = new uint8_t[fp8_bytes];
    v_entry->fp32_data = new float[elements_per_layer];
    v_entry->params = {1.0f, 1.0f, format_, {0, 0, 0}};
    v_entry->num_tokens = 0;
    v_entry->num_heads = num_heads_;
    v_entry->head_dim = head_dim_;
    v_entry->seq_len = 0;
    v_entry->is_quantized = false;

    // Zero initialize
    std::memset(k_entry->fp8_data, 0, fp8_bytes);
    std::memset(k_entry->fp32_data, 0, fp32_bytes);
    std::memset(v_entry->fp8_data, 0, fp8_bytes);
    std::memset(v_entry->fp32_data, 0, fp32_bytes);

    k_caches_[layer_idx] = std::move(k_entry);
    v_caches_[layer_idx] = std::move(v_entry);

    total_bytes_used_.fetch_add(fp8_bytes * 2 + fp32_bytes * 2);
    return true;
}

void KVCacheFP8Quantizer::freeLayerCache(uint32_t layer_idx) {
    if (layer_idx >= k_caches_.size()) return;

    if (k_caches_[layer_idx]) {
        delete[] k_caches_[layer_idx]->fp8_data;
        delete[] k_caches_[layer_idx]->fp32_data;
        k_caches_[layer_idx].reset();
    }

    if (v_caches_[layer_idx]) {
        delete[] v_caches_[layer_idx]->fp8_data;
        delete[] v_caches_[layer_idx]->fp32_data;
        v_caches_[layer_idx].reset();
    }
}

// ============================================================================
// Core Quantization: FP32 -> E4M3 (Sovereign Bit Manipulation)
// ============================================================================
uint8_t KVCacheFP8Quantizer::floatToE4M3(float value, float inv_scale) {
    // Apply scale
    value *= inv_scale;

    // Handle zero
    if (value == 0.0f) return 0;

    // Clamp to E4M3 range
    value = std::clamp(value, -E4M3_MAX, E4M3_MAX);

    // Extract IEEE-754 bits
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 31) & 0x1;
    uint32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;

    // Adjust exponent: FP32 bias (127) -> E4M3 bias (7)
    int new_exp = static_cast<int>(exp) - 127 + E4M3_BIAS;

    // Handle denormals and underflow
    if (new_exp <= 0) {
        if (new_exp < -3) {
            return static_cast<uint8_t>(sign << 7);  // Underflow to signed zero
        }

        // Denormal: shift mantissa
        uint32_t denorm_mant = mant | 0x800000;  // Add implicit 1
        denorm_mant >>= static_cast<uint32_t>(1 - new_exp);
        uint32_t mant_bits = (denorm_mant >> 20) & 0x7;
        return static_cast<uint8_t>((sign << 7) | mant_bits);
    }

    // Clamp exponent to 4-bit range (max 15)
    new_exp = std::min(new_exp, 15);

    // Extract top 3 bits of mantissa
    uint32_t mant_bits = (mant >> 20) & 0x7;

    // Round to nearest even
    uint32_t round_bit = (mant >> 19) & 0x1;
    uint32_t sticky = mant & 0x7FFFF;

    if (round_bit && (sticky != 0 || (mant_bits & 0x1))) {
        mant_bits++;
        if (mant_bits > 7) {
            mant_bits = 0;
            new_exp++;
            if (new_exp > 15) {
                new_exp = 15;
                mant_bits = 6;  // Clamp to max finite
            }
        }
    }

    // Pack: sign(1) | exp(4) | mant(3)
    return static_cast<uint8_t>((sign << 7) | (new_exp << 3) | mant_bits);
}

// ============================================================================
// Core Quantization: FP32 -> E5M2
// ============================================================================
uint8_t KVCacheFP8Quantizer::floatToE5M2(float value, float inv_scale) {
    value *= inv_scale;

    if (value == 0.0f) return 0;

    // Clamp to E5M2 range
    value = std::clamp(value, -E5M2_MAX, E5M2_MAX);

    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 31) & 0x1;
    uint32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;

    // Adjust exponent: FP32 bias (127) -> E5M2 bias (15)
    int new_exp = static_cast<int>(exp) - 127 + E5M2_BIAS;

    // Handle underflow
    if (new_exp <= 0) {
        if (new_exp < -1) {
            return static_cast<uint8_t>(sign << 7);
        }
        // Denormal
        uint32_t denorm_mant = mant | 0x800000;
        denorm_mant >>= static_cast<uint32_t>(1 - new_exp);
        uint32_t mant_bits = (denorm_mant >> 21) & 0x3;
        return static_cast<uint8_t>((sign << 7) | mant_bits);
    }

    // Clamp exponent to 5-bit range (max 31)
    new_exp = std::min(new_exp, 31);

    // Extract top 2 bits of mantissa
    uint32_t mant_bits = (mant >> 21) & 0x3;

    // Round to nearest even
    uint32_t round_bit = (mant >> 20) & 0x1;
    uint32_t sticky = mant & 0xFFFFF;

    if (round_bit && (sticky != 0 || (mant_bits & 0x1))) {
        mant_bits++;
        if (mant_bits > 3) {
            mant_bits = 0;
            new_exp++;
            if (new_exp > 31) {
                new_exp = 31;
                mant_bits = 2;
            }
        }
    }

    // Pack: sign(1) | exp(5) | mant(2)
    return static_cast<uint8_t>((sign << 7) | (new_exp << 2) | mant_bits);
}

// ============================================================================
// Core Dequantization: E4M3 -> FP32
// ============================================================================
float KVCacheFP8Quantizer::e4m3ToFloat(uint8_t bits, float scale) {
    if (bits == 0) return 0.0f;

    uint32_t sign = (bits >> 7) & 0x1;
    uint32_t exp = (bits >> 3) & 0xF;
    uint32_t mant = bits & 0x7;

    // Reconstruct FP32
    uint32_t fp32_sign = sign << 31;
    uint32_t fp32_exp;
    uint32_t fp32_mant;

    if (exp == 0) {
        // Denormal
        fp32_exp = 0;
        fp32_mant = mant << 20;
    } else {
        // Normal: adjust bias from E4M3(7) to FP32(127)
        fp32_exp = (exp + 127 - E4M3_BIAS) << 23;
        fp32_mant = mant << 20;
    }

    uint32_t result = fp32_sign | fp32_exp | fp32_mant;
    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f * scale;
}

// ============================================================================
// Core Dequantization: E5M2 -> FP32
// ============================================================================
float KVCacheFP8Quantizer::e5m2ToFloat(uint8_t bits, float scale) {
    if (bits == 0) return 0.0f;

    uint32_t sign = (bits >> 7) & 0x1;
    uint32_t exp = (bits >> 2) & 0x1F;
    uint32_t mant = bits & 0x3;

    uint32_t fp32_sign = sign << 31;
    uint32_t fp32_exp;
    uint32_t fp32_mant;

    if (exp == 0) {
        fp32_exp = 0;
        fp32_mant = mant << 21;
    } else {
        fp32_exp = (exp + 127 - E5M2_BIAS) << 23;
        fp32_mant = mant << 21;
    }

    uint32_t result = fp32_sign | fp32_exp | fp32_mant;
    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f * scale;
}

// ============================================================================
// Batch Quantization (scalar fallback — AVX-512 path can be added)
// ============================================================================
void KVCacheFP8Quantizer::quantizeBatchE4M3(const float* input, uint8_t* output,
                                           uint32_t count, float inv_scale) {
    for (uint32_t i = 0; i < count; ++i) {
        output[i] = floatToE4M3(input[i], inv_scale);
    }
}

void KVCacheFP8Quantizer::quantizeBatchE5M2(const float* input, uint8_t* output,
                                           uint32_t count, float inv_scale) {
    for (uint32_t i = 0; i < count; ++i) {
        output[i] = floatToE5M2(input[i], inv_scale);
    }
}

void KVCacheFP8Quantizer::dequantizeBatchE4M3(const uint8_t* input, float* output,
                                             uint32_t count, float scale) const {
    for (uint32_t i = 0; i < count; ++i) {
        output[i] = e4m3ToFloat(input[i], scale);
    }
}

void KVCacheFP8Quantizer::dequantizeBatchE5M2(const uint8_t* input, float* output,
                                             uint32_t count, float scale) const {
    for (uint32_t i = 0; i < count; ++i) {
        output[i] = e5m2ToFloat(input[i], scale);
    }
}

// ============================================================================
// Scale Computation (Calibration)
// ============================================================================
FP8QuantParams KVCacheFP8Quantizer::computeScale(const float* data,
                                                uint32_t num_elements,
                                                FP8Format format) const {
    // Find max absolute value
    float max_abs = 0.0f;
    for (uint32_t i = 0; i < num_elements; ++i) {
        max_abs = std::max(max_abs, std::abs(data[i]));
    }

    // Compute scale to map max to format max
    float format_max = (format == FP8Format::E4M3) ? E4M3_MAX : E5M2_MAX;
    float scale = max_abs / format_max;

    // Avoid division by zero
    if (scale < 1e-10f) scale = 1.0f;

    return {scale, 1.0f / scale, format, {0, 0, 0}};
}

// ============================================================================
// Quantize Operations
// ============================================================================
uint32_t KVCacheFP8Quantizer::quantizeKCache(const float* k_data, uint32_t layer_idx,
                                            uint32_t num_tokens,
                                            const FP8QuantParams* params) {
    if (!initialized_ || layer_idx >= num_layers_) return 0;

    auto& entry = k_caches_[layer_idx];
    if (!entry) return 0;

    // Compute or use provided scale
    FP8QuantParams quant_params = params ?
        *params : computeScale(k_data, num_tokens * num_heads_ * head_dim_, format_);
    entry->params = quant_params;

    const uint32_t elements = num_tokens * num_heads_ * head_dim_;

    // Quantize to FP8
    if (format_ == FP8Format::E4M3) {
        quantizeBatchE4M3(k_data, entry->fp8_data, elements, quant_params.inv_scale);
    } else {
        quantizeBatchE5M2(k_data, entry->fp8_data, elements, quant_params.inv_scale);
    }

    // Keep FP32 copy for active attention
    std::memcpy(entry->fp32_data, k_data, elements * sizeof(float));

    entry->num_tokens = num_tokens;
    entry->seq_len = num_tokens;
    entry->is_quantized = true;

    // Update stats
    quantize_ops_.fetch_add(1);
    const uint64_t fp32_bytes = elements * sizeof(float);
    const uint64_t fp8_bytes = elements;
    total_bytes_saved_.fetch_add(fp32_bytes - fp8_bytes);

    return num_tokens;
}

uint32_t KVCacheFP8Quantizer::quantizeVCache(const float* v_data, uint32_t layer_idx,
                                            uint32_t num_tokens,
                                            const FP8QuantParams* params) {
    if (!initialized_ || layer_idx >= num_layers_) return 0;

    auto& entry = v_caches_[layer_idx];
    if (!entry) return 0;

    FP8QuantParams quant_params = params ?
        *params : computeScale(v_data, num_tokens * num_heads_ * head_dim_, format_);
    entry->params = quant_params;

    const uint32_t elements = num_tokens * num_heads_ * head_dim_;

    if (format_ == FP8Format::E4M3) {
        quantizeBatchE4M3(v_data, entry->fp8_data, elements, quant_params.inv_scale);
    } else {
        quantizeBatchE5M2(v_data, entry->fp8_data, elements, quant_params.inv_scale);
    }

    std::memcpy(entry->fp32_data, v_data, elements * sizeof(float));

    entry->num_tokens = num_tokens;
    entry->seq_len = num_tokens;
    entry->is_quantized = true;

    quantize_ops_.fetch_add(1);
    const uint64_t fp32_bytes = elements * sizeof(float);
    const uint64_t fp8_bytes = elements;
    total_bytes_saved_.fetch_add(fp32_bytes - fp8_bytes);

    return num_tokens;
}

// ============================================================================
// Dequantize Operations
// ============================================================================
void KVCacheFP8Quantizer::dequantizeKCache(uint32_t layer_idx, uint32_t token_start,
                                          uint32_t num_tokens, float* output) const {
    if (!initialized_ || layer_idx >= num_layers_) return;

    const auto& entry = k_caches_[layer_idx];
    if (!entry || !entry->is_quantized) return;

    const uint32_t elements = num_tokens * num_heads_ * head_dim_;
    const uint32_t offset = token_start * num_heads_ * head_dim_;

    // Use FP32 cache if available (fast path)
    if (entry->fp32_data) {
        std::memcpy(output, entry->fp32_data + offset, elements * sizeof(float));
    } else {
        // Dequantize from FP8
        if (format_ == FP8Format::E4M3) {
            dequantizeBatchE4M3(entry->fp8_data + offset, output, elements, entry->params.scale);
        } else {
            dequantizeBatchE5M2(entry->fp8_data + offset, output, elements, entry->params.scale);
        }
    }

    dequantize_ops_.fetch_add(1);
}

void KVCacheFP8Quantizer::dequantizeVCache(uint32_t layer_idx, uint32_t token_start,
                                          uint32_t num_tokens, float* output) const {
    if (!initialized_ || layer_idx >= num_layers_) return;

    const auto& entry = v_caches_[layer_idx];
    if (!entry || !entry->is_quantized) return;

    const uint32_t elements = num_tokens * num_heads_ * head_dim_;
    const uint32_t offset = token_start * num_heads_ * head_dim_;

    if (entry->fp32_data) {
        std::memcpy(output, entry->fp32_data + offset, elements * sizeof(float));
    } else {
        if (format_ == FP8Format::E4M3) {
            dequantizeBatchE4M3(entry->fp8_data + offset, output, elements, entry->params.scale);
        } else {
            dequantizeBatchE5M2(entry->fp8_data + offset, output, elements, entry->params.scale);
        }
    }

    dequantize_ops_.fetch_add(1);
}

// ============================================================================
// Statistics
// ============================================================================
double KVCacheFP8Quantizer::getCompressionRatio() const {
    uint64_t saved = total_bytes_saved_.load();
    uint64_t used = total_bytes_used_.load();
    if (used == 0) return 1.0;
    return static_cast<double>(used) / (used - saved);
}

const KVCacheEntry* KVCacheFP8Quantizer::getKEntry(uint32_t layer_idx) const {
    if (layer_idx >= k_caches_.size()) return nullptr;
    return k_caches_[layer_idx].get();
}

const KVCacheEntry* KVCacheFP8Quantizer::getVEntry(uint32_t layer_idx) const {
    if (layer_idx >= v_caches_.size()) return nullptr;
    return v_caches_[layer_idx].get();
}

void KVCacheFP8Quantizer::prefetchLayer(uint32_t layer_idx) {
    // No-op for now — FP32 cache is always resident
    // Future: async prefetch from system RAM
}

void KVCacheFP8Quantizer::evictLayer(uint32_t layer_idx) {
    // No-op for now
    // Future: evict FP32 cache to FP8-only
}

// ============================================================================
// C API Implementation
// ============================================================================
extern "C" {

RawrXD_KVQuantizer rawrxd_kvquantizer_create(
    uint32_t num_layers, uint32_t num_heads, uint32_t head_dim,
    uint32_t max_seq_len, int format) {

    auto* quantizer = new KVCacheFP8Quantizer();
    FP8Format fmt = (format == 0) ? FP8Format::E4M3 : FP8Format::E5M2;

    if (!quantizer->initialize(num_layers, num_heads, head_dim, max_seq_len, fmt)) {
        delete quantizer;
        return nullptr;
    }

    return quantizer;
}

void rawrxd_kvquantizer_destroy(RawrXD_KVQuantizer handle) {
    if (handle) {
        auto* quantizer = static_cast<KVCacheFP8Quantizer*>(handle);
        quantizer->shutdown();
        delete quantizer;
    }
}

int rawrxd_kvquantizer_quantize_k(
    RawrXD_KVQuantizer handle, uint32_t layer_idx,
    const float* data, uint32_t num_tokens) {

    if (!handle) return -1;
    auto* quantizer = static_cast<KVCacheFP8Quantizer*>(handle);
    return static_cast<int>(quantizer->quantizeKCache(data, layer_idx, num_tokens));
}

int rawrxd_kvquantizer_quantize_v(
    RawrXD_KVQuantizer handle, uint32_t layer_idx,
    const float* data, uint32_t num_tokens) {

    if (!handle) return -1;
    auto* quantizer = static_cast<KVCacheFP8Quantizer*>(handle);
    return static_cast<int>(quantizer->quantizeVCache(data, layer_idx, num_tokens));
}

void rawrxd_kvquantizer_dequantize_k(
    RawrXD_KVQuantizer handle, uint32_t layer_idx,
    uint32_t token_start, uint32_t num_tokens, float* output) {

    if (!handle) return;
    auto* quantizer = static_cast<KVCacheFP8Quantizer*>(handle);
    quantizer->dequantizeKCache(layer_idx, token_start, num_tokens, output);
}

void rawrxd_kvquantizer_dequantize_v(
    RawrXD_KVQuantizer handle, uint32_t layer_idx,
    uint32_t token_start, uint32_t num_tokens, float* output) {

    if (!handle) return;
    auto* quantizer = static_cast<KVCacheFP8Quantizer*>(handle);
    quantizer->dequantizeVCache(layer_idx, token_start, num_tokens, output);
}

} // extern "C"

} // namespace KV
} // namespace RawrXD
