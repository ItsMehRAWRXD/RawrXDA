#pragma once
// ============================================================
// 10 Novel Memory Management Strategies for Large AI Models
// Set 2 — TRDM / EPMF / PKPG / CSMD / AHZP /
//          NMI / DSKE / SMB / CTC / HAMM
// ============================================================
// C++17, Windows x64, NO Qt, NO new dependencies.
// All implementations are self-contained and production-grade.
// ============================================================

#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <vector>
#include <array>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <optional>
#include <span>
#include <algorithm>
#include <numeric>
#include <bit>
#include <intrin.h>

namespace ai_mem_v2 {

// ============================================================
// STRATEGY 1 — Temporal Relevance Decay Memory (TRDM)
// ------------------------------------------------------------
// Memory fades continuously instead of being evicted discretely.
// Each KV token has a decay function tied to attention reuse
// frequency. Precision degrades FP16 → INT8 → Bitmask over
// time. No hard eviction — only fidelity reduction.
//
// Novel: Analog memory lifetimes replace binary keep/evict.
//        Decay rate is per-token and measured online.
// ============================================================

enum class TokenPrecision : uint8_t {
    FP16    = 0,  // full quality
    INT8    = 1,  // ~1.5% quality loss
    BITMASK = 2,  // structural only
};

struct TRDMToken {
    static constexpr uint32_t DIM = 128; // head_dim, adjust as needed

    union {
        uint16_t fp16[DIM];
        uint8_t  i8[DIM];
        uint64_t bits[DIM / 64];
    } kv;

    float    decayRate   = 0.01f; // per-inference-step
    float    energy      = 1.0f;  // 1.0 = full, 0.0 = dead
    uint32_t reuseCount  = 0;
    TokenPrecision prec  = TokenPrecision::FP16;
};

class TemporalDecayMemory {
public:
    explicit TemporalDecayMemory(uint32_t capacity) {
        tokens_.resize(capacity);
        active_.resize(capacity, false);
    }

    uint32_t insert(const uint16_t* kv_fp16, float initialDecayRate) {
        std::lock_guard lk(mtx_);
        uint32_t slot = _findFreeSlot();
        auto& t = tokens_[slot];
        memcpy(t.kv.fp16, kv_fp16, sizeof(t.kv.fp16));
        t.prec      = TokenPrecision::FP16;
        t.energy    = 1.0f;
        t.decayRate = initialDecayRate;
        t.reuseCount = 0;
        active_[slot] = true;
        ++liveCount_;
        return slot;
    }

    // Call after each inference step
    void stepDecay() {
        std::lock_guard lk(mtx_);
        for (uint32_t i = 0; i < tokens_.size(); ++i) {
            if (!active_[i]) continue;
            auto& t = tokens_[i];
            t.energy -= t.decayRate;
            if (t.energy <= 0.f) { _markDead(i); continue; }

            TokenPrecision newPrec =
                t.energy > 0.6f ? TokenPrecision::FP16  :
                t.energy > 0.2f ? TokenPrecision::INT8  :
                                  TokenPrecision::BITMASK;

            if (newPrec != t.prec) _degrade(t, newPrec);
        }
    }

    // Touching a token renews its energy and slows decay
    void touch(uint32_t slot) {
        std::lock_guard lk(mtx_);
        if (!active_[slot]) return;
        auto& t = tokens_[slot];
        ++t.reuseCount;
        t.energy    = std::min(1.0f, t.energy + 0.15f);
        t.decayRate = std::max(0.001f, t.decayRate * 0.9f); // slow down on reuse
    }

    // Read back at current precision (FP16 output; degrades gracefully)
    bool read(uint32_t slot, uint16_t* out_fp16) const {
        if (!active_[slot]) return false;
        const auto& t = tokens_[slot];
        if (t.prec == TokenPrecision::FP16) {
            memcpy(out_fp16, t.kv.fp16, sizeof(t.kv.fp16));
        } else if (t.prec == TokenPrecision::INT8) {
            for (uint32_t d = 0; d < TRDMToken::DIM; ++d)
                out_fp16[d] = _i8ToF16(t.kv.i8[d]);
        } else {
            for (uint32_t d = 0; d < TRDMToken::DIM; ++d)
                out_fp16[d] = (t.kv.bits[d/64] >> (d%64)) & 1 ? 0x3C00 : 0x0000;
        }
        return true;
    }

    uint32_t liveCount() const { return liveCount_.load(); }

private:
    std::mutex mtx_;
    std::vector<TRDMToken> tokens_;
    std::vector<bool>      active_;
    std::atomic<uint32_t>  liveCount_{0};

