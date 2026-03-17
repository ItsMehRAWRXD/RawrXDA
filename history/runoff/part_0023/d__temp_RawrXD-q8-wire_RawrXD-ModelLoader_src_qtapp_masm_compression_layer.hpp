/**
 * @file masm_compression_layer.hpp
 * @brief MASM Compression Integration Layer - Transparent compression for all IDE data exchange
 * 
 * Provides automatic compression/decompression using MASM RLE engine for:
 * - Network communication (hotpatch proxy, GGUF server)
 * - Memory module storage (agentic patterns, conversations)
 * - File I/O (GGUF models, session data)
 * - IPC (terminals, processes)
 * 
 * Features:
 * - Zero-copy compression for large buffers
 * - Adaptive compression (auto-disable for incompressible data)
 * - CRC32 integrity checking
 * - Streaming support for incremental data
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QFile>
#include <QDataStream>
#include <cstdint>
#include <cstring>

namespace MASMCompression {

// MASM compression/decompression C API
extern "C" {
    uint8_t* deflate_compress_masm(const uint8_t* src, size_t len, size_t* out_len);
    uint8_t* inflate_decompress_masm(const uint8_t* src, size_t len, size_t* out_len);
    void* gzip_masm_alloc(const void* src, size_t len, size_t* out_len);
}

// MASM GGUF Header (from masm_compressed_gguf.hpp)
struct MASMHeader {
    uint32_t magic;           // "MASM" = 0x4D41534D
    uint32_t version;         // Version 1
    uint32_t compression_mode; // 0=none, 1=RLE, 2=brutal_gzip
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum;
    uint32_t reserved;
    
    static constexpr uint32_t MAGIC = 0x4D41534D;
    static constexpr uint32_t VERSION = 1;
    static constexpr uint32_t MODE_NONE = 0;
    static constexpr uint32_t MODE_RLE = 1;
    static constexpr uint32_t MODE_BRUTAL_GZIP = 2;
};

/**
 * @brief CRC32 calculation for data integrity
 */
inline uint32_t crc32(const uint8_t* data, size_t len) {
    static uint32_t table[256] = {0};
    static bool initialized = false;
    
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }
    
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/**
 * @brief Check if data is MASM-compressed
 */
inline bool isCompressed(const QByteArray& data) {
    if (data.size() < static_cast<int>(sizeof(MASMHeader))) return false;
    const MASMHeader* header = reinterpret_cast<const MASMHeader*>(data.constData());
    return header->magic == MASMHeader::MAGIC && header->version == MASMHeader::VERSION;
}

/**
 * @brief Compress data using MASM RLE
 * @param data Input data
 * @param forceCompress If false, skip compression if ratio < 1.1
 * @return Compressed data with MASM header, or original if incompressible
 */
inline QByteArray compress(const QByteArray& data, bool forceCompress = false) {
    if (data.isEmpty()) return data;
    
    // Use gzip_masm_alloc for full gzip encoding
    size_t compressed_size = 0;
    uint8_t* compressed = reinterpret_cast<uint8_t*>(
        gzip_masm_alloc(data.constData(), data.size(), &compressed_size)
    );
    
    if (!compressed || compressed_size == 0) {
        return data; // Compression failed, return original
    }
    
    // Check compression ratio (only compress if we save >= 10%)
    if (!forceCompress && compressed_size >= static_cast<size_t>(data.size() * 0.9)) {
        free(compressed);
        return data; // Not worth compressing
    }
    
    // Build MASM header
    MASMHeader header;
    header.magic = MASMHeader::MAGIC;
    header.version = MASMHeader::VERSION;
    header.compression_mode = MASMHeader::MODE_BRUTAL_GZIP;
    header.original_size = data.size();
    header.compressed_size = compressed_size;
    header.checksum = crc32(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
    header.reserved = 0;
    
    // Combine header + compressed data
    QByteArray result(sizeof(header) + compressed_size, Qt::Uninitialized);
    std::memcpy(result.data(), &header, sizeof(header));
    std::memcpy(result.data() + sizeof(header), compressed, compressed_size);
    
    free(compressed);
    return result;
}

/**
 * @brief Decompress MASM-compressed data
 * @param data Compressed data with MASM header
 * @return Decompressed data, or original if not compressed
 */
inline QByteArray decompress(const QByteArray& data) {
    if (!isCompressed(data)) {
        return data; // Not compressed, return as-is
    }
    
    const MASMHeader* header = reinterpret_cast<const MASMHeader*>(data.constData());
    
    if (header->compression_mode == MASMHeader::MODE_NONE) {
        // No compression, extract payload
        return data.mid(sizeof(MASMHeader));
    }
    
    // Extract compressed payload (skip header)
    const uint8_t* compressed_data = reinterpret_cast<const uint8_t*>(data.constData() + sizeof(MASMHeader));
    size_t compressed_size = data.size() - sizeof(MASMHeader);
    
    // Decompress using inflate_decompress_masm
    size_t decompressed_size = 0;
    uint8_t* decompressed = inflate_decompress_masm(compressed_data, compressed_size, &decompressed_size);
    
    if (!decompressed || decompressed_size != header->original_size) {
        free(decompressed);
        throw std::runtime_error("MASM decompression failed: size mismatch");
    }
    
    // Verify CRC32
    uint32_t calculated_crc = crc32(decompressed, decompressed_size);
    if (calculated_crc != header->checksum) {
        free(decompressed);
        throw std::runtime_error("MASM decompression failed: CRC32 mismatch");
    }
    
    QByteArray result(reinterpret_cast<const char*>(decompressed), decompressed_size);
    free(decompressed);
    return result;
}

/**
 * @brief Compress and write to file
 */
inline bool compressToFile(const QString& filename, const QByteArray& data) {
    QByteArray compressed = compress(data, true);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(compressed);
    file.close();
    return true;
}

/**
 * @brief Read and decompress from file
 */
inline QByteArray decompressFromFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    QByteArray compressed = file.readAll();
    file.close();
    return decompress(compressed);
}

/**
 * @brief RAII wrapper for automatic compression/decompression
 */
class CompressedBuffer {
public:
    CompressedBuffer(const QByteArray& data) : m_original(data), m_compressed(compress(data)) {}
    
    const QByteArray& compressed() const { return m_compressed; }
    const QByteArray& original() const { return m_original; }
    
    size_t compressionRatio() const {
        if (m_original.isEmpty()) return 100;
        return (m_compressed.size() * 100) / m_original.size();
    }
    
    bool isWorthCompressing() const {
        return compressionRatio() < 90; // Worth if we save >= 10%
    }
    
private:
    QByteArray m_original;
    QByteArray m_compressed;
};

/**
 * @brief Stream compressor for incremental data
 */
class StreamCompressor {
public:
    void append(const QByteArray& chunk) {
        m_buffer.append(chunk);
    }
    
    QByteArray flush() {
        QByteArray compressed = compress(m_buffer);
        m_buffer.clear();
        return compressed;
    }
    
    void reset() {
        m_buffer.clear();
    }
    
    size_t bufferSize() const {
        return m_buffer.size();
    }
    
private:
    QByteArray m_buffer;
};

} // namespace MASMCompression
