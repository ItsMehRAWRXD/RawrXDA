/**
 * @file context_manager.h
 * @brief Context window management and shifting
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <mutex>

namespace RawrXD::Inference {

// ============================================================================
// Shift Strategy
// ============================================================================

enum class ShiftStrategy {
    TruncateStart,  // Remove oldest tokens
    TruncateEnd,    // Remove newest tokens
    SlidingWindow,  // Keep most recent window
    KeepSystem      // Keep system tokens, remove from middle
};

// ============================================================================
// Context Configuration
// ============================================================================

struct ContextConfig {
    int defaultMaxTokens = 4096;
    ShiftStrategy shiftStrategy = ShiftStrategy::TruncateStart;
    int systemTokenCount = 0;
    float memoryLimit = 0.0f; // 0 = unlimited
};

// ============================================================================
// KV Cache
// ============================================================================

struct KVCache {
    std::vector<float> keyCache;
    std::vector<float> valueCache;
    int numLayers = 0;
    int numHeads = 0;
    int headDim = 0;
    int cacheLen = 0;
};

// ============================================================================
// Context
// ============================================================================

struct Context {
    int64_t id = -1;
    int maxTokens = 4096;
    int currentTokens = 0;
    int position = 0;
    std::vector<uint32_t> tokens;
    KVCache kvCache;
};

// ============================================================================
// Context Statistics
// ============================================================================

struct ContextStats {
    int totalTokens = 0;
    int maxTokens = 0;
    int remainingTokens = 0;
    int kvCacheSize = 0;
};

// ============================================================================
// Context Manager
// ============================================================================

class ContextManager {
public:
    explicit ContextManager(const Config& config);
    ~ContextManager();
    
    // Context management
    int64_t createContext(int maxTokens = -1);
    bool destroyContext(int64_t contextId);
    bool resetContext(int64_t contextId);
    
    // Token management
    bool addTokens(int64_t contextId, const std::vector<uint32_t>& tokens);
    std::vector<uint32_t> getTokens(int64_t contextId) const;
    int getTokenCount(int64_t contextId) const;
    
    // KV cache management
    bool allocateKVCache(int64_t contextId, int numLayers, int numHeads, int headDim);
    bool freeKVCache(int64_t contextId);
    bool updateKVCache(int64_t contextId, const float* keys, const float* values, int numTokens);
    
    // Statistics
    ContextStats getStats(int64_t contextId) const;
    std::vector<int64_t> getActiveContexts() const;
    
private:
    bool shiftContext(Context& ctx, int tokensToRemove);
    bool shiftTruncateStart(Context& ctx, int tokensToRemove);
    bool shiftTruncateEnd(Context& ctx, int tokensToRemove);
    bool shiftSlidingWindow(Context& ctx, int tokensToRemove);
    bool shiftKeepSystem(Context& ctx, int tokensToRemove);
    
    Config m_config;
    mutable std::mutex m_mutex;
    int64_t m_nextContextId;
    std::map<int64_t, Context> m_contexts;
};

} // namespace RawrXD::Inference
