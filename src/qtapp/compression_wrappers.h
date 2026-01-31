#pragma once


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
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        // For now, return input unchanged (placeholder)
        // Real implementation would use gzip compression
        output = input;
        return true;
    }
    
    // Decompress gzip data
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        // For now, return input unchanged (placeholder)
        // Real implementation would use gzip decompression
        output = input;
        return true;
    }
    
    // Get compression ratio
    float getCompressionRatio() const { return 1.0f; }
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
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        // For now, return input unchanged (placeholder)
        // Real implementation would use deflate compression
        output = input;
        return true;
    }
    
    // Decompress deflate data
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        // For now, return input unchanged (placeholder)
        // Real implementation would use deflate decompression
        output = input;
        return true;
    }
    
    // Get compression ratio
    float getCompressionRatio() const { return 1.0f; }
    
    // Initialize with algorithm level
    bool initialize(int level)
    {
        m_level = level;
        return true;
    }
    
private:
    int m_level = 6;
};

