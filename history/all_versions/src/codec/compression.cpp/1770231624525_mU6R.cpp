#include "codec/compression.h"
#include <zlib.h>
#include <iostream>
#include <vector>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
    // Use zlib for basic deflate compression
    uLongf compressedSize = compressBound(data.size());
    std::vector<uint8_t> compressed(compressedSize);
    
    int result = ::compress(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                          reinterpret_cast<const Bytef*>(data.data()), data.size());
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        if (success) *success = true;
        return compressed;
    } else {
        if (success) *success = false;
        std::cerr << "Deflate compression failed with error:" << result << std::endl;
        return std::vector<uint8_t>();
    }
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
    // Estimate decompressed size (4x compression ratio)
    uLongf decompressedSize = data.size() * 4;
    std::vector<uint8_t> decompressed(decompressedSize);
    
    int result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
                           reinterpret_cast<const Bytef*>(data.data()), data.size());
    
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
    
    // Maximum compression with zlib (\"brutal\" mode)
    uLongf compressedSize = compressBound(data.size());
    QByteArray compressed(compressedSize, Qt::Uninitialized);
    
    int result = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                          reinterpret_cast<const Bytef*>(data.constData()), data.size(),
                          Z_BEST_COMPRESSION);  // Level 9
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        if (success) *success = true;
        qDebug() << \"[codec] Brutal compression:\" << data.size() << \"→\" << compressedSize << \"bytes\";\n        return compressed;
    } else {
        if (success) *success = false;
        qWarning() << \"Brutal compression failed with error:\" << result;
        return QByteArray();
    }
}

} // namespace codec