#pragma once
// ============================================================================
// RawrXD FP8 KV Cache Integration
// Drop-in replacement for PagedKVCache with 4x memory reduction
// ============================================================================

#include "../kv_cache/PagedKVCache.h"
#include "fp8_quantizer.h"
#include <memory>
#include <vector>

namespace RawrXD {
namespace KVCache {

// ============================================================================
// FP8-aware Paged KV Cache
// Replaces TitanKV::PagedKVCache with quantized storage
// ============================================================================

class FP8PagedKVCache {
public:
    // Configuration
    struct Config {
        Quantization::FP8Format format = Quantization::FP8Format::E4M3;
        bool useGPU = true;
        bool useAVX512 = true;
        bool stochasticRounding = true;
        int numBlocks = 1024;  // Number of physical blocks
        int blockSize = 16;    // Tokens per block
        int numHeads = 32;
        int headDim = 128;     // 4096 / 32
    };

    explicit FP8PagedKVCache(const Config& config);
    ~FP8PagedKVCache();

    // Disable copy/move
    FP8PagedKVCache(const FP8PagedKVCache&) = delete;
    FP8PagedKVCache& operator=(const FP8PagedKVCache&) = delete;

    // Core KV cache operations
    void appendToken(const float* k_tensor, const float* v_tensor);
    void appendTokenBatch(const float* k_tensors, const float* v_tensors, int numTokens);
    
    // Retrieve K/V for attention (returns dequantized FP32)
    void getKeyTensor(int tokenIdx, float* out_k) const;
    void getValueTensor(int tokenIdx, float* out_v) const;
    
    // Get block table for paged attention kernel
    const std::vector<int>& getBlockTable() const { return m_blockTable; }
    
    // Context length
    int getContextLength() const { return m_currentLength; }
    
    // Memory statistics
    size_t getMemoryUsed() const;
    size_t getMemorySaved() const;
    double getCompressionRatio() const { return 4.0; }  // FP32 -> FP8
    
    // Quantization statistics
    float getAverageKeyScale() const;
    float getAverageValueScale() const;
    float getQuantizationError() const;  // RMSE vs FP32 reference

private:
    // Block structure with per-block quantization scales
    struct QuantizedBlock {
        std::vector<uint8_t> k_data;  // FP8 quantized K
        std::vector<uint8_t> v_data;  // FP8 quantized V
        float k_scale = 1.0f;
        float v_scale = 1.0f;
        int tokenCount = 0;
        bool isAllocated = false;
    };

    // Block management
    std::vector<std::unique_ptr<QuantizedBlock>> m_physicalBlocks;
    std::vector<int> m_freeBlocks;
    std::vector<int> m_blockTable;  // Logical -> physical mapping
    
    // Configuration
    Config m_config;
    Quantization::SovereignFP8Quantizer m_quantizer;
    
    // State
    int m_currentLength = 0;
    int m_numAllocatedBlocks = 0;
    
    // Temporary buffers for dequantization
    mutable std::vector<float> m_tempK;
    mutable std::vector<float> m_tempV;
    
    // Statistics
    mutable double m_totalQuantizationError = 0.0;
    mutable size_t m_errorSamples = 0;
    
    // Internal methods
    int allocateBlock();
    void freeBlock(int blockId);
    QuantizedBlock* getBlock(int blockId);
    const QuantizedBlock* getBlock(int blockId) const;
    
    void quantizeAndStore(QuantizedBlock* block, int offset, 
                          const float* k, const float* v);
    void dequantizeAndRetrieve(const QuantizedBlock* block, int offset,
                               float* k_out, float* v_out) const;
    
    int getBlockIdx(int tokenIdx) const { return tokenIdx / m_config.blockSize; }
    int getOffsetInBlock(int tokenIdx) const { return tokenIdx % m_config.blockSize; }
    
    size_t getElementsPerToken() const { 
        return m_config.numHeads * m_config.headDim; 
    }
};

// ============================================================================
// Factory for creating appropriate KV cache implementation
// ============================================================================

enum class KVCacheType {
    FP32,   // Original full precision
    FP8,    // 4x compression with FP8
    INT8    // 4x compression with INT8 (legacy)
};

class KVCacheFactory {
public:
    // Create KV cache with specified precision
    static std::unique_ptr<FP8PagedKVCache> createFP8(const FP8PagedKVCache::Config& config);
    
    // Auto-select based on available VRAM
    // Returns FP8 if VRAM < threshold, FP32 otherwise
    static KVCacheType selectOptimalType(size_t availableVRAM, size_t modelSize);
    
    // Get recommended config for target hardware
    static FP8PagedKVCache::Config getRecommendedConfig();
    static FP8PagedKVCache::Config getRX7800XTConfig();
    static FP8PagedKVCache::Config getA6000Config();
};

// ============================================================================
// Performance monitoring
// ============================================================================

struct FP8PerformanceMetrics {
    double quantizeTimeMs = 0.0;
    double dequantizeTimeMs = 0.0;
    double throughputGBps = 0.0;
    size_t tokensProcessed = 0;
    double compressionRatio = 4.0;
    double memorySavingsMB = 0.0;
};

class FP8PerformanceMonitor {
public:
    void recordQuantization(size_t elements, double timeMs);
    void recordDequantization(size_t elements, double timeMs);
    
    FP8PerformanceMetrics getMetrics() const;
    void reset();
    
    void printReport() const;

private:
    FP8PerformanceMetrics m_metrics;
};

} // namespace KVCache
} // namespace RawrXD
