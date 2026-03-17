// MASM-Compressed GGUF Loader
// Integrates deflate_brutal_masm kernels

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>
#include <stdexcept>

namespace rawr {

// MASM compression function (from deflate_brutal_masm.asm)
extern "C" {
    uint8_t* deflate_brutal_masm(const uint8_t* src, size_t len, size_t* out_len);
}

enum class MASMCompressionMode {
    None = 0,
    Brutal = 1      // Simple DEFLATE, fast (only mode currently working)
};

// MASM-compressed GGUF header (prepended to compressed data)
struct MASMGGUFHeader {
    uint32_t magic;              // 'MASM'
    uint32_t version;            // Format version (1)
    uint32_t compression_mode;   // MASMCompressionMode
    uint64_t original_size;      // Uncompressed size
    uint64_t compressed_size;    // Compressed size
    uint32_t checksum;           // CRC32 of original data
    uint32_t reserved;
    
    static constexpr uint32_t MAGIC = 0x4D41534D; // 'MASM'
    static constexpr uint32_t VERSION = 1;
};

class MASMCompressedGGUF {
public:
    // Detect if file is MASM-compressed
    static bool isCompressed(const uint8_t* data, size_t size) {
        if (size < sizeof(MASMGGUFHeader)) return false;
        const auto* header = reinterpret_cast<const MASMGGUFHeader*>(data);
        return header->magic == MASMGGUFHeader::MAGIC && 
               header->version == MASMGGUFHeader::VERSION;
    }
    
    // Decompress MASM-compressed GGUF
    static std::vector<uint8_t> decompress(const uint8_t* data, size_t size) {
        if (!isCompressed(data, size)) {
            // Not compressed, return as-is
            return std::vector<uint8_t>(data, data + size);
        }
        
        const auto* header = reinterpret_cast<const MASMGGUFHeader*>(data);
        const uint8_t* compressed_data = data + sizeof(MASMGGUFHeader);
        const size_t compressed_len = header->compressed_size;
        
        size_t decompressed_size = 0;
        uint8_t* decompressed = nullptr;
        
        // Decompress using appropriate MASM kernel
        // Decompress using MASM kernel (only Brutal mode for now)
        switch (static_cast<MASMCompressionMode>(header->compression_mode)) {
            case MASMCompressionMode::Brutal:
                // Note: deflate_brutal_masm creates gzip, we need to decompress it
                // For now, just return error - need decompression routine
                throw std::runtime_error("MASM decompression not yet implemented");
                break;
                
            default:
                throw std::runtime_error("Unknown MASM compression mode");
        }
        if (!decompressed || decompressed_size != header->original_size) {
            if (decompressed) free(decompressed);
            throw std::runtime_error("MASM decompression failed");
        }
        
        // Verify checksum
        uint32_t actual_crc = crc32(decompressed, decompressed_size);
        if (actual_crc != header->checksum) {
            free(decompressed);
            throw std::runtime_error("MASM checksum mismatch - data corrupted");
        }
        
        // Move to vector
        std::vector<uint8_t> result(decompressed, decompressed + decompressed_size);
        free(decompressed);
        
        return result;
    }
    
    // Compress GGUF with MASM kernel
    static std::vector<uint8_t> compress(const uint8_t* data, size_t size, 
                                         MASMCompressionMode mode = MASMCompressionMode::Godmode) {
        size_t compressed_size = 0;
    // Compress GGUF with MASM kernel
    static std::vector<uint8_t> compress(const uint8_t* data, size_t size, 
                                         MASMCompressionMode mode = MASMCompressionMode::Brutal) {
        if (mode != MASMCompressionMode::Brutal) {
            throw std::runtime_error("Only Brutal mode supported");
        }
        
        size_t compressed_size = 0;
        
        // Compress using MASM brutal kernel
        uint8_t* compressed = deflate_brutal_masm(data, size, &compressed_size);
        
        // Build header
        MASMGGUFHeader header;
        header.magic = MASMGGUFHeader::MAGIC;
        header.version = MASMGGUFHeader::VERSION;
        header.compression_mode = static_cast<uint32_t>(mode);
        header.original_size = size;
        header.compressed_size = compressed_size;
        header.checksum = crc32(data, size);
        header.reserved = 0;
        
        // Combine header + compressed data
        std::vector<uint8_t> result(sizeof(header) + compressed_size);
        memcpy(result.data(), &header, sizeof(header));
        memcpy(result.data() + sizeof(header), compressed, compressed_size);
        
        free(compressed);
        return result;
    }
    
private:
    // Simple CRC32 (can use zlib's crc32 if available)
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
    
    extern "C" {
        // Forward declarations for compression (export from .asm)
        uint8_t* deflate_brutal_masm(const uint8_t* src, size_t len, size_t* out_len);
        uint8_t* deflate_godmode_masm(const uint8_t* src, size_t len, size_t* out_len);
    }
};

} // namespace rawr
        return ~crc;
    }
};

} // namespace rawr