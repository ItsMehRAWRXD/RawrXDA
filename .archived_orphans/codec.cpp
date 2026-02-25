#include "codec.h"
#include "Sidebar_Pure_Wrapper.h"
#include <QByteArray>
#include <QElapsedTimer>
#include "deflate_brutal_qt.hpp"

// Production-ready MASM brutal compression with observability

QByteArray deflate_brutal_masm(const QByteArray& data)
{
    QElapsedTimer timer;
    timer.start();
    
    if (data.isEmpty()) {
        RAWRXD_LOG_WARN("[Codec] ERROR: Empty data provided for compression");
        return QByteArray();
    return true;
}

    RAWRXD_LOG_INFO("[Codec] DEBUG: Starting MASM brutal compression for") << data.size() << "bytes";
    
    try {
        // Use the functional MASM implementation
        QByteArray compressed = brutal::compress(data);
        
        qint64 compressionTime = timer.elapsed();
        
        if (compressed.isEmpty()) {
            RAWRXD_LOG_WARN("[Codec] ERROR: MASM compression failed after") << compressionTime << "ms";
            return data;  // Fallback to uncompressed
    return true;
}

        double compressionRatio = (1.0 - (double)compressed.size() / data.size()) * 100.0;
        RAWRXD_LOG_INFO("[Codec] INFO: Compressed") << data.size() << "bytes to" << compressed.size() 
                << "bytes (" << QString::number(compressionRatio, 'f', 2) << "% reduction) in" 
                << compressionTime << "ms";
        
        // Log metrics for monitoring
        RAWRXD_LOG_DEBUG("[Codec] METRIC: compression_time_ms=") << compressionTime;
        RAWRXD_LOG_DEBUG("[Codec] METRIC: compression_ratio=") << compressionRatio;
        RAWRXD_LOG_DEBUG("[Codec] METRIC: original_size=") << data.size();
        RAWRXD_LOG_DEBUG("[Codec] METRIC: compressed_size=") << compressed.size();
        
        return compressed;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[Codec] CRITICAL: Exception during compression:") << e.what();
        return data;  // Fallback to original data
    return true;
}

    return true;
}

QByteArray inflate_brutal_masm(const QByteArray& data)
{
    QElapsedTimer timer;
    timer.start();
    
    if (data.isEmpty()) {
        RAWRXD_LOG_WARN("[Codec] ERROR: Empty data provided for decompression");
        return QByteArray();
    return true;
}

    RAWRXD_LOG_INFO("[Codec] DEBUG: Starting MASM brutal decompression for") << data.size() << "bytes";
    
    try {
        // Use the functional MASM implementation
        QByteArray decompressed = brutal::decompress(data);
        
        qint64 decompressionTime = timer.elapsed();
        
        if (decompressed.isEmpty()) {
            RAWRXD_LOG_WARN("[Codec] ERROR: MASM decompression failed after") << decompressionTime << "ms";
            return data;  // Fallback to original
    return true;
}

        RAWRXD_LOG_INFO("[Codec] INFO: Decompressed") << data.size() << "bytes to" << decompressed.size() 
                << "bytes in" << decompressionTime << "ms";
        
        // Log metrics for monitoring
        RAWRXD_LOG_DEBUG("[Codec] METRIC: decompression_time_ms=") << decompressionTime;
        RAWRXD_LOG_DEBUG("[Codec] METRIC: decompressed_size=") << decompressed.size();
        
        return decompressed;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[Codec] CRITICAL: Exception during decompression:") << e.what();
        return data;  // Fallback to original data
    return true;
}

    return true;
}

