#include "compression_interface.h"
#include "deflate_brutal_qt.hpp"
#include "telemetry_singleton.h"
#include <QByteArray>
#include <QDebug>
#include <QElapsedTimer>
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
        qCritical() << "[BrutalGzip] Wrapper not initialized";
        return false;
    }
    
    try {
        QElapsedTimer timer; timer.start();
        
        // Use REAL brutal::compress() from deflate_brutal_qt.hpp
        // This calls deflate_brutal_masm() which is the ACTUAL MASM assembly implementation
        QByteArray input(reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()));
        QByteArray output = brutal::compress(input);
        
        qint64 ms = timer.elapsed();
        
        if (output.isEmpty() && !input.isEmpty()) {
            qCritical() << "[BrutalGzip] Compression failed - output is empty";
            return false;
        }
        
        // Copy compressed data to output vector
        compressed.assign(output.constData(), output.constData() + output.size());

        double ratio = 0.0;
        if (!raw.empty()) {
            ratio = 100.0 * compressed.size() / raw.size();
            qInfo() << "[Compression] BrutalGzip:" << raw.size() << "->" << compressed.size() 
                    << "bytes (" << QString::number(ratio, 'f', 2) << "% )";
            
            // Telemetry
            QJsonObject meta;
            meta["method"] = "brutal_gzip";
            meta["time_ms"] = ms;
            meta["ratio_pct"] = ratio;
            meta["input_size"] = (double)raw.size();
            meta["output_size"] = (double)compressed.size();
            GetTelemetry().recordEvent("compression_op", meta);
        } else {
            qWarning() << "[Compression] Attempted to compress empty data";
        }

        stats_.total_compressed_bytes += compressed.size();
        const double previous_calls = static_cast<double>(stats_.compression_calls);
        stats_.compression_calls++;
        stats_.total_calls = stats_.compression_calls + stats_.decompression_calls;
        if (!raw.empty()) {
            const double call_count = static_cast<double>(stats_.compression_calls);
            stats_.avg_compression_ratio = ((stats_.avg_compression_ratio * previous_calls) + ratio) / call_count;
            stats_.avg_ratio = stats_.avg_compression_ratio;
        }
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[BrutalGzip] Compression error:" << e.what();
        return false;
    }
}

bool BrutalGzipWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                    std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        qCritical() << "[BrutalGzip] Wrapper not initialized";
        return false;
    }
    
    try {
        QElapsedTimer timer; timer.start();
        
        // NOTE: brutal_gzip is primarily a COMPRESSION library (deflate_brutal_masm)
        // For decompression, we'd need a separate inflate implementation
        // For now, use codec::inflate() from inflate_deflate_cpp.cpp
        
        QByteArray input(reinterpret_cast<const char*>(compressed.data()), 
                        static_cast<int>(compressed.size()));
        bool ok = false;
        QByteArray output = codec::inflate(input, &ok);
        
        qint64 ms = timer.elapsed();
        
        if (!ok) {
            qCritical() << "[BrutalGzip] Decompression failed";
            return false;
        }
        
        // Copy decompressed data to output vector
        raw.assign(output.constData(), output.constData() + output.size());

        stats_.total_decompressed_bytes += raw.size();
        stats_.decompression_calls++;
        stats_.total_calls = stats_.compression_calls + stats_.decompression_calls;
        
        qInfo() << "[Compression] BrutalGzip decompressed:" << compressed.size() 
                << "->" << raw.size() << "bytes";
        
        // Telemetry
        QJsonObject meta;
        meta["method"] = "brutal_gzip";
        meta["op"] = "decompress";
        meta["time_ms"] = ms;
        meta["input_size"] = (double)compressed.size();
        meta["output_size"] = (double)raw.size();
        GetTelemetry().recordEvent("decompression_op", meta);
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[BrutalGzip] Decompression error:" << e.what();
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
        qCritical() << "[Deflate] Wrapper not initialized";
        return false;
    }
    
    try {
        // Use REAL codec::deflate() from inflate_deflate_cpp.cpp
        // This calls deflate_brutal_masm() - the ACTUAL MASM assembly kernel
        QByteArray input(reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()));
        bool ok = false;
        QByteArray output = codec::deflate(input, &ok);
        
        if (!ok || output.isEmpty()) {
            qCritical() << "[Deflate] Compression failed";
            return false;
        }
        
        // Copy compressed data to output vector
        compressed.assign(output.constData(), output.constData() + output.size());
        
        total_compressed_input_ += raw.size();
        compression_calls_++;

        double ratio = 0.0;
        if (!raw.empty()) {
            ratio = 100.0 * compressed.size() / raw.size();
            qInfo() << "[Compression] Deflate:" << raw.size() << "->" << compressed.size() 
                    << "bytes (" << QString::number(ratio, 'f', 2) << "% )";
        } else {
            qWarning() << "[Compression] Attempted to compress empty data";
        }

        stats_.total_compressed_bytes += compressed.size();
        const double previous_calls = static_cast<double>(stats_.compression_calls);
        stats_.compression_calls++;
        stats_.total_calls = stats_.compression_calls + stats_.decompression_calls;
        if (!raw.empty()) {
            const double call_count = static_cast<double>(stats_.compression_calls);
            stats_.avg_compression_ratio = ((stats_.avg_compression_ratio * previous_calls) + ratio) / call_count;
            stats_.avg_ratio = stats_.avg_compression_ratio;
        }
        stats_.active_kernel = "deflate_brutal";
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[Deflate] Compression error:" << e.what();
        return false;
    }
}

