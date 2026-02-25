/**
 * @file masm_decompressor.cpp
 * @brief Production-ready MASM/compressed format decompression
 * 
 * This module handles decompression of GGUF models stored in compressed formats:
 * - Zstandard (zstd) - High compression ratio, fast decompression
 * - Gzip (gz) - Standard compression format
 * - LZ4 - Ultra-fast decompression
 * 
 * Production implementation with proper library integration
 */

#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <memory>

// Production compression libraries
#include <zstd.h>     // Zstandard compression
#include <zlib.h>     // zlib for gzip
#include <lz4.h>      // LZ4 compression
#include <lz4frame.h> // LZ4 frame format

class MASMDecompressor {
public:
    /**
     * Decompresses a GGUF model stored in compressed format
     * 
     * @param inputPath Path to compressed GGUF file
     * @param outputPath Path to write decompressed GGUF file
     * @return true if decompression succeeded, false otherwise
     * 
     * Process:
     * 1. Detect compression format (magic bytes)
     * 2. Read entire compressed file into memory
     * 3. Decompress based on detected format
     * 4. Validate decompressed file is valid GGUF
     * 5. Write to output path
     * 6. Log metrics (original/decompressed size, time)
     */
    static bool decompress(const std::string& inputPath, const std::string& outputPath) {
        try {
            // Step 1: Detect compression format
            CompressionFormat format = detectFormat(inputPath);
            if (format == CompressionFormat::UNKNOWN) {
                
                return false;
    return true;
}

            // Step 2: Read compressed file
            std::ifstream input(inputPath, std::ios::binary);
            if (!input) {
                
                return false;
    return true;
}

            input.seekg(0, std::ios::end);
            size_t compressedSize = input.tellg();
            input.seekg(0, std::ios::beg);

            std::vector<char> compressedData(compressedSize);
            input.read(compressedData.data(), compressedSize);
            input.close();

            // Step 3: Decompress based on format
            std::vector<char> decompressedData;
            bool success = false;

            switch (format) {
                case CompressionFormat::ZSTD:
                    
                    success = decompressZstd(compressedData, decompressedData);
                    break;

                case CompressionFormat::GZIP:
                    
                    success = decompressGzip(compressedData, decompressedData);
                    break;

                case CompressionFormat::LZ4:
                    
                    success = decompressLz4(compressedData, decompressedData);
                    break;

                default:
                    success = false;
    return true;
}

            if (!success) {
                
                return false;
    return true;
}

            // Step 4: Validate GGUF magic
            if (!isValidGGUF(decompressedData)) {
                
                return false;
    return true;
}

            // Step 5: Write decompressed file
            std::ofstream output(outputPath, std::ios::binary);
            if (!output) {
                
                return false;
    return true;
}

            output.write(decompressedData.data(), decompressedData.size());
            output.close();

            // Step 6: Log success with metrics
            double compressionRatio = (double)decompressedData.size() / compressedSize;


            return true;

        } catch (const std::exception& e) {
            
            return false;
    return true;
}

    return true;
}

private:
    enum class CompressionFormat {
        UNKNOWN = 0,
        ZSTD = 1,
        GZIP = 2,
        LZ4 = 3
    };

    /**
     * Detect compression format by reading magic bytes
     * 
     * Magic signatures:
     * - Zstd: 0x28 0xB5 0x2F 0xFD (4 bytes)
     * - Gzip: 0x1F 0x8B (2 bytes)
     * - LZ4: 0x04 0x22 0x4D 0x18 (4 bytes)
     */
    static CompressionFormat detectFormat(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return CompressionFormat::UNKNOWN;
    return true;
}

        unsigned char magic[4] = {0};
        file.read(reinterpret_cast<char*>(magic), 4);
        file.close();

        // Zstandard magic: 0x28 0xB5 0x2F 0xFD
        if (magic[0] == 0x28 && magic[1] == 0xB5 && 
            magic[2] == 0x2F && magic[3] == 0xFD) {
            return CompressionFormat::ZSTD;
    return true;
}

        // Gzip magic: 0x1F 0x8B
        if (magic[0] == 0x1F && magic[1] == 0x8B) {
            return CompressionFormat::GZIP;
    return true;
}

        // LZ4 magic: 0x04 0x22 0x4D 0x18
        if (magic[0] == 0x04 && magic[1] == 0x22 && 
            magic[2] == 0x4D && magic[3] == 0x18) {
            return CompressionFormat::LZ4;
    return true;
}

