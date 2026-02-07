// ============================================================================
// Custom ZLIB Wrapper - Qt/C++ interface to MASM compression
// Production-ready implementation with comprehensive error handling
// ============================================================================

#ifndef CUSTOM_ZLIB_WRAPPER_H
#define CUSTOM_ZLIB_WRAPPER_H

#include <QByteArray>
#include <QString>
#include <QDebug>
#include <cstdint>

// ============================================================================
// External MASM function declarations
// ============================================================================
extern "C" {
    int64_t CustomZlibCompress(const uint8_t* input, uint64_t inputSize,
                               uint8_t* output, uint64_t outputSize);
    
    int64_t CustomZlibDecompress(const uint8_t* input, uint64_t inputSize,
                                 uint8_t* output, uint64_t outputSize);
    
    uint64_t CustomZlibGetCompressedSize(uint64_t uncompressedSize);
    
    int64_t CustomZlibGetDecompressedSize(const uint8_t* compressed, uint64_t compressedSize);
    
    int64_t CustomZlibInit(void* context, uint64_t contextSize);
    
    int64_t CustomZlibFree(void* context);
}

// ============================================================================
// CustomZlibWrapper - High-level Qt interface
// ============================================================================
class CustomZlibWrapper {
public:
    // ========================================================================
    // Compression methods
    // ========================================================================
    
    /**
     * @brief Compress data using custom MASM DEFLATE implementation
     * @param data Input data to compress
     * @return Compressed data, or empty QByteArray on error
     */
    static QByteArray compress(const QByteArray& data) {
        if (data.isEmpty()) {
            qWarning() << "[CustomZlib] Cannot compress empty data";
            return QByteArray();
        }
        
        // Allocate output buffer with safety margin
        uint64_t maxCompressedSize = CustomZlibGetCompressedSize(data.size());
        QByteArray compressed(maxCompressedSize, Qt::Uninitialized);
        
        qDebug() << "[CustomZlib] Compressing" << data.size() << "bytes";
        
        int64_t compressedSize = CustomZlibCompress(
            reinterpret_cast<const uint8_t*>(data.constData()),
            static_cast<uint64_t>(data.size()),
            reinterpret_cast<uint8_t*>(compressed.data()),
            static_cast<uint64_t>(compressed.size())
        );
        
        if (compressedSize < 0) {
            qCritical() << "[CustomZlib] Compression failed with error code:" << compressedSize;
            return QByteArray();
        }
        
        compressed.resize(compressedSize);
        qInfo() << "[CustomZlib] Compressed" << data.size() << "bytes to"
                << compressedSize << "bytes (ratio:"
                << QString::number(100.0 * compressedSize / data.size(), 'f', 1) << "%)";
        
        return compressed;
    }
    
    /**
     * @brief Decompress data using custom MASM DEFLATE implementation
     * @param compressed Compressed data
     * @param maxDecompressedSize Maximum expected decompressed size (safety limit)
     * @return Decompressed data, or empty QByteArray on error
     */
    static QByteArray decompress(const QByteArray& compressed, qint64 maxDecompressedSize = 100 * 1024 * 1024) {
        if (compressed.isEmpty()) {
            qWarning() << "[CustomZlib] Cannot decompress empty data";
            return QByteArray();
        }
        
        // Estimate decompressed size
        int64_t estimatedSize = CustomZlibGetDecompressedSize(
            reinterpret_cast<const uint8_t*>(compressed.constData()),
            static_cast<uint64_t>(compressed.size())
        );
        
        if (estimatedSize < 0) {
            qCritical() << "[CustomZlib] Invalid compressed data format";
            return QByteArray();
        }
        
        // Apply safety limit
        uint64_t outputSize = qMin(static_cast<uint64_t>(estimatedSize), 
                                   static_cast<uint64_t>(maxDecompressedSize));
        
        QByteArray decompressed(outputSize, Qt::Uninitialized);
        
        qDebug() << "[CustomZlib] Decompressing" << compressed.size() << "bytes";
        
        int64_t decompressedSize = CustomZlibDecompress(
            reinterpret_cast<const uint8_t*>(compressed.constData()),
            static_cast<uint64_t>(compressed.size()),
            reinterpret_cast<uint8_t*>(decompressed.data()),
            static_cast<uint64_t>(decompressed.size())
        );
        
        if (decompressedSize < 0) {
            qCritical() << "[CustomZlib] Decompression failed with error code:" << decompressedSize;
            return QByteArray();
        }
        
        decompressed.resize(decompressedSize);
        qInfo() << "[CustomZlib] Decompressed" << compressed.size() << "bytes to"
                << decompressedSize << "bytes";
        
        return decompressed;
    }
    
    // ========================================================================
    // Utility methods
    // ========================================================================
    
    /**
     * @brief Get maximum compressed size for given input size
     * @param uncompressedSize Size of uncompressed data
     * @return Maximum possible compressed size
     */
    static qint64 getMaxCompressedSize(qint64 uncompressedSize) {
        return static_cast<qint64>(CustomZlibGetCompressedSize(uncompressedSize));
    }
    
    /**
     * @brief Check if data appears to be ZLIB compressed
     * @param data Data to check
     * @return True if data has valid ZLIB header
     */
    static bool isCompressed(const QByteArray& data) {
        if (data.size() < 2) {
            return false;
        }
        
        uint8_t cmf = static_cast<uint8_t>(data[0]);
        uint8_t flg = static_cast<uint8_t>(data[1]);
        
        // Check CMF: compression method must be 8 (DEFLATE)
        if ((cmf & 0x0F) != 8) {
            return false;
        }
        
        // Check CMF + FLG checksum
        uint16_t check = (static_cast<uint16_t>(cmf) << 8) | flg;
        if ((check % 31) != 0) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Get compression ratio as percentage
     * @param compressed Compressed data
     * @param uncompressed Original uncompressed data
     * @return Compression ratio (0-100)
     */
    static double getCompressionRatio(const QByteArray& compressed, const QByteArray& uncompressed) {
        if (uncompressed.isEmpty()) {
            return 0.0;
        }
        return 100.0 * static_cast<double>(compressed.size()) / static_cast<double>(uncompressed.size());
    }
};

#endif // CUSTOM_ZLIB_WRAPPER_H
