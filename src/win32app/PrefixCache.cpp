// ============================================================================
// PrefixCache.cpp — Prefix Cache Implementation
// ============================================================================

#include "PrefixCache.h"
#include <algorithm>
#include <cassert>

namespace RawrXD {

// ---------------------------------------------------------------------------
// FNV-1a hash implementation
// ---------------------------------------------------------------------------

uint64_t PrefixCache::HashFile(const std::string& filePath) {
    // FNV-1a 64-bit hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : filePath) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint32_t PrefixCache::HashLine(const std::string& line) {
    // FNV-1a 32-bit hash
    uint32_t hash = 2166136261U;
    for (char c : line) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= 16777619U;
    }
    return hash;
}

uint32_t PrefixCache::HashLanguage(const std::string& languageId) {
    // Simple hash for language IDs (typically short strings)
    uint32_t hash = 0;
    for (char c : languageId) {
        hash = hash * 31 + static_cast<uint32_t>(static_cast<unsigned char>(c));
    }
    return hash;
}

// ---------------------------------------------------------------------------
// PrefixCache Implementation
// ---------------------------------------------------------------------------

PrefixCache::PrefixCache(size_t maxEntries, int64_t maxAgeMs)
    : m_maxEntries(maxEntries)
    , m_maxAgeMs(maxAgeMs)
    , m_hitCount(0)
    , m_missCount(0)
    , m_nextSequenceId(1)
    , m_lastKeystroke(std::chrono::steady_clock::now())
    , m_lastCursorMove(std::chrono::steady_clock::now())
{
}

PrefixCache::~PrefixCache() {
    // Nothing to clean up - all members have destructors
}

bool PrefixCache::Lookup(const PrefixCacheKey& key, PrefixCacheEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_missCount++;
        return false;
    }
    
    // Check age
    if (it->second.AgeMs() > m_maxAgeMs) {
        // Expired
        m_cache.erase(it);
        m_missCount++;
        return false;
    }
    
    // Found and valid
    it->second.hitCount++;
    entry = it->second;
    m_hitCount++;
    return true;
}

void PrefixCache::Store(const PrefixCacheKey& key, const PrefixCacheEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Evict if at capacity
    while (m_cache.size() >= m_maxEntries) {
        EvictOldest();
    }
    
    m_cache[key] = entry;
}

void PrefixCache::Invalidate(const PrefixCacheKey& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(key);
}

void PrefixCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
    m_prefetchQueue.clear();
}

uint64_t PrefixCache::QueuePrefetch(const std::string& prefix,
                                    const std::string& filePath,
                                    const std::string& languageId,
                                    const std::vector<std::string>& symbols) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PrefetchRequest req;
    req.key.fileHash = HashFile(filePath);
    req.key.lineHash = 0;  // Will be set when we have line content
    req.key.cursorPos = static_cast<uint32_t>(prefix.length());
    req.key.languageId = HashLanguage(languageId);
    req.prefix = prefix;
    req.filePath = filePath;
    req.languageId = languageId;
    req.symbols = symbols;
    req.state = PrefetchState::PENDING;
    req.startTime = std::chrono::steady_clock::now();
    req.sequenceId = m_nextSequenceId++;
    
    m_prefetchQueue.push_back(req);
    return req.sequenceId;
}

void PrefixCache::MarkPrefetchComplete(uint64_t sequenceId, const std::string& suggestion) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& req : m_prefetchQueue) {
        if (req.sequenceId == sequenceId) {
            req.state = PrefetchState::VALID;
            
            // Store in cache
            PrefixCacheEntry entry;
            entry.prefix = req.prefix;
            entry.suggestion = suggestion;
            entry.timestamp = std::chrono::steady_clock::now();
            entry.hitCount = 0;
            entry.confidence = 1.0f;
            entry.isStreaming = false;
            entry.isComplete = true;
            
            m_cache[req.key] = entry;
            break;
        }
    }
}

void PrefixCache::MarkPrefetchStale(uint64_t sequenceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& req : m_prefetchQueue) {
        if (req.sequenceId == sequenceId) {
            req.state = PrefetchState::STALE;
            break;
        }
    }
}

void PrefixCache::CancelPendingPrefetches() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& req : m_prefetchQueue) {
        if (req.state == PrefetchState::PENDING) {
            req.state = PrefetchState::STALE;
        }
    }
}

bool PrefixCache::HasValidPrefetch(const PrefixCacheKey& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& req : m_prefetchQueue) {
        if (req.key == key && req.state == PrefetchState::VALID) {
            return true;
        }
    }
    return false;
}

bool PrefixCache::GetPrefetchResult(const PrefixCacheKey& key, std::string& suggestion) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& req : m_prefetchQueue) {
        if (req.key == key && req.state == PrefetchState::VALID) {
            // Find in cache
            auto it = m_cache.find(key);
            if (it != m_cache.end() && it->second.AgeMs() < m_maxAgeMs) {
                suggestion = it->second.suggestion;
                return true;
            }
        }
    }
    return false;
}

size_t PrefixCache::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}

double PrefixCache::HitRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = m_hitCount + m_missCount;
    if (total == 0) return 0.0;
    return static_cast<double>(m_hitCount) / static_cast<double>(total);
}

void PrefixCache::OnKeystroke() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastKeystroke = std::chrono::steady_clock::now();
}

void PrefixCache::OnCursorMove() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastCursorMove = std::chrono::steady_clock::now();
}

bool PrefixCache::IsIdle(uint32_t thresholdMs) const {
    auto now = std::chrono::steady_clock::now();
    
    auto keystrokeAge = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastKeystroke).count();
    auto cursorAge = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastCursorMove).count();
    
    return keystrokeAge >= thresholdMs && cursorAge >= thresholdMs;
}

void PrefixCache::EvictOldest() {
    // Find oldest entry
    auto oldest = m_cache.end();
    auto oldestTime = std::chrono::steady_clock::time_point::max();
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.timestamp < oldestTime) {
            oldestTime = it->second.timestamp;
            oldest = it;
        }
    }
    
    if (oldest != m_cache.end()) {
        m_cache.erase(oldest);
    }
}

void PrefixCache::PruneExpired() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.timestamp).count();
        if (age > m_maxAgeMs) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// Global cache instance
// ---------------------------------------------------------------------------
PrefixCache& GetGlobalPrefixCache() {
    static PrefixCache instance(256, 30000);  // 256 entries, 30 second TTL
    return instance;
}

} // namespace RawrXD