#ifndef CODE_ANALYSIS_COMPRESSION_HPP
#define CODE_ANALYSIS_COMPRESSION_HPP

/**
 * @file code_analysis_compression.hpp
 * @brief Compression utilities for large code analysis results
 * 
 * Provides:
 * - Streaming compression for large analysis results
 * - Intelligent truncation with context preservation
 * - Memory-efficient result storage
 */

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QBuffer>
#include <QDataStream>
#include <QCryptographicHash>
#include <zlib.h>
#include <memory>

namespace RawrXD {
namespace Analysis {

/**
 * @class CodeAnalysisCompressor
 * @brief Compresses and decompresses code analysis results
 */
class CodeAnalysisCompressor {
public:
    struct CompressionStats {
        qint64 originalSize = 0;
        qint64 compressedSize = 0;
        double compressionRatio = 0.0;
        qint64 compressionTimeMs = 0;
        QString algorithm = "zlib";
    };
    
    struct Config {
        int compressionLevel = 6;  // 1-9, higher = better compression but slower
        qint64 maxUncompressedSize = 10 * 1024 * 1024;  // 10 MB max
        bool enableChecksum = true;
        int chunkSize = 65536;  // 64 KB chunks
    };
    
    explicit CodeAnalysisCompressor() = default;
    
    void setConfig(const Config& config) { m_config = config; }
    
    /**
     * @brief Compress analysis results
     */
    QByteArray compress(const QString& data, CompressionStats* stats = nullptr) {
        if (data.isEmpty()) {
            return QByteArray();
        }
        
        QElapsedTimer timer;
        timer.start();
        
        QByteArray input = data.toUtf8();
        QByteArray output;
        
        // Prepare zlib compression
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        if (deflateInit(&strm, m_config.compressionLevel) != Z_OK) {
            qWarning() << "[CodeAnalysisCompressor] Failed to initialize compression";
            return input;  // Return uncompressed on failure
        }
        
        // Reserve estimated output size
        output.reserve(input.size());
        
        strm.next_in = reinterpret_cast<Bytef*>(input.data());
        strm.avail_in = input.size();
        
        char outBuffer[65536];
        
        do {
            strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
            strm.avail_out = sizeof(outBuffer);
            
            int ret = deflate(&strm, Z_FINISH);
            if (ret == Z_STREAM_ERROR) {
                deflateEnd(&strm);
                qWarning() << "[CodeAnalysisCompressor] Compression stream error";
                return input;
            }
            
            int have = sizeof(outBuffer) - strm.avail_out;
            output.append(outBuffer, have);
            
        } while (strm.avail_out == 0);
        
        deflateEnd(&strm);
        
        // Add header with metadata
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << QString("RAXC");  // Magic header
        stream << static_cast<qint32>(1);  // Version
        stream << static_cast<qint64>(input.size());  // Original size
        
        if (m_config.enableChecksum) {
            QByteArray checksum = QCryptographicHash::hash(input, QCryptographicHash::Sha256);
            stream << checksum;
        }
        
        result.append(output);
        
        if (stats) {
            stats->originalSize = input.size();
            stats->compressedSize = result.size();
            stats->compressionRatio = static_cast<double>(input.size()) / result.size();
            stats->compressionTimeMs = timer.elapsed();
            stats->algorithm = "zlib";
        }
        
        qDebug() << "[CodeAnalysisCompressor] Compressed" << input.size() 
                 << "bytes to" << result.size() << "bytes"
                 << "ratio:" << (static_cast<double>(input.size()) / result.size());
        
        return result;
    }
    
    /**
     * @brief Decompress analysis results
     */
    QString decompress(const QByteArray& data) {
        if (data.isEmpty()) {
            return QString();
        }
        
        // Parse header
        QDataStream stream(data);
        QString magic;
        qint32 version;
        qint64 originalSize;
        
        stream >> magic >> version >> originalSize;
        
        if (magic != "RAXC") {
            qWarning() << "[CodeAnalysisCompressor] Invalid compression header";
            return QString::fromUtf8(data);  // Assume uncompressed
        }
        
        QByteArray checksum;
        if (m_config.enableChecksum) {
            stream >> checksum;
        }
        
        // Get compressed data (rest of the buffer)
        int headerSize = data.indexOf('\0', 4) + 1;
        QByteArray compressed = data.mid(stream.device()->pos());
        
        // Decompress
        QByteArray output;
        output.reserve(originalSize);
        
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = compressed.size();
        strm.next_in = reinterpret_cast<Bytef*>(compressed.data());
        
        if (inflateInit(&strm) != Z_OK) {
            qWarning() << "[CodeAnalysisCompressor] Failed to initialize decompression";
            return QString();
        }
        
        char outBuffer[65536];
        
        do {
            strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
            strm.avail_out = sizeof(outBuffer);
            
            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&strm);
                qWarning() << "[CodeAnalysisCompressor] Decompression error";
                return QString();
            }
            
            int have = sizeof(outBuffer) - strm.avail_out;
            output.append(outBuffer, have);
            
        } while (strm.avail_out == 0);
        
        inflateEnd(&strm);
        