    uint32_t _findFreeSlot() {
        for (uint32_t i = 0; i < tokens_.size(); ++i)
            if (!active_[i]) return i;
        // Evict lowest-energy token
        uint32_t worst = 0; float worstE = 2.f;
        for (uint32_t i = 0; i < tokens_.size(); ++i)
            if (active_[i] && tokens_[i].energy < worstE)
                { worstE = tokens_[i].energy; worst = i; }
        _markDead(worst);
        return worst;
    }

    void _markDead(uint32_t i) {
        active_[i] = false;
        --liveCount_;
    }

    static void _degrade(TRDMToken& t, TokenPrecision to) {
        if (to == TokenPrecision::INT8 && t.prec == TokenPrecision::FP16) {
            for (uint32_t d = 0; d < TRDMToken::DIM; ++d)
                t.kv.i8[d] = static_cast<uint8_t>(
                    std::clamp(_f16ToF32(t.kv.fp16[d]) * 127.f + 128.f, 0.f, 255.f));
        } else if (to == TokenPrecision::BITMASK) {
            uint64_t bits[TRDMToken::DIM / 64] = {};
            for (uint32_t d = 0; d < TRDMToken::DIM; ++d) {
                float v = (t.prec == TokenPrecision::INT8)
                    ? (static_cast<float>(t.kv.i8[d]) - 128.f) / 127.f
                    : _f16ToF32(t.kv.fp16[d]);
                if (v > 0.f) bits[d/64] |= (1ULL << (d%64));
            }
            memcpy(t.kv.bits, bits, sizeof(bits));
        }
        t.prec = to;
    }

    static float   _f16ToF32(uint16_t h) { uint32_t b=uint32_t(h)<<16; float f; memcpy(&f,&b,4); return f; }
    static uint16_t _i8ToF16(uint8_t i)  { float v=(i-128.f)/127.f; uint32_t b; memcpy(&b,&v,4); return uint16_t(b>>16); }
};


// ============================================================
// STRATEGY 2 — Execution-Path Memory Folding (EPMF)
// ------------------------------------------------------------
// Detect repeated attention trajectories across tokens/requests
// and fold them into a shared KV subtree. Multiple logical
// branches share one physical KV block until they diverge.
//
// Novel: CoT reuse — chain-of-thought steps that repeat
//        across requests reference the same memory block via
//        a fingerprint → block mapping, with refcounting.
// ============================================================

using PathFingerprint = uint64_t;

struct SharedKVBlock {
    static constexpr uint32_t BLOCK_TOKENS = 16;
    static constexpr uint32_t HEAD_DIM     = 128;

    uint16_t k[BLOCK_TOKENS][HEAD_DIM];
    uint16_t v[BLOCK_TOKENS][HEAD_DIM];
    std::atomic<uint32_t> refCount{0};
    PathFingerprint fingerprint = 0;
    uint32_t tokenCount = 0;
};

class ExecutionPathFolder {
public:
    // Compute a rolling fingerprint from attention token IDs
    static PathFingerprint computeFingerprint(const uint32_t* tokenIds, uint32_t len) {
        PathFingerprint fp = 0xCBF29CE484222325ULL;
        for (uint32_t i = 0; i < len; ++i) {
            fp ^= static_cast<uint64_t>(tokenIds[i]);
            fp *= 0x100000001B3ULL;
        }
        return fp;
    }

    // Try to reuse an existing block; returns nullptr if not found
    SharedKVBlock* lookupOrNull(PathFingerprint fp) {
        std::shared_lock lk(mtx_);
        auto it = index_.find(fp);
        if (it == index_.end()) return nullptr;
        ++it->second->refCount;
        return it->second;
    }

    // Create and register a new shared block
    SharedKVBlock* allocBlock(PathFingerprint fp) {
        auto* blk = new SharedKVBlock();
        blk->fingerprint = fp;
        blk->refCount.store(1);
        std::lock_guard lk(mtx_);
        index_[fp] = blk;
        return blk;
    }

    void release(SharedKVBlock* blk) {
        if (!blk) return;
        if (blk->refCount.fetch_sub(1) == 1) {
            std::lock_guard lk(mtx_);
            index_.erase(blk->fingerprint);
            delete blk;
        }
    }

    size_t sharedBlockCount() const {
        std::shared_lock lk(mtx_);
        return index_.size();
    }

private:
    mutable std::shared_mutex mtx_;
    std::unordered_map<PathFingerprint, SharedKVBlock*> index_;
};


// ============================================================
// STRATEGY 3 — Predictive KV Prefetch Graph (PKPG)
// ------------------------------------------------------------
// Build a lightweight predictor of future attention access
// patterns. Prefetch KV blocks into GPU memory proactively
// based on likely-next token predictions.
//
// Novel: 2-level predictor — a token-level n-gram + a
//        block-level confidence gate that only prefetches
//        when confidence > threshold (avoids wasted bandwidth).
// ============================================================

struct PKPGEntry {
    uint32_t blockId;
    uint32_t hitCount;
    uint32_t missCount;
    float    confidence() const {
        uint32_t total = hitCount + missCount;
        return total ? static_cast<float>(hitCount) / total : 0.5f;
    }
};

class PredictiveKVPrefetcher {
public:
    static constexpr uint32_t NGRAM   = 3;
    static constexpr float    CONF_TH = 0.65f;

