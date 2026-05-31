#pragma once
/**
 * CheckpointCompression - Enhancement #1: Disk Space Optimization
 * 
 * Provides transparent compression for large checkpoint state snapshots.
 * Uses LZ4 for speed-critical paths and Zstd for maximum compression.
 * 
 * Symbols: CP_COMPRESS_LZ4, CP_COMPRESS_ZSTD, CP_COMPRESS_NONE
 */

#include <string>
#include <vector>
#include <cstdint>

// Compression algorithm symbols
#define CP_COMPRESS_NONE    0x00  // No compression
#define CP_COMPRESS_LZ4     0x01  // LZ4 fast compression
#define CP_COMPRESS_ZSTD    0x02  // Zstd high compression
#define CP_COMPRESS_AUTO    0xFF  // Auto-select based on size

// Compression thresholds
#define CP_THRESHOLD_LZ4    1024    // Use LZ4 above 1KB
#define CP_THRESHOLD_ZSTD   65536   // Use Zstd above 64KB
#define CP_MAX_BLOCK_SIZE   (16 * 1024 * 1024)  // 16MB max block

namespace CheckpointCompression {

    struct CompressionResult {
        std::vector<uint8_t> data;
        uint8_t algorithm;
        size_t originalSize;
        float compressionRatio;
    };

    /**
     * Compress checkpoint data
     * @param input Raw checkpoint data
     * @param algorithm Compression algorithm (CP_COMPRESS_*)
     * @return Compressed result with metadata
     */
    CompressionResult compress(
        const std::vector<uint8_t>& input,
        uint8_t algorithm = CP_COMPRESS_AUTO);

    /**
     * Decompress checkpoint data
     * @param compressed Compressed data with header
     * @return Original data or empty on error
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed);

    /**
     * Get compression statistics
     */
    struct Stats {
        size_t totalCompressed = 0;
        size_t totalDecompressed = 0;
        float avgCompressionRatio = 0.0f;
        uint64_t compressionTimeUs = 0;
        uint64_t decompressionTimeUs = 0;
    };
    Stats getStats();
    void resetStats();

    /**
     * Estimate compressed size without actual compression
     */
    size_t estimateCompressedSize(size_t originalSize, uint8_t algorithm);

} // namespace CheckpointCompression
