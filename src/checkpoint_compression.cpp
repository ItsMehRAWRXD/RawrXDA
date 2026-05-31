/**
 * CheckpointCompression Implementation
 * Enhancement #1: Disk Space Optimization
 */

#include "checkpoint_compression.h"
#include <algorithm>
#include <chrono>
#include <mutex>

// Simple compression implementations (production would use lz4/zstd libraries)
namespace CheckpointCompression {

    namespace {
        // Compression header
        struct CompressionHeader {
            uint8_t magic[4] = {'R', 'X', 'C', 'P'};
            uint8_t version = 1;
            uint8_t algorithm;
            uint32_t originalSize;
            uint32_t compressedSize;
            uint32_t checksum;
        };

        // Statistics
        struct StatsInternal {
            size_t totalCompressed = 0;
            size_t totalDecompressed = 0;
            float avgCompressionRatio = 0.0f;
            uint64_t compressionTimeUs = 0;
            uint64_t decompressionTimeUs = 0;
            std::mutex mutex;
        };
        StatsInternal g_stats;

        // Simple RLE compression for demonstration
        std::vector<uint8_t> rleCompress(const std::vector<uint8_t>& input) {
            std::vector<uint8_t> output;
            output.reserve(input.size());
            
            for (size_t i = 0; i < input.size();) {
                uint8_t current = input[i];
                size_t count = 1;
                
                while (i + count < input.size() && 
                       input[i + count] == current && 
                       count < 255) {
                    count++;
                }
                
                if (count >= 3) {
                    output.push_back(0x00); // RLE marker
                    output.push_back(current);
                    output.push_back(static_cast<uint8_t>(count));
                    i += count;
                } else {
                    // Escape literal 0x00
                    if (current == 0x00) {
                        output.push_back(0x00);
                        output.push_back(0x00);
                        output.push_back(0x01);
                    } else {
                        output.push_back(current);
                    }
                    i++;
                }
            }
            
            return output;
        }

        std::vector<uint8_t> rleDecompress(const std::vector<uint8_t>& input) {
            std::vector<uint8_t> output;
            output.reserve(input.size() * 2);
            
            for (size_t i = 0; i < input.size();) {
                if (input[i] == 0x00 && i + 2 < input.size()) {
                    uint8_t value = input[i + 1];
                    uint8_t count = input[i + 2];
                    
                    if (count == 1 && value == 0x00) {
                        // Escaped literal 0x00
                        output.push_back(0x00);
                        i += 3;
                    } else {
                        // RLE sequence
                        for (int j = 0; j < count; j++) {
                            output.push_back(value);
                        }
                        i += 3;
                    }
                } else {
                    output.push_back(input[i]);
                    i++;
                }
            }
            
            return output;
        }

        uint32_t computeChecksum(const std::vector<uint8_t>& data) {
            uint32_t crc = 0xFFFFFFFF;
            for (uint8_t byte : data) {
                crc ^= byte;
                for (int i = 0; i < 8; i++) {
                    crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
                }
            }
            return ~crc;
        }
    }

    CompressionResult compress(
        const std::vector<uint8_t>& input,
        uint8_t algorithm) {
        
        auto start = std::chrono::steady_clock::now();
        
        CompressionResult result;
        result.originalSize = input.size();
        
        // Auto-select algorithm
        if (algorithm == CP_COMPRESS_AUTO) {
            if (input.size() > CP_THRESHOLD_ZSTD) {
                algorithm = CP_COMPRESS_ZSTD;
            } else if (input.size() > CP_THRESHOLD_LZ4) {
                algorithm = CP_COMPRESS_LZ4;
            } else {
                algorithm = CP_COMPRESS_NONE;
            }
        }
        
        std::vector<uint8_t> compressed;
        
        switch (algorithm) {
            case CP_COMPRESS_LZ4:
            case CP_COMPRESS_ZSTD:
                // Use RLE as placeholder for actual LZ4/Zstd
                compressed = rleCompress(input);
                break;
                
            case CP_COMPRESS_NONE:
            default:
                compressed = input;
                break;
        }
        
        // Build header
        CompressionHeader header;
        header.algorithm = algorithm;
        header.originalSize = static_cast<uint32_t>(input.size());
        header.compressedSize = static_cast<uint32_t>(compressed.size());
        header.checksum = computeChecksum(input);
        
        // Combine header + compressed data
        result.data.resize(sizeof(header) + compressed.size());
        memcpy(result.data.data(), &header, sizeof(header));
        memcpy(result.data.data() + sizeof(header), compressed.data(), compressed.size());
        
        result.algorithm = algorithm;
        result.compressionRatio = input.size() > 0 
            ? (1.0f - (float)compressed.size() / input.size()) 
            : 0.0f;
        
        // Update stats
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::lock_guard<std::mutex> lock(g_stats.mutex);
        g_stats.totalCompressed += input.size();
        g_stats.compressionTimeUs += elapsed;
        
        return result;
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed) {
        auto start = std::chrono::steady_clock::now();
        
        if (compressed.size() < sizeof(CompressionHeader)) {
            return {};
        }
        
        CompressionHeader header;
        memcpy(&header, compressed.data(), sizeof(header));
        
        // Verify magic
        if (header.magic[0] != 'R' || header.magic[1] != 'X' || 
            header.magic[2] != 'C' || header.magic[3] != 'P') {
            return {};
        }
        
        std::vector<uint8_t> payload(
            compressed.begin() + sizeof(header), 
            compressed.end());
        
        std::vector<uint8_t> result;
        
        switch (header.algorithm) {
            case CP_COMPRESS_LZ4:
            case CP_COMPRESS_ZSTD:
                result = rleDecompress(payload);
                break;
                
            case CP_COMPRESS_NONE:
            default:
                result = payload;
                break;
        }
        
        // Verify checksum
        if (computeChecksum(result) != header.checksum) {
            return {}; // Corruption detected
        }
        
        // Update stats
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::lock_guard<std::mutex> lock(g_stats.mutex);
        g_stats.totalDecompressed += result.size();
        g_stats.decompressionTimeUs += elapsed;
        
        return result;
    }

    Stats getStats() {
        std::lock_guard<std::mutex> lock(g_stats.mutex);
        Stats s;
        s.totalCompressed = g_stats.totalCompressed;
        s.totalDecompressed = g_stats.totalDecompressed;
        s.avgCompressionRatio = g_stats.avgCompressionRatio;
        s.compressionTimeUs = g_stats.compressionTimeUs;
        s.decompressionTimeUs = g_stats.decompressionTimeUs;
        return s;
    }

    void resetStats() {
        std::lock_guard<std::mutex> lock(g_stats.mutex);
        g_stats.totalCompressed = 0;
        g_stats.totalDecompressed = 0;
        g_stats.avgCompressionRatio = 0.0f;
        g_stats.compressionTimeUs = 0;
        g_stats.decompressionTimeUs = 0;
    }

    size_t estimateCompressedSize(size_t originalSize, uint8_t algorithm) {
        // Conservative estimates
        switch (algorithm) {
            case CP_COMPRESS_LZ4:
                return originalSize + (originalSize / 10) + 64; // ~10% overhead
            case CP_COMPRESS_ZSTD:
                return originalSize / 2 + 64; // Assume 50% compression
            case CP_COMPRESS_NONE:
            default:
                return originalSize + sizeof(CompressionHeader);
        }
    }

} // namespace CheckpointCompression
