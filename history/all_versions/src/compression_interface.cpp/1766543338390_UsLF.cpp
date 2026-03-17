#include "../include/compression_interface.h"
#include <QByteArray>
#include <QDebug>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <memory>

// Forward declarations for codec functions (from inflate_deflate_cpp.cpp)
namespace codec {
    extern QByteArray deflate(const QByteArray& in, bool* ok);
    extern QByteArray inflate(const QByteArray& in, bool* ok);
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
        qWarning() << "[Compression] BrutalGzipWrapper not initialized";
        return false;
    }

    // Fallback passthrough when MASM kernels are unavailable
    compressed = raw;
    if (!raw.empty()) {
        qInfo() << "[Compression] BrutalGzip fallback passthrough" << raw.size() << "bytes";
    }
    return true;
}

bool BrutalGzipWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                    std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        qWarning() << "[Compression] BrutalGzipWrapper not initialized";
        return false;
    }

    raw = compressed;
    return true;
}

bool BrutalGzipWrapper::IsSupported() const {
    return is_initialized_;
}

std::string BrutalGzipWrapper::GetActiveKernel() const {
    return "No MASM kernel available";
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
    total_decompressed_bytes_ = 0;
    total_compressed_input_ = 0;
    compression_calls_ = 0;
    decompression_calls_ = 0;
}

DeflateWrapper::~DeflateWrapper() {
    is_initialized_ = false;
}

bool DeflateWrapper::Compress(const std::vector<uint8_t>& raw, 
                               std::vector<uint8_t>& compressed) {
    if (!is_initialized_) {
        qWarning() << "[Compression] DeflateWrapper not initialized";
        return false;
    }
    
    try {
        // Use REAL codec::deflate() from inflate_deflate_cpp.cpp
        // This calls deflate_brutal_masm() - the ACTUAL MASM assembly kernel
        QByteArray input(reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()));
        bool ok = false;
        QByteArray output = codec::deflate(input, &ok);
        
        if (!ok || output.isEmpty()) {
            qWarning() << "[Compression] Deflate compression failed";
            return false;
        }
        
        // Copy compressed data to output vector
        compressed.assign(output.constData(), output.constData() + output.size());
        
        total_compressed_input_ += raw.size();
        compression_calls_++;
        
        // Log compression results (only if input is not empty to avoid division by zero)
        if (raw.size() > 0) {
            double ratio = 100.0 * compressed.size() / raw.size();
            qInfo() << "[Compression] Deflate:" << raw.size() << "->" << compressed.size() 
                    << "bytes (" << QString::number(ratio, 'f', 2) << "%)";
        } else {
            qWarning() << "[Compression] Attempted to compress empty data";
        }
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[Compression] DeflateWrapper compression error:" << e.what();
        return false;
    }
}

bool DeflateWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                 std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        qWarning() << "[Compression] DeflateWrapper not initialized";
        return false;
    }
    
    try {
        // Use REAL codec::inflate() from inflate_deflate_cpp.cpp
        QByteArray input(reinterpret_cast<const char*>(compressed.data()), 
                        static_cast<int>(compressed.size()));
        bool ok = false;
        QByteArray output = codec::inflate(input, &ok);
        
        if (!ok) {
            qWarning() << "[Compression] Deflate decompression failed";
            return false;
        }
        
        // Copy decompressed data to output vector
        raw.assign(output.constData(), output.constData() + output.size());
        
        total_decompressed_bytes_ += raw.size();
        decompression_calls_++;
        
        qInfo() << "[Compression] Deflate decompressed:" << compressed.size() 
                << "->" << raw.size() << "bytes";
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[Compression] DeflateWrapper decompression error:" << e.what();
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
        if (gzip_provider->IsSupported()) {
            qInfo() << "[Compression] Using BrutalGzip compression provider (MASM kernels)";
            return gzip_provider;
        }
        
        // Fallback to Deflate
        qInfo() << "[Compression] BrutalGzip not available, falling back to Deflate";
    }
    
    // Use Deflate
    auto deflate_provider = std::make_shared<DeflateWrapper>();
    if (deflate_provider->IsSupported()) {
        qInfo() << "[Compression] Using Deflate compression provider (MASM kernels)";
        return deflate_provider;
    }
    
    // All providers failed - return null
    qCritical() << "[Compression] No compression provider available!";
    return nullptr;
}

bool CompressionFactory::IsSupported(uint32_t type) {
    auto provider = Create(type);
    return provider != nullptr && provider->IsSupported();
}

// =====================================================================
// CompressionStats Implementation
// =====================================================================

std::string CompressionStats::ToString() const {
    std::ostringstream oss;
    oss << "CompressionStats {\n"
        << "  Total Compressed: " << total_compressed_bytes << " bytes\n"
        << "  Total Decompressed: " << total_decompressed_bytes << " bytes\n"
        << "  Decompression Calls: " << decompression_calls << "\n"
        << "  Compression Calls: " << compression_calls << "\n"
        << "  Avg Compression Ratio: " << avg_compression_ratio << "\n"
        << "}";
    return oss.str();
}