        // Verify checksum if enabled
        if (m_config.enableChecksum && !checksum.isEmpty()) {
            QByteArray computedChecksum = QCryptographicHash::hash(output, QCryptographicHash::Sha256);
            if (computedChecksum != checksum) {
                qWarning() << "[CodeAnalysisCompressor] Checksum mismatch!";
            }
        }
        
        return QString::fromUtf8(output);
    }
    
    /**
     * @brief Compress JSON analysis results
     */
    QByteArray compressJson(const QJsonObject& data, CompressionStats* stats = nullptr) {
        QJsonDocument doc(data);
        return compress(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)), stats);
    }
    
    /**
     * @brief Decompress to JSON
     */
    QJsonObject decompressJson(const QByteArray& data) {
        QString jsonStr = decompress(data);
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        return doc.object();
    }
    
    /**
     * @brief Intelligently truncate large results while preserving context
     */
    static QString truncateWithContext(const QString& content, int maxLength, int contextLines = 5) {
        if (content.length() <= maxLength) {
            return content;
        }
        
        QStringList lines = content.split('\n');
        int totalLines = lines.size();
        
        if (totalLines <= contextLines * 2 + 1) {
            // Too few lines to truncate meaningfully
            return content.left(maxLength) + "\n... [truncated]";
        }
        
        // Keep first and last contextLines
        QStringList result;
        
        // First context lines
        for (int i = 0; i < contextLines && i < lines.size(); ++i) {
            result.append(lines[i]);
        }
        
        // Add truncation marker
        int omittedLines = totalLines - (contextLines * 2);
        result.append(QString("\n... [%1 lines omitted] ...\n").arg(omittedLines));
        
        // Last context lines
        for (int i = totalLines - contextLines; i < totalLines; ++i) {
            result.append(lines[i]);
        }
        
        QString truncated = result.join('\n');
        
        // If still too long, hard truncate
        if (truncated.length() > maxLength) {
            truncated = truncated.left(maxLength) + "\n... [truncated]";
        }
        
        return truncated;
    }
    
private:
    Config m_config;
};

/**
 * @class StreamingAnalysisBuffer
 * @brief Memory-efficient buffer for streaming analysis results
 */
class StreamingAnalysisBuffer {
public:
    struct Config {
        qint64 maxMemoryBytes = 50 * 1024 * 1024;  // 50 MB max in memory
        QString spillDirectory;  // Directory for spilled data
        bool enableCompression = true;
        int compressionLevel = 6;
    };
    
    explicit StreamingAnalysisBuffer() {
        m_compressor = std::make_unique<CodeAnalysisCompressor>();
    }
    
    void setConfig(const Config& config) { 
        m_config = config;
        
        CodeAnalysisCompressor::Config compConfig;
        compConfig.compressionLevel = config.compressionLevel;
        m_compressor->setConfig(compConfig);
    }
    
    void append(const QString& chunk) {
        QMutexLocker locker(&m_mutex);
        
        m_buffer.append(chunk);
        m_totalBytes += chunk.size();
        
        // Check if we need to spill to disk
        if (m_totalBytes > m_config.maxMemoryBytes && !m_config.spillDirectory.isEmpty()) {
            spillToDisk();
        }
    }
    
    QString finalize() {
        QMutexLocker locker(&m_mutex);
        
        if (!m_spillFile.isEmpty()) {
            // Read back from disk
            loadFromDisk();
        }
        
        QString result = m_buffer;
        m_buffer.clear();
        m_totalBytes = 0;
        
        return result;
    }
    
    void clear() {
        QMutexLocker locker(&m_mutex);
        m_buffer.clear();
        m_totalBytes = 0;
        
        if (!m_spillFile.isEmpty()) {
            QFile::remove(m_spillFile);
            m_spillFile.clear();
        }
    }
    
    qint64 size() const { return m_totalBytes; }
    bool isSpilled() const { return !m_spillFile.isEmpty(); }
    
private:
    void spillToDisk() {
        QString fileName = QString("%1/analysis_spill_%2.tmp")
            .arg(m_config.spillDirectory)
            .arg(QDateTime::currentMSecsSinceEpoch());
        
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray data;
            if (m_config.enableCompression) {
                data = m_compressor->compress(m_buffer);
            } else {
                data = m_buffer.toUtf8();
            }
            file.write(data);
            file.close();
            
            m_spillFile = fileName;
            m_buffer.clear();
            
            qInfo() << "[StreamingAnalysisBuffer] Spilled" << m_totalBytes 
                    << "bytes to disk:" << fileName;
        }
    }
    
    void loadFromDisk() {
        if (m_spillFile.isEmpty()) return;
        
        QFile file(m_spillFile);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            if (m_config.enableCompression) {
                m_buffer = m_compressor->decompress(data);
            } else {
                m_buffer = QString::fromUtf8(data);
            }
            
            QFile::remove(m_spillFile);
            m_spillFile.clear();
            
            qInfo() << "[StreamingAnalysisBuffer] Loaded data back from disk";
        }
    }
    
    Config m_config;
    QString m_buffer;
    qint64 m_totalBytes = 0;
    QString m_spillFile;
    QMutex m_mutex;
    std::unique_ptr<CodeAnalysisCompressor> m_compressor;
};

} // namespace Analysis
} // namespace RawrXD

#endif // CODE_ANALYSIS_COMPRESSION_HPP
