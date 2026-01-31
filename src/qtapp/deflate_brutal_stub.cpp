// deflate_brutal_stub.cpp
// Fallback C++ implementation for deflate_brutal_masm when MASM binary is not available
// This provides a simple stored-blocks implementation (no actual compression)

#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {

// Simple stored-blocks DEFLATE implementation (no compression, just wrapping)
// This is a fallback when the actual MASM implementation isn't linked
void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len)
{
    if (!src || len == 0 || !out_len) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // DEFLATE stored block format:
    // Header: 3 bits (BFINAL=1, BTYPE=00 for stored)
    // LEN: 2 bytes (little-endian)
    // NLEN: 2 bytes (~LEN, little-endian)
    // Data: raw bytes
    
    // For simplicity, we split into 65535-byte blocks (max stored block size)
    const size_t max_block_size = 65535;
    size_t num_full_blocks = len / max_block_size;
    size_t remainder = len % max_block_size;
    size_t num_blocks = num_full_blocks + (remainder > 0 ? 1 : 0);
    
    // Each block has: 1 byte header + 2 bytes LEN + 2 bytes NLEN + data
    // But header is only 1 byte for first block, 1 byte for rest
    size_t output_size = len + num_blocks * 5;  // 5 bytes overhead per block
    
    uint8_t* output = static_cast<uint8_t*>(malloc(output_size));
    if (!output) {
        *out_len = 0;
        return nullptr;
    }

    const uint8_t* input = static_cast<const uint8_t*>(src);
    uint8_t* out_ptr = output;
    size_t remaining = len;
    
    while (remaining > 0) {
        size_t block_size = (remaining > max_block_size) ? max_block_size : remaining;
        bool is_final = (remaining <= max_block_size);
        
        // Block header: BFINAL (1 bit) + BTYPE (2 bits) = 00 for stored
        // Packed into 1 byte: BFINAL | (BTYPE << 1)
        *out_ptr++ = is_final ? 0x01 : 0x00;  // BFINAL=1 for last, BTYPE=00
        
        // LEN (little-endian)
        *out_ptr++ = static_cast<uint8_t>(block_size & 0xFF);
        *out_ptr++ = static_cast<uint8_t>((block_size >> 8) & 0xFF);
        
        // NLEN (one's complement of LEN)
        uint16_t nlen = ~static_cast<uint16_t>(block_size);
        *out_ptr++ = static_cast<uint8_t>(nlen & 0xFF);
        *out_ptr++ = static_cast<uint8_t>((nlen >> 8) & 0xFF);
        
        // Raw data
        memcpy(out_ptr, input, block_size);
        out_ptr += block_size;
        input += block_size;
        remaining -= block_size;
    }

    *out_len = static_cast<size_t>(out_ptr - output);
    return output;
}

// ARM NEON version - same fallback implementation
void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len)
{
    // Use the same implementation as MASM fallback
    return deflate_brutal_masm(src, len, out_len);
}

}  // extern "C"