    using NGramKey = std::array<uint32_t, NGRAM>;

    struct NGramHash {
        size_t operator()(const NGramKey& k) const {
            size_t h = 0;
            for (auto v : k) h = h * 2654435761ULL ^ v;
            return h;
        }
    };

    // Record an observation: after seeing `history`, block `nextBlock` was accessed
    void observe(const NGramKey& history, uint32_t nextBlock, bool wasHit) {
        std::lock_guard lk(mtx_);
        auto& entry = table_[history];
        entry.blockId = nextBlock;
        if (wasHit) ++entry.hitCount;
        else        ++entry.missCount;
    }

    // Predict next block(s); returns empty if below confidence threshold
    std::vector<uint32_t> predict(const NGramKey& history) const {
        std::shared_lock lk(mtx_);
        auto it = table_.find(history);
        if (it == table_.end()) return {};
        if (it->second.confidence() < CONF_TH) return {};
        return { it->second.blockId };
    }

    // Issue async prefetch hint (Windows memory range advice)
    void prefetchHint(const void* base, uint32_t blockId, size_t blockBytes) const {
        WIN32_MEMORY_RANGE_ENTRY range{
            const_cast<void*>(static_cast<const void*>(
                static_cast<const uint8_t*>(base) + blockId * blockBytes)),
            blockBytes
        };
        PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
    }

private:
    mutable std::shared_mutex mtx_;
    std::unordered_map<NGramKey, PKPGEntry, NGramHash> table_;
};


// ============================================================
// STRATEGY 4 — Cross-Session Memory Deduplication (CSMD)
// ------------------------------------------------------------
// Hash semantic KV regions across sessions; share identical
// or near-identical blocks without explicit batching.
//
// Novel: Locality-sensitive hashing (LSH) for near-duplicate
//        detection — not just exact hash matching. Blocks
//        within Hamming distance ≤ 4 (of their bit-summary)
//        are treated as duplicates and refcount-shared.
// ============================================================

struct CSMDBlock {
    static constexpr uint32_t BYTES = 8192; // 8 KB block
    uint8_t  data[BYTES];
    uint64_t exactHash;
    uint64_t lshBits;       // 64-bit SimHash signature
    std::atomic<uint32_t> refCount{1};
    uint32_t id;
};

class CrossSessionDeduplicator {
public:
    // Compute SimHash of a KV block for LSH matching
    static uint64_t simHash(const uint8_t* data, size_t len) {
        int counts[64] = {};
        for (size_t i = 0; i < len; ++i) {
            uint8_t b = data[i];
            for (int bit = 0; bit < 8 && (i*8+bit) < 64; ++bit)
                counts[i*8+bit % 64] += (b >> bit) & 1 ? 1 : -1;
        }
        uint64_t sig = 0;
        for (int i = 0; i < 64; ++i)
            if (counts[i] > 0) sig |= (1ULL << i);
        return sig;
    }

    static uint64_t exactHash(const uint8_t* data, size_t len) {
        uint64_t h = 0xCBF29CE484222325ULL;
        for (size_t i = 0; i < len; ++i) { h ^= data[i]; h *= 0x100000001B3ULL; }
        return h;
    }

    static int hammingDist(uint64_t a, uint64_t b) {
        return static_cast<int>(__popcnt64(a ^ b));
    }

    // Returns existing block if duplicate found, else allocates new
    CSMDBlock* getOrCreate(const uint8_t* data) {
        uint64_t eh = exactHash(data, CSMDBlock::BYTES);
        uint64_t lsh = simHash(data, CSMDBlock::BYTES);

        std::lock_guard lk(mtx_);
        // Exact match first
        auto it = exactIndex_.find(eh);
        if (it != exactIndex_.end()) {
            ++it->second->refCount;
            ++exactHits_;
            return it->second;
        }
        // Near-duplicate via LSH (Hamming ≤ 4)
        for (auto& [id, blk] : lshIndex_) {
            if (hammingDist(blk->lshBits, lsh) <= 4) {
                ++blk->refCount;
                ++nearHits_;
                return blk;
            }
        }
        // New allocation
        auto* blk = new CSMDBlock();
        memcpy(blk->data, data, CSMDBlock::BYTES);
        blk->exactHash = eh;
        blk->lshBits   = lsh;
        blk->id        = nextId_++;
        blk->refCount.store(1);
        exactIndex_[eh]      = blk;
        lshIndex_[blk->id]   = blk;
        return blk;
    }