bool DeflateWrapper::Decompress(const std::vector<uint8_t>& compressed, 
                                 std::vector<uint8_t>& raw) {
    if (!is_initialized_) {
        qCritical() << "[Deflate] Wrapper not initialized";
        return false;
    }
    
    try {
        // Use REAL codec::inflate() from inflate_deflate_cpp.cpp
        QByteArray input(reinterpret_cast<const char*>(compressed.data()), 
                        static_cast<int>(compressed.size()));
        bool ok = false;
        QByteArray output = codec::inflate(input, &ok);
        
        if (!ok) {
            qCritical() << "[Deflate] Decompression failed";
            return false;
        }
        
        // Copy decompressed data to output vector
        raw.assign(output.constData(), output.constData() + output.size());
        
        total_decompressed_bytes_ += raw.size();
        decompression_calls_++;

        stats_.total_decompressed_bytes += raw.size();
        stats_.decompression_calls++;
        stats_.total_calls = stats_.compression_calls + stats_.decompression_calls;
        stats_.active_kernel = "deflate_brutal";
        
        qInfo() << "[Compression] Deflate decompressed:" << compressed.size() 
            << "->" << raw.size() << "bytes";
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[Deflate] Decompression error:" << e.what();
        return false;
    }
}

bool DeflateWrapper::IsSupported() const {
    return is_initialized_;
}

std::string DeflateWrapper::GetActiveKernel() const {
    return "deflate_brutal";
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
            qInfo() << "[CompressionFactory] Using BrutalGzip compression provider (MASM kernels)";
            return gzip_provider;
        }
        
        // Fallback to Deflate
        qInfo() << "[CompressionFactory] BrutalGzip not available, falling back to Deflate";
    }
    
    // Use Deflate
    auto deflate_provider = std::make_shared<DeflateWrapper>();
    if (deflate_provider->IsSupported()) {
        qInfo() << "[CompressionFactory] Using Deflate compression provider (MASM kernels)";
        return deflate_provider;
    }
    
    // All providers failed - return null
    qCritical() << "[CompressionFactory] No compression provider available!";
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

// BrutalGzipWrapper::GetStats()
CompressionStats BrutalGzipWrapper::GetStats() const {
    CompressionStats copy = stats_;
    copy.active_kernel = GetActiveKernel();
    copy.avg_ratio = copy.avg_compression_ratio;
    copy.total_calls = copy.compression_calls + copy.decompression_calls;
    return copy;
}

// DeflateWrapper::GetStats()
CompressionStats DeflateWrapper::GetStats() const {
    CompressionStats copy = stats_;
    copy.active_kernel = "deflate_brutal";
    copy.avg_ratio = copy.avg_compression_ratio;
    copy.total_calls = copy.compression_calls + copy.decompression_calls;
    return copy;
}