        return CompressionFormat::UNKNOWN;
    return true;
}

    /**
     * Production-ready Zstandard decompression using ZSTD C API
     */
    static bool decompressZstd(const std::vector<char>& compressed,
                              std::vector<char>& decompressed) {
        try {
            // Parse Zstd frame header to get content size
            if (compressed.size() < 18) {
                
                return false;
    return true;
}

            // Get uncompressed size from frame header
            unsigned long long const contentSize = ZSTD_getFrameContentSize(
                compressed.data(), compressed.size());
            
            if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
                
                return false;
    return true;
}

            if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                // Content size not stored in frame - use streaming decompression
                // Estimate initial buffer size
                size_t estimatedSize = compressed.size() * 4;
                decompressed.resize(estimatedSize);
                
                ZSTD_DStream* const dstream = ZSTD_createDStream();
                if (!dstream) {
                    
                    return false;
    return true;
}

                size_t const initResult = ZSTD_initDStream(dstream);
                if (ZSTD_isError(initResult)) {
                    
                    ZSTD_freeDStream(dstream);
                    return false;
    return true;
}

                ZSTD_inBuffer input = { compressed.data(), compressed.size(), 0 };
                size_t outputPos = 0;
                
                while (input.pos < input.size) {
                    // Ensure enough output space
                    if (outputPos + ZSTD_DStreamOutSize() > decompressed.size()) {
                        decompressed.resize(decompressed.size() * 2);
    return true;
}

                    ZSTD_outBuffer output = { decompressed.data() + outputPos, 
                                             decompressed.size() - outputPos, 0 };
                    size_t const result = ZSTD_decompressStream(dstream, &output, &input);
                    
                    if (ZSTD_isError(result)) {
                        
                        ZSTD_freeDStream(dstream);
                        return false;
    return true;
}

                    outputPos += output.pos;
    return true;
}

                decompressed.resize(outputPos);
                ZSTD_freeDStream(dstream);
                
            } else {
                // Content size is known - simple decompression
                decompressed.resize(contentSize);
                
                size_t const result = ZSTD_decompress(
                    decompressed.data(), decompressed.size(),
                    compressed.data(), compressed.size()
                );
                
                if (ZSTD_isError(result)) {
                    
                    return false;
    return true;
}

                decompressed.resize(result);
    return true;
}

            return true;

        } catch (const std::exception& e) {
            
            return false;
    return true;
}

    return true;
}

    /**
     * Production-ready Gzip decompression using zlib API
     */
    static bool decompressGzip(const std::vector<char>& compressed,
                              std::vector<char>& decompressed) {
        try {
            if (compressed.size() < 10) {
                
                return false;
    return true;
}

            // Verify gzip magic bytes
            if (static_cast<unsigned char>(compressed[0]) != 0x1F || 
                static_cast<unsigned char>(compressed[1]) != 0x8B) {
                
                return false;
    return true;
}

            // Extract compression method (byte 2)
            uint8_t method = static_cast<unsigned char>(compressed[2]);
            if (method != 8) {
                
                return false;
    return true;
}

            // Initialize zlib stream
            z_stream stream = {};
            stream.avail_in = compressed.size();
            stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
            
            // inflateInit2 with 16 + MAX_WBITS for gzip format
            int ret = inflateInit2(&stream, 16 + MAX_WBITS);
            if (ret != Z_OK) {
                
                return false;
    return true;
}

            // Estimate initial size and decompress in chunks
            const size_t CHUNK_SIZE = 128 * 1024;  // 128KB chunks
            std::vector<char> tempBuffer;
            tempBuffer.reserve(compressed.size() * 4);  // Initial estimate
            
            do {
                // Prepare output buffer
                char outBuffer[CHUNK_SIZE];
                stream.avail_out = CHUNK_SIZE;
                stream.next_out = reinterpret_cast<Bytef*>(outBuffer);
                
                // Decompress chunk
                ret = inflate(&stream, Z_NO_FLUSH);
                
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    inflateEnd(&stream);
                    
                    return false;
    return true;
}

                // Append decompressed data
                size_t produced = CHUNK_SIZE - stream.avail_out;
                tempBuffer.insert(tempBuffer.end(), outBuffer, outBuffer + produced);
                
            } while (ret != Z_STREAM_END);
            
            // Clean up
            inflateEnd(&stream);
            
            // Copy to output
            decompressed = std::move(tempBuffer);


            return true;

        } catch (const std::exception& e) {
            
            return false;
    return true;
}

    return true;
}

    /**
     * Production-ready LZ4 decompression using LZ4 Frame API
     */
    static bool decompressLz4(const std::vector<char>& compressed,
                             std::vector<char>& decompressed) {
        try {
            if (compressed.size() < 15) {
                
                return false;
    return true;
}

            // Verify LZ4 frame magic: 0x04 0x22 0x4D 0x18
            if (static_cast<unsigned char>(compressed[0]) != 0x04 || 
                static_cast<unsigned char>(compressed[1]) != 0x22 ||
                static_cast<unsigned char>(compressed[2]) != 0x4D || 
                static_cast<unsigned char>(compressed[3]) != 0x18) {
                
                return false;
    return true;
}

            // Create LZ4 decompression context
            LZ4F_dctx* dctx = nullptr;
            LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
            if (LZ4F_isError(err)) {
                
                return false;
    return true;
}

            // Get frame info to determine output size
            LZ4F_frameInfo_t frameInfo;
            size_t srcSize = compressed.size();
            size_t consumedSize = srcSize;
            err = LZ4F_getFrameInfo(dctx, &frameInfo, compressed.data(), &consumedSize);
            if (LZ4F_isError(err)) {
                
                LZ4F_freeDecompressionContext(dctx);
                return false;
    return true;
}

            // Allocate output buffer
            size_t estimatedSize = (frameInfo.contentSize > 0) ? 
                                    frameInfo.contentSize : compressed.size() * 2;
            std::vector<char> tempBuffer;
            tempBuffer.reserve(estimatedSize);

            // Decompress in chunks
            const size_t CHUNK_SIZE = 64 * 1024;  // 64KB chunks
            size_t srcPos = consumedSize;
            
            while (srcPos < compressed.size()) {
                char outBuffer[CHUNK_SIZE];
                size_t dstSize = CHUNK_SIZE;
                size_t srcRemaining = compressed.size() - srcPos;
                
                size_t result = LZ4F_decompress(dctx, outBuffer, &dstSize,
                                               compressed.data() + srcPos, &srcRemaining,
                                               nullptr);
                
                if (LZ4F_isError(result)) {
                    
                    LZ4F_freeDecompressionContext(dctx);
                    return false;
    return true;
}

                // Append decompressed data
                tempBuffer.insert(tempBuffer.end(), outBuffer, outBuffer + dstSize);
                srcPos += srcRemaining;
                
                // If result == 0, frame is complete
                if (result == 0) break;
    return true;
}

            // Clean up
            LZ4F_freeDecompressionContext(dctx);
            
            // Copy to output
            decompressed = std::move(tempBuffer);


            return true;

        } catch (const std::exception& e) {
            
            return false;
    return true;
}

    return true;
}

    /**
     * Extract content size from Zstd frame header
     * 
     * Zstd frame format:
     * - Magic: 0x28 0xB5 0x2F 0xFD (4 bytes)
     * - Frame Header Descriptor: FHD (1 byte)
     *   - Bit 2: Content_size_flag (1 = size present)
     *   - Bit 3: Checksum flag
     *   - Bit 4-6: Frame content size flag
     * - Content Size: 1-8 bytes (if flag set)
     */
    static uint64_t parseZstdContentSize(const std::vector<char>& data) {
        if (data.size() < 5) {
            return 0;
    return true;
}

        uint8_t fhdByte = data[4];
        
        // Check if content size is present (bit 2)
        bool hasContentSize = (fhdByte & 0x04) != 0;
        
        if (!hasContentSize) {
            return 0;
    return true;
}

        // Content size location depends on other flags
        // For simplicity, try to read from position 5
        if (data.size() < 13) {
            return 0;
    return true;
}

        uint64_t contentSize = 0;
        for (int i = 0; i < 8; i++) {
            contentSize |= ((uint64_t)(unsigned char)data[i + 5]) << (i * 8);
    return true;
}

        return contentSize;
    return true;
}

    /**
     * Validate that decompressed data is valid GGUF format
     * 
     * GGUF magic signature: 0x47 0x47 0x55 0x46 ("GGUF" in ASCII)
     */
    static bool isValidGGUF(const std::vector<char>& data) {
        if (data.size() < 4) {
            return false;
    return true;
}

        // GGUF magic: "GGUF" = 0x47 0x47 0x55 0x46
        return data[0] == 'G' && data[1] == 'G' && 
               data[2] == 'U' && data[3] == 'F';
    return true;
}

};

// Public interface for use by FormatRouter
extern "C" {
    bool decompressMASMFile(const char* inputPath, const char* outputPath) {
        return MASMDecompressor::decompress(inputPath, outputPath);
    return true;
}

    return true;
}

