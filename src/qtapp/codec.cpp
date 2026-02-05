#include "codec.h"
#include <QDebug>
#include <QByteArray>
#include <QElapsedTimer>
#include "deflate_brutal_qt.hpp"

// Production-ready MASM brutal compression with observability

QByteArray deflate_brutal_masm(const QByteArray& data)
{
    QElapsedTimer timer;
    timer.start();
    
    if (data.isEmpty()) {
        qWarning() << "[Codec] ERROR: Empty data provided for compression";
        return QByteArray();
    }
    
    qInfo() << "[Codec] DEBUG: Starting MASM brutal compression for" << data.size() << "bytes";
    
    try {
        // Use the functional MASM implementation
        QByteArray compressed = brutal::compress(data);
        
        qint64 compressionTime = timer.elapsed();
        
        if (compressed.isEmpty()) {
            qWarning() << "[Codec] ERROR: MASM compression failed after" << compressionTime << "ms";
            return data;  // Fallback to uncompressed
        }
        
        double compressionRatio = (1.0 - (double)compressed.size() / data.size()) * 100.0;
        qInfo() << "[Codec] INFO: Compressed" << data.size() << "bytes to" << compressed.size() 
                << "bytes (" << QString::number(compressionRatio, 'f', 2) << "% reduction) in" 
                << compressionTime << "ms";
        
        // Log metrics for monitoring
        qDebug() << "[Codec] METRIC: compression_time_ms=" << compressionTime;
        qDebug() << "[Codec] METRIC: compression_ratio=" << compressionRatio;
        qDebug() << "[Codec] METRIC: original_size=" << data.size();
        qDebug() << "[Codec] METRIC: compressed_size=" << compressed.size();
        
        return compressed;
    } catch (const std::exception& e) {
        qCritical() << "[Codec] CRITICAL: Exception during compression:" << e.what();
        return data;  // Fallback to original data
    }
}

QByteArray inflate_brutal_masm(const QByteArray& data)
{
    QElapsedTimer timer;
    timer.start();
    
    if (data.isEmpty()) {
        qWarning() << "[Codec] ERROR: Empty data provided for decompression";
        return QByteArray();
    }
    
    qInfo() << "[Codec] DEBUG: Starting MASM brutal decompression for" << data.size() << "bytes";
    
    try {
        // Use the functional MASM implementation
        QByteArray decompressed = brutal::decompress(data);
        
        qint64 decompressionTime = timer.elapsed();
        
        if (decompressed.isEmpty()) {
            qWarning() << "[Codec] ERROR: MASM decompression failed after" << decompressionTime << "ms";
            return data;  // Fallback to original
        }
        
        qInfo() << "[Codec] INFO: Decompressed" << data.size() << "bytes to" << decompressed.size() 
                << "bytes in" << decompressionTime << "ms";
        
        // Log metrics for monitoring
        qDebug() << "[Codec] METRIC: decompression_time_ms=" << decompressionTime;
        qDebug() << "[Codec] METRIC: decompressed_size=" << decompressed.size();
        
        return decompressed;
    } catch (const std::exception& e) {
        qCritical() << "[Codec] CRITICAL: Exception during decompression:" << e.what();
        return data;  // Fallback to original data
    }
}

