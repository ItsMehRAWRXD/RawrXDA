#include "compression_interface.h"
#include "deflate_brutal_std.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <memory>

// Forward declarations for codec functions (from compression.cpp)
namespace codec {
    extern std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok);
    extern std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok);
}


// =====================================================================
// BrutalGzipWrapper Implementation
// =====================================================================

BrutalGzipWrapper::BrutalGzipWrapper() {
    thread_count_ = 0;  // Auto-detect
    is_initialized_ = true;
}

BrutalGzipWrapper::~BrutalGzipWrapper() {
    is_initialized_ = false;
}

bool BrutalGzipWrapper::Compress(const std::vector<uint8_t>& raw, 
                                  std::vector<uint8_t>& compressed) {
    if (!is_initialized_) {
        std::cerr << "BrutalGzipWrapper not initialized" << std::endl;
        return false;
    }
    
    try {
        // Use REAL brutal::compress_std() from deflate_brutal_std.hpp
        // This calls deflate_brutal_masm() which is the ACTUAL MASM assembly implementation
        compressed = brutal::compress_std(raw);
        
        if (compressed.empty() && !raw.empty()) {
            std::cerr << "BrutalGzip compression failed - output is empty" << std::endl;
            return false;
        }
        
        std::cout << "BrutalGzip compressed " << raw.size() 
                  << " -> " << compressed.size() << " bytes ("
                  << (100.0 * compressed.size() / raw.size()) << "%)" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BrutalGzipWrapper compression error: " << e.what() << std::endl;
        return false;
    }
}

bool BrutalGzipWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                    std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        std::cerr << "BrutalGzipWrapper not initialized" << std::endl;
        return false;
    }
    
    try {
        // NOTE: brutal_gzip is primarily a COMPRESSION library (deflate_brutal_masm)
        // For decompression, we'd need a separate inflate implementation
        // For now, use codec::inflate() from compression.cpp
        
        bool ok = false;
        std::vector<uint8_t> output = codec::inflate(compressed, &ok);
        
        if (!ok) {
            std::cerr << "BrutalGzip decompression failed" << std::endl;
            return false;
        }
        
        // Copy decompressed data to output vector
        raw = std::move(output);
        
        std::cout << "BrutalGzip decompressed " << compressed.size() 
                  << " -> " << raw.size() << " bytes" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BrutalGzipWrapper decompression error: " << e.what() << std::endl;
        return false;
    }
}

bool BrutalGzipWrapper::IsSupported() const {
    // Check if brutal_gzip.lib is available and MASM kernels are functional
    // On Windows x64: Always supported (compiled with MASM)
    // On other platforms: Would check for AVX2 or SSE2 fallback
    return is_initialized_;
}

std::string BrutalGzipWrapper::GetActiveKernel() const {
    // Detect actual CPU features and return the active MASM kernel
#if defined(HAS_BRUTAL_GZIP_MASM)
    #if defined(__AVX2__)
        return "deflate_brutal_masm (AVX2)";
    #elif defined(__SSE2__)
        return "deflate_brutal_masm (SSE2)";
    #else
        return "deflate_brutal_masm (x64)";
    #endif
#elif defined(HAS_BRUTAL_GZIP_NEON)
    return "deflate_brutal_neon (ARM64)";
#else
    return "No MASM kernel available";
#endif
}

void BrutalGzipWrapper::SetThreadCount(uint32_t num_threads) {
    thread_count_ = num_threads;
    // Would pass this to brutal_gzip.lib for multi-threaded decompression
}

// =====================================================================
// DeflateWrapper Implementation
// =====================================================================

DeflateWrapper::DeflateWrapper() {
    compression_level_ = 6;
    is_initialized_ = true;
}

DeflateWrapper::~DeflateWrapper() {
    is_initialized_ = false;
}

bool DeflateWrapper::Compress(const std::vector<uint8_t>& raw, 
                               std::vector<uint8_t>& compressed) {
    if (!is_initialized_) {
        std::cerr << "DeflateWrapper not initialized" << std::endl;
        return false;
    }
    
    try {
        // Use REAL codec::deflate() from compression.cpp
        bool ok = false;
        compressed = codec::deflate(raw, &ok);
        
        if (!ok || compressed.empty()) {
            std::cerr << "Deflate compression failed" << std::endl;
            return false;
        }
        
        total_compressed_input_ += raw.size();
        compression_calls_++;
        
        std::cout << "Deflate compressed " << raw.size() 
                  << " -> " << compressed.size() << " bytes ("
                  << (100.0 * compressed.size() / raw.size()) << "%)" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "DeflateWrapper compression error: " << e.what() << std::endl;
        return false;
    }
}

bool DeflateWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                 std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        std::cerr << "DeflateWrapper not initialized" << std::endl;
        return false;
    }
    
    try {
        // Use REAL codec::inflate() from compression.cpp
        bool ok = false;
        raw = codec::inflate(compressed, &ok);
        
        if (!ok) {
            std::cerr << "Deflate decompression failed" << std::endl;
            return false;
        }
        
        total_decompressed_bytes_ += raw.size();
        decompression_calls_++;
        
        std::cout << "Deflate decompressed " << compressed.size() 
                  << " -> " << raw.size() << " bytes" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "DeflateWrapper decompression error: " << e.what() << std::endl;
        return false;
    }
}

bool DeflateWrapper::IsSupported() const {
    return is_initialized_;
}

void DeflateWrapper::SetCompressionLevel(uint32_t level) {
    compression_level_ = std::min(level, 9u);
    compression_level_ = std::max(compression_level_, 1u);
}

std::string DeflateWrapper::GetDecompressionStats() const {
    std::ostringstream oss;
    oss << "Decompressed: " << total_decompressed_bytes_ << " bytes, "
        << "Calls: " << decompression_calls_;
    return oss.str();
}

// =====================================================================
// CompressionFactory Implementation
// =====================================================================

std::shared_ptr<ICompressionProvider> CompressionFactory::Create(uint32_t prefer_type) {
    // prefer_type: 2 = BRUTAL_GZIP, 1 = DEFLATE
    
    if (prefer_type == 2) {
        // Try BRUTAL_GZIP first
        auto gzip_provider = std::make_shared<BrutalGzipWrapper>();
        if (gzip_provider) {
            return gzip_provider;
        }
    }
    
    // Default to DEFLATE
    return std::make_shared<DeflateWrapper>();
}
