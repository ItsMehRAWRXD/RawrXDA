/**
 * @file context_manager.cpp
 * @brief Context window management and shifting
 * 
 * Provides:
 * - Context window allocation and management
 * - Sliding window for long sequences
 * - KV cache eviction strategies
 * - Token budget management
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "context_manager.h"
#include <algorithm>
#include <math>

namespace RawrXD::Inference {

// ============================================================================
// ContextManager Implementation
// ============================================================================

ContextManager::ContextManager(const Config& config)
    : m_config(config)
    , m_nextContextId(1)
{
}

ContextManager::~ContextManager() = default;

// ============================================================================
// Context Management
// ============================================================================

int64_t ContextManager::createContext(int maxTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int64_t contextId = m_nextContextId++;
    
    Context ctx;
    ctx.id = contextId;
    ctx.maxTokens = maxTokens > 0 ? maxTokens : m_config.defaultMaxTokens;
    ctx.currentTokens = 0;
    ctx.position = 0;
    
    m_contexts[contextId] = ctx;
    
    return contextId;
}

bool ContextManager::destroyContext(int64_t contextId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    m_contexts.erase(it);
    return true;
}

bool ContextManager::resetContext(int64_t contextId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    it->second.tokens.clear();
    it->second.currentTokens = 0;
    it->second.position = 0;
    
    return true;
}

// ============================================================================
// Token Management
// ============================================================================

bool ContextManager::addTokens(int64_t contextId, const std::vector<uint32_t>& tokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    auto& ctx = it->second;
    
    // Check if we need to shift
    if (ctx.currentTokens + static_cast<int>(tokens.size()) > ctx.maxTokens) {
        if (!shiftContext(ctx, ctx.currentTokens + static_cast<int>(tokens.size()) - ctx.maxTokens)) {
            return false;
        }
    }
    
    // Add tokens
    ctx.tokens.insert(ctx.tokens.end(), tokens.begin(), tokens.end());
    ctx.currentTokens += static_cast<int>(tokens.size());
    ctx.position += static_cast<int>(tokens.size());
    
    return true;
}

std::vector<uint32_t> ContextManager::getTokens(int64_t contextId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return {};
    }
    
    return it->second.tokens;
}

int ContextManager::getTokenCount(int64_t contextId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return 0;
    }
    
    return it->second.currentTokens;
}

// ============================================================================
// Context Shifting
// ============================================================================

bool ContextManager::shiftContext(Context& ctx, int tokensToRemove) {
    if (tokensToRemove <= 0) return true;
    
    switch (m_config.shiftStrategy) {
        case ShiftStrategy::TruncateStart:
            return shiftTruncateStart(ctx, tokensToRemove);
        case ShiftStrategy::TruncateEnd:
            return shiftTruncateEnd(ctx, tokensToRemove);
        case ShiftStrategy::SlidingWindow:
            return shiftSlidingWindow(ctx, tokensToRemove);
        case ShiftStrategy::KeepSystem:
            return shiftKeepSystem(ctx, tokensToRemove);
        default:
            return shiftTruncateStart(ctx, tokensToRemove);
    }
}

bool ContextManager::shiftTruncateStart(Context& ctx, int tokensToRemove) {
    if (tokensToRemove >= ctx.currentTokens) {
        ctx.tokens.clear();
        ctx.currentTokens = 0;
        return true;
    }
    
    ctx.tokens.erase(ctx.tokens.begin(), ctx.tokens.begin() + tokensToRemove);
    ctx.currentTokens -= tokensToRemove;
    
    return true;
}

bool ContextManager::shiftTruncateEnd(Context& ctx, int tokensToRemove) {
    if (tokensToRemove >= ctx.currentTokens) {
        ctx.tokens.clear();
        ctx.currentTokens = 0;
        return true;
    }
    
    ctx.tokens.resize(ctx.tokens.size() - tokensToRemove);
    ctx.currentTokens -= tokensToRemove;
    
    return true;
}

bool ContextManager::shiftSlidingWindow(Context& ctx, int tokensToRemove) {
    // Keep the most recent tokens, remove oldest
    return shiftTruncateStart(ctx, tokensToRemove);
}

bool ContextManager::shiftKeepSystem(Context& ctx, int tokensToRemove) {
    // Keep system tokens (first N tokens), remove from middle
    int systemTokens = m_config.systemTokenCount;
    
    if (systemTokens >= ctx.currentTokens - tokensToRemove) {
        // Not enough non-system tokens to keep
        return shiftTruncateStart(ctx, tokensToRemove);
    }
    
    // Remove from middle (after system tokens)
    int removeFromMiddle = tokensToRemove;
    int keepAfterMiddle = ctx.currentTokens - systemTokens - removeFromMiddle;
    
    if (keepAfterMiddle < 0) {
        return shiftTruncateStart(ctx, tokensToRemove);
    }
    
    std::vector<uint32_t> newTokens;
    newTokens.reserve(systemTokens + keepAfterMiddle);
    
    // Keep system tokens
    newTokens.insert(newTokens.end(), ctx.tokens.begin(), 
                    ctx.tokens.begin() + systemTokens);
    
    // Keep recent tokens
    newTokens.insert(newTokens.end(), 
                    ctx.tokens.end() - keepAfterMiddle, ctx.tokens.end());
    
    ctx.tokens = std::move(newTokens);
    ctx.currentTokens = static_cast<int>(ctx.tokens.size());
    
    return true;
}

// ============================================================================
// KV Cache Management
// ============================================================================

bool ContextManager::allocateKVCache(int64_t contextId, int numLayers,
                                   int numHeads, int headDim) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    auto& ctx = it->second;
    
    // Calculate cache size
    int cacheSize = ctx.maxTokens * numLayers * numHeads * headDim;
    
    // Allocate cache
    ctx.kvCache.keyCache.resize(cacheSize, 0.0f);
    ctx.kvCache.valueCache.resize(cacheSize, 0.0f);
    ctx.kvCache.numLayers = numLayers;
    ctx.kvCache.numHeads = numHeads;
    ctx.kvCache.headDim = headDim;
    ctx.kvCache.cacheLen = 0;
    
    return true;
}

bool ContextManager::freeKVCache(int64_t contextId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    auto& ctx = it->second;
    ctx.kvCache.keyCache.clear();
    ctx.kvCache.valueCache.clear();
    ctx.kvCache.cacheLen = 0;
    
    return true;
}

bool ContextManager::updateKVCache(int64_t contextId, const float* keys,
                                  const float* values, int numTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return false;
    }
    
    auto& ctx = it->second;
    auto& cache = ctx.kvCache;
    
    int tokenSize = cache.numLayers * cache.numHeads * cache.headDim;
    int offset = cache.cacheLen * tokenSize;
    
    if (offset + numTokens * tokenSize > static_cast<int>(cache.keyCache.size())) {
        return false;
    }
    
    // Copy keys and values
    std::memcpy(cache.keyCache.data() + offset, keys, 
               numTokens * tokenSize * sizeof(float));
    std::memcpy(cache.valueCache.data() + offset, values,
               numTokens * tokenSize * sizeof(float));
    
    cache.cacheLen += numTokens;
    
    return true;
}

// ============================================================================
// Statistics
// ============================================================================

ContextStats ContextManager::getStats(int64_t contextId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ContextStats stats;
    
    auto it = m_contexts.find(contextId);
    if (it == m_contexts.end()) {
        return stats;
    }
    
    const auto& ctx = it->second;
    stats.totalTokens = ctx.currentTokens;
    stats.maxTokens = ctx.maxTokens;
    stats.remainingTokens = ctx.maxTokens - ctx.currentTokens;
    stats.kvCacheSize = static_cast<int>(ctx.kvCache.keyCache.size() * sizeof(float));
    
    return stats;
}

std::vector<int64_t> ContextManager::getActiveContexts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<int64_t> contexts;
    contexts.reserve(m_contexts.size());
    
    for (const auto& [id, _] : m_contexts) {
        contexts.push_back(id);
    }
    
    return contexts;
}

} // namespace RawrXD::Inference