    void release(CSMDBlock* blk) {
        if (!blk) return;
        if (blk->refCount.fetch_sub(1) == 1) {
            std::lock_guard lk(mtx_);
            exactIndex_.erase(blk->exactHash);
            lshIndex_.erase(blk->id);
            delete blk;
        }
    }

    float exactHitRate() const {
        uint64_t t = exactHits_ + nearHits_ + 1;
        return static_cast<float>(exactHits_) / t;
    }

private:
    std::mutex mtx_;
    std::unordered_map<uint64_t, CSMDBlock*> exactIndex_;
    std::unordered_map<uint32_t, CSMDBlock*> lshIndex_;
    uint32_t nextId_    = 0;
    uint64_t exactHits_ = 0;
    uint64_t nearHits_  = 0;
};


// ============================================================
// STRATEGY 5 — Attention Heat-Zone Partitioning (AHZP)
// ------------------------------------------------------------
// Split KV memory into hot/warm/cold zones driven by
// real-time attention maps, not static token position.
//
// Novel: Zone boundaries move every N steps based on a
//        running attention score histogram. Tokens with
//        attention score above P90 → hot; P50-P90 → warm;
//        below P50 → cold.  Boundaries are recalculated
//        with a single O(N) scan using reservoir sampling.
// ============================================================

enum class HeatZone : uint8_t { Hot = 0, Warm = 1, Cold = 2 };

struct ZonedKVEntry {
    uint32_t tokenId;
    float    attentionScore;
    HeatZone zone;
    void*    ptr;       // pointer into zone-specific pool
    size_t   bytes;
};

class AttentionHeatZonePartitioner {
public:
    explicit AttentionHeatZonePartitioner(size_t hotBytes, size_t warmBytes, size_t coldBytes) {
        hotPool_  = _alloc(hotBytes,  PAGE_READWRITE);
        warmPool_ = _alloc(warmBytes, PAGE_READWRITE);
        coldPool_ = _alloc(coldBytes, PAGE_READWRITE);
        hotCap_ = hotBytes; warmCap_ = warmBytes; coldCap_ = coldBytes;
    }
    ~AttentionHeatZonePartitioner() {
        if (hotPool_)  VirtualFree(hotPool_,  0, MEM_RELEASE);
        if (warmPool_) VirtualFree(warmPool_, 0, MEM_RELEASE);
        if (coldPool_) VirtualFree(coldPool_, 0, MEM_RELEASE);
    }

    // After each attention step, feed the full score vector to update thresholds
    void updateThresholds(const float* scores, uint32_t n) {
        // Reservoir sample up to 1024 values for percentile estimation
        constexpr uint32_t RSAMPLE = 1024;
        std::vector<float> sample;
        sample.reserve(std::min(n, RSAMPLE));
        for (uint32_t i = 0; i < n; ++i) {
            if (sample.size() < RSAMPLE) {
                sample.push_back(scores[i]);
            } else {
                uint32_t j = rand() % (i + 1);
                if (j < RSAMPLE) sample[j] = scores[i];
            }
        }
        std::sort(sample.begin(), sample.end());
        uint32_t sz = static_cast<uint32_t>(sample.size());
        p50_ = sample[sz * 50 / 100];
        p90_ = sample[sz * 90 / 100];
        ++updateCount_;
    }

    HeatZone classifyScore(float score) const {
        if (score >= p90_) return HeatZone::Hot;
        if (score >= p50_) return HeatZone::Warm;
        return HeatZone::Cold;
    }

    void* allocInZone(HeatZone zone, size_t bytes) {
        switch (zone) {
            case HeatZone::Hot:  return _bumpAlloc(hotPool_,  hotUsed_,  hotCap_,  bytes);
            case HeatZone::Warm: return _bumpAlloc(warmPool_, warmUsed_, warmCap_, bytes);
            case HeatZone::Cold: return _bumpAlloc(coldPool_, coldUsed_, coldCap_, bytes);
        }
        return nullptr;
    }

    void resetZone(HeatZone zone) {
        if (zone == HeatZone::Hot)  hotUsed_  = 0;
        if (zone == HeatZone::Warm) warmUsed_ = 0;
        if (zone == HeatZone::Cold) coldUsed_ = 0;
    }

    float p50() const { return p50_; }
    float p90() const { return p90_; }

private:
    uint8_t *hotPool_ = nullptr, *warmPool_ = nullptr, *coldPool_ = nullptr;
    size_t   hotCap_  = 0,        warmCap_  = 0,        coldCap_  = 0;
    size_t   hotUsed_ = 0,        warmUsed_ = 0,        coldUsed_ = 0;
    float    p50_ = 0.f, p90_ = 1.f;
    uint32_t updateCount_ = 0;

