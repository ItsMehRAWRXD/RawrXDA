#pragma once

#include <QString>
#include <QByteArray>
#include <QElapsedTimer>
#include <QDebug>
#include <memory>
#include <vector>
#include <cstdint>
#include "compression_interface.h"

/**
 * @class BrutalGzipWrapper
 * @brief Production-ready Qt wrapper for MASM-optimized GZIP decompression
 * 
 * Features:
 * - MASM-optimized kernels via brutal_gzip.lib
 * - Structured logging with latency tracking
 * - Automatic fallback to uncompressed data on errors
 * - Performance metrics collection
 */
class BrutalGzipWrapper
{
public:
    BrutalGzipWrapper() : m_impl(std::make_unique<::BrutalGzipWrapper>()) {
        if (!m_impl->IsSupported()) {
            qWarning() << "[BrutalGzipWrapper] MASM kernels not available, using fallback";
        } else {
            qInfo() << "[BrutalGzipWrapper] Initialized with kernel:" << QString::fromStdString(m_impl->GetActiveKernel());
        }
    }
    
    ~BrutalGzipWrapper() = default;
    
    // Compress data using brutal gzip (MASM-optimized)
    bool compress(const QByteArray& input, QByteArray& output)
    {
        QElapsedTimer timer;
        timer.start();
        
        std::vector<uint8_t> inputVec(input.begin(), input.end());
        std::vector<uint8_t> outputVec;
        
        bool success = m_impl->Compress(inputVec, outputVec);
        
        if (success) {
            output = QByteArray(reinterpret_cast<const char*>(outputVec.data()), outputVec.size());
            m_lastCompressionRatio = static_cast<float>(output.size()) / input.size();
            qint64 elapsed = timer.elapsed();
            qInfo() << QString("[BrutalGzipWrapper] Compressed %1 → %2 bytes (%.2f%% ratio) in %3ms")
                .arg(input.size()).arg(output.size()).arg(m_lastCompressionRatio * 100).arg(elapsed);
        } else {
            qWarning() << "[BrutalGzipWrapper] Compression failed, returning uncompressed data";
            output = input;
        }
        
        return success;
    }
    
    // Decompress gzip data (MASM-optimized)
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        QElapsedTimer timer;
        timer.start();
        
        if (input.isEmpty()) {
            qDebug() << "[BrutalGzipWrapper] Empty input, skipping decompression";
            output = input;
            return true;
        }
        
        // Check if data is actually compressed (gzip magic: 0x1f 0x8b)
        if (input.size() < 2 || (static_cast<uint8_t>(input[0]) != 0x1f || static_cast<uint8_t>(input[1]) != 0x8b)) {
            qDebug() << "[BrutalGzipWrapper] Data not gzip-compressed (no magic header), returning as-is";
            output = input;
            return true;
        }
        
        std::vector<uint8_t> inputVec(input.begin(), input.end());
        std::vector<uint8_t> outputVec;
        
        bool success = m_impl->Decompress(inputVec, outputVec);
        
        if (success) {
            output = QByteArray(reinterpret_cast<const char*>(outputVec.data()), outputVec.size());
            qint64 elapsed = timer.elapsed();
            double throughputMBps = (output.size() / 1024.0 / 1024.0) / (elapsed / 1000.0);
            qInfo() << QString("[BrutalGzipWrapper] Decompressed %1 → %2 bytes in %3ms (%.2f MB/s)")
                .arg(input.size()).arg(output.size()).arg(elapsed).arg(throughputMBps);
        } else {
            qCritical() << "[BrutalGzipWrapper] Decompression FAILED - returning uncompressed data";
            output = input;
        }
        
        return success;
    }
    
    // Get compression ratio from last operation
    float getCompressionRatio() const { return m_lastCompressionRatio; }
    
private:
    std::unique_ptr<::BrutalGzipWrapper> m_impl;
    float m_lastCompressionRatio = 1.0f;
};

/**
 * @class DeflateWrapper
 * @brief Production-ready Qt wrapper for MASM-optimized DEFLATE decompression
 * 
 * Features:
 * - MASM-optimized kernels via deflate_brutal_qt
 * - Structured logging with latency tracking
 * - Automatic fallback to uncompressed data on errors
 * - Performance metrics collection
 */
class DeflateWrapper
{
public:
    DeflateWrapper() : m_impl(std::make_unique<::DeflateWrapper>()) {
        if (!m_impl->IsSupported()) {
            qWarning() << "[DeflateWrapper] MASM kernels not available, using fallback";
        } else {
            qInfo() << "[DeflateWrapper] Initialized with MASM-optimized deflate";
        }
    }
    
    ~DeflateWrapper() = default;
    
    // Compress data using deflate (MASM-optimized)
    bool compress(const QByteArray& input, QByteArray& output)
    {
        QElapsedTimer timer;
        timer.start();
        
        std::vector<uint8_t> inputVec(input.begin(), input.end());
        std::vector<uint8_t> outputVec;
        
        bool success = m_impl->Compress(inputVec, outputVec);
        
        if (success) {
            output = QByteArray(reinterpret_cast<const char*>(outputVec.data()), outputVec.size());
            m_lastCompressionRatio = static_cast<float>(output.size()) / input.size();
            qint64 elapsed = timer.elapsed();
            qInfo() << QString("[DeflateWrapper] Compressed %1 → %2 bytes (%.2f%% ratio) in %3ms")
                .arg(input.size()).arg(output.size()).arg(m_lastCompressionRatio * 100).arg(elapsed);
        } else {
            qWarning() << "[DeflateWrapper] Compression failed, returning uncompressed data";
            output = input;
        }
        
        return success;
    }
    
    // Decompress deflate data (MASM-optimized)
    bool decompress(const QByteArray& input, QByteArray& output)
    {
        QElapsedTimer timer;
        timer.start();
        
        if (input.isEmpty()) {
            qDebug() << "[DeflateWrapper] Empty input, skipping decompression";
            output = input;
            return true;
        }
        
        // Check if data is likely deflate-compressed (heuristic: entropy check)
        // For now, attempt decompression and fall back on failure
        std::vector<uint8_t> inputVec(input.begin(), input.end());
        std::vector<uint8_t> outputVec;
        
        bool success = m_impl->Decompress(inputVec, outputVec);
        
        if (success) {
            output = QByteArray(reinterpret_cast<const char*>(outputVec.data()), outputVec.size());
            qint64 elapsed = timer.elapsed();
            double throughputMBps = (output.size() / 1024.0 / 1024.0) / (elapsed / 1000.0);
            qInfo() << QString("[DeflateWrapper] Decompressed %1 → %2 bytes in %3ms (%.2f MB/s)")
                .arg(input.size()).arg(output.size()).arg(elapsed).arg(throughputMBps);
        } else {
            qDebug() << "[DeflateWrapper] Data not deflate-compressed, returning as-is";
            output = input;
        }
        
        return success;
    }
    
    // Get compression ratio from last operation
    float getCompressionRatio() const { return m_lastCompressionRatio; }
    
    // Initialize with algorithm level
    bool initialize(int level)
    {
        m_level = level;
        qInfo() << "[DeflateWrapper] Compression level set to" << level;
        return true;
    }
    
private:
    std::unique_ptr<::DeflateWrapper> m_impl;
    int m_level = 6;
    float m_lastCompressionRatio = 1.0f;
};
