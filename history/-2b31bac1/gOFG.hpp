/**
 * @file masm_codec.hpp
 * @brief MASM compression/decompression wrapper for IDE-wide data exchange
 * 
 * Provides C++ interface to MASM assembly compression routines with automatic
 * compression for all network, file, and memory operations throughout the IDE.
 */

#pragma once
#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QDataStream>
#include <cstdint>
#include <cstdlib>

namespace rawr {
namespace codec {

// External MASM assembly functions
extern "C" {
    // deflate_compress_masm.asm - RLE compression with DEFLATE wrapper
    void* gzip_masm_alloc(const void* src, size_t len, size_t* out_len);
    uint8_t* deflate_compress_masm(const uint8_t* src, size_t len, size_t* out_len);
    uint8_t* inflate_decompress_masm(const uint8_t* src, size_t len, size_t* out_len);
    
    // kernels/deflate_godmode_masm.asm - God-mode DEFLATE with AVX2/CRC32/lazy matching
    // Returns: pointer to gzip-compressed data (caller must free())
    // out_len: receives total compressed size (header + compressed + footer)
    // hash_buf: working buffer (HASH_SIZE * 4 bytes = 32KB)
    uint8_t* deflate_godmode(const uint8_t* src, size_t len, size_t* out_len, uint32_t* hash_buf);
    
    // kernels/inflate_match_masm.asm - Ultra-fast match copier for DEFLATE decompression
    // Fast path: copies bytes from (dst - distance) to dst for length bytes
    // Updates: rdi register (destination pointer) automatically
    // Note: Called from decompression loop, not standalone
    // Input: rdi=dst, r10=window_base, r11d=distance, r12d=length
    // Output: rdi=updated dst pointer
    void inflate_match(uint8_t* dst, uint8_t* window_base, uint32_t distance, uint32_t length);
    
    // Bounds-checked version for validation/debugging
    // Additional inputs: r13=window_size, r14=output_end
    void inflate_match_safe(uint8_t* dst, uint8_t* window_base, uint32_t distance, uint32_t length, 
                           uint32_t window_size, uint8_t* output_end);
}

/**
 * @brief MASM compression configuration
 */
struct MASMCodecConfig {
    bool enabled = true;               ///< Enable/disable compression
    bool auto_detect = true;           ///< Auto-detect compressed data
    size_t min_size_threshold = 1024; ///< Min size to compress (bytes)
    uint8_t compression_level = 9;     ///< 1-9, higher = more compression
    
    static MASMCodecConfig& global() {
        static MASMCodecConfig instance;
        return instance;
    }
};

/**
 * @brief MASM-compressed data envelope
 * 
 * Format: [MAGIC(4)][VERSION(1)][FLAGS(1)][ORIG_SIZE(8)][CRC32(4)][DATA...]
 */
struct MASMEnvelope {
    static constexpr uint32_t MAGIC = 0x4D41534D;  // "MASM"
    static constexpr uint8_t VERSION = 2;
    
    uint32_t magic;
    uint8_t version;
    uint8_t flags;         // Bit 0: gzip format, Bit 1: RLE mode
    uint64_t original_size;
    uint32_t crc32;
    // Compressed data follows
    
    static constexpr size_t HEADER_SIZE = 18;
} __attribute__((packed));

/**
 * @brief Main MASM codec interface
 */
class MASMCodec {
public:
    /**
     * @brief Compress data using MASM assembly routines
     * @param input Uncompressed data
     * @param use_gzip Use gzip format (default: true for network compat)
     * @return Compressed data with MASM envelope, or empty on failure
     */
    static QByteArray compress(const QByteArray& input, bool use_gzip = true) {
        auto& cfg = MASMCodecConfig::global();
        
        // Skip compression for small data
        if (!cfg.enabled || input.size() < cfg.min_size_threshold) {
            return input;
        }
        
        size_t compressed_size = 0;
        void* compressed_data = nullptr;
        
        if (use_gzip) {
            // Use gzip_masm_alloc for network-compatible gzip format
            compressed_data = gzip_masm_alloc(input.constData(), input.size(), &compressed_size);
        } else {
            // Use raw MASM deflate for maximum speed
            compressed_data = reinterpret_cast<void*>(
                deflate_compress_masm(
                    reinterpret_cast<const uint8_t*>(input.constData()),
                    input.size(),
                    &compressed_size
                )
            );
        }
        
        if (!compressed_data || compressed_size == 0) {
            return QByteArray();  // Compression failed
        }
        
        // Build envelope
        MASMEnvelope envelope;
        envelope.magic = MASMEnvelope::MAGIC;
        envelope.version = MASMEnvelope::VERSION;
        envelope.flags = (use_gzip ? 0x01 : 0x00) | 0x02;  // Bit 1 = RLE mode
        envelope.original_size = input.size();
        envelope.crc32 = crc32(reinterpret_cast<const uint8_t*>(input.constData()), input.size());
        
        QByteArray result(MASMEnvelope::HEADER_SIZE + compressed_size, Qt::Uninitialized);
        std::memcpy(result.data(), &envelope, MASMEnvelope::HEADER_SIZE);
        std::memcpy(result.data() + MASMEnvelope::HEADER_SIZE, compressed_data, compressed_size);
        
        free(compressed_data);
        
        return result;
    }
    
