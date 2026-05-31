// ============================================================================
// kv_cache_serialization.cpp — KV Cache Serialization Implementation
// ============================================================================
#include "kv_cache_serialization.h"
#include <cstring>
#include <stdexcept>

namespace rawrxd {

// ============================================================================
// CRC64 ECMA Polynomial Implementation
// ============================================================================
namespace {
    // Precomputed CRC64 table for ECMA polynomial
    class CRC64Table {
    public:
        CRC64Table() {
            const uint64_t ECMA_POLY = 0x42F0_E1EB_A9EA_3693ULL;
            for (int i = 0; i < 256; ++i) {
                uint64_t crc = i;
                for (int j = 0; j < 8; ++j) {
                    if (crc & 1) {
                        crc = (crc >> 1) ^ ECMA_POLY;
                    } else {
                        crc >>= 1;
                    }
                }
                m_table[i] = crc;
            }
        }

        uint64_t getEntry(int idx) const { return m_table[idx]; }

    private:
        uint64_t m_table[256];
    };

    static const CRC64Table& getCRC64Table() {
        static const CRC64Table table;
        return table;
    }
}  // namespace

// Calculate CRC64 (ECMA polynomial)
uint64_t KVCacheSerializationFormat::calculateCRC64(const uint8_t* data, size_t len) {
    uint64_t crc = 0ULL;
    const auto& table = getCRC64Table();

    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        int idx = (crc ^ byte) & 0xFF;
        crc = (crc >> 8) ^ table.getEntry(idx);
    }

    return crc;
}

// ============================================================================
// Serialization
// ============================================================================
std::vector<uint8_t> KVCacheSerializationFormat::serialize(
    const Metadata& meta,
    const std::vector<LayerCache>& layers) {
    
    // Validate input
    if (layers.size() != meta.num_layers) {
        return {};
    }

    // Calculate total size
    size_t totalSize = sizeof(Header) + sizeof(Metadata);
    for (const auto& layer : layers) {
        totalSize += sizeof(uint32_t);  // layer_idx
        totalSize += sizeof(uint64_t) + layer.k_cache.size();  // k_cache_bytes + data
        totalSize += sizeof(uint64_t) + layer.v_cache.size();  // v_cache_bytes + data
    }
    totalSize += sizeof(Checksum);

    std::vector<uint8_t> result;
    result.reserve(totalSize);

    // Write header
    Header header = Header::create();
    result.insert(result.end(), (uint8_t*)&header, (uint8_t*)&header + sizeof(Header));

    // Write metadata
    result.insert(result.end(), (uint8_t*)&meta, (uint8_t*)&meta + sizeof(Metadata));

    // Write layers
    for (const auto& layer : layers) {
        // Layer index
        uint32_t layer_idx = layer.layer_idx;
        result.insert(result.end(), (uint8_t*)&layer_idx, (uint8_t*)&layer_idx + sizeof(uint32_t));

        // K cache size and data
        uint64_t k_size = layer.k_cache.size();
        result.insert(result.end(), (uint8_t*)&k_size, (uint8_t*)&k_size + sizeof(uint64_t));
        result.insert(result.end(), layer.k_cache.begin(), layer.k_cache.end());

        // V cache size and data
        uint64_t v_size = layer.v_cache.size();
        result.insert(result.end(), (uint8_t*)&v_size, (uint8_t*)&v_size + sizeof(uint64_t));
        result.insert(result.end(), layer.v_cache.begin(), layer.v_cache.end());
    }

    // Calculate and write checksum (CRC64 of everything except checksum itself)
    uint64_t crc = calculateCRC64(result.data(), result.size());
    Checksum checksum{crc, 0};
    result.insert(result.end(), (uint8_t*)&checksum, (uint8_t*)&checksum + sizeof(Checksum));

    return result;
}

// ============================================================================
// Deserialization
// ============================================================================
bool KVCacheSerializationFormat::deserialize(
    const std::vector<uint8_t>& data,
    Metadata& outMeta,
    std::vector<LayerCache>& outLayers) {
    
    // Minimum size check
    if (data.size() < sizeof(Header) + sizeof(Metadata) + sizeof(Checksum)) {
        return false;
    }

    size_t offset = 0;

    // Read and validate header
    Header header;
    std::memcpy(&header, data.data() + offset, sizeof(Header));
    offset += sizeof(Header);

    if (!header.isValid()) {
        return false;
    }

    // Read metadata
    std::memcpy(&outMeta, data.data() + offset, sizeof(Metadata));
    offset += sizeof(Metadata);

    // Read layers
    outLayers.clear();
    outLayers.reserve(outMeta.num_layers);

    for (uint32_t i = 0; i < outMeta.num_layers; ++i) {
        if (offset + sizeof(uint32_t) > data.size()) {
            return false;
        }

        LayerCache layer;
        std::memcpy(&layer.layer_idx, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Read K cache
        if (offset + sizeof(uint64_t) > data.size()) {
            return false;
        }
        uint64_t k_size;
        std::memcpy(&k_size, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        if (offset + k_size > data.size()) {
            return false;
        }
        layer.k_cache.assign(data.data() + offset, data.data() + offset + k_size);
        offset += k_size;

        // Read V cache
        if (offset + sizeof(uint64_t) > data.size()) {
            return false;
        }
        uint64_t v_size;
        std::memcpy(&v_size, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        if (offset + v_size > data.size()) {
            return false;
        }
        layer.v_cache.assign(data.data() + offset, data.data() + offset + v_size);
        offset += v_size;

        outLayers.push_back(layer);
    }

    // Verify checksum
    if (offset + sizeof(Checksum) != data.size()) {
        return false;
    }

    Checksum checksum;
    std::memcpy(&checksum, data.data() + offset, sizeof(Checksum));

    uint64_t calculated_crc = calculateCRC64(data.data(), offset);
    if (calculated_crc != checksum.crc64) {
        return false;  // Checksum mismatch
    }

    return true;
}

// ============================================================================
// Format Validation
// ============================================================================
bool KVCacheSerializationFormat::validateFormat(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(Header)) {
        return false;
    }

    Header header;
    std::memcpy(&header, data.data(), sizeof(Header));
    return header.isValid();
}

// ============================================================================
// KVCacheBuilder Implementation
// ============================================================================
KVCacheBuilder::KVCacheBuilder(uint32_t num_layers, uint32_t max_seq_len,
                               uint32_t num_heads, uint32_t head_dim,
                               KVCacheSerializationFormat::CacheDataType dtype)
    : m_meta{num_layers, max_seq_len, num_heads, head_dim, static_cast<uint32_t>(dtype)} {
    m_layers.reserve(num_layers);
}

void KVCacheBuilder::addLayer(uint32_t layer_idx,
                              const std::vector<uint8_t>& k_cache,
                              const std::vector<uint8_t>& v_cache) {
    if (layer_idx != m_nextExpectedLayer) {
        throw std::runtime_error("KVCacheBuilder: layers must be added in order");
    }

    m_layers.push_back(KVCacheSerializationFormat::LayerCache{layer_idx, k_cache, v_cache});
    m_nextExpectedLayer++;
}

std::vector<uint8_t> KVCacheBuilder::build() {
    if (!isComplete()) {
        return {};
    }

    return KVCacheSerializationFormat::serialize(m_meta, m_layers);
}

bool KVCacheBuilder::isComplete() const {
    return m_nextExpectedLayer == m_meta.num_layers;
}

}  // namespace rawrxd
