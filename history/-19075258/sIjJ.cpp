#include "codec.h"
#include <QDebug>
#include <QByteArray>
#include "deflate_brutal_qt.hpp"

// Use our functional MASM brutal compression

QByteArray deflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    qInfo() << "[Codec] MASM brutal compression for" << data.size() << "bytes";
    
    // Use the functional MASM implementation
    QByteArray compressed = brutal::compress(data);
    
    if (compressed.isEmpty()) {
        qWarning() << "[Codec] MASM compression failed";
        return data;  // Fallback to uncompressed
    }
    
    qInfo() << "[Codec] Compressed" << data.size() << "bytes to" << compressed.size() << "bytes";
    return compressed;
}

QByteArray inflate_brutal_masm(const QByteArray& data)
{
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    qInfo() << "[Codec] MASM brutal decompression for" << data.size() << "bytes";
    
    // Use the functional MASM implementation
    QByteArray decompressed = brutal::decompress(data);
    
    if (decompressed.isEmpty()) {
        qWarning() << "[Codec] MASM decompression failed";
        return data;  // Fallback to original
    }
    
    qInfo() << "[Codec] Decompressed" << data.size() << "bytes to" << decompressed.size() << "bytes";
    return decompressed;
}

