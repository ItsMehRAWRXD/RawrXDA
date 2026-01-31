#include "kv_cache_optimizer.h"
#include "../gpu_masm/gpu_masm_bridge.h"
#include "../ggml_masm/ggml_masm_bridge.h"
KVCacheOptimizer::KVCacheOptimizer()
    
    , m_cacheSizeLimit(32000) // Default limit: 32k tokens
    , m_slidingWindowSize(1000) // Default sliding window size
    , m_gpuCacheInitialized(false)
{
    // Initialize GPU KV cache if available
    if (KVCacheInit(m_cacheSizeLimit) == 0) {
        m_gpuCacheInitialized = true;
    } else {
        m_gpuCacheInitialized = false;
    }
}

KVCacheOptimizer::~KVCacheOptimizer()
{
    // No explicit cleanup needed - MASM handles it
}

void KVCacheOptimizer::setCacheSizeLimit(int limit)
{
    m_cacheSizeLimit = limit;
}

void KVCacheOptimizer::addTokens(const std::vector<int> &tokens)
{
    if (m_gpuCacheInitialized && !tokens.empty()) {
        // Use GPU-accelerated token addition
        std::vector<int> tokenVec = tokens.toVector();
        int result = KVCacheAddTokens(tokenVec.data(), tokenVec.size());
        
        if (result == 0) {
            // Success - update local copy for queries
            m_cachedTokens.append(tokens);
        } else {
            // CPU fallback
            m_cachedTokens.append(tokens);
        }
    } else {
        // CPU-only path
        m_cachedTokens.append(tokens);
    }
    
    evictIfNeeded();
    m_lastAccessTime = // DateTime::currentDateTime();
}

std::vector<int> KVCacheOptimizer::getCachedTokens() const
{
    return m_cachedTokens;
}

void KVCacheOptimizer::evictIfNeeded()
{
    if (m_cachedTokens.size() > m_cacheSizeLimit) {
        int tokensToEvict = m_cachedTokens.size() - m_cacheSizeLimit;
        
        if (tokensToEvict > 0) {
            if (m_gpuCacheInitialized) {
                // Use GPU-accelerated eviction
                int result = KVCacheEvict(tokensToEvict);
                
                if (result == 0) {
                } else {
                }
            }
            
            // Update local cache (both GPU and CPU paths)
            m_cachedTokens.erase(m_cachedTokens.begin(), m_cachedTokens.begin() + tokensToEvict);
        }
    }
}

void KVCacheOptimizer::setSlidingWindowSize(int size)
{
    m_slidingWindowSize = size;
}

