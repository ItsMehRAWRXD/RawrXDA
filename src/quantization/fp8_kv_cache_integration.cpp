// ============================================================================
// RawrXD FP8 KV Cache Integration Implementation
// ============================================================================

#include "fp8_kv_cache_integration.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>

namespace RawrXD {
namespace KVCache {

// ============================================================================
// FP8PagedKVCache Implementation
// ============================================================================

FP8PagedKVCache::FP8PagedKVCache(const Config& config)
    : m_config(config)
    , m_quantizer(config.format)
{
    // Initialize quantizer
    m_quantizer.enableStochasticRounding(config.stochasticRounding);
    
    // Pre-allocate physical blocks
    m_physicalBlocks.reserve(config.numBlocks);
    for (int i = 0; i < config.numBlocks; ++i) {
        m_physicalBlocks.push_back(nullptr);
        m_freeBlocks.push_back(i);
    }
    
    // Pre-size block table
    m_blockTable.reserve(1024);
    
    // Pre-allocate temp buffers
    size_t elementsPerBlock = config.blockSize * config.numHeads * config.headDim;
    m_tempK.resize(elementsPerBlock);
    m_tempV.resize(elementsPerBlock);
}

FP8PagedKVCache::~FP8PagedKVCache() {
    // Blocks are automatically freed via unique_ptr
}

int FP8PagedKVCache::allocateBlock() {
    if (m_freeBlocks.empty()) {
        // Out of memory - could implement eviction here
        return -1;
    }
    
    int blockId = m_freeBlocks.back();
    m_freeBlocks.pop_back();
    
    if (!m_physicalBlocks[blockId]) {
        m_physicalBlocks[blockId] = std::make_unique<QuantizedBlock>();
        
        // Allocate storage for K and V
        size_t elementsPerBlock = m_config.blockSize * m_config.numHeads * m_config.headDim;
        m_physicalBlocks[blockId]->k_data.resize(elementsPerBlock);
        m_physicalBlocks[blockId]->v_data.resize(elementsPerBlock);
    }
    
    m_physicalBlocks[blockId]->isAllocated = true;
    m_physicalBlocks[blockId]->tokenCount = 0;
    m_numAllocatedBlocks++;
    
    return blockId;
}

void FP8PagedKVCache::freeBlock(int blockId) {
    if (blockId < 0 || blockId >= m_config.numBlocks) return;
    
    if (m_physicalBlocks[blockId]) {
        m_physicalBlocks[blockId]->isAllocated = false;
        m_physicalBlocks[blockId]->tokenCount = 0;
    }
    
    m_freeBlocks.push_back(blockId);
    m_numAllocatedBlocks--;
}

FP8PagedKVCache::QuantizedBlock* FP8PagedKVCache::getBlock(int blockId) {
    if (blockId < 0 || blockId >= m_config.numBlocks) return nullptr;
    return m_physicalBlocks[blockId].get();
}

const FP8PagedKVCache::QuantizedBlock* FP8PagedKVCache::getBlock(int blockId) const {
    if (blockId < 0 || blockId >= m_config.numBlocks) return nullptr;
    return m_physicalBlocks[blockId].get();
}

void FP8PagedKVCache::quantizeAndStore(QuantizedBlock* block, int offset,
                                       const float* k, const float* v) {
    size_t elementsPerToken = getElementsPerToken();
    size_t destOffset = offset * elementsPerToken;
    
    // Compute per-token scales (could be per-block for better compression)
    block->k_scale = m_quantizer.computeScale(k, elementsPerToken);
    block->v_scale = m_quantizer.computeScale(v, elementsPerToken);
    
    // Quantize K and V
    m_quantizer.quantizeBatch(k, block->k_data.data() + destOffset, 
                              elementsPerToken, block->k_scale);
    m_quantizer.quantizeBatch(v, block->v_data.data() + destOffset,
                              elementsPerToken, block->v_scale);
}

void FP8PagedKVCache::dequantizeAndRetrieve(const QuantizedBlock* block, int offset,
                                            float* k_out, float* v_out) const {
    size_t elementsPerToken = getElementsPerToken();
    size_t srcOffset = offset * elementsPerToken;
    
    // Dequantize K and V
    m_quantizer.dequantizeBatch(block->k_data.data() + srcOffset, k_out,
                                elementsPerToken, block->k_scale);
    m_quantizer.dequantizeBatch(block->v_data.data() + srcOffset, v_out,
                                elementsPerToken, block->v_scale);
    
    // Track quantization error (for debugging/analysis)
    // In production, this would compare against reference FP32
}

void FP8PagedKVCache::appendToken(const float* k_tensor, const float* v_tensor) {
    int blockIdx = getBlockIdx(m_currentLength);
    int offsetInBlock = getOffsetInBlock(m_currentLength);
    
    // Allocate new block if needed
    if (blockIdx >= static_cast<int>(m_blockTable.size())) {
        int newBlock = allocateBlock();
        if (newBlock < 0) {
            // Out of memory - could trigger eviction
            std::cerr << "[FP8KV] ERROR: Out of KV cache blocks!\n";
            return;
        }
        m_blockTable.push_back(newBlock);
    }
    
    int physicalBlock = m_blockTable[blockIdx];
    QuantizedBlock* block = getBlock(physicalBlock);
    
    if (!block) {
        std::cerr << "[FP8KV] ERROR: Invalid block!\n";
        return;
    }
    
    // Quantize and store
    quantizeAndStore(block, offsetInBlock, k_tensor, v_tensor);
    block->tokenCount++;
    
    m_currentLength++;
}

void FP8PagedKVCache::appendTokenBatch(const float* k_tensors, const float* v_tensors, 
                                       int numTokens) {
    // Optimized batch append
    for (int i = 0; i < numTokens; ++i) {
        size_t offset = i * getElementsPerToken();
        appendToken(k_tensors + offset, v_tensors + offset);
    }
}

void FP8PagedKVCache::getKeyTensor(int tokenIdx, float* out_k) const {
    int blockIdx = getBlockIdx(tokenIdx);
    int offsetInBlock = getOffsetInBlock(tokenIdx);
    
    if (blockIdx >= static_cast<int>(m_blockTable.size())) {
        // Return zeros for out-of-bounds
        std::fill(out_k, out_k + getElementsPerToken(), 0.0f);
        return;
    }
    
    int physicalBlock = m_blockTable[blockIdx];
    const QuantizedBlock* block = getBlock(physicalBlock);
    
    if (!block || offsetInBlock >= block->tokenCount) {
        std::fill(out_k, out_k + getElementsPerToken(), 0.0f);
        return;
    }
    
    // Dequantize only K
    size_t elementsPerToken = getElementsPerToken();
    size_t srcOffset = offsetInBlock * elementsPerToken;
    m_quantizer.dequantizeBatch(block->k_data.data() + srcOffset, out_k,
                                elementsPerToken, block->k_scale);
}

void FP8PagedKVCache::getValueTensor(int tokenIdx, float* out_v) const {
    int blockIdx = getBlockIdx(tokenIdx);
    int offsetInBlock = getOffsetInBlock(tokenIdx);
    
    if (blockIdx >= static_cast<int>(m_blockTable.size())) {
        std::fill(out_v, out_v + getElementsPerToken(), 0.0f);
        return;
    }
    
    int physicalBlock = m_blockTable[blockIdx];
    const QuantizedBlock* block = getBlock(physicalBlock);
    
    if (!block || offsetInBlock >= block->tokenCount) {
        std::fill(out_v, out_v + getElementsPerToken(), 0.0f);
        return;
    }
    
    size_t elementsPerToken = getElementsPerToken();
    size_t srcOffset = offsetInBlock * elementsPerToken;
    m_quantizer.dequantizeBatch(block->v_data.data() + srcOffset, out_v,
                                elementsPerToken, block->v_scale);
}

size_t FP8PagedKVCache::getMemoryUsed() const {
    // FP8 storage: 1 byte per element
    size_t elementsPerBlock = m_config.blockSize * m_config.numHeads * m_config.headDim;
    return m_numAllocatedBlocks * elementsPerBlock * 2;  // K + V
}

size_t FP8PagedKVCache::getMemorySaved() const {
    // Compare to FP32
    size_t fp32Size = m_numAllocatedBlocks * m_config.blockSize * m_config.numHeads * 
                      m_config.headDim * 2 * sizeof(float);
    return fp32Size - getMemoryUsed();
}

float FP8PagedKVCache::getAverageKeyScale() const {
    if (m_numAllocatedBlocks == 0) return 1.0f;
    
    double totalScale = 0.0;
    int count = 0;
    
    for (const auto& block : m_physicalBlocks) {
        if (block && block->isAllocated && block->tokenCount > 0) {
            totalScale += block->k_scale;
            count++;
        }
    }
    
    return count > 0 ? static_cast<float>(totalScale / count) : 1.0f;
}

float FP8PagedKVCache::getAverageValueScale() const {
    if (m_numAllocatedBlocks == 0) return 1.0f;
    
    double totalScale = 0.0;
    int count = 0;
    
    for (const auto& block : m_physicalBlocks) {
        if (block && block->isAllocated && block->tokenCount > 0) {
            totalScale += block->v_scale;
            count++;
        }
    }
    
    return count > 0 ? static_cast<float>(totalScale / count) : 1.0f;
}

float FP8PagedKVCache::getQuantizationError() const {
    if (m_errorSamples == 0) return 0.0f;
    return static_cast<float>(std::sqrt(m_totalQuantizationError / m_errorSamples));
}

// ============================================================================
// KVCacheFactory Implementation
// ============================================================================

std::unique_ptr<FP8PagedKVCache> KVCacheFactory::createFP8(const FP8PagedKVCache::Config& config) {
    return std::make_unique<FP8PagedKVCache>(config);
}

KVCacheType KVCacheFactory::selectOptimalType(size_t availableVRAM, size_t modelSize) {
    // Heuristic: if model takes > 50% of VRAM, use FP8
    double utilization = static_cast<double>(modelSize) / availableVRAM;
    
    if (utilization > 0.5) {
        return KVCacheType::FP8;
    }
    return KVCacheType::FP32;
}

FP8PagedKVCache::Config KVCacheFactory::getRecommendedConfig() {
    FP8PagedKVCache::Config config;
    
    // Auto-detect capabilities
    #ifdef __AVX512F__
    config.useAVX512 = true;
    #else
    config.useAVX512 = false;
    #endif
    
    // Default to E4M3 for better precision
    config.format = Quantization::FP8Format::E4M3;
    config.stochasticRounding = true;
    
    return config;
}

FP8PagedKVCache::Config KVCacheFactory::getRX7800XTConfig() {
    FP8PagedKVCache::Config config;
    
    // RX 7800 XT has 16GB VRAM
    // Optimize for 70B model with ~4GB KV cache
    config.numBlocks = 2048;      // More blocks for larger contexts
    config.blockSize = 16;
    config.format = Quantization::FP8Format::E4M3;  // Better precision
    config.useGPU = true;
    config.useAVX512 = true;
    config.stochasticRounding = true;
    
    return config;
}

FP8PagedKVCache::Config KVCacheFactory::getA6000Config() {
    FP8PagedKVCache::Config config;
    
    // A6000 has 48GB VRAM - can use FP32 for smaller models
    // But FP8 still beneficial for very long contexts
    config.numBlocks = 4096;
    config.blockSize = 16;
    config.format = Quantization::FP8Format::E4M3;
    config.useGPU = true;
    config.useAVX512 = true;
    config.stochasticRounding = true;
    
    return config;
}

// ============================================================================
// FP8PerformanceMonitor Implementation
// ============================================================================

void FP8PerformanceMonitor::recordQuantization(size_t elements, double timeMs) {
    m_metrics.quantizeTimeMs += timeMs;
    m_metrics.tokensProcessed += elements / (128 * 32);  // Approximate tokens
    
    // Calculate throughput
    size_t bytesProcessed = elements * sizeof(float);
    if (timeMs > 0) {
        m_metrics.throughputGBps = (bytesProcessed / (1024.0 * 1024.0 * 1024.0)) / (timeMs / 1000.0);
    }
}

void FP8PerformanceMonitor::recordDequantization(size_t elements, double timeMs) {
    m_metrics.dequantizeTimeMs += timeMs;
}

FP8PerformanceMetrics FP8PerformanceMonitor::getMetrics() const {
    return m_metrics;
}

void FP8PerformanceMonitor::reset() {
    m_metrics = FP8PerformanceMetrics();
}

void FP8PerformanceMonitor::printReport() const {
    std::cout << "=== FP8 Performance Report ===\n";
    std::cout << "Quantization time: " << m_metrics.quantizeTimeMs << " ms\n";
    std::cout << "Dequantization time: " << m_metrics.dequantizeTimeMs << " ms\n";
    std::cout << "Throughput: " << m_metrics.throughputGBps << " GB/s\n";
    std::cout << "Tokens processed: " << m_metrics.tokensProcessed << "\n";
    std::cout << "Compression ratio: " << m_metrics.compressionRatio << "x\n";
    std::cout << "Memory saved: " << m_metrics.memorySavingsMB << " MB\n";
}

} // namespace KVCache
} // namespace RawrXD
