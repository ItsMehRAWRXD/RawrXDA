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
    uint8_t* inflate_decompress_masm(const uint8_t* src, size_t len, size_t* out_len);
}

struct MASMGGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t compression_mode;
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum;
    uint32_t reserved;
    
    static constexpr uint32_t MAGIC = 0x4D41534D;  // "MASM"
    static constexpr uint32_t VERSION = 1;
};

class MASMCompressedGGUF {
public:
    // Check if data is MASM-compressed GGUF
    static bool isCompressed(const uint8_t* data, size_t size) {
        if (size < sizeof(MASMGGUFHeader)) return false;
        const MASMGGUFHeader* header = reinterpret_cast<const MASMGGUFHeader*>(data);
        return header->magic == MASMGGUFHeader::MAGIC && header->version == MASMGGUFHeader::VERSION;
    }
    
    // Compress GGUF data
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
    
    // Decompress MASM-compressed GGUF data
    static std::vector<uint8_t> decompress(const uint8_t* data, size_t size) {
        if (size < sizeof(MASMGGUFHeader)) {
            throw std::runtime_error("Data too small for MASM header");
        }
        
        const MASMGGUFHeader* header = reinterpret_cast<const MASMGGUFHeader*>(data);
        
        if (header->magic != MASMGGUFHeader::MAGIC) {
            throw std::runtime_error("Invalid MASM magic number");
        }
        if (header->version != MASMGGUFHeader::VERSION) {
            throw std::runtime_error("Unsupported MASM version");
        }
        
        const uint8_t* compressed_data = data + sizeof(MASMGGUFHeader);
        size_t compressed_size = size - sizeof(MASMGGUFHeader);
        
        // For RLE: decompress manually (inflate_decompress_masm not needed for simple RLE)
        std::vector<uint8_t> result;
        result.reserve(header->original_size);
        
        // Skip gzip header (10 bytes)
        size_t pos = 10;
        
        while (pos < compressed_size - 8) {  // -8 for gzip footer
            uint8_t byte = compressed_data[pos++];
            
            if (byte == 0xFF && pos + 2 <= compressed_size - 8) {
                // RLE encoded run
                uint8_t value = compressed_data[pos++];
                uint16_t count = *reinterpret_cast<const uint16_t*>(&compressed_data[pos]);
                pos += 2;
                
                for (uint16_t i = 0; i < count; ++i) {
                    result.push_back(value);
                }
            } else {
                // Literal byte
                result.push_back(byte);
            }
        }
        
        // Verify checksum
        uint32_t calculated_crc = crc32(result.data(), result.size());
        if (calculated_crc != header->checksum) {
            throw std::runtime_error("CRC32 checksum mismatch");
        }
        
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
