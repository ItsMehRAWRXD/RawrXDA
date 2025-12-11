#include "codec.h"
#include <QByteArray>
#include <zlib.h>
#include <QDebug>

namespace codec {
    QByteArray deflate(const QByteArray& input, bool* success) {
        if (input.isEmpty()) {
            if (success) *success = true;
            return QByteArray();
        }
        
        // Use zlib for compression
        uLongf compressedSize = compressBound(input.size());
        QByteArray compressed(compressedSize, 0);
        
        int result = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                              reinterpret_cast<const Bytef*>(input.constData()), input.size(),
                              Z_BEST_COMPRESSION);
        
        if (result == Z_OK) {
            compressed.resize(compressedSize);
            if (success) *success = true;
            return compressed;
        } else {
            qWarning() << "Deflate compression failed with error:" << result;
            if (success) *success = false;
            return input; // Return original on failure
        }
    }
    
    QByteArray inflate(const QByteArray& input, bool* success) {
        if (input.isEmpty()) {
            if (success) *success = true;
            return QByteArray();
        }
        
        // Estimate decompressed size (4x compression ratio)
        uLongf decompressedSize = input.size() * 4;
        QByteArray decompressed(decompressedSize, 0);
        
        int result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                               reinterpret_cast<const Bytef*>(input.constData()), input.size());
        
        if (result == Z_OK) {
            decompressed.resize(decompressedSize);
            if (success) *success = true;
            return decompressed;
        } else if (result == Z_BUF_ERROR) {
            // Buffer too small, try with larger size
            decompressedSize = input.size() * 16;
            decompressed.resize(decompressedSize);
            
            result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                               reinterpret_cast<const Bytef*>(input.constData()), input.size());
            
            if (result == Z_OK) {
                decompressed.resize(decompressedSize);
                if (success) *success = true;
                return decompressed;
            }
        }
        
        qWarning() << "Inflate decompression failed with error:" << result;
        if (success) *success = false;
        return input; // Return original on failure
    }
}