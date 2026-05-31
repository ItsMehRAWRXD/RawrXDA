// ============================================================================
// kv_cache_serialization.h — KV Cache Binary Serialization Format
// ============================================================================
// Binary format for saving/restoring KV cache state during model reload.
// Enables zero-downtime model hot-swap by preserving in-flight attention
// state across model generations.
//
// Format Overview:
//   [HEADER] → [METADATA] → [LAYERS] → [CHECKSUM]
//
//   HEADER (16 bytes):
//     - u32 magic = 0x4B56_5341 ("KVSA" - KV State Archive)
//     - u32 version = 1
//     - u32 flags (reserved)
//     - u32 reserved
//
//   METADATA (variable):
//     - u32 num_layers
//     - u32 max_seq_len
//     - u32 num_heads
//     - u32 head_dim
//     - u32 cache_dtype (0=f32, 1=f16, 2=int8)
//
//   LAYERS[num_layers]:
//     - u32 layer_idx
//     - u64 k_cache_bytes
//     - [k_cache_bytes] K cache data (row-major: [seq, heads, dim])
//     - u64 v_cache_bytes
//     - [v_cache_bytes] V cache data (row-major: [seq, heads, dim])
//
//   CHECKSUM (16 bytes):
//     - u64 crc64 (ECMA polynomial)
//     - u64 reserved
//
// Threading: Serialization must be lock-free or externally synchronized.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rawrxd {

// ============================================================================
// KVCacheSerializationFormat — constants and utilities
// ============================================================================
class KVCacheSerializationFormat {
public:
    // Magic number for validation
    static constexpr uint32_t MAGIC = 0x4B56_5341;  // "KVSA"
    static constexpr uint32_t VERSION = 1;

    // Data type enum (matches quantization schemes)
    enum class CacheDataType : uint32_t {
        Float32       = 0,
        Float16       = 1,
        Int8Quantized = 2,
    };

    // Header structure (16 bytes)
    struct Header {
        uint32_t magic;      // 0x4B56_5341
        uint32_t version;    // 1
        uint32_t flags;      // reserved
        uint32_t reserved;   // for alignment

        bool isValid() const {
            return magic == MAGIC && version == VERSION;
        }

        static Header create() {
            return Header{MAGIC, VERSION, 0, 0};
        }
    };

    // Metadata structure (variable, but at least 20 bytes)
    struct Metadata {
        uint32_t num_layers;
        uint32_t max_seq_len;
        uint32_t num_heads;
        uint32_t head_dim;
        uint32_t cache_dtype;  // CacheDataType

        // Total cache size in bytes per layer
        uint64_t totalCacheBytes() const {
            uint32_t dtype_size = (cache_dtype == 0) ? 4 : ((cache_dtype == 1) ? 2 : 1);
            return static_cast<uint64_t>(max_seq_len) * num_heads * head_dim * dtype_size * 2;
        }
    };

    // Layer cache structure
    struct LayerCache {
        uint32_t layer_idx;
        std::vector<uint8_t> k_cache;  // K cache data
        std::vector<uint8_t> v_cache;  // V cache data
    };

    // Checksum trailer (16 bytes)
    struct Checksum {
        uint64_t crc64;      // ECMA CRC64
        uint64_t reserved;   // alignment
    };

    // ========================================================================
    // Serialization Utilities
    // ========================================================================

    // Calculate CRC64 (ECMA polynomial) for data validation
    static uint64_t calculateCRC64(const uint8_t* data, size_t len);

    // Serialize KV cache to binary format
    // Returns serialized bytes, or empty vector on error
    static std::vector<uint8_t> serialize(
        const Metadata& meta,
        const std::vector<LayerCache>& layers);

    // Deserialize KV cache from binary format
    // Returns true on success, false if validation fails
    static bool deserialize(
        const std::vector<uint8_t>& data,
        Metadata& outMeta,
        std::vector<LayerCache>& outLayers);

    // Validate header and checksum without full deserialization
    static bool validateFormat(const std::vector<uint8_t>& data);
};

// ============================================================================
// KVCacheBuilder — helper for progressive KV cache construction
// ============================================================================
class KVCacheBuilder {
public:
    explicit KVCacheBuilder(uint32_t num_layers, uint32_t max_seq_len,
                           uint32_t num_heads, uint32_t head_dim,
                           KVCacheSerializationFormat::CacheDataType dtype =
                               KVCacheSerializationFormat::CacheDataType::Float32);

    // Add layer cache data (calls will assert if indices are out of order)
    void addLayer(uint32_t layer_idx,
                  const std::vector<uint8_t>& k_cache,
                  const std::vector<uint8_t>& v_cache);

    // Get the serialized binary representation
    std::vector<uint8_t> build();

    // Check if all layers have been added
    bool isComplete() const;

private:
    KVCacheSerializationFormat::Metadata m_meta;
    std::vector<KVCacheSerializationFormat::LayerCache> m_layers;
    uint32_t m_nextExpectedLayer = 0;
};

}  // namespace rawrxd
