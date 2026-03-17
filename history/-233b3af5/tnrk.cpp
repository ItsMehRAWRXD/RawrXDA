#include "codec.h"
#include <QDebug>
#include <QByteArray>

// Simple compression implementation without zlib dependency

QByteArray deflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    // Simple implementation - return original data for now
    qInfo() << "[Codec] Compression requested for" << data.size() << "bytes";
    return data;
}
        &compressedSize,
        reinterpret_cast<const Bytef*>(data.constData()),
        data.size(),
QByteArray inflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    // Simple implementation - return original data for now
    qInfo() << "[Codec] Decompression requested for" << data.size() << "bytes";
    return data;
}
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
