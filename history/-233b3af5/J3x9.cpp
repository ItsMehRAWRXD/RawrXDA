#include "codec.h"
#include <QDebug>
#include <zlib.h>

// Real zlib-based compression implementation
// MASM acceleration can be added later without changing the API

QByteArray deflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    // Allocate buffer for compressed data
    uLongf compressedSize = compressBound(data.size());
    QByteArray compressed(compressedSize, Qt::Uninitialized);
    
    // Maximum compression level (9) for "brutal" compression
    int result = compress2(
        reinterpret_cast<Bytef*>(compressed.data()),
        &compressedSize,
        reinterpret_cast<const Bytef*>(data.constData()),
        data.size(),
        Z_BEST_COMPRESSION  // Level 9 - maximum compression
    );
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        qDebug() << "[Codec] Compressed" << data.size() << "→" << compressedSize << "bytes"
                 << QString("(%1%)").arg(100.0 * compressedSize / data.size(), 0, 'f', 1);
        return compressed;
    } else {
        qWarning() << "[Codec] Compression failed with error:" << result;
        return data;  // Return original on failure
    }
}

QByteArray inflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    // Start with 4x decompression ratio estimate
    uLongf decompressedSize = data.size() * 4;
    QByteArray decompressed(decompressedSize, Qt::Uninitialized);
    
    int result = uncompress(
        reinterpret_cast<Bytef*>(decompressed.data()),
        &decompressedSize,
        reinterpret_cast<const Bytef*>(data.constData()),
        data.size()
    );
    
    // If buffer too small, try with 10x ratio
    if (result == Z_BUF_ERROR) {
        decompressedSize = data.size() * 10;
        decompressed.resize(decompressedSize);
        
        result = uncompress(
            reinterpret_cast<Bytef*>(decompressed.data()),
            &decompressedSize,
            reinterpret_cast<const Bytef*>(data.constData()),
            data.size()
        );
    }
    
    if (result == Z_OK) {
        decompressed.resize(decompressedSize);
        qDebug() << "[Codec] Decompressed" << data.size() << "→" << decompressedSize << "bytes";
        return decompressed;
    } else {
        qWarning() << "[Codec] Decompression failed with error:" << result;
        return QByteArray();  // Return empty on failure
    }
}