    /**
     * @brief Decompress MASM-compressed data
     * @param input Compressed data with MASM envelope
     * @param verify_crc Verify CRC32 checksum (default: true)
     * @return Decompressed data, or empty on failure
     */
    static QByteArray decompress(const QByteArray& input, bool verify_crc = true) {
        auto& cfg = MASMCodecConfig::global();
        
        // Auto-detect: if not MASM-compressed, return as-is
        if (cfg.auto_detect && !isCompressed(input)) {
            return input;
        }
        
        if (input.size() < MASMEnvelope::HEADER_SIZE) {
            return QByteArray();  // Invalid data
        }
        
        const MASMEnvelope* envelope = reinterpret_cast<const MASMEnvelope*>(input.constData());
        
        if (envelope->magic != MASMEnvelope::MAGIC || envelope->version != MASMEnvelope::VERSION) {
            return QByteArray();  // Invalid envelope
        }
        
        const uint8_t* compressed_data = reinterpret_cast<const uint8_t*>(input.constData() + MASMEnvelope::HEADER_SIZE);
        size_t compressed_size = input.size() - MASMEnvelope::HEADER_SIZE;
        
        size_t decompressed_size = 0;
        uint8_t* decompressed_data = inflate_decompress_masm(
            compressed_data, 
            compressed_size, 
            &decompressed_size
        );
        
        if (!decompressed_data || decompressed_size != envelope->original_size) {
            if (decompressed_data) free(decompressed_data);
            return QByteArray();  // Decompression failed
        }
        
        // Verify CRC32
        if (verify_crc) {
            uint32_t calculated_crc = crc32(decompressed_data, decompressed_size);
            if (calculated_crc != envelope->crc32) {
                free(decompressed_data);
                return QByteArray();  // CRC mismatch
            }
        }
        
        QByteArray result(reinterpret_cast<const char*>(decompressed_data), decompressed_size);
        free(decompressed_data);
        
        return result;
    }
    
    /**
     * @brief Check if data is MASM-compressed
     */
    static bool isCompressed(const QByteArray& data) {
        if (data.size() < MASMEnvelope::HEADER_SIZE) return false;
        const MASMEnvelope* envelope = reinterpret_cast<const MASMEnvelope*>(data.constData());
        return envelope->magic == MASMEnvelope::MAGIC && envelope->version == MASMEnvelope::VERSION;
    }
    
    /**
     * @brief Get compression ratio (compressed_size / original_size)
     */
    static double getCompressionRatio(const QByteArray& compressed_data) {
        if (!isCompressed(compressed_data)) return 1.0;
        const MASMEnvelope* envelope = reinterpret_cast<const MASMEnvelope*>(compressed_data.constData());
        return static_cast<double>(compressed_data.size()) / envelope->original_size;
    }
    
    /**
     * @brief Get original size from compressed data (without decompressing)
     */
    static qint64 getOriginalSize(const QByteArray& compressed_data) {
        if (!isCompressed(compressed_data)) return compressed_data.size();
        const MASMEnvelope* envelope = reinterpret_cast<const MASMEnvelope*>(compressed_data.constData());
        return envelope->original_size;
    }

private:
    static uint32_t crc32(const uint8_t* data, size_t len) {
        static uint32_t table[256] = {0};
        static bool table_init = false;
        
        if (!table_init) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                table[i] = c;
            }
            table_init = true;
        }
        
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }
};

/**
 * @brief QIODevice wrapper with transparent MASM compression
 */
class MASMCompressedDevice : public QIODevice {
    Q_OBJECT
public:
    explicit MASMCompressedDevice(QIODevice* device, QObject* parent = nullptr)
        : QIODevice(parent), m_device(device), m_compress_on_write(true) {
        if (device) {
            connect(device, &QIODevice::readyRead, this, &QIODevice::readyRead);
            connect(device, &QIODevice::bytesWritten, this, &QIODevice::bytesWritten);
        }
    }
    
    void setCompressOnWrite(bool enable) { m_compress_on_write = enable; }
    
    bool open(OpenMode mode) override {
        if (!m_device) return false;
        setOpenMode(mode);
        return m_device->open(mode);
    }
    
    void close() override {
        flush();
        if (m_device) m_device->close();
        setOpenMode(NotOpen);
    }
    
    bool isSequential() const override {
        return m_device ? m_device->isSequential() : true;
    }
    
    qint64 bytesAvailable() const override {
        return m_readBuffer.size() + QIODevice::bytesAvailable();
    }
    
    qint64 readData(char* data, qint64 maxSize) override {
        if (!m_device) return -1;
        
        // Fill read buffer if empty
        if (m_readBuffer.isEmpty() && m_device->bytesAvailable() > 0) {
            QByteArray compressed = m_device->readAll();
            m_readBuffer = MASMCodec::decompress(compressed);
        }
        
        qint64 toRead = qMin(maxSize, static_cast<qint64>(m_readBuffer.size()));
        if (toRead > 0) {
            std::memcpy(data, m_readBuffer.constData(), toRead);
            m_readBuffer.remove(0, toRead);
        }
        
        return toRead;
    }
    
    qint64 writeData(const char* data, qint64 maxSize) override {
        if (!m_device) return -1;
        
        QByteArray input(data, maxSize);
        
        if (m_compress_on_write) {
            QByteArray compressed = MASMCodec::compress(input, true);
            if (compressed.isEmpty()) {
                // Compression failed, write uncompressed
                return m_device->write(input);
            }
            return m_device->write(compressed);
        }
        
        return m_device->write(input);
    }
    
    void flush() {
        if (m_device && m_device->isWritable()) {
            // QIODevice doesn't have flush(), but we can ensure data is written
            m_device->waitForBytesWritten(1000);
        }
    }

private:
    QIODevice* m_device;
    QByteArray m_readBuffer;
    bool m_compress_on_write;
};

} // namespace codec
} // namespace rawr
