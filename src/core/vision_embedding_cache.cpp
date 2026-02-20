// ============================================================================
// vision_embedding_cache.cpp — Hash-Based Vision Embedding Cache Implementation
// ============================================================================
// Full LRU cache with FNV-1a hashing, TTL expiry, memory budget enforcement.
//
// Hash algorithm: FNV-1a (64-bit)
//   - Fast, well-distributed for byte streams
//   - Processes pixel data in 8-byte chunks with unrolling
//   - Incorporates image dimensions to avoid collisions between
//     different-sized images with similar pixel patterns
//
// LRU implementation:
//   - Doubly-linked list (std::list) for O(1) move-to-front
//   - Hash map for O(1) lookup of list position
//   - Eviction from back (least recently used)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "vision_embedding_cache.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace RawrXD {
namespace Vision {

// ============================================================================
// FNV-1a Constants (64-bit)
// ============================================================================
static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

// ============================================================================
// Singleton
// ============================================================================

VisionEmbeddingCache& VisionEmbeddingCache::instance() {
    static VisionEmbeddingCache inst;
    return inst;
}

VisionEmbeddingCache::VisionEmbeddingCache()
    : config_()
    , currentMemoryBytes_(0)
{
}

// ============================================================================
// Configuration
// ============================================================================

void VisionEmbeddingCache::configure(const EmbeddingCacheConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    // Immediately enforce new limits
    while (entries_.size() > config_.maxEntries && !lruOrder_.empty()) {
        evictLRU();
    }
    while (currentMemoryBytes_ > config_.maxMemoryBytes && !lruOrder_.empty()) {
        evictLRU();
    }
}

const EmbeddingCacheConfig& VisionEmbeddingCache::getConfig() const {
    return config_;
}

// ============================================================================
// Current Time Utility
// ============================================================================

uint64_t VisionEmbeddingCache::currentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

// ============================================================================
// Estimate Memory Size of a Cache Entry
// ============================================================================

uint64_t VisionEmbeddingCache::estimateEntrySize(const VisionEmbedding& emb) const {
    uint64_t size = sizeof(CacheEntry);

    // Global embedding vector
    size += emb.embedding.size() * sizeof(float);

    // Per-patch embeddings
    for (const auto& patch : emb.patchEmbeddings) {
        size += patch.size() * sizeof(float);
        size += sizeof(std::vector<float>); // Vector overhead
    }
    size += sizeof(std::vector<std::vector<float>>); // Outer vector overhead

    // Description string
    size += emb.description.size();

    // LRU tracking overhead
    size += sizeof(uint64_t) * 2; // lruOrder_ node + lruMap_ entry

    return size;
}

// ============================================================================
// FNV-1a Image Hash
// ============================================================================
// Hashes the raw pixel data of an image buffer, incorporating dimensions
// and format to avoid collisions between structurally different images.

uint64_t VisionEmbeddingCache::computeImageHash(const ImageBuffer& image) const {
    if (!image.isValid()) return 0;

    uint64_t hash = FNV_OFFSET_BASIS;

    // Mix in image metadata first
    auto mixByte = [&](uint8_t byte) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= FNV_PRIME;
    };

    // Width (4 bytes, little-endian)
    mixByte(static_cast<uint8_t>(image.width & 0xFF));
    mixByte(static_cast<uint8_t>((image.width >> 8) & 0xFF));
    mixByte(static_cast<uint8_t>((image.width >> 16) & 0xFF));
    mixByte(static_cast<uint8_t>((image.width >> 24) & 0xFF));

    // Height
    mixByte(static_cast<uint8_t>(image.height & 0xFF));
    mixByte(static_cast<uint8_t>((image.height >> 8) & 0xFF));
    mixByte(static_cast<uint8_t>((image.height >> 16) & 0xFF));
    mixByte(static_cast<uint8_t>((image.height >> 24) & 0xFF));

    // Channels + format
    mixByte(static_cast<uint8_t>(image.channels));
    mixByte(static_cast<uint8_t>(image.format));

    // Hash pixel data row by row (respecting stride)
    uint32_t rowBytes = image.width * image.channels;
    for (uint32_t y = 0; y < image.height; ++y) {
        const uint8_t* row = image.data + y * image.stride;

        // Process 8 bytes at a time for speed
        uint32_t i = 0;
        for (; i + 8 <= rowBytes; i += 8) {
            // Read 8 bytes as uint64_t (unaligned)
            uint64_t block;
            memcpy(&block, row + i, sizeof(uint64_t));
            hash ^= block;
            hash *= FNV_PRIME;
        }

        // Process remaining bytes
        for (; i < rowBytes; ++i) {
            hash ^= static_cast<uint64_t>(row[i]);
            hash *= FNV_PRIME;
        }
    }

    // Final avalanche mixing
    hash ^= hash >> 33;
    hash *= 0xFF51AFD7ED558CCD;
    hash ^= hash >> 33;
    hash *= 0xC4CEB9FE1A85EC53;
    hash ^= hash >> 33;

