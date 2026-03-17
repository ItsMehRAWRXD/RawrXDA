#include "codec/compression.h"
#include <zlib.h>
#include <QDebug>

namespace codec {

QByteArray deflate(const QByteArray& data, bool* success) {
    if (data.isEmpty()) {
        if (success) *success = true;
        return QByteArray();
    }
    
    // Use zlib for basic deflate compression
    uLongf compressedSize = compressBound(data.size());
    QByteArray compressed(compressedSize, Qt::Uninitialized);
    
    int result = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                          reinterpret_cast<const Bytef*>(data.constData()), data.size(),
                          Z_DEFAULT_COMPRESSION);
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        if (success) *success = true;
        return compressed;
    } else {
        if (success) *success = false;
        qWarning() << "Deflate compression failed with error:" << result;
        return QByteArray();
    }
}

QByteArray inflate(const QByteArray& data, bool* success) {
    if (data.isEmpty()) {
        if (success) *success = true;
        return QByteArray();
    }
    
    // Estimate decompressed size (4x compression ratio)
    uLongf decompressedSize = data.size() * 4;
    QByteArray decompressed(decompressedSize, Qt::Uninitialized);
    
    int result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                           reinterpret_cast<const Bytef*>(data.constData()), data.size());
    
    if (result == Z_OK) {
        decompressed.resize(decompressedSize);
        if (success) *success = true;
        return decompressed;
    } else {
        // Try with larger buffer if first attempt failed
        decompressedSize = data.size() * 10;
        decompressed.resize(decompressedSize);
        
        result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                           reinterpret_cast<const Bytef*>(data.constData()), data.size());
        
        if (result == Z_OK) {
            decompressed.resize(decompressedSize);
            if (success) *success = true;
            return decompressed;
        } else {
            if (success) *success = false;
            qWarning() << "Inflate decompression failed with error:" << result;
            return QByteArray();
        }
    }
}

QByteArray deflate_brutal_masm(const QByteArray& data, bool* success) {
    if (data.isEmpty()) {
        if (success) *success = true;
        return QByteArray();
    }
    
    // Maximum compression with zlib ("brutal" mode)
    uLongf compressedSize = compressBound(data.size());
    QByteArray compressed(compressedSize, Qt::Uninitialized);
    
    int result = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                          reinterpret_cast<const Bytef*>(data.constData()), data.size(),
                          Z_BEST_COMPRESSION);  // Level 9
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        if (success) *success = true;
        qDebug() << "[codec] Brutal compression:" << data.size() << "→" << compressedSize << "bytes";
        return compressed;
    } else {
        if (success) *success = false;
        qWarning() << "Brutal compression failed with error:" << result;
        return QByteArray();
    }
}

} // namespace codec