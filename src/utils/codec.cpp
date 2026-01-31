#include "codec.h"

#include <zlib.h>

namespace codec {
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success) {
        if (input.empty()) {
            if (success) *success = true;
            return std::vector<uint8_t>();
        }
        
        // Use zlib for compression
        uLongf compressedSize = compressBound(input.size());
        std::vector<uint8_t> compressed(compressedSize, 0);
        
        int result = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                              reinterpret_cast<const Bytef*>(input.constData()), input.size(),
                              Z_BEST_COMPRESSION);
        
        if (result == Z_OK) {
            compressed.resize(compressedSize);
            if (success) *success = true;
            return compressed;
        } else {
            if (success) *success = false;
            return input; // Return original on failure
        }
    }
    
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success) {
        if (input.empty()) {
            if (success) *success = true;
            return std::vector<uint8_t>();
        }
        
        // Estimate decompressed size (4x compression ratio)
        uLongf decompressedSize = input.size() * 4;
        std::vector<uint8_t> decompressed(decompressedSize, 0);
        
        int result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                               reinterpret_cast<const Bytef*>(input.constData()), input.size());
        
        if (result == Z_OK) {
            decompressed.resize(decompressedSize);
            if (success) *success = true;
            return decompressed;
        } else if (result == Z_BUF_ERROR) {
            // Buffer too small, try with larger size
            decompressedSize = input.size() * 16;
            decompressed.resize(decompressedSize);
            
            result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                               reinterpret_cast<const Bytef*>(input.constData()), input.size());
            
            if (result == Z_OK) {
                decompressed.resize(decompressedSize);
                if (success) *success = true;
                return decompressed;
            }
        }
        
        if (success) *success = false;
        return input; // Return original on failure
    }
}