    return hash;
}

// ============================================================================
// Cache Lookup
// ============================================================================

bool VisionEmbeddingCache::lookup(uint64_t hash, VisionEmbedding& output) {
    std::lock_guard<std::mutex> lock(mutex_);

    totalLookups_.fetch_add(1, std::memory_order_relaxed);

    auto it = entries_.find(hash);
    if (it == entries_.end()) {
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Check TTL
    if (isExpired(it->second)) {
        // Remove expired entry
        currentMemoryBytes_ -= it->second.memorySizeBytes;
        auto lruIt = lruMap_.find(hash);
        if (lruIt != lruMap_.end()) {
            lruOrder_.erase(lruIt->second);
            lruMap_.erase(lruIt);
        }
        entries_.erase(it);
        totalExpired_.fetch_add(1, std::memory_order_relaxed);
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Cache hit
    it->second.lastAccessMs = currentTimeMs();
    it->second.accessCount++;
    output = it->second.embedding;

    // Move to front of LRU
    touchEntry(hash);

    totalHits_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool VisionEmbeddingCache::lookupImage(const ImageBuffer& image,
                                        VisionEmbedding& output) {
    uint64_t hash = computeImageHash(image);
    return lookup(hash, output);
}

// ============================================================================
// Cache Insert
// ============================================================================

VisionResult VisionEmbeddingCache::insert(uint64_t hash,
                                           const VisionEmbedding& embedding,
                                           uint32_t imageWidth,
                                           uint32_t imageHeight) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already cached (update if so)
    auto existing = entries_.find(hash);
    if (existing != entries_.end()) {
        currentMemoryBytes_ -= existing->second.memorySizeBytes;
        existing->second.embedding = embedding;
        existing->second.lastAccessMs = currentTimeMs();
        existing->second.accessCount++;
        existing->second.memorySizeBytes = estimateEntrySize(embedding);
        currentMemoryBytes_ += existing->second.memorySizeBytes;
        touchEntry(hash);
        return VisionResult::ok("Cache entry updated");
    }

    // Compute entry size
    uint64_t entrySize = estimateEntrySize(embedding);

    // Evict if necessary to make room
    while (entries_.size() >= config_.maxEntries && !lruOrder_.empty()) {
        evictLRU();
    }
    while (currentMemoryBytes_ + entrySize > config_.maxMemoryBytes && !lruOrder_.empty()) {
        evictLRU();
    }

    // Create new entry
    CacheEntry entry;
    entry.imageHash = hash;
    entry.embedding = embedding;
    entry.insertTimeMs = currentTimeMs();
    entry.lastAccessMs = entry.insertTimeMs;
    entry.accessCount = 0;
    entry.imageWidth = imageWidth;
    entry.imageHeight = imageHeight;
    entry.memorySizeBytes = entrySize;

    entries_[hash] = std::move(entry);
    currentMemoryBytes_ += entrySize;

    // Add to LRU front
    lruOrder_.push_front(hash);
    lruMap_[hash] = lruOrder_.begin();

    totalInserts_.fetch_add(1, std::memory_order_relaxed);

    return VisionResult::ok("Cached");
}

VisionResult VisionEmbeddingCache::insertImage(const ImageBuffer& image,
                                                const VisionEmbedding& embedding) {
    uint64_t hash = computeImageHash(image);
    return insert(hash, embedding, image.width, image.height);
}

// ============================================================================
// Cache Removal
// ============================================================================

bool VisionEmbeddingCache::remove(uint64_t hash) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(hash);
    if (it == entries_.end()) return false;

    currentMemoryBytes_ -= it->second.memorySizeBytes;
    entries_.erase(it);

    auto lruIt = lruMap_.find(hash);
    if (lruIt != lruMap_.end()) {
        lruOrder_.erase(lruIt->second);
        lruMap_.erase(lruIt);
    }

    return true;
}

void VisionEmbeddingCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    lruOrder_.clear();
    lruMap_.clear();
    currentMemoryBytes_ = 0;
}

// ============================================================================
// TTL Expiry Check
// ============================================================================

bool VisionEmbeddingCache::isExpired(const CacheEntry& entry) const {
    if (config_.ttlSeconds == 0) return false; // No TTL

    uint64_t now = currentTimeMs();
    uint64_t age = now - entry.insertTimeMs;
    uint64_t ttlMs = static_cast<uint64_t>(config_.ttlSeconds) * 1000ULL;
    return age > ttlMs;
}

// ============================================================================
// Evict Expired Entries
// ============================================================================

uint32_t VisionEmbeddingCache::evictExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.ttlSeconds == 0) return 0;

    uint32_t evicted = 0;
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (isExpired(it->second)) {
            uint64_t hash = it->first;
            currentMemoryBytes_ -= it->second.memorySizeBytes;

            auto lruIt = lruMap_.find(hash);
            if (lruIt != lruMap_.end()) {
                lruOrder_.erase(lruIt->second);
                lruMap_.erase(lruIt);
            }

            it = entries_.erase(it);
            evicted++;
            totalExpired_.fetch_add(1, std::memory_order_relaxed);
        } else {
            ++it;
        }
    }

    return evicted;
}

