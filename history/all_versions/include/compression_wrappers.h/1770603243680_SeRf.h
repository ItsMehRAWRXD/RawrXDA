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
        output = qCompress(input, m_level);
        if (output.isEmpty()) { output = input; return false; }
        m_lastInputSize = input.size();
        m_lastOutputSize = output.size();
        return true;
    }
    
    // Decompress gzip data
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        output = qUncompress(input);
        return !output.isEmpty();
    }
    
    // Get compression ratio
    float getCompressionRatio() const {
        if (m_lastInputSize == 0) return 1.0f;
        return static_cast<float>(m_lastOutputSize) / static_cast<float>(m_lastInputSize);
    }

private:
    int m_level = 6;
    qint64 m_lastInputSize = 0;
    qint64 m_lastOutputSize = 0;
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
        if (output.isEmpty()) { output = input; return false; }
        m_lastInputSize = input.size();
        m_lastOutputSize = output.size();
        return true;
    }
    
    // Decompress deflate data
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        if (input.isEmpty()) { output.clear(); return true; }
        output = qUncompress(input);
        return !output.isEmpty();
    }
    
    // Get compression ratio
    float getCompressionRatio() const {
        if (m_lastInputSize == 0) return 1.0f;
        return static_cast<float>(m_lastOutputSize) / static_cast<float>(m_lastInputSize);
    }
    
    // Initialize with algorithm level
    bool initialize(int level)
    {
        m_level = level;
        return true;
    }
    
private:
    int m_level = 6;
    qint64 m_lastInputSize = 0;
    qint64 m_lastOutputSize = 0;
};
