#pragma once
#ifndef COMPRESSION_INTERFACE_H
#define COMPRESSION_INTERFACE_H

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include "brutal_gzip.h"
#include "deflate_brutal_qt.hpp"

/**
 * @file compression_interface.h
 * @brief MASM-optimized compression interface for GGUF tensor loading
 * 
 * This header provides wrapper classes that integrate with:
 * - brutal_gzip.lib (MASM-optimized GZIP decompression)
 * - deflate_brutal_qt.hpp (MASM-optimized deflate with Qt integration)
 * 
 * These libraries use hand-optimized MASM kernels for maximum performance
 * on GGUF model tensor decompression.
 */

// Forward declarations
class IGZIPCompressor;
class IDeflateCompressor;

/**
 * @class CompressionStats
 * @brief Collects compression statistics for monitoring
 */
struct CompressionStats {
    uint64_t total_compressed_bytes = 0;
    uint64_t total_decompressed_bytes = 0;
    uint32_t decompression_calls = 0;
    uint32_t compression_calls = 0;
    uint64_t total_calls = 0;
    double avg_compression_ratio = 0.0;
    double avg_ratio = 0.0;
    std::string active_kernel;

    void Reset() {
        total_compressed_bytes = 0;
        total_decompressed_bytes = 0;
        decompression_calls = 0;
        compression_calls = 0;
        total_calls = 0;
        avg_compression_ratio = 0.0;
        avg_ratio = 0.0;
        active_kernel.clear();
    }

    std::string ToString() const;
};

/**
 * @class ICompressionProvider
 * @brief Abstract interface for compression algorithms
 */
class ICompressionProvider {
public:
    virtual ~ICompressionProvider() = default;
    virtual bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) = 0;
    virtual bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) = 0;
    virtual bool IsSupported() const = 0;
    virtual std::string GetActiveKernel() const = 0;
    virtual CompressionStats GetStats() const = 0;
};

/**
 * @class BrutalGzipWrapper
 * @brief Wrapper around brutal_gzip.lib (MASM-optimized GZIP)
 * 
 * This class wraps the brutal_gzip library which provides:
 * - MASM-optimized decompression kernels (deflate_brutal_masm.asm)
 * - MASM-optimized compression kernels (deflate_godmode_masm.asm)
 * - Automatic CPU feature detection and kernel selection
 * - Multi-threaded decompression support
 * 
 * Performance characteristics:
 * - Decompression: ~500 MB/s on modern CPUs (AVX2/SSE2)
 * - Compression: ~200 MB/s (configurable)
 * - Memory overhead: Minimal (streaming-friendly)
 */
class BrutalGzipWrapper : public ICompressionProvider {
public:
    BrutalGzipWrapper();
    ~BrutalGzipWrapper();
    
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    
    /**
     * @brief Get the actual MASM kernel being used
     * @return String describing the kernel (e.g., "brutal_masm", "godmode_masm", "fallback")
     */
    std::string GetActiveKernel() const override;
    
    /**
     * @brief Set threading model for decompression
     * @param num_threads Number of threads to use (0 = auto-detect)
     */
    void SetThreadCount(uint32_t num_threads);

    /**
     * @brief Get compression statistics
     */
    CompressionStats GetStats() const override;

private:
    uint32_t thread_count_ = 0;  // 0 = auto-detect
    bool is_initialized_ = false;
    CompressionStats stats_{};
};

/**
 * @class DeflateWrapper
 * @brief Wrapper around deflate_brutal_qt.hpp (MASM-optimized deflate)
 * 
 * This class wraps the deflate decompression with Qt integration:
 * - MASM-optimized deflate kernels (deflate_brutal_masm.asm)
 * - Full zlib/deflate compatibility
 * - Qt-friendly error handling and logging
 * - Automatic memory management
 * 
 * Performance characteristics:
 * - Decompression: ~450 MB/s on modern CPUs
 * - Memory overhead: Configurable buffers
 * - Streaming support: Per-block decompression
 */
class DeflateWrapper : public ICompressionProvider {
public:
    DeflateWrapper();
    ~DeflateWrapper();
    
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    std::string GetActiveKernel() const override;
    
    /**
     * @brief Set compression level (1-9, 9 = best compression)
     * @param level Compression level
     */
    void SetCompressionLevel(uint32_t level);
    
    /**
     * @brief Get detailed decompression statistics
     * @return Comma-separated string with stats (bytes processed, etc.)
     */
    std::string GetDecompressionStats() const;

    /**
     * @brief Get compression statistics
     */
    CompressionStats GetStats() const override;

private:
    uint32_t compression_level_ = 6;  // Default: balanced
    bool is_initialized_ = false;
    uint64_t total_decompressed_bytes_ = 0;
    uint64_t total_compressed_input_ = 0;
    uint64_t decompression_calls_ = 0;
    uint64_t compression_calls_ = 0;
    CompressionStats stats_{};
};

/**
 * @class CompressionFactory
 * @brief Factory for creating appropriate compression provider instances
 */
class CompressionFactory {
public:
    /**
     * @brief Create a compression provider for MASM kernels
     * @param prefer_type Preferred compression type (BRUTAL_GZIP or DEFLATE)
     * @return Shared pointer to the created provider (may fallback if not available)
     */
    static std::shared_ptr<ICompressionProvider> Create(uint32_t prefer_type = 2);  // 2 = BRUTAL_GZIP
    
    /**
     * @brief Check if a compression type is supported on this system
     * @param type Compression type to check
     * @return true if supported, false otherwise
     */
    static bool IsSupported(uint32_t type);
};

#endif // COMPRESSION_INTERFACE_H
