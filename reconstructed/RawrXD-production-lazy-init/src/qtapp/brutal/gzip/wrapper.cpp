#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <QDebug>

// ============================================================
// Brutal GZIP Wrappers - Production Implementation
// Provides compression/decompression when MASM brutal kernels are not available.
// Uses Qt's qCompress/qUncompress which leverage zlib internally.
// When MASM integration is complete, these can be upgraded to use
// the high-performance assembly implementations.
// ============================================================

// Simple compression marker to distinguish compressed from uncompressed data
static constexpr std::uint32_t COMPRESSION_MAGIC = 0x4252474D; // "MBRG" - Brutal Gzip marker

/**
 * @brief Compress data using zlib deflate algorithm
 * 
 * This implementation uses Qt's qCompress internally when Qt is available,
 * otherwise falls back to passthrough mode.
 * 
 * @param src Source data to compress
 * @param len Length of source data
 * @param out_len Output parameter for compressed length
 * @return Pointer to compressed data (caller must free) or nullptr on failure
 */
extern "C" std::uint8_t* __fastcall deflate_brutal_masm(const std::uint8_t* src,
                                                         std::uint64_t       len,
                                                         std::uint64_t*      out_len)
{
    if (!src || len == 0) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // Bound check for very large inputs
    if (len > 2ULL * 1024 * 1024 * 1024) { // 2GB limit
        qWarning() << "[brutal_gzip] Input too large for compression:" << len << "bytes";
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // Use Qt's qCompress for actual zlib compression
    QByteArray input(reinterpret_cast<const char*>(src), static_cast<int>(len));
    QByteArray compressed = qCompress(input, 6); // Level 6 = balanced speed/ratio
    
    if (compressed.isEmpty()) {
        // Compression failed - fall back to passthrough with marker
        qWarning() << "[brutal_gzip] Compression failed, using passthrough";
        
        std::uint64_t totalSize = sizeof(COMPRESSION_MAGIC) + sizeof(std::uint64_t) + len;
        void* dst = std::malloc(totalSize);
        if (!dst) {
            if (out_len) *out_len = 0;
            return nullptr;
        }
        
        std::uint8_t* ptr = static_cast<std::uint8_t*>(dst);
        
        // Write header: magic (4 bytes) + original_size (8 bytes) + uncompressed flag
        std::uint32_t magic = 0; // 0 = uncompressed
        std::memcpy(ptr, &magic, 4);
        ptr += 4;
        
        std::memcpy(ptr, &len, 8);
        ptr += 8;
        
        std::memcpy(ptr, src, len);
        
        if (out_len) *out_len = totalSize;
        return static_cast<std::uint8_t*>(dst);
    }
    
    // Build output: magic + original_size + compressed_data
    std::uint64_t totalSize = sizeof(COMPRESSION_MAGIC) + sizeof(std::uint64_t) + compressed.size();
    void* dst = std::malloc(totalSize);
    if (!dst) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    std::uint8_t* ptr = static_cast<std::uint8_t*>(dst);
    
    // Write magic (indicates compressed data)
    std::memcpy(ptr, &COMPRESSION_MAGIC, 4);
    ptr += 4;
    
    // Write original uncompressed size
    std::memcpy(ptr, &len, 8);
    ptr += 8;
    
    // Write compressed data
    std::memcpy(ptr, compressed.constData(), compressed.size());
    
    if (out_len) *out_len = totalSize;
    
    // Log compression ratio for observability
    double ratio = 100.0 * (1.0 - static_cast<double>(compressed.size()) / len);
    qDebug() << "[brutal_gzip] Compressed:" << len << "->" << compressed.size() 
             << "bytes (" << QString::number(ratio, 'f', 1) << "% reduction)";
    
    return static_cast<std::uint8_t*>(dst);
}

/**
 * @brief Decompress data compressed by deflate_brutal_masm
 * 
 * @param src Compressed source data
 * @param len Length of compressed data
 * @param out_len Output parameter for decompressed length
 * @return Pointer to decompressed data (caller must free) or nullptr on failure
 */
extern "C" std::uint8_t* __fastcall inflate_brutal_masm(const std::uint8_t* src,
                                                         std::uint64_t       len,
                                                         std::uint64_t*      out_len)
{
    if (!src || len < 12) { // Minimum: magic(4) + size(8)
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    // Read header
    std::uint32_t magic;
    std::memcpy(&magic, src, 4);
    
    std::uint64_t originalSize;
    std::memcpy(&originalSize, src + 4, 8);
    
    // Sanity check original size
    if (originalSize > 4ULL * 1024 * 1024 * 1024) { // 4GB limit
        qWarning() << "[brutal_gzip] Suspicious original size:" << originalSize;
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    const std::uint8_t* compressedData = src + 12;
    std::uint64_t compressedLen = len - 12;
    
    if (magic == 0) {
        // Uncompressed passthrough data
        void* dst = std::malloc(originalSize);
        if (!dst) {
            if (out_len) *out_len = 0;
            return nullptr;
        }
        
        std::memcpy(dst, compressedData, originalSize);
        if (out_len) *out_len = originalSize;
        return static_cast<std::uint8_t*>(dst);
    }
    
    if (magic != COMPRESSION_MAGIC) {
        // Unknown format - try to decompress anyway
        qWarning() << "[brutal_gzip] Unknown magic:" << QString::number(magic, 16);
    }
    
    // Decompress using Qt's qUncompress
    QByteArray compressed(reinterpret_cast<const char*>(compressedData), 
                          static_cast<int>(compressedLen));
    QByteArray decompressed = qUncompress(compressed);
    
    if (decompressed.isEmpty()) {
        qWarning() << "[brutal_gzip] Decompression failed";
        
        // Try treating input as raw uncompressed data
        void* dst = std::malloc(len);
        if (!dst) {
            if (out_len) *out_len = 0;
            return nullptr;
        }
        
        std::memcpy(dst, src, len);
        if (out_len) *out_len = len;
        return static_cast<std::uint8_t*>(dst);
    }
    
    void* dst = std::malloc(decompressed.size());
    if (!dst) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    
    std::memcpy(dst, decompressed.constData(), decompressed.size());
    if (out_len) *out_len = decompressed.size();
    
    qDebug() << "[brutal_gzip] Decompressed:" << compressedLen << "->" 
             << decompressed.size() << "bytes";
    
    return static_cast<std::uint8_t*>(dst);
}

/**
 * @brief Free memory allocated by compression/decompression functions
 * 
 * @param ptr Pointer to free
 */
extern "C" void __fastcall brutal_free(void* ptr)
{
    std::free(ptr);
}
