#pragma once

#include <QString>
#include <QByteArray>
#include <memory>
#include <vector>
#include <cstdint>

/**
 * @class BrutalGzipWrapper
 * @brief Qt-friendly wrapper for GZIP compression/decompression
 */
class BrutalGzipWrapper
{
public:
    BrutalGzipWrapper() = default;
    ~BrutalGzipWrapper() = default;
    
    // Compress data using brutal gzip (optimized)
    bool compress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        // Use Qt's built-in zlib compression (qCompress adds a 4-byte length header)
        output = qCompress(input, 9); // level 9 = best compression
        m_lastRatio = input.isEmpty() ? 1.0f : static_cast<float>(output.size()) / input.size();
        return !output.isEmpty();
    }
    
    // Decompress gzip data
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        output = qUncompress(input);
        return !output.isEmpty();
    }
    
    // Get compression ratio
    float getCompressionRatio() const { return m_lastRatio; }

private:
    float m_lastRatio = 1.0f;
};

/**
 * @class DeflateWrapper
 * @brief Qt-friendly wrapper for DEFLATE compression/decompression
 */
class DeflateWrapper
{
public:
    DeflateWrapper() = default;
    ~DeflateWrapper() = default;
    
    // Compress data using deflate (optimized)
    bool compress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        output = qCompress(input, m_level);
        m_lastRatio = input.isEmpty() ? 1.0f : static_cast<float>(output.size()) / input.size();
        return !output.isEmpty();
    }
    
    // Decompress deflate data
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        output = qUncompress(input);
        return !output.isEmpty();
    }
    
    // Get compression ratio
    float getCompressionRatio() const { return m_lastRatio; }

    float m_lastRatio = 1.0f;
    
    // Initialize with algorithm level
    bool initialize(int level)
    {
        m_level = level;
        return true;
    }
    
private:
    int m_level = 6;
};