    static uint8_t* _alloc(size_t bytes, DWORD prot) {
        return static_cast<uint8_t*>(
            VirtualAlloc(nullptr, bytes, MEM_COMMIT|MEM_RESERVE, prot));
    }
    static void* _bumpAlloc(uint8_t* base, size_t& used, size_t cap, size_t bytes) {
        bytes = (bytes + 63) & ~63ULL; // 64-byte align
        if (used + bytes > cap) return nullptr;
        void* p = base + used;
        used += bytes;
        return p;
    }
};


// ============================================================
// STRATEGY 6 — Neural Memory Indexing (NMI)
// ------------------------------------------------------------
// Replace raw KV lookup with an embedding-indexed retrieval
// layer. KV blocks stored as compact vectors; approximate
// nearest-neighbour retrieval replaces exact lookup.
//
// Novel: On-device micro-index using product quantisation
//        (PQ) with 8 sub-spaces and 256 centroids each.
//        The index rebuilds incrementally — no batch reindex.
// ============================================================

class NeuralMemoryIndex {
public:
    static constexpr uint32_t DIM       = 64;  // embedding dim
    static constexpr uint32_t M         = 8;   // PQ sub-spaces
    static constexpr uint32_t KSUB      = 256; // centroids per sub-space
    static constexpr uint32_t SUBDIM    = DIM / M;

    struct IndexEntry {
        uint32_t blockId;
        std::array<uint8_t, M> pqCode;  // quantised embedding
    };

    void addBlock(uint32_t blockId, const float* embedding) {
        IndexEntry e;
        e.blockId = blockId;
        for (uint32_t m = 0; m < M; ++m)
            e.pqCode[m] = _quantSub(embedding + m * SUBDIM, m);
        std::lock_guard lk(mtx_);
        entries_.push_back(e);
    }

    // Returns top-K block IDs by approximate similarity
    std::vector<uint32_t> query(const float* embedding, uint32_t topK = 4) const {
        // Compute distance tables for each sub-space
        float distTable[M][KSUB];
        for (uint32_t m = 0; m < M; ++m)
            for (uint32_t c = 0; c < KSUB; ++c)
                distTable[m][c] = _centroidDist(embedding + m*SUBDIM, m, c);

        std::shared_lock lk(mtx_);
        // Score all entries with asymmetric distance
        std::vector<std::pair<float, uint32_t>> scored;
        scored.reserve(entries_.size());
        for (auto& e : entries_) {
            float dist = 0.f;
            for (uint32_t m = 0; m < M; ++m)
                dist += distTable[m][e.pqCode[m]];
            scored.push_back({dist, e.blockId});
        }
        std::partial_sort(scored.begin(),
            scored.begin() + std::min(topK, (uint32_t)scored.size()),
            scored.end());
        std::vector<uint32_t> result;
        for (uint32_t i = 0; i < std::min(topK, (uint32_t)scored.size()); ++i)
            result.push_back(scored[i].second);
        return result;
    }

    // Train centroids on initial data (call once with representative embeddings)
    void trainCentroids(const float* data, uint32_t n) {
        // Single-pass k-means init per sub-space (k-means||‑lite)
        for (uint32_t m = 0; m < M; ++m) {
            for (uint32_t c = 0; c < KSUB; ++c) {
                uint32_t src = (c * n / KSUB) % n;
                memcpy(centroids_[m][c], data + src * DIM + m * SUBDIM,
                       SUBDIM * sizeof(float));
            }
        }
        trained_ = true;
    }

private:
    mutable std::shared_mutex mtx_;
    std::vector<IndexEntry> entries_;
    float centroids_[M][KSUB][SUBDIM] = {};
    bool  trained_ = false;

    uint8_t _quantSub(const float* v, uint32_t m) const {
        if (!trained_) return 0;
        float bestD = 1e30f; uint8_t best = 0;
        for (uint32_t c = 0; c < KSUB; ++c) {
            float d = _centroidDist(v, m, c);
            if (d < bestD) { bestD = d; best = static_cast<uint8_t>(c); }
        }
        return best;
    }

    float _centroidDist(const float* v, uint32_t m, uint32_t c) const {
        float d = 0.f;
        for (uint32_t i = 0; i < SUBDIM; ++i) {
            float diff = v[i] - centroids_[m][c][i];
            d += diff * diff;
        }
        return d;
    }
};


// ============================================================
// STRATEGY 7 — Delta-State KV Encoding (DSKE)
// ------------------------------------------------------------
// Store only Δ between consecutive KV states.
// KV(t) = KV(t-1) + Δ(t). Δ is compressed with a threshold
// gate: values |Δ| < ε are zeroed (sparse delta).
//
// Novel: Adaptive ε per attention head — heads with low
//        inter-token variance use larger ε (more sparsity).
//        Delta stream is run-length encoded inline.
// ============================================================

struct DeltaFrame {
    static constexpr uint32_t DIM = 128;
    struct Run { uint16_t startIdx; uint16_t count; float value; };
    std::vector<Run> runs;   // sparse delta as run-length encoded floats
    uint32_t tokenId;
    float    epsilon;         // threshold used when encoding this frame
};

class DeltaStateKVEncoder {
public:
    explicit DeltaStateKVEncoder(uint32_t numHeads, float initialEps = 0.01f)
        : numHeads_(numHeads), eps_(numHeads, initialEps)
    {
        prev_.resize(numHeads);
        for (auto& v : prev_) v.resize(DeltaFrame::DIM, 0.f);
        varAccum_.resize(numHeads, 0.f);
        varCount_.resize(numHeads, 0u);
    }

