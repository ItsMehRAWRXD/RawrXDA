// MASM-Compressed GGUF Loader - Simple Version
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace rawr {

extern "C" {
    uint8_t* deflate_compress_masm(const uint8_t* src, size_t len, size_t* out_len);
}

struct MASMGGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t compression_mode;
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum;
    uint32_t reserved;
    
    static constexpr uint32_t MAGIC = 0x4D41534D;
    static constexpr uint32_t VERSION = 1;
};

class MASMCompressedGGUF {
public:
    static std::vector<uint8_t> compress(const uint8_t* data, size_t size) {
        size_t compressed_size = 0;
        uint8_t* compressed = deflate_compress_masm(data, size, &compressed_size);
        
        if (!compressed) throw std::runtime_error("Compression failed");
        
        MASMGGUFHeader header;
        header.magic = MASMGGUFHeader::MAGIC;
        header.version = MASMGGUFHeader::VERSION;
        header.compression_mode = 1;
        header.original_size = size;
        header.compressed_size = compressed_size;
        header.checksum = crc32(data, size);
        header.reserved = 0;
        
        std::vector<uint8_t> result(sizeof(header) + compressed_size);
        std::memcpy(result.data(), &header, sizeof(header));
        std::memcpy(result.data() + sizeof(header), compressed, compressed_size);
        
        free(compressed);
        return result;
    }
    
private:
    static uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        return ~crc;
    }
};

} // namespace rawr
