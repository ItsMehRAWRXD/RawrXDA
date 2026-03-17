#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// Forward declare ICompressionProvider interface
class ICompressionProvider {
public:
    virtual ~ICompressionProvider() = default;
    virtual bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) = 0;
    virtual bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) = 0;
    virtual bool IsSupported() const = 0;
};

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
    bool is_initialized_;
    uint32_t thread_count_;
};

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
    bool is_initialized_;
    uint32_t compression_level_;
    uint64_t total_compressed_input_{0};
    uint64_t total_decompressed_{0};
    uint64_t compression_calls_{0}; 
    uint64_t decompression_calls_{0};
};

class CompressionFactory {
public:
    static std::shared_ptr<ICompressionProvider> Create(uint32_t prefer_type);
};