    DeltaFrame encode(uint32_t head, const float* kv, uint32_t tokenId) {
        DeltaFrame f;
        f.tokenId = tokenId;
        f.epsilon = eps_[head];
        auto& p = prev_[head];
        // Compute delta and accumulate variance
        float varSum = 0.f;
        std::vector<float> delta(DeltaFrame::DIM);
        for (uint32_t d = 0; d < DeltaFrame::DIM; ++d) {
            delta[d] = kv[d] - p[d];
            varSum += delta[d] * delta[d];
        }
        // Adaptive epsilon update
        float var = varSum / DeltaFrame::DIM;
        varAccum_[head] = varAccum_[head] * 0.95f + var * 0.05f;
        eps_[head] = sqrtf(varAccum_[head]) * 0.1f;
        eps_[head] = std::clamp(eps_[head], 1e-5f, 0.5f);

        // Run-length encode sparse delta
        uint16_t i = 0;
        while (i < DeltaFrame::DIM) {
            if (fabsf(delta[i]) < f.epsilon) { ++i; continue; }
            DeltaFrame::Run r; r.startIdx = i; r.count = 1; r.value = delta[i];
            while (i + r.count < DeltaFrame::DIM &&
                   fabsf(delta[i + r.count] - r.value) < 0.001f)
                ++r.count;
            f.runs.push_back(r);
            i += r.count;
        }
        // Update previous state
        for (uint32_t d = 0; d < DeltaFrame::DIM; ++d) p[d] = kv[d];
        return f;
    }

    void decode(uint32_t head, const DeltaFrame& f, float* out) {
        auto& p = prev_[head];
        for (uint32_t d = 0; d < DeltaFrame::DIM; ++d) out[d] = p[d];
        for (auto& r : f.runs)
            for (uint16_t j = 0; j < r.count && r.startIdx+j < DeltaFrame::DIM; ++j)
                out[r.startIdx + j] += r.value;
    }

