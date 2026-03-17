// compact_wire.h — Gzip + JSON minification for bandwidth optimization
// Target: ≥3× compression ratio, ≤5ms latency overhead

#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <chrono>

namespace CompactWire {

// Compress JSON object to gzipped minified bytes
// Returns: compressed data ready for wire transmission
inline QByteArray compact(const QJsonObject& obj) {
    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return qCompress(json, 9);  // Level 9 for maximum compression
}

// Decompress gzipped bytes back to JSON object
// Returns: expanded JSON object for UI rendering
inline QJsonObject expand(const QByteArray& compressed) {
    QByteArray json = qUncompress(compressed);
    return QJsonDocument::fromJson(json).object();
}

// Benchmark compression ratio and latency
struct CompressionStats {
    size_t original_bytes;
    size_t compressed_bytes;
    double compression_ratio;
    double compress_ms;
    double decompress_ms;
};

inline CompressionStats benchmark(const QJsonObject& obj) {
    using namespace std::chrono;
    
    CompressionStats stats;
    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    stats.original_bytes = json.size();
    
    // Measure compression
    auto t0 = high_resolution_clock::now();
    QByteArray compressed = qCompress(json, 9);
    auto t1 = high_resolution_clock::now();
    stats.compress_ms = duration<double, std::milli>(t1 - t0).count();
    stats.compressed_bytes = compressed.size();
    
    // Measure decompression
    t0 = high_resolution_clock::now();
    QByteArray decompressed = qUncompress(compressed);
    t1 = high_resolution_clock::now();
    stats.decompress_ms = duration<double, std::milli>(t1 - t0).count();
    
    stats.compression_ratio = static_cast<double>(stats.original_bytes) / stats.compressed_bytes;
    return stats;
}

} // namespace CompactWire