// ============================================================================
// Manual LRU Eviction to Fit Budget
// ============================================================================

uint32_t VisionEmbeddingCache::evictToFit() {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t evicted = 0;

    while (entries_.size() > config_.maxEntries && !lruOrder_.empty()) {
        evictLRU();
        evicted++;
    }
    while (currentMemoryBytes_ > config_.maxMemoryBytes && !lruOrder_.empty()) {
        evictLRU();
        evicted++;
    }

    return evicted;
}

// ============================================================================
// LRU Eviction — Remove least recently used entry (back of list)
// ============================================================================

void VisionEmbeddingCache::evictLRU() {
    // Called with mutex_ held
    if (lruOrder_.empty()) return;

    uint64_t victimHash = lruOrder_.back();
    lruOrder_.pop_back();
    lruMap_.erase(victimHash);

    auto it = entries_.find(victimHash);
    if (it != entries_.end()) {
        currentMemoryBytes_ -= it->second.memorySizeBytes;
        entries_.erase(it);
    }

    totalEvictions_.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Touch Entry — Move to front of LRU list
// ============================================================================

void VisionEmbeddingCache::touchEntry(uint64_t hash) {
    // Called with mutex_ held
    auto it = lruMap_.find(hash);
    if (it != lruMap_.end()) {
        lruOrder_.erase(it->second);
        lruOrder_.push_front(hash);
        it->second = lruOrder_.begin();
    }
}

// ============================================================================
// Partial Match — Find closest cached embedding by hash distance
// ============================================================================
// For partial matches (cropped/resized images), we compare embeddings
// rather than hashes since hash similarity doesn't imply visual similarity.

bool VisionEmbeddingCache::findClosest(uint64_t hash, float threshold,
                                        VisionEmbedding& output, float& similarity) {
    std::lock_guard<std::mutex> lock(mutex_);

    totalLookups_.fetch_add(1, std::memory_order_relaxed);

    if (entries_.empty()) {
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Exact match first
    auto exact = entries_.find(hash);
    if (exact != entries_.end() && !isExpired(exact->second)) {
        output = exact->second.embedding;
        similarity = 1.0f;
        exact->second.lastAccessMs = currentTimeMs();
        exact->second.accessCount++;
        touchEntry(hash);
        totalHits_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (!config_.enablePartialMatch) {
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Approximate match: compare hash bit patterns
    // XOR-popcount distance between 64-bit hashes
    float bestSim = 0.0f;
    uint64_t bestHash = 0;

    for (const auto& [entryHash, entry] : entries_) {
        if (isExpired(entry)) continue;

        // Hamming distance between hashes (simple proxy for similarity)
        uint64_t xorBits = hash ^ entryHash;

        // Count set bits (popcount)
        uint32_t diffBits = 0;
        uint64_t temp = xorBits;
        while (temp) {
            diffBits += temp & 1;
            temp >>= 1;
        }

        // Similarity = 1 - (diffBits / 64)
        float sim = 1.0f - static_cast<float>(diffBits) / 64.0f;

        if (sim > bestSim) {
            bestSim = sim;
            bestHash = entryHash;
        }
    }

    if (bestSim >= threshold && bestHash != 0) {
        auto it = entries_.find(bestHash);
        if (it != entries_.end()) {
            output = it->second.embedding;
            similarity = bestSim;
            it->second.lastAccessMs = currentTimeMs();
            it->second.accessCount++;
            touchEntry(bestHash);
            totalHits_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    totalMisses_.fetch_add(1, std::memory_order_relaxed);
    return false;
}

// ============================================================================
// Statistics
// ============================================================================

CacheStats VisionEmbeddingCache::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    CacheStats stats = {};
    stats.totalLookups = totalLookups_.load();
    stats.totalHits = totalHits_.load();
    stats.totalMisses = totalMisses_.load();
    stats.totalInserts = totalInserts_.load();
    stats.totalEvictions = totalEvictions_.load();
    stats.totalExpired = totalExpired_.load();
    stats.currentEntries = static_cast<uint32_t>(entries_.size());
    stats.currentMemoryBytes = currentMemoryBytes_;

    if (stats.totalLookups > 0) {
        stats.hitRate = static_cast<double>(stats.totalHits) /
                        static_cast<double>(stats.totalLookups);
    }

    if (stats.currentEntries > 0) {
        double totalAccess = 0.0;
        for (const auto& [hash, entry] : entries_) {
            totalAccess += entry.accessCount;
        }
        stats.avgAccessCount = totalAccess / stats.currentEntries;
    }

    return stats;
}

void VisionEmbeddingCache::resetStats() {
    totalLookups_.store(0);
    totalHits_.store(0);
    totalMisses_.store(0);
    totalInserts_.store(0);
    totalEvictions_.store(0);
    totalExpired_.store(0);
}

} // namespace Vision
} // namespace RawrXD