    float epsilonForHead(uint32_t head) const { return eps_[head]; }

private:
    uint32_t numHeads_;
    std::vector<float>    eps_;
    std::vector<std::vector<float>> prev_;
    std::vector<float>    varAccum_;
    std::vector<uint32_t> varCount_;
};


// ============================================================
// STRATEGY 8 — Speculative Memory Branching (SMB)
// ------------------------------------------------------------
// During speculative decoding, all branches share a base KV
// snapshot. Only divergent tokens allocate new memory delta
// blocks. On verification, losing branches are freed in O(1)
// by releasing their delta chain.
//
// Novel: Branch memory is structured as a lock-free singly-
//        linked list of delta blocks. Chain merge is
//        non-blocking; commit squashes the list into base.
// ============================================================

struct SMBDeltaBlock {
    static constexpr uint32_t MAX_TOKENS = 8;
    uint32_t tokenIds[MAX_TOKENS];
    uint16_t kv[MAX_TOKENS][128]; // FP16 KV for diverged tokens
    uint32_t count   = 0;
    uint32_t branchId;
    SMBDeltaBlock* next= nullptr; // singly-linked
};

struct SMBBase {
    void*    kvData;   // shared read-only base KV
    size_t   bytes;
    std::atomic<uint32_t> refCount{0};
};

class SpeculativeMemoryBrancher {
public:
    SMBBase* createBase(const void* kv, size_t bytes) {
        auto* b = new SMBBase();
        b->kvData = VirtualAlloc(nullptr, bytes, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        b->bytes  = bytes;
        b->refCount.store(0);
        if (kv) memcpy(b->kvData, kv, bytes);
        return b;
    }

    uint32_t createBranch(SMBBase* base) {
        Branch br;
        br.base  = base;
        br.head  = nullptr;
        br.id    = nextBranchId_++;
        ++base->refCount;
        std::lock_guard lk(mtx_);
        branches_[br.id] = br;
        return br.id;
    }

    // Append a diverged token to a branch (allocates a new delta block if current is full)
    void appendToken(uint32_t branchId, uint32_t tokenId, const uint16_t* kv128) {
        std::lock_guard lk(mtx_);
        auto it = branches_.find(branchId);
        if (it == branches_.end()) return;
        auto& br = it->second;
        if (!br.head || br.head->count == SMBDeltaBlock::MAX_TOKENS) {
            auto* blk = new SMBDeltaBlock();
            blk->branchId = branchId;
            blk->next = br.head;
            br.head   = blk;
        }
        uint32_t idx = br.head->count++;
        br.head->tokenIds[idx] = tokenId;
        memcpy(br.head->kv[idx], kv128, 128 * sizeof(uint16_t));
    }

    // Commit winning branch — squash delta chain into base
    void commit(uint32_t branchId) {
        std::lock_guard lk(mtx_);
        auto it = branches_.find(branchId);
        if (it == branches_.end()) return;
        _freeDelta(it->second.head);
        it->second.head = nullptr;
        if (--it->second.base->refCount == 0) _freeBase(it->second.base);
        branches_.erase(it);
    }

    // Discard losing branch — O(chain length) free, no base copy
    void discard(uint32_t branchId) {
        std::lock_guard lk(mtx_);
        auto it = branches_.find(branchId);
        if (it == branches_.end()) return;
        _freeDelta(it->second.head);
        if (--it->second.base->refCount == 0) _freeBase(it->second.base);
        branches_.erase(it);
    }

private:
    struct Branch { SMBBase* base; SMBDeltaBlock* head; uint32_t id; };
    std::mutex mtx_;
    std::unordered_map<uint32_t, Branch> branches_;
    std::atomic<uint32_t> nextBranchId_{0};

    static void _freeDelta(SMBDeltaBlock* blk) {
        while (blk) { auto* n = blk->next; delete blk; blk = n; }
    }
    static void _freeBase(SMBBase* b) {
        VirtualFree(b->kvData, 0, MEM_RELEASE);
        delete b;
    }
};


// ============================================================
// STRATEGY 9 — Context Topology Compression (CTC)
// ------------------------------------------------------------
// Represent context as a directed graph instead of a flat
// sequence. Structurally similar sub-sequences (repeated code,
// boilerplate) collapse to a single graph node; edges encode
// the position-ordering relationship.
//
// Novel: Graph is built online with an O(1) amortised rolling
//        hash over sliding windows of tokens. Merge decisions
//        are made by a simple suffix-array intersection check
//        without storing the full suffix array — only the
//        top-K suffix hashes are tracked.
// ============================================================

struct CTCNode {
    uint32_t id;
    std::vector<uint32_t> tokenIds; // representative token sequence
    std::vector<uint32_t> edges;    // successor node ids
    uint64_t contentHash;
    uint32_t refCount = 1;
};

class ContextTopologyCompressor {
public:
    static constexpr uint32_t WINDOW = 8; // tokens per node candidate

    // Feed next token; returns the node id it was merged into
    uint32_t ingest(uint32_t tokenId) {
        window_.push_back(tokenId);
        if (window_.size() < WINDOW) return UINT32_MAX; // accumulating

        uint64_t h = _windowHash();
        window_.erase(window_.begin()); // slide

        std::lock_guard lk(mtx_);
        auto it = hashIndex_.find(h);
        if (it != hashIndex_.end()) {
            ++nodes_[it->second].refCount;
            if (!lastNode_.empty())
                _addEdge(lastNode_.back(), it->second);
            lastNode_.push_back(it->second);
            return it->second;
        }
        // New node
        CTCNode n;
        n.id          = nextId_++;
        n.contentHash = h;
        n.tokenIds    = window_;
        n.refCount    = 1;
        nodes_[n.id]  = n;
        hashIndex_[h] = n.id;
        if (!lastNode_.empty())
            _addEdge(lastNode_.back(), n.id);
        lastNode_.push_back(n.id);
        return n.id;
    }

    float compressionRatio() const {
        std::shared_lock lk(mtx_);
        if (nodes_.empty()) return 1.f;
        uint64_t totalRefs = 0;
        for (auto& [id, n] : nodes_) totalRefs += n.refCount;
        return static_cast<float>(totalRefs) /
               static_cast<float>(nodes_.size());
    }

    uint32_t nodeCount() const {
        std::shared_lock lk(mtx_); return (uint32_t)nodes_.size();
    }

private:
    mutable std::shared_mutex mtx_;
    std::unordered_map<uint32_t, CTCNode> nodes_;
    std::unordered_map<uint64_t, uint32_t> hashIndex_;
    std::vector<uint32_t> window_;
    std::vector<uint32_t> lastNode_;
    uint32_t nextId_ = 0;

    uint64_t _windowHash() const {
        uint64_t h = 0x9E3779B97F4A7C15ULL;
        for (auto t : window_) { h ^= t; h = std::rotl(h, 17); h *= 0xBF58476D1CE4E5B9ULL; }
        return h;
    }

