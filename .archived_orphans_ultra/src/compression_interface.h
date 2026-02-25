#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <sstream>

// =====================================================================
// Abstract compression provider interface
// =====================================================================
class ICompressionProvider {
public:
    virtual ~ICompressionProvider() = default;
    virtual bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) = 0;
    virtual bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) = 0;
    virtual bool IsSupported() const = 0;
};

// =====================================================================
// BrutalGzip MASM kernel wrapper
// =====================================================================
class BrutalGzipWrapper : public ICompressionProvider {
public:
    BrutalGzipWrapper();
    ~BrutalGzipWrapper();
    
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    std::string GetActiveKernel() const;
    void SetThreadCount(uint32_t num_threads);

private:
    int thread_count_ = 1;
    bool is_initialized_ = false;
};

// =====================================================================
// Deflate wrapper (inflate/deflate via codec namespace)
// =====================================================================
class DeflateWrapper : public ICompressionProvider {
public:
    DeflateWrapper();
    ~DeflateWrapper();

    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    void SetCompressionLevel(uint32_t level);
    std::string GetDecompressionStats() const;

private:
    uint32_t compression_level_ = 6;
    bool is_initialized_ = false;
    uint64_t total_compressed_input_ = 0;
    uint64_t total_decompressed_bytes_ = 0;
    uint64_t compression_calls_ = 0;
    uint64_t decompression_calls_ = 0;
};

// =====================================================================
// Factory
// =====================================================================
class CompressionFactory {
public:
    static std::shared_ptr<ICompressionProvider> Create(uint32_t prefer_type);
    static bool IsSupported(uint32_t type);
};

// =====================================================================
// Stats
// =====================================================================
struct CompressionStats {
    uint64_t total_compressed_bytes = 0;
    uint64_t total_decompressed_bytes = 0;
    uint64_t decompression_calls = 0;
    uint64_t compression_calls = 0;
    double avg_compression_ratio = 0.0;
    std::string ToString() const;
};