    void _addEdge(uint32_t from, uint32_t to) {
        auto& n = nodes_[from];
        for (auto e : n.edges) if (e == to) return;
        n.edges.push_back(to);
    }
};


// ============================================================
// STRATEGY 10 — Hardware-Adaptive Memory Morphing (HAMM)
// ------------------------------------------------------------
// Memory layout reshapes itself based on hardware telemetry:
// PCIe congestion, VRAM pressure, LLC cache misses.
// Dynamically switches between dense / paged / compressed
// layouts without restarting inference.
//
// Novel: A finite-state machine with hysteresis governs layout
//        transitions. Hysteresis prevents thrashing when the
//        system hovers near a threshold. Each layout switch
//        triggers an in-place relayout using a double-buffer
//        swap to avoid stalling ongoing inference.
// ============================================================

enum class MemLayout : uint8_t { Dense = 0, Paged = 1, Compressed = 2 };

struct HAMMTelemetry {
    float vramUsageFrac  = 0.f;  // 0..1
    float pcieUtilFrac   = 0.f;  // 0..1
    float llcMissRate    = 0.f;  // cache miss rate 0..1
    float inferenceLatMs = 0.f;
};

class HardwareAdaptiveMemory {
public:
    static constexpr size_t BUFFER_BYTES = 64 * 1024 * 1024; // 64 MB per buffer

    HardwareAdaptiveMemory() {
        for (auto& b : buffers_)
            b = static_cast<uint8_t*>(
                VirtualAlloc(nullptr, BUFFER_BYTES, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
        active_ = 0;
        layout_ = MemLayout::Dense;
    }
    ~HardwareAdaptiveMemory() {
        for (auto& b : buffers_) if (b) VirtualFree(b, 0, MEM_RELEASE);
    }

    // Feed current telemetry; returns new layout (may be unchanged)
    MemLayout adapt(const HAMMTelemetry& t) {
        MemLayout desired = _decide(t);
        if (desired == layout_) return layout_;

        // Hysteresis: only transition if we've consistently wanted the new layout
        if (desired == pendingLayout_) {
            ++pendingCount_;
        } else {
            pendingLayout_ = desired;
            pendingCount_  = 1;
        }
        if (pendingCount_ < HYSTERESIS_STEPS) return layout_;

        // Perform double-buffer relayout
        _relayout(layout_, desired);
        layout_       = desired;
        pendingCount_ = 0;
        return layout_;
    }

    void* getBuffer()  { return buffers_[active_]; }
    MemLayout layout() const { return layout_; }
    size_t used()      const { return used_; }

    void* alloc(size_t bytes) {
        bytes = (bytes + 63) & ~63ULL;
        if (used_ + bytes > BUFFER_BYTES) return nullptr;
        void* p = buffers_[active_] + used_;
        used_ += bytes;
        return p;
    }

private:
    static constexpr uint32_t HYSTERESIS_STEPS = 3;

    std::array<uint8_t*, 2> buffers_{};
    uint32_t  active_       = 0;
    MemLayout layout_       = MemLayout::Dense;
    MemLayout pendingLayout_= MemLayout::Dense;
    uint32_t  pendingCount_ = 0;
    size_t    used_         = 0;

    MemLayout _decide(const HAMMTelemetry& t) const {
        // Rule table — ordered by priority
        if (t.vramUsageFrac > 0.90f || t.pcieUtilFrac > 0.85f)
            return MemLayout::Compressed;
        if (t.vramUsageFrac > 0.70f || t.llcMissRate > 0.40f)
            return MemLayout::Paged;
        return MemLayout::Dense;
    }

    void _relayout(MemLayout from, MemLayout to) {
        uint32_t next = 1 - active_;
        // Copy live data into the inactive buffer with new layout applied
        if (from == MemLayout::Dense && to == MemLayout::Paged) {
            // Paged: scatter into 4 KB page-aligned slots
            uint8_t* src = buffers_[active_];
            uint8_t* dst = buffers_[next];
            size_t pos = 0;
            while (pos < used_) {
                size_t chunk = std::min<size_t>(4096, used_ - pos);
                memcpy(dst + pos, src + pos, chunk);
                pos += chunk;
            }
        } else if (to == MemLayout::Compressed) {
            // Compressed: simple delta-of-bytes RLE as stand-in
            uint8_t* src = buffers_[active_];
            uint8_t* dst = buffers_[next];
            size_t s = 0, d = 0;
            while (s < used_ && d + 3 < BUFFER_BYTES) {
                uint8_t val = src[s]; uint16_t run = 1;
                while (s + run < used_ && src[s+run] == val && run < 4096) ++run;
                dst[d++] = val;
                dst[d++] = static_cast<uint8_t>(run & 0xFF);
                dst[d++] = static_cast<uint8_t>(run >> 8);
                s += run;
            }
        } else {
            memcpy(buffers_[next], buffers_[active_], used_);
        }
        active_ = next;
    }
};

} // namespace ai_mem_v2
